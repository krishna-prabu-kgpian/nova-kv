#include "../internal/value_arena.h"

#include <array>
#include <cstdint>
#include <iostream>

using nova::internal::ArenaStatus;
using nova::internal::ValueArena;
using nova::internal::ValueRef;

namespace {

#define CHECK(condition)                                      \
    do {                                                      \
        if (!(condition)) {                                   \
            std::cerr                                         \
                << "CHECK failed at "                         \
                << __FILE__                                   \
                << ':'                                        \
                << __LINE__                                   \
                << ": " #condition                            \
                << '\n';                                      \
            return false;                                     \
        }                                                     \
    } while (false)

bool basic_append_and_copy() {
    ValueArena arena(64);

    const std::array<std::uint8_t, 5> input{
        1, 2, 3, 4, 5
    };

    ValueRef ref{};

    CHECK(
        arena.append(
            input.data(),
            input.size(),
            ref
        ) == ArenaStatus::kOk
    );

    CHECK(ref.offset == 0);
    CHECK(ref.length == input.size());

    std::array<std::uint8_t, 5> output{};

    CHECK(
        arena.copy_out(
            ref,
            output.data()
        ) == ArenaStatus::kOk
    );

    CHECK(output == input);

    return true;
}

bool multiple_values() {
    ValueArena arena(64);

    const std::array<std::uint8_t, 3> a{
        10, 20, 30
    };

    const std::array<std::uint8_t, 4> b{
        40, 50, 60, 70
    };

    ValueRef ar{};
    ValueRef br{};

    CHECK(
        arena.append(
            a.data(),
            a.size(),
            ar
        ) == ArenaStatus::kOk
    );

    CHECK(
        arena.append(
            b.data(),
            b.size(),
            br
        ) == ArenaStatus::kOk
    );

    CHECK(ar.offset == 0);
    CHECK(br.offset == a.size());

    std::array<std::uint8_t, 3> ao{};
    std::array<std::uint8_t, 4> bo{};

    CHECK(
        arena.copy_out(
            ar,
            ao.data()
        ) == ArenaStatus::kOk
    );

    CHECK(
        arena.copy_out(
            br,
            bo.data()
        ) == ArenaStatus::kOk
    );

    CHECK(ao == a);
    CHECK(bo == b);

    return true;
}

bool growth_preserves_offsets() {
    // Deliberately tiny so the second append forces growth.
    ValueArena arena(4);

    const std::array<std::uint8_t, 4> a{
        1, 2, 3, 4
    };

    const std::array<std::uint8_t, 20> b{
        5, 6, 7, 8, 9,
        10, 11, 12, 13, 14,
        15, 16, 17, 18, 19,
        20, 21, 22, 23, 24
    };

    ValueRef ar{};
    ValueRef br{};

    CHECK(
        arena.append(
            a.data(),
            a.size(),
            ar
        ) == ArenaStatus::kOk
    );

    const auto old_capacity =
        arena.capacity();

    CHECK(
        arena.append(
            b.data(),
            b.size(),
            br
        ) == ArenaStatus::kOk
    );

    CHECK(
        arena.capacity() > old_capacity
    );

    // The arena moved internally, but the original
    // offset must still locate A.
    std::array<std::uint8_t, 4> ao{};

    CHECK(
        arena.copy_out(
            ar,
            ao.data()
        ) == ArenaStatus::kOk
    );

    CHECK(ao == a);

    return true;
}

bool binary_zero_bytes() {
    ValueArena arena(8);

    // Confirms this really stores opaque bytes rather
    // than treating data as C strings.
    const std::array<std::uint8_t, 8> input{
        0, 1, 0, 2, 0, 3, 0, 4
    };

    ValueRef ref{};

    CHECK(
        arena.append(
            input.data(),
            input.size(),
            ref
        ) == ArenaStatus::kOk
    );

    std::array<std::uint8_t, 8> output{};

    CHECK(
        arena.copy_out(
            ref,
            output.data()
        ) == ArenaStatus::kOk
    );

    CHECK(output == input);

    return true;
}

bool invalid_arguments() {
    ValueArena arena(8);

    ValueRef ref{};

    CHECK(
        arena.append(
            nullptr,
            4,
            ref
        ) == ArenaStatus::kInvalidArgument
    );

    return true;
}

}  // namespace

int main() {
    const struct {
        const char* name;
        bool (*fn)();
    } tests[] = {
        {
            "basic append/copy",
            basic_append_and_copy
        },
        {
            "multiple values",
            multiple_values
        },
        {
            "growth preserves offsets",
            growth_preserves_offsets
        },
        {
            "binary zero bytes",
            binary_zero_bytes
        },
        {
            "invalid arguments",
            invalid_arguments
        },
    };

    for (const auto& test : tests) {
        if (!test.fn()) {
            std::cerr
                << "FAILED: "
                << test.name
                << '\n';

            return 1;
        }

        std::cout
            << "PASS: "
            << test.name
            << '\n';
    }

    return 0;
}