// DAREU EM901X receiver (260D:1074) and USB cable (260D:1090), verified on hardware.
// Battery query from the official EM901X AIM-WL driver v1.0 (GetPower / SendCommand); protocol analysis:
// github.com/wushuai0802/myfinder-device-battery/blob/main/engine/upstream-protocols.md
// Unlike the Kysona transport, Feature 08 is sent on FF02:0002 and Input 09 arrives on
// the sibling FF01:0000 collection. Only command 04 is needed; no initialization/settings.
// Reply: [09][04][status=0]...[6=percent][7=charging]. No reply checksum rule is established.
#include "drivers.h"


// DAREU Togran protocol on receiver 260D:1131 (A980 Pro, verified on hardware), read from DAREU's own driver
// feature report 0A (65 bytes) on the consumer-control collection: [0A][slot<<4][len][class][cmd], reply echoes class/cmd
// slot 0 class 00/A0 lists paired mice, slot n class 07/80 gives [7]=status [8]=level [9]=max, status 1 = charging
static bool TogranQuery(HidDev& dev, BYTE slot, BYTE len, BYTE cls, BYTE cmd, std::vector<BYTE>& r) {
    std::vector<BYTE> req(65, 0);
    req[0] = 0x0A;
    req[1] = (BYTE)(slot << 4);
    req[2] = len;
    req[3] = cls;
    req[4] = cmd;
    if (!dev.SetFeature(req)) return false;
    for (int tries = 0; tries < 15; tries++) {
        Sleep(30);
        if (!dev.GetFeature(0x0A, r) || r.size() < 11) return false;
        if ((r[1] & 0x0F) == 2 && r[3] == cls && r[4] == cmd) return true;
    }
    return false;
}

static void PollDareuTogran(PollContext& ctx) {
    const USHORT vid = 0x260D, pid = 0x1131;
    if (ctx.IsClaimed(vid, pid)) return;
    auto colls = ctx.Find(vid, pid, [](const HidColl& c) {
        return c.page() == 0x000C && c.usage() == 1 && c.featLen() == 65;
    });
    if (colls.size() != 1) return;
    HidDev dev;
    if (!dev.OpenFeatureOnly(*colls[0])) return;
    std::vector<BYTE> r;
    if (!TogranQuery(dev, 0, 6, 0x00, 0xA0, r)) return;
    int count = r[7];
    for (int slot = 1; slot <= count && slot <= 5; slot++) {
        BYTE status = r[7 + (slot - 1) * 3 + 1];
        USHORT mousePid = (USHORT)((r[7 + (slot - 1) * 3 + 2] << 8) | r[7 + (slot - 1) * 3 + 3]);
        if (!status) continue;
        std::vector<BYTE> b;
        if (!TogranQuery(dev, (BYTE)slot, 4, 0x07, 0x80, b)) continue;
        BYTE maxLevel = b[9] ? b[9] : 100;
        if (b[8] > maxLevel) continue;
        DeviceStatus s;
        s.key = L"dareu:" + Hex4(vid) + L":" + Hex4(pid) + L":" + Hex4(mousePid);
        s.name = mousePid == 0x1148 ? L"DAREU A980 Pro" : L"DAREU " + Hex4(mousePid);
        s.kind = DeviceKind::Mouse;
        s.via = L"2.4 GHz receiver";
        s.percent = (int)b[8] * 100 / maxLevel;
        s.charging = b[7] == 1;
        ctx.Add(s, vid, pid);
    }
}

void PollDareu(PollContext& ctx) {
    PollDareuTogran(ctx);
    const USHORT vid = 0x260D;
    // Prefer a valid cable reading over potentially cached receiver telemetry.
    for (USHORT pid : { USHORT(0x1090), USHORT(0x1074) }) {
        if (ctx.IsClaimed(vid, pid)) continue;
        auto inputs = ctx.Find(vid, pid, [](const HidColl& c) {
            return c.iface == 1 && c.page() == 0xFF01 && c.usage() == 0 && c.inLen() == 17;
        });
        auto features = ctx.Find(vid, pid, [](const HidColl& c) {
            return c.iface == 1 && c.page() == 0xFF02 && c.usage() == 2 && c.featLen() == 17;
        });
        // Do not pair arbitrary collections when multiple devices share a product ID.
        if (inputs.size() != 1 || features.size() != 1) continue;

        HidDev input, feature;
        if (!input.Open(*inputs[0]) || !feature.OpenFeatureOnly(*features[0])) continue;
        std::vector<BYTE> req(17, 0), r;
        req[0] = 0x08;
        req[1] = 0x04;
        req[16] = 0x49;  // 0x55 minus the sum of bytes 0..15
        input.Flush();
        if (!feature.SetFeature(req)) continue;
        ULONGLONG end = GetTickCount64() + 1000;
        for (int reports = 0; reports < 32; reports++) {
            int left = (int)((LONGLONG)end - (LONGLONG)GetTickCount64());
            if (left <= 0 || input.Read(r, left) < 0) break;
            if (r.size() != 17 || r[0] != 0x09 || r[1] != 0x04 || r[2] != 0 || r[6] > 100) continue;
            DeviceStatus s;
            s.key = L"dareu:" + Hex4(vid) + L":" + Hex4(pid);
            s.name = L"DAREU EM901X";
            s.kind = DeviceKind::Mouse;
            s.via = pid == 0x1090 ? L"USB cable" : L"2.4 GHz receiver";
            s.percent = r[6];
            s.charging = r[7] != 0;
            ctx.Add(s, vid, pid);
            return;
        }
    }
}
