#pragma once

#include <cstddef>

namespace nova::internal {

struct ValueRef {
    std::size_t offset{0};
    std::size_t length{0};
};

}  // namespace nova::internal