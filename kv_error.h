#pragma once

#include <cstdint>

enum class KVError : std::uint8_t {
    NONE = 0,

    KEY_NOT_FOUND,
    INVALID_ARGUMENT,
    BUFFER_TOO_SMALL,

    OUT_OF_MEMORY,
    TABLE_FULL,
    SIZE_OVERFLOW,
};

KVError kv_last_error() noexcept;

const char* kv_error_string(
    KVError error
) noexcept;

namespace nova::detail {

void set_last_error(
    KVError error
) noexcept;

}  // namespace nova::detail