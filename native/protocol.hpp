// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <vector>
namespace miao
{
using Report = std::array<unsigned char, 65>;
struct Battery
{
    int mouse, dock;
};
struct Stage
{
    int x, y;
};
struct Dpi
{
    int profile, active;
    std::vector<Stage> stages;
};
inline Report packet(unsigned char opcode, unsigned char arg = 0, bool checksum = false)
{
    Report r{};
    r[1] = opcode;
    r[2] = arg;
    if (checksum)
        r[8] = static_cast<unsigned char>(255 - ((opcode + arg) & 255));
    return r;
}
inline Battery parseBattery(const Report &r)
{
    if (r[0] != 5 || r[1] > 1 || r[2] != 0 || r[3] > 100 || r[11] > 100 ||
        std::all_of(r.begin() + 2, r.end(), [](auto b) { return b == 0; }))
        throw std::runtime_error("Invalid battery status");
    return {r[3], r[11]};
}
inline Dpi parseDpi(const Report &r)
{
    if (r[0] != 0 || r[1] != 0xD4 || r[2] > 7 || r[4] < 1 || r[4] > 8 || r[3] >= r[4])
        throw std::runtime_error("Invalid DPI response");
    Dpi d{r[2], r[3], {}};
    for (int i = 0; i < r[4]; ++i)
    {
        int x = r[9 + 2 * i] | (r[10 + 2 * i] << 8), y = r[25 + 2 * i] | (r[26 + 2 * i] << 8);
        if (x < 1 || y < 1)
            throw std::runtime_error("Invalid DPI value");
        d.stages.push_back({x, y});
    }
    return d;
}
inline Report switchRequest(const Report &source, int stage)
{
    auto d = parseDpi(source);
    if (stage < 0 || stage >= static_cast<int>(d.stages.size()))
        throw std::out_of_range("Disabled DPI stage");
    auto r = source;
    r[1] = 0x54;
    r[3] = static_cast<unsigned char>(stage);
    int sum = 0;
    for (int i = 1; i <= 7; ++i)
        sum += r[i];
    r[8] = static_cast<unsigned char>(255 - (sum & 255));
    return r;
}
inline void verifySwitch(const Report &before, const Report &after, int stage)
{
    auto a = parseDpi(after);
    auto b = parseDpi(before);
    if (a.active != stage || a.profile != b.profile)
        throw std::runtime_error("DPI stage unconfirmed");
    for (int i = 0; i < 65; ++i)
        if (i != 1 && i != 3 && i != 8 && before[i] != after[i])
            throw std::runtime_error("DPI configuration changed concurrently");
}
} // namespace miao
