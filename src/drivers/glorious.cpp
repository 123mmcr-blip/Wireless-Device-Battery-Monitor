// Glorious (VID 258A) and Hitscan (VID 3770) wireless mice. Both use the same supplier protocol:
// Request: 64-byte feature report (id 0), bytes [3]=02 [4]=02 [6]=83.
// Reply:   [1]=status, [4]=02, [6]=83, [7]=charging (1), [8]=percent.
// Status A1 = awake, A4 = asleep, A0 = waking up, others = unreachable.
// Protocol: github.com/ikajdan/glorious-indicator, github.com/louis4craft/glorious-ctl,
// and Hitscan's own web configurator (control.hitscan.com, getBatPer).
#include "drivers.h"

namespace {

struct Vendor {
    USHORT vid;
    const wchar_t* keyPrefix;
    int replyDelayMs;
    const wchar_t* (*name)(USHORT pid);
    bool (*isCable)(USHORT pid);
};

const wchar_t* GloriousName(USHORT pid) {
    switch (pid) {
        case 0x2022: case 0x2011: return L"Glorious Model O Wireless";
        case 0x2023: case 0x2012: return L"Glorious Model D Wireless";
        case 0x2024: case 0x2013: return L"Glorious Model O- Wireless";
        case 0x2025: case 0x2014: return L"Glorious Model D- Wireless";
        case 0x2033: return L"Glorious Model O 2 Wireless";
        case 0x2034: case 0x2036: case 0x201A: return L"Glorious Model D 2 PRO";
        default: return L"Glorious wireless mouse";
    }
}
bool GloriousCable(USHORT pid) { return pid == 0x2011 || pid == 0x2012 || pid == 0x2013 || pid == 0x2014 || pid == 0x201A; }

const wchar_t* HitscanName(USHORT pid) {
    switch (pid) {
        case 0x0136: return L"Hitscan Hyperlight X";
        case 0x0194: return L"Hitscan Hyperlight N";
        default: return L"Hitscan Hyperlight";
    }
}
bool HitscanCable(USHORT pid) { return pid == 0x0136 || pid == 0x0194; }

const Vendor kVendors[] = {
    { 0x258A, L"glorious:", 50, GloriousName, GloriousCable },
    { 0x3770, L"hitscan:", 200, HitscanName, HitscanCable },
};

}  // namespace

void PollGlorious(PollContext& ctx) {
    for (const Vendor& v : kVendors) {
        for (auto [vid, pid] : ctx.Products(v.vid)) {
            for (const HidColl* c : ctx.Find(vid, pid, [](const HidColl& c) { return c.featLen() == 65; })) {
                HidDev d;
                if (!d.OpenFeatureOnly(*c)) continue;
                std::vector<BYTE> r;
                bool ok = false;
                for (int attempt = 0; attempt < 2; attempt++) {
                    std::vector<BYTE> w(65, 0);
                    w[3] = 0x02; w[4] = 0x02; w[6] = 0x83;
                    if (!d.SetFeature(w)) break;
                    Sleep(v.replyDelayMs);
                    if (!d.GetFeature(0, r) || r.size() < 9) break;
                    if (r[6] != 0x83 || (r[1] & 0xF0) != 0xA0) break;  // not this protocol
                    ok = true;
                    if (r[1] == 0xA0 && attempt == 0) { Sleep(400); continue; }  // waking up: ask again
                    break;
                }
                if (!ok) continue;
                DeviceStatus s;
                s.key = v.keyPrefix + Hex4(pid);
                s.name = v.name(pid);
                s.kind = DeviceKind::Mouse;
                s.via = v.isCable(pid) ? L"USB cable" : L"2.4 GHz receiver";
                s.charging = v.isCable(pid) || r[7] == 0x01;
                if (r[1] == 0xA1 && r[8] <= 100) s.percent = Clamp100(r[8] ? r[8] : 1);
                else s.asleep = true;
                ctx.Add(s, vid, pid);
                break;
            }
        }
    }
}
