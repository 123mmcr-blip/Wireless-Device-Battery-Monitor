// Smaller mouse brands, each with a community-documented battery protocol:
//  * Motospeed X6 (Realtek VID 0BDA, receivers FFE0 / FFF2, interface 4): output report B5 [06],
//    reply report B4, byte 20 = level (+128 while charging). The cable (FFF1) reports no battery.
//    From github.com/Jacobz888/LGSTrayBattery-MotospeedX6.
//  * Delux M900 Pro and siblings (VID 1D57, receivers FA60 / FA65): no command needed; the receiver
//    broadcasts input report 03 on usage page 000A every ~2 s: [03][model][40][state][percent],
//    state 2 = full, 3 = charging (percent frozen). From github.com/AeonArcHQ/omarchy-delux-m900.
//  * Ajazz AJ179 family (VID 249A receiver, 248A cable, interface 2): unnumbered 32-byte report,
//    command 10 (device info, answered by the receiver): reply [10][00 = ok]..., [4..7] model id,
//    [12] charging, [13] percent, [14] mouse connected (receiver), [31] = sum([4..30]).
//    From github.com/Rockeyxx/aj179-linux (docs/PROTOCOL.md).
//  * AJAZZ AJ-series mice on the SONiX vendor id 3151 (AJ159 PRO / APEX and siblings). The dongle
//    exposes a 65-byte feature collection on usage page FFFF / usage 02. A SET_FEATURE status poll
//    (report 0x00, opcode 0xF7, zero payload) brings the 2.4 GHz telemetry link up; ~30 ms later
//    GET_FEATURE report 0x05 carries the charge in byte 3 (0..100). A frame with a non-zero byte 1
//    or 2 is transient reconnect garbage and is rejected.
//    From github.com/Aiacos/ajazz-control-center (docs/protocols/mouse/aj_series.md).
#include "drivers.h"

#include <string>

namespace {

template <class Match>
bool ReadUntil(HidDev& d, std::vector<BYTE>& r, int timeoutMs, Match match) {
    ULONGLONG end = GetTickCount64() + timeoutMs;
    while ((LONGLONG)end - (LONGLONG)GetTickCount64() > 0) {
        int left = (int)((LONGLONG)end - (LONGLONG)GetTickCount64());
        if (d.Read(r, left) < 0) return false;
        if (match(r)) return true;
    }
    return false;
}

void PollMotospeed(PollContext& ctx) {
    for (USHORT pid : { (USHORT)0xFFE0, (USHORT)0xFFF2, (USHORT)0xFFF1 }) {
        if (ctx.IsClaimed(0x0BDA, pid)) continue;
        for (const HidColl* c : ctx.Find(0x0BDA, pid, [](const HidColl& c) { return c.iface == 4 && c.outLen() >= 21; })) {
            DeviceStatus s;
            s.key = L"motospeed:" + Hex4(pid);
            s.name = L"Motospeed X6";
            s.kind = DeviceKind::Mouse;
            if (pid == 0xFFF1) {  // plugged in: charging, no battery reading over the cable
                s.via = L"USB cable";
                s.charging = true;
                ctx.Add(s, 0x0BDA, pid);
                break;
            }
            HidDev d;
            if (!d.Open(*c)) continue;
            d.Flush();
            if (!d.Write({ 0xB5, 0x06 })) continue;
            std::vector<BYTE> r;
            if (!ReadUntil(d, r, 1500, [](const std::vector<BYTE>& r) { return r.size() > 20 && r[0] == 0xB4; })) continue;
            int raw = r[20];
            s.via = L"2.4 GHz receiver";
            s.charging = raw >= 128;
            s.percent = Clamp100(s.charging ? raw - 128 : raw);
            ctx.Add(s, 0x0BDA, pid);
            break;
        }
    }
}

void PollDelux(PollContext& ctx) {
    for (USHORT pid : { (USHORT)0xFA60, (USHORT)0xFA65 }) {
        if (ctx.IsClaimed(0x1D57, pid)) continue;
        for (const HidColl* c : ctx.Find(0x1D57, pid, [](const HidColl& c) { return c.page() == 0x000A && c.inLen() >= 5; })) {
            HidDev d;
            if (!d.Open(*c)) continue;
            std::vector<BYTE> r;
            // Passive: wait for the receiver's periodic power report (sent every ~2 s).
            if (!ReadUntil(d, r, 3000, [](const std::vector<BYTE>& r) { return r.size() >= 5 && r[0] == 0x03 && r[2] == 0x40; })) continue;
            DeviceStatus s;
            s.key = L"delux:" + Hex4(pid);
            s.name = r[1] == 0x3D ? L"Delux M900 Pro" : L"Delux wireless mouse";
            s.kind = DeviceKind::Mouse;
            s.via = L"2.4 GHz receiver";
            BYTE state = r[3], pct = r[4];
            if (state == 2) { s.percent = 100; s.charging = true; }
            else if (state == 3) { s.charging = true; if (pct >= 1 && pct <= 100) s.percent = pct; }
            else if (pct >= 1 && pct <= 100) s.percent = pct;
            ctx.Add(s, 0x1D57, pid);
            break;
        }
    }
}

void PollAjazz(PollContext& ctx) {
    for (USHORT vendor : { (USHORT)0x249A, (USHORT)0x248A }) {
        for (auto [vid, pid] : ctx.Products(vendor)) {
            for (const HidColl* c : ctx.Find(vid, pid, [](const HidColl& c) { return c.iface == 2 && c.outLen() == 33 && c.inLen() == 33; })) {
                HidDev d;
                if (!d.Open(*c)) continue;
                d.Flush();
                if (!d.Write({ 0x00, 0x10 })) continue;  // report id 0, command 10, zero payload (checksum 0)
                std::vector<BYTE> r;
                bool ok = ReadUntil(d, r, 1500, [](const std::vector<BYTE>& r) {
                    if (r.size() < 32 || r[0] != 0x10 || r[1] != 0x00) return false;
                    unsigned sum = 0;
                    for (int i = 4; i <= 30; i++) sum += r[i];
                    return (BYTE)sum == r[31];
                });
                if (!ok || r[13] > 100) continue;
                DeviceStatus s;
                s.key = L"ajazz:" + Hex4(vid) + L":" + Hex4(pid);
                std::string model;
                for (int i = 4; i < 8 && r[i] >= 0x20 && r[i] < 0x7F; i++) model += (char)r[i];
                s.name = model.size() == 4 && model[0] == 'M' ? L"Ajazz AJ" + std::wstring(model.begin() + 1, model.end()) : L"Ajazz mouse";
                s.kind = DeviceKind::Mouse;
                bool receiver = vid == 0x249A;
                s.via = receiver ? L"2.4 GHz receiver" : L"USB cable";
                s.connected = !receiver || r[14] != 0;
                s.charging = r[12] != 0;
                if (s.connected) s.percent = r[13];
                ctx.Add(s, vid, pid);
                break;
            }
        }
    }
}

void PollAjazzAjSeries(PollContext& ctx) {
    for (auto [vid, pid] : ctx.Products(0x3151)) {
        for (const HidColl* c : ctx.Find(vid, pid, [](const HidColl& c) {
                 return c.page() == 0xFFFF && c.usage() == 0x02 && c.featLen() >= 65;
             })) {
            HidDev d;
            if (!d.OpenFeatureOnly(*c)) continue;
            std::vector<BYTE> req(c->featLen(), 0);
            req[0] = 0x00;  // unnumbered report
            req[1] = 0xF7;  // status poll: brings the 2.4 GHz telemetry link up
            if (!d.SetFeature(req)) continue;
            Sleep(30);
            std::vector<BYTE> r;
            if (!d.GetFeature(0x05, r) || r.size() < 4) continue;
            if (r[0] != 0x05 || r[1] != 0 || r[2] != 0) continue;  // malformed / reconnect frame
            int pct = r[3];
            if (pct > 100) continue;
            DeviceStatus s;
            s.key = L"ajazzaj:" + Hex4(vid) + L":" + Hex4(pid);
            s.name = HidProduct(*c);
            if (s.name.empty()) s.name = L"AJAZZ mouse";
            s.kind = DeviceKind::Mouse;
            s.via = L"2.4 GHz receiver";
            s.connected = pct > 0;  // 0 means the link is up but the mouse is off
            if (s.connected) s.percent = pct;
            ctx.Add(s, vid, pid);
            break;
        }
    }
}

}  // namespace

void PollMiscMice(PollContext& ctx) {
    PollMotospeed(ctx);
    PollDelux(ctx);
    PollAjazz(ctx);
    PollAjazzAjSeries(ctx);
}
