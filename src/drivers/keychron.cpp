// Keychron wireless mice (VID 3434, e.g. M4), from Keychron's own web Launcher (launcher.keychron.com).
// 64-byte output report without report id: [01][00][81][01][0...][checksum], checksum (byte 63)
// = 0xA1 - sum(bytes 0..62). The reply [01][..][..][01][state][rate][vid lo][vid hi][pid lo][pid hi]
// [charging][battery %]... carries the mouse's own vendor id, which is checked so that only a
// genuine reply is accepted. Keychron keyboards do not report their battery this way (only over
// Bluetooth, which the Bluetooth fallback already covers).
#include "drivers.h"

namespace {

void SetChecksum(std::vector<BYTE>& payload) {  // payload = 64 bytes without the report id
    unsigned sum = 0;
    for (int i = 0; i < 63; i++) sum += payload[i];
    payload[63] = (BYTE)(0xA1 - sum);
}

std::wstring CleanName(std::wstring n) {
    if (n.empty() || n.find(L"Link") != std::wstring::npos || n.find(L"Receiver") != std::wstring::npos) return L"Keychron mouse";
    return n;
}

}  // namespace

void PollKeychron(PollContext& ctx) {
    for (auto [vid, pid] : ctx.Products(0x3434)) {
        for (const HidColl* c : ctx.Find(vid, pid, [](const HidColl& c) { return c.page() >= 0xFF00 && c.outLen() == 65 && c.inLen() == 65; })) {
            HidDev d;
            if (!d.Open(*c)) continue;
            std::vector<BYTE> payload(64, 0);
            payload[0] = 0x01;
            payload[2] = 0x81;
            payload[3] = 0x01;
            SetChecksum(payload);
            std::vector<BYTE> req(1, 0x00);  // report id 0 (unnumbered)
            req.insert(req.end(), payload.begin(), payload.end());
            d.Flush();
            if (!d.Write(req)) continue;
            std::vector<BYTE> r;
            bool ok = false;
            ULONGLONG end = GetTickCount64() + 600;
            while ((LONGLONG)end - (LONGLONG)GetTickCount64() > 0) {
                int left = (int)((LONGLONG)end - (LONGLONG)GetTickCount64());
                if (d.Read(r, left) < 12) break;
                USHORT replyVid = (USHORT)(r[6] | (r[7] << 8));
                if (r[0] == 0x01 && r[3] == 0x01 && replyVid == 0x3434) { ok = true; break; }
            }
            if (!ok || r[11] > 100) continue;
            DeviceStatus s;
            s.key = L"keychron:" + Hex4(pid);
            s.name = CleanName(HidProduct(*c));
            s.kind = DeviceKind::Mouse;
            s.via = L"2.4 GHz receiver";
            s.percent = r[11];
            s.charging = r[10] != 0;
            ctx.Add(s, vid, pid);
            break;
        }
    }
}
