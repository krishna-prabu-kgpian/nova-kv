#include "value_arena.h"

#include <cstdlib>
#include <cstring>
#include <limits>

namespace nova::internal {

ValueArena::ValueArena(std::size_t initial_capacity) noexcept {
    if (initial_capacity == 0) {
        return;
    }

    void* raw = std::malloc(initial_capacity);

    if (raw == nullptr) {
        // Initial preallocation is best-effort.
        //
        // If it fails, the arena remains empty. The first append()
        // will try allocating the actual amount it needs and can
        // report kOutOfMemory cleanly.
        return;
    }

    data_ = static_cast<std::byte*>(raw);
    capacity_ = initial_capacity;
}

ValueArena::~ValueArena() {
    std::free(data_);
}

ArenaStatus ValueArena::append(
    const void* src,
    std::size_t length,
    ValueRef& out
) noexcept {
    if (length != 0 && src == nullptr) {
        return ArenaStatus::kInvalidArgument;
    }

    // Prevent:
    //
    // used_ + length
    //
    // from wrapping size_t.
    if (length >
        std::numeric_limits<std::size_t>::max() - used_) {
        return ArenaStatus::kSizeOverflow;
    }

    const std::size_t required = used_ + length;

    if (required > capacity_) {
        const ArenaStatus status = grow(required);

        if (status != ArenaStatus::kOk) {
            return status;
        }
    }

    const std::size_t offset = used_;

    if (length != 0) {
        std::memcpy(
            data_ + offset,
            src,
            length
        );
    }

    used_ = required;

    out = ValueRef{
        offset,
        length
    };

    return ArenaStatus::kOk;
}

ArenaStatus ValueArena::copy_out(
    ValueRef ref,
    void* dst
) const noexcept {
    if (ref.length != 0 && dst == nullptr) {
        return ArenaStatus::kInvalidArgument;
    }

    if (!contains(ref)) {
        return ArenaStatus::kInvalidArgument;
    }

    if (ref.length != 0) {
        std::memcpy(
            dst,
            data_ + ref.offset,
            ref.length
        );
    }

    return ArenaStatus::kOk;
}

ArenaStatus ValueArena::grow(
    std::size_t required_capacity
) noexcept {
    if (required_capacity <= capacity_) {
        return ArenaStatus::kOk;
    }

    std::size_t new_capacity =
        capacity_ == 0 ? 1 : capacity_;

    const std::size_t max =
        std::numeric_limits<std::size_t>::max();

    while (new_capacity < required_capacity) {
        // Avoid overflow while doubling.
        if (new_capacity > max / 2) {
            new_capacity = required_capacity;
            break;
        }

        new_capacity *= 2;
    }

    void* raw = std::malloc(new_capacity);

    if (raw == nullptr) {
        return ArenaStatus::kOutOfMemory;
    }

    auto* new_data =
        static_cast<std::byte*>(raw);

    if (used_ != 0) {
        std::memcpy(
            new_data,
            data_,
            used_
        );
    }

    std::free(data_);

    data_ = new_data;
    capacity_ = new_capacity;

    return ArenaStatus::kOk;
}

bool ValueArena::contains(
    ValueRef ref
) const noexcept {
    if (ref.offset > used_) {
        return false;
    }

    // Instead of:
    //
    // ref.offset + ref.length <= used_
    //
    // write it this way so the addition cannot overflow.
    return ref.length <= used_ - ref.offset;
}

}  // namespace nova::internal