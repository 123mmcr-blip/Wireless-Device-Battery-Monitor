// Protocol reference: https://github.com/OpenMouse-Project/mouse-protocol/blob/main/docs/redragon-m690-pro-testing.md
#include "drivers.h"

namespace {
bool Query(HANDLE handle, BYTE command, BYTE (&reply)[8]) {
    BYTE request[8] = {5, command};
    if (!HidD_SetFeature(handle, request, sizeof(request))) return false;
    Sleep(100);
    reply[0] = 5;
    return HidD_GetFeature(handle, reply, sizeof(reply)) &&
           reply[0] == 5 && reply[1] == command;
}
}

void PollSinowealthReceiver(PollContext& ctx) {
    // Short status reports documented by OpenMouse for M690 Pro and verified
    // on an M602-KS. The USB ID alone is shared by other receivers.
    for (const auto* c : ctx.Find(0x258A, 0x002F, [](const HidColl& c) {
             return c.iface == 1 && c.page() == 0xFF00 && c.usage() == 1 &&
                    c.featLen() == 8 && c.inLen() == 0 && c.outLen() == 0;
         })) {
        HANDLE handle = CreateFileW(c->path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE) continue;
        BYTE link[8] = {}, battery[8] = {};
        bool valid = Query(handle, 0x80, link) && link[2] == 1 &&
                     Query(handle, 0x90, battery) && battery[2] == 0x11 && battery[3] <= 100;
        CloseHandle(handle);
        if (!valid) continue;

        DeviceStatus s;
        s.key = L"sinowealth:258A:002F";
        s.name = L"Sinowealth wireless mouse";
        s.via = L"2.4 GHz receiver";
        s.kind = DeviceKind::Mouse;
        s.percent = battery[3];
        ctx.Add(s, c->vid, c->pid);
        break;
    }
}
