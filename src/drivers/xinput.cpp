// Xbox controllers (and headsets plugged into them) through XInput. XInput only reports coarse
// levels (empty / low / medium / full), so these are shown as text rather than a percentage.
#include "drivers.h"

#include <xinput.h>

namespace {

using GetBatteryFn = DWORD(WINAPI*)(DWORD, BYTE, XINPUT_BATTERY_INFORMATION*);

GetBatteryFn LoadXInput() {
    static GetBatteryFn fn = [] {
        for (const wchar_t* dll : { L"xinput1_4.dll", L"xinput9_1_0.dll" }) {
            if (HMODULE m = LoadLibraryExW(dll, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32))
                if (auto f = (GetBatteryFn)GetProcAddress(m, "XInputGetBatteryInformation")) return f;
        }
        return (GetBatteryFn) nullptr;
    }();
    return fn;
}

}  // namespace

void PollXInput(PollContext& ctx) {
    GetBatteryFn get = LoadXInput();
    if (!get) return;
    // An Xbox controller on Bluetooth may already be listed with a real percentage.
    bool btXbox = false;
    for (auto& s : ctx.out) btXbox |= s.via == L"Bluetooth" && s.name.find(L"Xbox") != std::wstring::npos;

    for (DWORD user = 0; user < 4; user++) {
        for (BYTE type : { (BYTE)BATTERY_DEVTYPE_GAMEPAD, (BYTE)BATTERY_DEVTYPE_HEADSET }) {
            XINPUT_BATTERY_INFORMATION bi{};
            if (get(user, type, &bi) != ERROR_SUCCESS) continue;
            if (bi.BatteryType == BATTERY_TYPE_DISCONNECTED || bi.BatteryType == BATTERY_TYPE_WIRED) continue;
            if (type == BATTERY_DEVTYPE_GAMEPAD && btXbox) continue;
            DeviceStatus s;
            s.key = L"xinput:" + std::to_wstring(user) + L":" + std::to_wstring(type);
            s.name = type == BATTERY_DEVTYPE_GAMEPAD ? L"Xbox controller " + std::to_wstring(user + 1) : L"Xbox headset";
            s.kind = type == BATTERY_DEVTYPE_GAMEPAD ? DeviceKind::Gamepad : DeviceKind::Headset;
            s.via = L"Xbox Wireless";
            switch (bi.BatteryLevel) {
                case BATTERY_LEVEL_EMPTY:  s.percent = 5;   s.levelText = L"Empty";  break;
                case BATTERY_LEVEL_LOW:    s.percent = 25;  s.levelText = L"Low";    break;
                case BATTERY_LEVEL_MEDIUM: s.percent = 60;  s.levelText = L"Medium"; break;
                default:                   s.percent = 100; s.levelText = L"Full";   break;
            }
            ctx.out.push_back(s);
        }
    }
}
