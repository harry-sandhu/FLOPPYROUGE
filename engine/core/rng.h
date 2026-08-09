#pragma once
#include <cstdint>

namespace RNG {
    inline uint32_t& State() {
        static uint32_t state = 0x6C8E9CF5u;
        return state;
    }

    inline void Seed(uint32_t seed) {
        State() = seed ? seed : 0x6C8E9CF5u;
    }

    inline uint32_t NextU32() {
        uint32_t x = State();
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        State() = x;
        return x;
    }

    inline int Range(int minInclusive, int maxInclusive) {
        if (maxInclusive <= minInclusive) return minInclusive;
        uint32_t span = (uint32_t)(maxInclusive - minInclusive + 1);
        return minInclusive + (int)(NextU32() % span);
    }

    inline float Range(float minInclusive, float maxInclusive) {
        if (maxInclusive <= minInclusive) return minInclusive;
        float t = (NextU32() & 0xFFFFFFu) / 16777215.0f;
        return minInclusive + (maxInclusive - minInclusive) * t;
    }

    inline bool Chance(float probability01) {
        if (probability01 <= 0.0f) return false;
        if (probability01 >= 1.0f) return true;
        return Range(0.0f, 1.0f) < probability01;
    }
}
