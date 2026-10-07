#pragma once

#include <cstddef>
#include <cstdint>

#include "value_ref.h"

namespace nova::internal {

enum class ArenaStatus : std::uint8_t {
    kOk = 0,
    kInvalidArgument,
    kOutOfMemory,
    kSizeOverflow,
};

class ValueArena {
public:
    explicit ValueArena(std::size_t initial_capacity = 0) noexcept;
    ~ValueArena();

    ValueArena(const ValueArena&) = delete;
    ValueArena& operator=(const ValueArena&) = delete;
    ValueArena(ValueArena&&) = delete;
    ValueArena& operator=(ValueArena&&) = delete;

    // Append `length` bytes from src.
    //
    // On success:
    //   out.offset -> start of the copied value
    //   out.length -> number of bytes copied
    ArenaStatus append(
        const void* src,
        std::size_t length,
        ValueRef& out
    ) noexcept;

    // Copy exactly ref.length bytes into dst.
    ArenaStatus copy_out(
        ValueRef ref,
        void* dst
    ) const noexcept;

    std::size_t used() const noexcept {
        return used_;
    }

    std::size_t capacity() const noexcept {
        return capacity_;
    }

    std::size_t remaining() const noexcept {
        return capacity_ - used_;
    }

private:
    ArenaStatus grow(std::size_t required_capacity) noexcept;

    bool contains(ValueRef ref) const noexcept;

    std::byte* data_{nullptr};
    std::size_t used_{0};
    std::size_t capacity_{0};
};

}  // namespace nova::internal