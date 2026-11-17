// MOONDROP GM01 Pro wireless headset on its 2.4 GHz dongle (35D8:0133, WalkPlay chipset).
// The dongle exposes the WalkPlay control channel on interface 3: consumer-page collection with
// 64-byte input/output reports, report id 4B. Commands are [4B][dir][cmd][len][data...] with
// dir 80 = read, 01 = write; only reads are sent here.
//   Request: [4B][80][27][00] -> [4B][80][27][06][00][00][00][00][link][percent]
//   [8] link: 01 while the headset is on and paired (assumed; 00 is treated as switched off).
//   [9] percent: matched the level Moondrop Link showed (50%) on hardware.
// Verified on hardware (35D8:0133, firmware 1.5.3). Charging state is not known yet.
// Protocol framing: github.com/jeromeof/devicePEQ (walkplayHidHandler.js); the battery command
// was found with a read-only scan of the READ command ids.
#include "drivers.h"

namespace {

const USHORT kVid = 0x35D8;  // WalkPlay; shared by other DACs/dongles, so only known product ids
const USHORT kGM01ProDongle = 0x0133;
const BYTE kReport = 0x4B, kRead = 0x80, kCmdBattery = 0x27;

bool QueryBattery(HidDev& d, std::vector<BYTE>& r) {
    d.Flush();
    if (!d.Write({ kReport, kRead, kCmdBattery, 0x00 })) return false;
    ULONGLONG end = GetTickCount64() + 500;
    for (;;) {
        int left = (int)((LONGLONG)end - (LONGLONG)GetTickCount64());
        if (left <= 0 || d.Read(r, left) < 10) return false;
        if (r[0] == kReport && r[1] == kRead && r[2] == kCmdBattery && r[3] >= 6) return true;
    }
}

}  // namespace

void PollMoondrop(PollContext& ctx) {
    for (auto [vid, pid] : ctx.Products(kVid)) {
        if (pid != kGM01ProDongle) continue;
        for (const HidColl* c : ctx.Find(vid, pid, [](const HidColl& c) {
                 return c.iface == 3 && c.page() == 0x000C && c.outLen() >= 4 && c.inLen() >= 10;
             })) {
            HidDev d;
            std::vector<BYTE> r;
            if (!d.Open(*c) || !QueryBattery(d, r)) continue;
            DeviceStatus s;
            s.key = L"moondrop:" + Hex4(pid);
            s.name = L"MOONDROP GM01 Pro";
            s.kind = DeviceKind::Headset;
            s.via = L"2.4 GHz receiver";
            s.connected = r[8] != 0;
            if (s.connected && r[9] <= 100) s.percent = r[9];
            s.graceSec = 1800;
            ctx.Add(s, vid, pid);
            break;
        }
    }
}
