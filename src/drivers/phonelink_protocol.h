
#pragma once

#include <cwchar>
#include <cwctype>
#include <string>

namespace PhoneLink {

struct Reading {
    std::wstring name;
    int percent = -1;
    bool charging = false;
};

// Read a JSON string value, including simple escaped characters.
// The cursor allows searching multiple occurrences of the same key.
inline bool JsonString(const std::wstring& json, const wchar_t* key,
                       size_t& cursor, std::wstring& value) {
    const std::wstring pattern = L"\"" + std::wstring(key) + L"\"";

    while (cursor < json.size()) {
        size_t pos = json.find(pattern, cursor);
        if (pos == std::wstring::npos) return false;

        size_t i = pos + pattern.size();
        while (i < json.size() && iswspace(json[i])) ++i;
        if (i >= json.size() || json[i] != L':') {
            cursor = pos + pattern.size();
            continue;
        }

        ++i;
        while (i < json.size() && iswspace(json[i])) ++i;
        if (i >= json.size() || json[i] != L'"') {
            cursor = i;
            continue;
        }

        value.clear();
        ++i;

        for (; i < json.size(); ++i) {
            wchar_t c = json[i];

            if (c == L'"') {
                cursor = i + 1;
                return true;
            }

            if (c != L'\\') {
                value += c;
                continue;
            }

            if (++i >= json.size()) return false;
            wchar_t escaped = json[i];

            if (escaped == L'u') {
                if (i + 4 >= json.size()) return false;

                for (size_t j = i + 1; j <= i + 4; ++j)
                    if (!iswxdigit(json[j])) return false;

                value += static_cast<wchar_t>(
                    wcstoul(json.substr(i + 1, 4).c_str(), nullptr, 16));
                i += 4;
            } else if (escaped == L'n' || escaped == L't') {
                value += L' ';
            } else {
                value += escaped;
            }
        }

        return false;
    }

    return false;
}

inline bool ConnectedPhoneName(const std::wstring& speak,
                               std::wstring& name) {
    const wchar_t* suffixes[] = {
        L" - Connected to your PC",
        L" - PC に接続しました"
    };

    for (const wchar_t* suffix : suffixes) {
        const std::wstring ending(suffix);

        if (speak.size() > ending.size() &&
            speak.compare(speak.size() - ending.size(),
                          ending.size(), ending) == 0) {
            name = speak.substr(0, speak.size() - ending.size());
            return !name.empty();
        }
    }

    return false;
}

inline int BatteryFromTooltip(const std::wstring& tooltip,
                              bool& charging) {
    const std::wstring english = L"Battery ";
    const std::wstring japanese = L"バッテリー 残量 ";

    bool en = tooltip.compare(0, english.size(), english) == 0;
    bool ja = tooltip.compare(0, japanese.size(), japanese) == 0;

    if (!en && !ja) return -1;

    size_t start = en ? english.size() : japanese.size();
    size_t end = start;

    while (end < tooltip.size() &&
           tooltip[end] >= L'0' && tooltip[end] <= L'9' &&
           end - start < 3) {
        ++end;
    }

    if (end == start || end >= tooltip.size() ||
        tooltip[end] != L'%') return -1;

    int level = std::stoi(tooltip.substr(start, end - start));
    if (level > 100) return -1;

    const std::wstring remainder = tooltip.substr(end + 1);

    if (en) {
        if (remainder != L" remaining" &&
            remainder != L" remaining, charging")
            return -1;

        charging = remainder == L" remaining, charging";
    } else {
        if (!remainder.empty()) return -1;
        charging = false;  // Charging state not established for Japanese cards.
    }

    return level;
}


inline bool Parse(const std::wstring& json, Reading& out) {
    size_t cursor = 0;
    std::wstring speak;

    if (!JsonString(json, L"speak", cursor, speak))
        return false;

    Reading parsed;
    if (!ConnectedPhoneName(speak, parsed.name))
        return false;

    const std::wstring japaneseSuffix = L" - PC に接続しました";
    const bool japanese =
        speak.size() >= japaneseSuffix.size() &&
        speak.compare(
            speak.size() - japaneseSuffix.size(),
            japaneseSuffix.size(),
            japaneseSuffix) == 0;

    cursor = 0;
    std::wstring tooltip;

    while (JsonString(json, L"tooltip", cursor, tooltip)) {
        // Ignore tooltip languages that do not match the
        // recognized phone connection message.
        const bool japaneseTooltip =
            tooltip.rfind(L"バッテリー 残量 ", 0) == 0;

        if (japaneseTooltip != japanese)
            continue;

        bool charging = false;
        const int percent =
            BatteryFromTooltip(tooltip, charging);

        if (percent < 0)
            continue;

        parsed.percent = percent;
        parsed.charging = charging;
        out = parsed;
        return true;
    }

    return false;
}


}  // namespace PhoneLink
