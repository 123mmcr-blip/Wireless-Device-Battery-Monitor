// Exercise the production driver with recorded packet layouts and a simulated HID transport.
#include "../src/drivers/drivers.h"
#include <cassert>
#include <cstdio>
#include <deque>

namespace {
std::deque<std::vector<BYTE>> replies;
std::vector<std::vector<BYTE>> writes;
int opens = 0;
bool writeOk = true;
HidColl Collection() {
    HidColl c;
    c.vid = 0x37D7; c.pid = 0x2401; c.path = L"vader5";
    c.caps.UsagePage = 0xFFA0; c.caps.Usage = 1;
    c.caps.InputReportByteLength = 33; c.caps.OutputReportByteLength = 33;
    return c;
}
std::vector<BYTE> Info(BYTE battery, BYTE connection = 2) {
    std::vector<BYTE> r(32, 0);
    r[0] = 0x5A; r[1] = 0xA5; r[2] = 1;
    r[5] = 130; r[6] = connection; r[11] = battery;
    return r;
}
const std::vector<BYTE> legacy = { 0x5A, 0xA5, 7, 0, 0 };
const std::vector<BYTE> modern = { 0x5A, 0xA5, 7, 1, 0 };
PollContext Run(std::deque<std::vector<BYTE>> packets) {
    replies = std::move(packets); writes.clear();
    PollContext ctx; ctx.colls = { Collection() };
    PollFlydigi(ctx);
    return ctx;
}
}

bool HidDev::Open(const HidColl& c) { c_ = c; h_ = (HANDLE)1; ++opens; return true; }
void HidDev::Close() { h_ = INVALID_HANDLE_VALUE; }
void HidDev::Flush() {}
bool HidDev::Write(const std::vector<BYTE>& data, int timeoutMs) {
    assert(timeoutMs > 0); writes.push_back(data); return writeOk;
}
int HidDev::Read(std::vector<BYTE>& out, int timeoutMs) {
    assert(timeoutMs > 0 && timeoutMs <= 800);
    if (replies.empty()) return -1;
    out = replies.front(); replies.pop_front();
    return (int)out.size();
}

int main() {
    // Owner-confirmed Windows traces: 05 = full on receiver; 15 = full while charging.
    auto ctx = Run({ legacy, Info(0x05) });
    assert(ctx.out.size() == 1 && ctx.out[0].percent == 100 && !ctx.out[0].charging);
    assert(ctx.out[0].via == L"2.4 GHz receiver" && ctx.out[0].kind == DeviceKind::Gamepad);
    assert(ctx.IsClaimed(0x37D7, 0x2401));
    assert(writes == std::vector<std::vector<BYTE>>({{0, 0x5A, 0xA5, 7, 0, 0}, {0, 0x5A, 0xA5, 1, 2, 0}}));
    ctx = Run({ legacy, Info(0x15, 1) });
    assert(ctx.out.size() == 1 && ctx.out[0].charging && ctx.out[0].percent == 100);
    assert(ctx.out[0].via == L"USB cable");
    for (BYTE level = 0; level <= 5; ++level) {
        ctx = Run({ legacy, Info(level) });
        assert(ctx.out.size() == 1 && ctx.out[0].percent == level * 20);
    }
    ctx = Run({ legacy, Info(0x25) });
    assert(ctx.out.size() == 1 && !ctx.out[0].charging && ctx.out[0].percent == 100);
    // Ignore unrelated input frames and handle a retained report ID.
    auto numbered = Info(3); numbered.insert(numbered.begin(), 4);
    ctx = Run({ {0xEF, 0}, legacy, {0x5A, 0xA5, 0x12, 0, 0}, numbered });
    assert(ctx.out.size() == 1 && ctx.out[0].percent == 60);
    ctx = Run({ modern, Info(2, 1) });
    assert(ctx.out.size() == 1 && ctx.out[0].via == L"2.4 GHz receiver");
    ctx = Run({ modern, Info(0x12, 0) });
    assert(ctx.out.size() == 1 && ctx.out[0].via == L"USB cable" && ctx.out[0].charging);
    for (BYTE invalid : { (BYTE)6, (BYTE)0xFF, (BYTE)0x35 }) {
        ctx = Run({ legacy, Info(invalid) });
        assert(ctx.out.empty() && ctx.claimed.empty());
    }
    auto wrongModel = Info(5); wrongModel[5] = 128;
    ctx = Run({ legacy, wrongModel }); assert(ctx.out.empty());
    auto shortInfo = Info(5); shortInfo.resize(11);
    ctx = Run({ legacy, shortInfo }); assert(ctx.out.empty());
    ctx = Run({ legacy, Info(5, 0) }); assert(ctx.out.empty());
    ctx = Run({}); assert(ctx.out.empty() && ctx.claimed.empty());
    writeOk = false;
    ctx = Run({ legacy, Info(5) }); assert(ctx.out.empty() && writes.size() == 1);
    writeOk = true;
    // Never probe charging docks, other products, or keyboard/mouse collections.
    for (int variant = 0; variant < 6; ++variant) {
        auto c = Collection();
        switch (variant) {
            case 0: c.vid = 0x1234; break;
            case 1: c.pid = 0x6001; break;
            case 2: c.caps.UsagePage = 1; break;
            case 3: c.caps.Usage = 2; break;
            case 4: c.caps.OutputReportByteLength = 0; break;
            case 5: c.caps.InputReportByteLength = 0; break;
        }
        ctx = {}; ctx.colls = {c}; opens = 0;
        PollFlydigi(ctx); assert(opens == 0 && ctx.out.empty());
    }
    ctx = {}; ctx.colls = {Collection()}; ctx.Claim(0x37D7, 0x2401); opens = 0;
    PollFlydigi(ctx); assert(opens == 0);
    std::puts("Flydigi driver tests passed (queries, receiver, cable, charging, framing, levels, filtering, timeout).");
}
