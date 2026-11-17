// Pulsar X2 V3 / X2 V3 eS on Pulsar's Sonix vendor id 3710: wired 3406, 8K dongle 5403 (the dongle is
// shared with the X3 family). These do not speak the 17-byte report 08 protocol of the Kysona driver.
// Protocol from github.com/darthsoup/PulsarBattery (Sonix64Protocol.cs / Sonix64Backend.cs):
//   Feature report 0, 64-byte packet: [00][category][register][sub][00 ...][sum lo][sum hi],
//   sum = 16-bit sum of packet bytes 0..61 (little endian in bytes 62..63).
//   Battery read = 08 81 01. SET_FEATURE, then GET_FEATURE every ~50 ms until the reply echoes the
//   command: [01][08][81][01][..][..][percent] (packet byte 6). The reply arrives asynchronously.
//   The vendor collection is on interface 3; interface 2 rejects SET_FEATURE, so try higher ones first.
//   No usable charging register: on the cable the mouse is charging; 0% from the dongle means the
//   mouse is off or out of range.
#include "drivers.h"

#include <algorithm>

namespace {

const USHORT kVid = 0x3710;
const USHORT kWired = 0x3406;
const USHORT kDongle = 0x5403;

// Returns the battery percent, or -1 when the device did not answer.
int ReadBattery(HidDev& d) {
    std::vector<BYTE> req(65, 0);  // [0] = report id 0, then the 64-byte packet
    BYTE* p = req.data() + 1;
    p[1] = 0x08;
    p[2] = 0x81;
    p[3] = 0x01;
    unsigned sum = 0;
    for (int i = 0; i < 62; i++) sum += p[i];
    p[62] = (BYTE)(sum & 0xFF);
    p[63] = (BYTE)(sum >> 8);
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (!d.SetFeature(req)) return -1;
        for (int read = 0; read < 6; ++read) {
            Sleep(50);
            std::vector<BYTE> r;
            if (!d.GetFeature(0x00, r) || r.size() < 65) return -1;
            const BYTE* w = r.data() + 1;
            if (w[0] == 0x01 && w[1] == 0x08 && w[2] == 0x81 && w[3] == 0x01) return w[6] <= 100 ? w[6] : -1;
        }
    }
    return -1;
}

}  // namespace

void PollPulsar(PollContext& ctx) {
    for (USHORT pid : { kWired, kDongle }) {
        if (ctx.IsClaimed(kVid, pid)) continue;
        auto colls = ctx.Find(kVid, pid, [](const HidColl& c) { return c.featLen() >= 65; });
        std::sort(colls.begin(), colls.end(), [](const HidColl* a, const HidColl* b) { return a->iface > b->iface; });
        for (const HidColl* c : colls) {
            HidDev d;
            if (!d.OpenFeatureOnly(*c)) continue;
            int pct = ReadBattery(d);
            if (pct < 0) continue;
            bool cable = pid == kWired;
            DeviceStatus s;
            s.key = L"pulsar:" + Hex4(pid);
            s.kind = DeviceKind::Mouse;
            if (cable) {
                s.name = L"Pulsar X2 V3";
                s.via = L"USB cable";
                s.charging = true;
            } else {
                std::wstring n = HidProduct(*c);
                bool generic = n.empty() || n.find(L"Dongle") != std::wstring::npos || n.find(L"Receiver") != std::wstring::npos;
                s.name = generic ? L"Pulsar wireless mouse" : n;  // the 8K dongle is shared by several models
                s.via = L"2.4 GHz receiver";
                s.connected = pct > 0;
            }
            if (pct > 0 || cable) s.percent = pct;
            ctx.Add(s, kVid, pid);
            break;
        }
    }
}
