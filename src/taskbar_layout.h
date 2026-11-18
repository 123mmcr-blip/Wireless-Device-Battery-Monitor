#pragma once
#include <windows.h>
#include <climits>
#include <vector>

// Screen coordinates; move toward the task buttons until the whole strip fits.
inline int TaskbarFreeLeft(int left, int width, int minLeft, int maxRight, int gap,
                           const std::vector<RECT>& occupied, bool rtl = false) {
    for (size_t pass = 0; pass <= occupied.size(); ++pass) {
        bool moved = false;
        for (const auto& r : occupied) {
            if (r.right <= r.left || left >= r.right + gap || left + width + gap <= r.left) continue;
            left = rtl ? r.right + gap : r.left - width - gap;
            moved = true;
        }
        if (left < minLeft || left > maxRight - width) return INT_MIN;
        if (!moved) return left;
    }
    return INT_MIN;
}
