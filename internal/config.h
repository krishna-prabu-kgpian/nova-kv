#pragma once

#include <cstddef>

namespace nova::internal {

inline constexpr std::size_t kGroupWidth = 16;

// Default workload is ~100k entries. 2^17 = 131072 gives us
// comfortable space for the initial implementation.
inline constexpr std::size_t kInitialTableCapacity = 1u << 17;

// Provisional arena preallocation.
// We will tune this later based on actual value sizes/workload.
inline constexpr std::size_t kInitialArenaBytes =
    16u * 1024u * 1024u;

}  // namespace nova::internal