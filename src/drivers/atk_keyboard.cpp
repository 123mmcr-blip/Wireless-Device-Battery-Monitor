// ATK V75X and Z87 keyboards. The V75X protocol comes from HaloBattery
// (github.com/HeyOkay/HaloBattery, providers/atk_keyboard.py, MIT), based on ATK HUB's Evision driver.
// A direct power query was also verified on an awake Z87 receiver (373B:1012, issue #1194).
// On FF1C:0092 both send report 04 with a 16-bit sequence and command 1A. The power reply echoes
// those fields, followed by status and the battery percentage (byte 8) and charge state (byte 9).
#include "drivers.h"

namespace {

constexpr USHORT kV75xVid = 0x320F, kV75xReceiver = 0x5088, kV75xCable = 0x5055;
constexpr USHORT kZ87Vid = 0x373B, kZ87Receiver = 0x1012;
constexpr ULONGLONG kReceiverIntervalMs = 10 * 60 * 1000;

bool ReadBattery(const HidColl& c, USHORT& seq, int& percent, bool& charging) {
    seq = (USHORT)(seq % 0xFFFF + 1);
    HidDev d;
    if (!d.Open(c)) return false;
    d.Flush();
    std::vector<BYTE> req(32, 0);  // HidDev pads to the output report length (64 on the Z87).
    req[0] = 0x04;
    req[1] = (BYTE)(seq & 0xFF);
    req[2] = (BYTE)(seq >> 8);
    req[3] = 0x1A;
    ULONGLONG end = GetTickCount64() + 1000;
    if (!d.Write(req)) return false;
    std::vector<BYTE> r;
    for (;;) {
        ULONGLONG now = GetTickCount64();
        if (now >= end) return false;
        int n = d.Read(r, (int)(end - now));
        if (n < 0) return false;
        if (n < 4 || r[0] != 0x04 || r[1] != req[1] || r[2] != req[2]) continue;
        if (c.vid == kZ87Vid && r[3] == 0xFF) return false;  // sleeping keyboard
        if (n < 10 || r[3] != 0x1A || r[5] || r[6]) continue;
        if (r[8] > 100 || r[9] > 2) return false;
        percent = r[8];
        charging = r[9] == 1 || r[9] == 2;
        return true;
    }
}

struct BatteryCache {
    USHORT sequence = 0;
    ULONGLONG nextAsk = 0;
    int percent = -1;
    bool charging = false;
};

bool PollKeyboard(PollContext& ctx, USHORT vid, USHORT pid, const wchar_t* product, BatteryCache& cache) {
    if (ctx.IsClaimed(vid, pid)) return false;
    bool z87 = vid == kZ87Vid;
    bool cable = pid == kV75xCable;
    for (const HidColl* c : ctx.Find(vid, pid, [z87](const HidColl& c) {
        return c.page() == 0xFF1C && c.usage() == 0x0092 && (!z87 || (c.inLen() == 64 && c.outLen() == 64));
    })) {
        std::wstring name = HidProduct(*c);
        while (!name.empty() && name.back() == L' ') name.pop_back();
        if (_wcsicmp(name.c_str(), product) != 0) continue;
        DeviceStatus s;
        s.key = z87 ? L"atk_keyboard:1012" : L"atk_keyboard:5088";
        s.name = z87 ? L"ATK Z87" : L"ATK V75X";
        s.kind = DeviceKind::Keyboard;
        s.via = cable ? L"USB cable" : L"2.4 GHz receiver";
        s.graceSec = 1800;
        ULONGLONG now = GetTickCount64();
        if (cable || now >= cache.nextAsk) {
            cache.nextAsk = now + kReceiverIntervalMs;
            if (!ReadBattery(*c, cache.sequence, cache.percent, cache.charging)) {
                s.asleep = true;
                if (z87) {
                    cache.percent = -1;
                    cache.charging = false;
                    cache.nextAsk = now + 30 * 1000;  // retry after wake without polling continuously
                }
            }
        }
        if (!s.asleep) {
            s.percent = cache.percent;
            s.charging = cache.charging;
            s.asleep = s.percent < 0;
        }
        ctx.Add(s, vid, pid);
        return true;
    }
    return false;
}

}  // namespace

void PollAtkKeyboard(PollContext& ctx) {
    static BatteryCache v75x, z87;
    if (PollKeyboard(ctx, kV75xVid, kV75xCable, L"ATK V75X", v75x))
        ctx.Claim(kV75xVid, kV75xReceiver);  // the cable wins when both are connected
    else
        PollKeyboard(ctx, kV75xVid, kV75xReceiver, L"ATK V75X 2.4G Dongle", v75x);
    if (!PollKeyboard(ctx, kZ87Vid, kZ87Receiver, L"ATK Z87 Dongle", z87))
        z87 = {};  // sample immediately when the receiver is plugged in again
}
