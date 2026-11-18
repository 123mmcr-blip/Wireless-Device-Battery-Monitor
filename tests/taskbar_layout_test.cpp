#include "../src/taskbar_layout.h"
#include <cassert>

int main() {
    assert(TaskbarFreeLeft(800, 100, 0, 1000, 2, {}) == 800);
    // Weather and a speed widget, deliberately in the opposite order to placement.
    const std::vector<RECT> occupied{{660, 0, 760, 48}, {780, 0, 900, 48}};
    assert(TaskbarFreeLeft(800, 100, 0, 1000, 2, occupied) == 558);
    assert(TaskbarFreeLeft(800, 100, 600, 1000, 2, occupied) == INT_MIN);
    assert(TaskbarFreeLeft(558, 100, 0, 1000, 2, occupied) == 558);
    // Secondary display to the left of the primary, including a DPI-scaled gap.
    assert(TaskbarFreeLeft(-200, 100, -1920, 0, 4, {{-220, 0, -80, 48}}) == -324);
    assert(TaskbarFreeLeft(100, 100, 0, 1000, 2, {{90, 0, 220, 48}}, true) == 222);
    assert(TaskbarFreeLeft(900, 100, 0, 1000, 2, {{900, 0, 1000, 48}}, true) == INT_MIN);
    assert(TaskbarFreeLeft(800, 100, 0, 1000, 2, {{850, 0, 850, 48}}) == 800);
    // A widget closing returns the strip to its preferred position on the next pass.
    assert(TaskbarFreeLeft(800, 100, 0, 1000, 2, {}) == 800);
}
