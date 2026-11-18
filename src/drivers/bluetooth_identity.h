#pragma once
// Bluetooth device identity is its Windows device-container ID, not its display name.
// Keep the legacy name-based fallback only for already-reported non-Bluetooth devices
// (e.g. vendor drivers), while allowing two Bluetooth controllers with identical names.
#include "../devices.h"

#include <vector>

inline bool IsDuplicateBluetoothDevice(const DeviceStatus& candidate, const std::vector<DeviceStatus>& existing) {
    for (const auto& other : existing) {
        if (other.key == candidate.key) return true;
        if (other.via != L"Bluetooth" && other.name == candidate.name) return true;
    }
    return false;
}
