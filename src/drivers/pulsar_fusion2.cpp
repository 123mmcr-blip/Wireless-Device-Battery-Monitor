// Pulsar Fusion2 mice (Xlite v4, X3, X2 v3 ...) on the Pulsar 1K wireless dongle (VID 3710, PID 5402).
// Older Pulsar receivers answer the 17-byte report 08 protocol in kysona.cpp; this dongle does not, and
// speaks the Sonix-style 64-byte feature report protocol of Pulsar Fusion2 instead.
// Protocol from github.com/packerlschupfer/pulsar-mouse-linux (MIT), drivers/xlite_v4_wireless.py and
// drivers/x2a.py, decoded there from USBPcap captures of Fusion2:
//   Interface 3 (collection FFFF:0001), feature report id 0, 64-byte packet:
//     [0] direction (00 = request)  [1] category  [2] register (bit 7 set = read)  [3] sub-register
//     [4..5] 0  [6] profile (0 = global)  [7..61] data  [62..63] sum(bytes 0..61) & 0xFFFF, little-endian
//   Windows prepends the report id, so every index below is the packet's + 1.
//   01/84/04 -> [6..7] VID, [8..9] PID of the paired mouse (LE), answered by the dongle itself
//   08/81/01 -> [6] battery percent, FF = no mouse behind the dongle
//   08/83/01 -> [6] 1 while the mouse is on its charging cable
//   After SET_FEATURE, GET_FEATURE is re-polled about every 10 ms. [0] of the reply is 01 = answered,
//   02 = request echoed, still pending, 05 = no link to the mouse (asleep or out of range; never answered,
//   so it ends the exchange). Only a reply echoing the request's category and register is ours: the reply
//   buffer belongs to the dongle and may hold another program's answer. A request is never re-sent while
//   it is pending (the dongle drops it and later hands its answer to the next read); it is re-sent only
//   after the buffer has gone back to an older answer for several polls, which is how the dongle loses a
//   request with the mouse moving. Nor is a request sent while the dongle shows any request pending:
//   a dongle stuck on 02 is left alone rather than given more to drop.
// The dongle must be claimed even when it does not answer: one round of kysona.cpp's 17-byte report 08
// probe on its vendor interfaces left every read relayed to the mouse (Fusion2's too) pending on 02 until
// the dongle was replugged and the mouse power-cycled. Reads the dongle answers itself (01/84/04) still
// worked. Seen on an Xlite v4 behind the dongle, Windows 11.
// Only reads are sent. One absolute deadline of 1 s covers all requests to the dongle.
#include "drivers.h"

namespace {

constexpr USHORT kVid = 0x3710;
constexpr USHORT kPid = 0x5402;
constexpr int kReportLen = 65;   // report id + 64-byte packet
constexpr int kDeadlineMs = 1000;
constexpr int kPollMs = 10;
constexpr int kStalePolls = 5;    // polls showing an older answer before the request counts as lost
constexpr int kStaleResends = 2;  // resends allowed per request
constexpr int kResendRoomMs = 350;  // no resend this close to the deadline: it would be left pending

constexpr BYTE kAnswered = 0x01;
constexpr BYTE kPending = 0x02;
constexpr BYTE kNoLink = 0x05;

enum class Reply { Ok, NoLink, NoReply, IoError };

std::vector<BYTE> BuildRead(BYTE cat, BYTE reg, BYTE sub) {
    std::vector<BYTE> buf(kReportLen, 0);  // buf[0] = report id 0
    BYTE* p = buf.data() + 1;
    p[1] = cat;
    p[2] = reg | 0x80;
    p[3] = sub;
    unsigned sum = 0;
    for (int i = 0; i < 62; i++) sum += p[i];
    p[62] = (BYTE)(sum & 0xFF);
    p[63] = (BYTE)((sum >> 8) & 0xFF);
    return buf;
}

// Category and register match. The profile is 0 for every request here, so it is not compared.
bool Echoes(const std::vector<BYTE>& rsp, const std::vector<BYTE>& req) {
    return req.size() >= 4 && rsp.size() >= 4 && rsp[2] == req[2] && rsp[3] == req[3];
}

class Dongle {
public:
    Dongle(const HidColl& c, ULONGLONG deadline) : c_(c), deadline_(deadline) {}

    bool Open() { return d_.OpenFeatureOnly(c_); }

    // On success `rsp` holds the reply; rsp[1 + i] is byte i of the 64-byte packet.
    Reply Read(BYTE cat, BYTE reg, BYTE sub, std::vector<BYTE>& rsp) {
        std::vector<BYTE> req = BuildRead(cat, reg, sub);
        std::vector<BYTE> prev = prev_;
        prev_ = req;
        if (!WaitIdle(rsp)) return Reply::NoReply;
        if (!Send(req)) return Reply::IoError;
        int sends = 1, stale = 0;
        bool shown = false;
        for (;;) {
            if (!d_.GetFeature(0, rsp) || rsp.size() < kReportLen) return Reply::IoError;
            BYTE status = rsp[1];
            if (Echoes(rsp, req)) {
                stale = 0;
                if (status == kAnswered) return Reply::Ok;
                if (status == kNoLink) return Reply::NoLink;
                shown = true;  // pending: wait, never re-send
            } else if (status == kAnswered && (shown || Echoes(rsp, prev))) {
                // The buffer went back to an older answer: the dongle lost our request.
                if (++stale >= kStalePolls && sends <= kStaleResends &&
                    GetTickCount64() + kResendRoomMs < deadline_) {
                    if (!Send(req)) return Reply::IoError;
                    sends++;
                    stale = 0;
                }
            } else {
                stale = 0;
            }
            if (GetTickCount64() >= deadline_) return Reply::NoReply;
            Sleep(kPollMs);
        }
    }

private:
    // Waits while the dongle shows any request pending (ours from an earlier poll that timed out, or
    // another program's). A request sent on top of a pending one is dropped and can leave the dongle
    // stuck on 02, so nothing is sent until it settles. False when it is still pending at the deadline.
    bool WaitIdle(std::vector<BYTE>& rsp) {
        for (;;) {
            if (!d_.GetFeature(0, rsp) || rsp.size() < kReportLen || rsp[1] != kPending) return true;
            if (GetTickCount64() >= deadline_) return false;
            Sleep(kPollMs);
        }
    }

    // A zero-access handle is enough for feature reports on most collections; fall back to a
    // read/write handle once if the dongle refuses it.
    bool Send(const std::vector<BYTE>& req) {
        if (d_.SetFeature(req)) return true;
        if (triedReadWrite_) return false;
        triedReadWrite_ = true;
        return d_.Open(c_) && d_.SetFeature(req);
    }

    const HidColl& c_;
    ULONGLONG deadline_;
    HidDev d_;
    std::vector<BYTE> prev_;
    bool triedReadWrite_ = false;
};

// Cabled PIDs the dongle has been seen reporting for the mouse it carries (pulsar-mouse-linux).
const wchar_t* MouseName(USHORT vid, USHORT pid) {
    if (vid != kVid) return nullptr;
    switch (pid) {
        case 0x3401: return L"Pulsar Xlite v4";
        case 0x3409: return L"Pulsar X3";
        default: return nullptr;
    }
}

}  // namespace

void PollPulsarFusion2(PollContext& ctx) {
    if (ctx.IsClaimed(kVid, kPid)) return;
    bool present = false;
    for (const auto& c : ctx.colls) present |= c.vid == kVid && c.pid == kPid;
    if (!present) return;
    // Claimed whether or not it answers, so PollKysona never sends its 17-byte report 08 here.
    ctx.Claim(kVid, kPid);
    for (const HidColl* c : ctx.Find(kVid, kPid, [](const HidColl& c) {
             return c.iface == 3 && c.page() == 0xFFFF && c.usage() == 0x0001 && c.featLen() == kReportLen;
         })) {
        Dongle dongle(*c, GetTickCount64() + kDeadlineMs);
        if (!dongle.Open()) continue;
        std::vector<BYTE> r;
        DeviceStatus s;
        s.key = L"pulsar-fusion2:" + Hex4(kVid) + L":" + Hex4(kPid);
        s.name = L"Pulsar wireless mouse";
        s.kind = DeviceKind::Mouse;
        s.via = L"2.4 GHz receiver";

        Reply id = dongle.Read(0x01, 0x04, 0x04, r);
        if (id == Reply::IoError) continue;
        if (id == Reply::Ok) {
            USHORT mvid = (USHORT)(r[7] | (r[8] << 8));
            USHORT mpid = (USHORT)(r[9] | (r[10] << 8));
            if (const wchar_t* n = MouseName(mvid, mpid)) s.name = n;
        }

        Reply bat = dongle.Read(0x08, 0x01, 0x01, r);
        if (bat == Reply::IoError) continue;
        if (bat == Reply::Ok && r[7] == 0xFF) {
            s.connected = false;  // nothing paired / in range
        } else if (bat == Reply::Ok && r[7] <= 100) {
            s.percent = r[7];
            if (dongle.Read(0x08, 0x03, 0x01, r) == Reply::Ok) s.charging = r[7] != 0;
        } else {
            s.asleep = true;  // no link (asleep) or no answer in time: keep the last known value
        }
        ctx.Add(s, kVid, kPid);
        break;
    }
}
