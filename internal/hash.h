#pragma once

#include <cstdint>

namespace nova::internal {

inline std::uint64_t hash_key(
    long key
) noexcept {
    // SplitMix64-style 64-bit integer mixer.
    //
    // This is an initial sensible choice,
    // not a conclusion that it is the fastest.

    std::uint64_t x =
        static_cast<std::uint64_t>(key);

    x += 0x9e3779b97f4a7c15ULL;

    x =
        (x ^ (x >> 30)) *
        0xbf58476d1ce4e5b9ULL;

    x =
        (x ^ (x >> 27)) *
        0x94d049bb133111ebULL;

    return x ^ (x >> 31);
}

inline std::uint8_t hash_h2(
    std::uint64_t hash
) noexcept {
    return static_cast<std::uint8_t>(
        hash & 0x7FU
    );
}

inline std::uint64_t hash_h1(
    std::uint64_t hash
) noexcept {
    return hash >> 7;
}

}  // namespace nova::internal