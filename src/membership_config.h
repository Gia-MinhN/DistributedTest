#pragma once

#include <cstdint>

inline constexpr uint64_t TICK_MS    = 1000;
inline constexpr uint64_t SUSPECT_MS = 6000;
inline constexpr uint64_t DEAD_MS    = 12000;

inline constexpr size_t FANOUT = 3;
inline constexpr size_t PIGGY_K = 3;