// AULA A7Pro 2.4 GHz receiver (38A6:0001, interface 0, FF90:0001, 64-byte reports).
// AULA Audio Center 1.0.1.0 sends the read-only battery query 55 65 01, padded with zeros.
// Verified on hardware via interrupt I/O: 55 65 64 02 ... reports 100% (byte 2).
// The byte after the percentage has not been identified, so charging status is not inferred.
#include "drivers.h"

namespace {

constexpr USHORT kVid = 0x38A6, kPid = 0x0001;
constexpr BYTE kReport = 0x55, kBattery = 0x65;
constexpr int kReportLength = 64;

bool IsBatteryCollection(const HidColl& c) {
    return c.iface == 0 && c.page() == 0xFF90 && c.usage() == 0x0001 &&
           c.inLen() == kReportLength && c.outLen() == kReportLength && c.featLen() == 0;
}

int ReadBattery(const HidColl& c) {
    HidDev d;
    if (!d.Open(c)) return -1;
    d.Flush();
    const ULONGLONG deadline = GetTickCount64() + 750;
    if (!d.Write({kReport, kBattery, 0x01}, 200)) return -1;

    std::vector<BYTE> r;
    for (ULONGLONG now = GetTickCount64(); now < deadline; now = GetTickCount64()) {
        if (d.Read(r, static_cast<int>(deadline - now)) < 0) break;
        if (r.size() == kReportLength && r[0] == kReport && r[1] == kBattery && r[2] <= 100)
            return r[2];
    }
    return -1;
}

}  // namespace

void PollAulaHeadset(PollContext& ctx) {
    if (ctx.IsClaimed(kVid, kPid)) return;
    for (const HidColl* c : ctx.Find(kVid, kPid, IsBatteryCollection)) {
        // AULA also uses this VID:PID and collection layout for the A700 (upstream issue #3436).
        if (HidProduct(*c) != L"AULA-A7Pro") continue;
        int percent = ReadBattery(*c);
        if (percent < 0) continue;

        DeviceStatus s;
        s.key = L"aula:a7pro";
        s.name = L"AULA A7Pro";
        s.kind = DeviceKind::Headset;
        s.via = L"2.4 GHz receiver";
        s.percent = percent;
        ctx.Add(s, kVid, kPid);
        break;
    }
}
