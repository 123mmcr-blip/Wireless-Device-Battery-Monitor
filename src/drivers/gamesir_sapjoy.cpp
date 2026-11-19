// GameSir Tarantula Pro / NovaPro (Super Nova, T4NPro) — battery over the SAPJoy "device info" frame.
//
// These pads show up as a wired Xbox controller to XInput, so the level is read from their vendor
// collection (usage page 0xFFA0, output report 0x07 / 32 bytes, input report 0x06 / 64 bytes, no
// feature channel) with the read-only GetDeviceInfo query GameSir's own client sends:
//   Request: 07 03 0A 01, zero padded to 32 bytes
//   Reply:   [0]=06 [1]=len [2]=0A [3]=01 [4]=id [5..6]=firmware [7..12]=MAC
//            [13]=battery (0..100 percent, FF = charging) [14]=type
//
// Protocol from the captured traffic of GameSir's client on a NovaPro (Super Nova / T4NPro) and its
// main.js decoder (chargeState = 255 == battery, battery clamped at 100). Confirmed on real
// hardware: the pad reported 0x60 (96%) while GameSir's own UI showed 96%.
//
// 3537:103E is shared by the Tarantula Pro and the NovaPro on their receivers, and the two answer
// this same frame, so the product string is used for the name instead of a fixed one.
// 3537:100E is the NovaPro on a cable: same frame, and the pad reports a level there too.
#include "drivers.h"
#include "gamesir_sapjoy_protocol.h"

namespace {

const USHORT kGameSirVid = 0x3537;

// Cable and receiver ids that answer the SAPJoy frame. Nova / Nova 2 / Nova 2 Lite (0x1098, 0x100F,
// 0x1040) and Cyclone 2 (0x100B) are handled by the drivers above and are deliberately not listed.
bool IsSapjoy(USHORT pid) { return pid == 0x1050 || pid == 0x103E || pid == 0x100E; }

}  // namespace

void PollGameSirSapjoy(PollContext& ctx) {
    for (auto [vid, pid] : ctx.Products(kGameSirVid)) {
        if (!IsSapjoy(pid)) continue;
        // 0xFFA0 is the page the vendor library hard-checks; the keyboard/gamepad pages either time
        // out or refuse the write.
        for (const HidColl* c : ctx.Find(vid, pid, [](const HidColl& c) {
                 return c.page() == 0xFFA0 && c.outLen() >= 4 && c.inLen() >= 15;
             })) {
            HidDev d;
            if (!d.Open(*c)) continue;
            d.Flush();

            BYTE req[GameSirSapjoy::kOutputReportLen];
            if (!GameSirSapjoy::BuildDeviceInfoRequest(req, sizeof(req))) continue;
            if (!d.Write(std::vector<BYTE>(req, req + sizeof(req)))) continue;

            std::vector<BYTE> r;
            GameSirSapjoy::Reading reading;
            const ULONGLONG end = GetTickCount64() + 900;
            while ((LONGLONG)end - (LONGLONG)GetTickCount64() > 0) {
                if (d.Read(r, 200) < 15) continue;   // state frames are shorter or use another header
                reading = GameSirSapjoy::DecodeDeviceInfo(r.data(), r.size());
                if (reading.answered) break;
            }
            if (!reading.answered) continue;

            DeviceStatus s;
            s.key = L"gamesir:" + Hex4(pid);
            const wchar_t* product = HidProduct(*c);
            s.name = product && *product ? product : L"GameSir NovaPro";
            s.kind = DeviceKind::Gamepad;
            // The level is only published over the 2.4 GHz link; the cable id is kept so the pad is
            // claimed and named instead of being reported twice by the XInput fallback.
            s.via = pid == 0x100E ? L"USB cable" : L"2.4 GHz receiver";
            s.charging = reading.charging;
            if (!reading.charging && reading.percent >= 0) s.percent = reading.percent;
            ctx.Add(s, vid, pid);
            break;
        }
    }
}
