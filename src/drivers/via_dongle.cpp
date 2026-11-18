// Keyboards on the shared "Wireless 2.4G Dongle" 36B0:3002: EVO80 (issue 1406, product string
// "EVO80 2.4G"), URX Core75 ("URX Core75 Wireless"), WOBKEY Wave 75 (issue 343). The dongle passes
// the keyboard's VIA raw HID channel through on interface 1, usage page FF60 / usage 61, 33-byte
// input and output reports (report id 0 + 32 bytes). The battery is VIA custom value 7 on channel 0,
// read with VIA's read-only custom-get command 0x08:
//   request [00] 08 00 07 00 ...       report id 0, custom get, channel 0, value 7
//   reply        08 00 07 <level> ...  echoed header, level in percent (no charging state)
// A firmware without that value answers FF (VIA "unhandled"), which is ignored.
// Source: HaloBattery (github.com/HeyOkay/HaloBattery, providers/urx.py and docs/protocols.md,
// "URX Core75 (2.4 GHz)"), taken from URX HUB's web driver and confirmed on a real Core75 on this
// dongle (HaloBattery issue 245). Untested on the EVO80 and the Wave 75.
#include "drivers.h"

#include <cwchar>

namespace {

constexpr USHORT kVid = 0x36B0;
constexpr USHORT kPid = 0x3002;
const size_t kLen = 32;  // payload bytes after the report id

// Returns the level (1..100), or -1 when the keyboard did not answer with one.
int QueryLevel(HidDev& d) {
    std::vector<BYTE> req(1 + kLen, 0);  // [0] = report id 0
    req[1] = 0x08;                       // VIA id_custom_get_value
    req[2] = 0x00;                       // channel 0 (keyboard-specific values)
    req[3] = 0x07;                       // value 7: battery level
    d.Flush();
    if (!d.Write(req)) return -1;
    ULONGLONG end = GetTickCount64() + 600;
    for (;;) {
        int left = (int)((LONGLONG)end - (LONGLONG)GetTickCount64());
        if (left <= 0) return -1;
        std::vector<BYTE> r;
        int n = d.Read(r, left);  // report id 0 is stripped: r[0] is the first payload byte
        if (n < 0) return -1;
        if (n < 4 || r[0] != 0x08 || r[1] != 0x00 || r[2] != 0x07) continue;
        return (r[3] >= 1 && r[3] <= 100) ? r[3] : -1;
    }
}

// "EVO80 2.4G" -> "EVO80", "URX Core75 Wireless" -> "URX Core75"; the generic dongle name -> fallback.
std::wstring KeyboardName(const HidColl& c) {
    std::wstring name = HidProduct(c);
    if (name.find(L"Dongle") != std::wstring::npos) return L"Wireless keyboard";
    for (const wchar_t* suffix : { L" 2.4G", L" Wireless" }) {
        size_t n = wcslen(suffix);
        if (name.size() > n && name.compare(name.size() - n, n, suffix) == 0) name.resize(name.size() - n);
    }
    return name.empty() ? L"Wireless keyboard" : name;
}

}  // namespace

void PollViaDongle(PollContext& ctx) {
    if (ctx.IsClaimed(kVid, kPid)) return;
    for (const HidColl* c : ctx.Find(kVid, kPid, [](const HidColl& c) {
             return c.iface == 1 && c.page() == 0xFF60 && c.usage() == 0x0061 && c.outLen() == 1 + kLen &&
                    c.inLen() == 1 + kLen;
         })) {
        HidDev d;
        if (!d.Open(*c)) continue;
        int level = QueryLevel(d);
        if (level < 0) level = QueryLevel(d);  // the first request can be missed while the link wakes up
        if (level < 0) continue;
        DeviceStatus s;
        s.key = L"via:" + Hex4(kVid) + L":" + Hex4(kPid);
        s.name = KeyboardName(*c);
        s.kind = DeviceKind::Keyboard;
        s.via = L"2.4 GHz receiver";
        s.percent = level;
        s.graceSec = 1800;  // keep the last value if a sleeping keyboard stops answering
        ctx.Add(s, kVid, kPid);
        break;
    }
}
