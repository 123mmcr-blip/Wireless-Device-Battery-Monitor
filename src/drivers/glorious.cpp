// Glorious wireless mice (Sinowealth based receivers, VID 258A).
// Request: feature report id 0, bytes [3]=02 [4]=02 [6]=83. Reply: [1]=status, [6]=83, [8]=percent.
// Status A1 = awake, A4 = asleep, A0 = waking up, others = unreachable.
// Protocol: github.com/ikajdan/glorious-indicator, github.com/louis4craft/glorious-ctl
#include "drivers.h"

namespace {

const wchar_t* Name(USHORT pid) {
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

bool IsCable(USHORT pid) { return pid == 0x2011 || pid == 0x2012 || pid == 0x2013 || pid == 0x2014 || pid == 0x201A; }

}  // namespace

void PollGlorious(PollContext& ctx) {
    for (auto [vid, pid] : ctx.Products(0x258A)) {
        for (const HidColl* c : ctx.Find(vid, pid, [](const HidColl& c) { return c.featLen() == 65; })) {
            HidDev d;
            if (!d.OpenFeatureOnly(*c)) continue;
            std::vector<BYTE> r;
            bool ok = false;
            for (int attempt = 0; attempt < 2; attempt++) {
                std::vector<BYTE> w(65, 0);
                w[3] = 0x02; w[4] = 0x02; w[6] = 0x83;
                if (!d.SetFeature(w)) break;
                Sleep(50);
                if (!d.GetFeature(0, r) || r.size() < 9) break;
                if (r[6] != 0x83 || (r[1] & 0xF0) != 0xA0) break;  // not this protocol
                ok = true;
                if (r[1] == 0xA0 && attempt == 0) { Sleep(400); continue; }  // waking up: ask again
                break;
            }
            if (!ok) continue;
            DeviceStatus s;
            s.key = L"glorious:" + Hex4(pid);
            s.name = Name(pid);
            s.kind = DeviceKind::Mouse;
            s.via = IsCable(pid) ? L"USB cable" : L"2.4 GHz receiver";
            s.charging = IsCable(pid);
            if (r[1] == 0xA1) s.percent = Clamp100(r[8] ? r[8] : 1);
            else s.asleep = true;
            ctx.Add(s, vid, pid);
            break;
        }
    }
}
