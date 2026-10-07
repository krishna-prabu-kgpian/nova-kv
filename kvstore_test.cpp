#include "kvstore.h"
#include "kv_error.h"

#include <array>
#include <climits>
#include <cstdint>
#include <iostream>

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

bool put_get_round_trip() {
    KVStore store;

    const std::array<std::uint8_t, 6> value{
        1, 2, 0, 4, 5, 6
    };

    std::array<std::uint8_t, 6> output{};

    CHECK(
        store.put(
            10,
            value.data(),
            value.size()
        )
    );

    CHECK(
        kv_last_error() ==
        KVError::NONE
    );

    CHECK(
        store.get(
            10,
            output.data(),
            output.size()
        )
    );

    CHECK(output == value);

    return true;
}

bool overwrite_changes_visible_value() {
    KVStore store;

    const std::array<std::uint8_t, 3> first{
        1, 2, 3
    };

    const std::array<std::uint8_t, 7> second{
        9, 8, 7, 6, 5, 4, 3
    };

    CHECK(
        store.put(
            7,
            first.data(),
            first.size()
        )
    );

    CHECK(
        store.put(
            7,
            second.data(),
            second.size()
        )
    );

    std::array<std::uint8_t, 7> output{};

    CHECK(
        store.get(
            7,
            output.data(),
            output.size()
        )
    );

    CHECK(output == second);

    return true;
}

bool erase_and_missing_errors() {
    KVStore store;

    std::uint64_t value = 123;
    std::uint64_t output = 0;

    CHECK(
        store.put(
            99,
            &value,
            sizeof(value)
        )
    );

    CHECK(
        store.erase(99)
    );

    CHECK(
        !store.get(
            99,
            &output,
            sizeof(output)
        )
    );

    CHECK(
        kv_last_error() ==
        KVError::KEY_NOT_FOUND
    );

    CHECK(
        !store.erase(99)
    );

    CHECK(
        kv_last_error() ==
        KVError::KEY_NOT_FOUND
    );

    return true;
}

bool buffer_too_small_copies_nothing() {
    KVStore store;

    const std::array<std::uint8_t, 4> value{
        1, 2, 3, 4
    };

    // Sentinel contents let us prove get() did not
    // partially overwrite the output buffer.
    std::array<std::uint8_t, 3> output{
        9, 9, 9
    };

    CHECK(
        store.put(
            5,
            value.data(),
            value.size()
        )
    );

    CHECK(
        !store.get(
            5,
            output.data(),
            output.size()
        )
    );

    CHECK(
        kv_last_error() ==
        KVError::BUFFER_TOO_SMALL
    );

    CHECK(
        (
            output ==
            std::array<std::uint8_t, 3>{
                9, 9, 9
            }
        )
    );

    return true;
}

bool invalid_arguments() {
    KVStore store;

    std::uint64_t value = 1;

    CHECK(
        !store.put(
            1,
            nullptr,
            sizeof(value)
        )
    );

    CHECK(
        kv_last_error() ==
        KVError::INVALID_ARGUMENT
    );

    CHECK(
        !store.put(
            1,
            &value,
            0
        )
    );

    CHECK(
        kv_last_error() ==
        KVError::INVALID_ARGUMENT
    );

    CHECK(
        store.put(
            1,
            &value,
            sizeof(value)
        )
    );

    CHECK(
        !store.get(
            1,
            nullptr,
            sizeof(value)
        )
    );

    CHECK(
        kv_last_error() ==
        KVError::INVALID_ARGUMENT
    );

    return true;
}

bool unusual_keys() {
    KVStore store;

    const std::array<long, 4> keys{
        0L,
        -1L,
        LONG_MIN,
        LONG_MAX
    };

    for (
        std::size_t i = 0;
        i < keys.size();
        ++i
    ) {
        const std::uint64_t value =
            1000 + i;

        CHECK(
            store.put(
                keys[i],
                &value,
                sizeof(value)
            )
        );
    }

    for (
        std::size_t i = 0;
        i < keys.size();
        ++i
    ) {
        std::uint64_t output = 0;

        CHECK(
            store.get(
                keys[i],
                &output,
                sizeof(output)
            )
        );

        CHECK(
            output ==
            1000 + i
        );
    }

    return true;
}

}  // namespace

int main() {
    const struct {
        const char* name;
        bool (*fn)();
    } tests[] = {
        {
            "put/get round trip",
            put_get_round_trip
        },
        {
            "overwrite",
            overwrite_changes_visible_value
        },
        {
            "erase/missing",
            erase_and_missing_errors
        },
        {
            "buffer too small",
            buffer_too_small_copies_nothing
        },
        {
            "invalid arguments",
            invalid_arguments
        },
        {
            "unusual keys",
            unusual_keys
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