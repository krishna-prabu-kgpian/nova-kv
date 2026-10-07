#pragma once

#include <cstddef>

#include "config.h"

namespace nova::internal {

class ProbeSequence {
public:
    ProbeSequence(
        std::size_t start,
        std::size_t capacity
    ) noexcept
        : mask_(capacity - 1),
          offset_(start & mask_) {}

    std::size_t group_start() const noexcept {
        return offset_;
    }

    void next() noexcept {
        // Group offsets are:
        //
        // 0
        // 1
        // 1 + 2 = 3
        // 1 + 2 + 3 = 6
        // 1 + 2 + 3 + 4 = 10
        // ...
        //
        // multiplied by GROUP_WIDTH.

        step_ += kGroupWidth;

        offset_ =
            (offset_ + step_) &
            mask_;
    }

private:
    std::size_t mask_;
    std::size_t offset_;
    std::size_t step_{0};
};

}  // namespace nova::internal