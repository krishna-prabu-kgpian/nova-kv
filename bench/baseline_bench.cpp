#include "../kvstore.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

std::uint64_t next_random(std::uint64_t& state) {
    state =
        state * 6364136223846793005ULL +
        1442695040888963407ULL;

    return state;
}

void print_result(
    const char* name,
    std::size_t operations,
    Clock::duration elapsed
) {
    const double seconds =
        std::chrono::duration<double>(elapsed).count();

    const double ns_per_op =
        std::chrono::duration<double, std::nano>(
            elapsed
        ).count() /
        static_cast<double>(operations);

    const double mops =
        static_cast<double>(operations) /
        seconds /
        1'000'000.0;

    std::cout
        << std::left
        << std::setw(20)
        << name
        << "  "
        << std::fixed
        << std::setprecision(2)
        << mops
        << " Mops/s"
        << "    "
        << ns_per_op
        << " ns/op\n";
}

std::size_t parse_size(
    int argc,
    char** argv,
    int index,
    std::size_t fallback
) {
    if (argc <= index) {
        return fallback;
    }

    return static_cast<std::size_t>(
        std::strtoull(
            argv[index],
            nullptr,
            10
        )
    );
}

}  // namespace

int main(
    int argc,
    char** argv
) {
    const std::size_t entries =
        parse_size(
            argc,
            argv,
            1,
            100'000
        );

    const std::size_t query_ops =
        parse_size(
            argc,
            argv,
            2,
            5'000'000
        );

    const std::size_t value_size =
        parse_size(
            argc,
            argv,
            3,
            64
        );

    if (
        entries == 0 ||
        query_ops == 0 ||
        value_size == 0
    ) {
        std::cerr
            << "entries, query_ops and value_size "
            << "must all be non-zero\n";

        return 1;
    }

    std::cout
        << "entries    = "
        << entries
        << '\n'
        << "query ops  = "
        << query_ops
        << '\n'
        << "value size = "
        << value_size
        << " bytes\n\n";

    /*
     * Keep the source value fixed.
     *
     * That deliberately measures KV-store cost rather
     * than application-side record generation cost.
     */
    std::vector<std::byte> value(
        value_size
    );

    for (
        std::size_t i = 0;
        i < value.size();
        ++i
    ) {
        value[i] =
            static_cast<std::byte>(
                i & 0xFF
            );
    }

    std::vector<std::byte> output(
        value_size
    );

    /*
     * Generate queries BEFORE timing.
     *
     * We do not want pseudo-random-number generation
     * included in get() timings.
     */
    std::vector<long> hit_keys(
        query_ops
    );

    std::vector<long> miss_keys(
        query_ops
    );

    std::uint64_t rng =
        0x123456789abcdef0ULL;

    for (
        std::size_t i = 0;
        i < query_ops;
        ++i
    ) {
        const std::size_t id =
            static_cast<std::size_t>(
                next_random(rng) %
                entries
            );

        hit_keys[i] =
            static_cast<long>(id);

        miss_keys[i] =
            static_cast<long>(
                entries + 1 + id
            );
    }

    KVStore store;

    /*
     * INSERT
     */
    auto begin =
        Clock::now();

    for (
        std::size_t i = 0;
        i < entries;
        ++i
    ) {
        if (
            !store.put(
                static_cast<long>(i),
                value.data(),
                value.size()
            )
        ) {
            std::cerr
                << "put failed at key "
                << i
                << '\n';

            return 1;
        }
    }

    auto end =
        Clock::now();

    print_result(
        "insert",
        entries,
        end - begin
    );

    /*
     * SUCCESSFUL GET
     */
    std::uint64_t checksum = 0;

    begin =
        Clock::now();

    for (
        const long key :
        hit_keys
    ) {
        if (
            !store.get(
                key,
                output.data(),
                output.size()
            )
        ) {
            std::cerr
                << "unexpected miss\n";

            return 1;
        }

        checksum +=
        static_cast<unsigned char>(
            output[value_size - 1]
        );
    }

    end =
        Clock::now();

    print_result(
        "get-hit",
        query_ops,
        end - begin
    );

    /*
     * FAILED GET
     */
    std::size_t expected_misses = 0;

    begin =
        Clock::now();

    for (
        const long key :
        miss_keys
    ) {
        if (
            !store.get(
                key,
                output.data(),
                output.size()
            )
        ) {
            ++expected_misses;
        }
    }

    end =
        Clock::now();

    if (
        expected_misses !=
        query_ops
    ) {
        std::cerr
            << "miss benchmark found "
            << "unexpected keys\n";

        return 1;
    }

    print_result(
        "get-miss",
        query_ops,
        end - begin
    );

    /*
     * OVERWRITE
     *
     * One update per key.
     * This intentionally produces stale arena bytes,
     * matching our current Part A design.
     */
    begin =
        Clock::now();

    for (
        std::size_t i = 0;
        i < entries;
        ++i
    ) {
        if (
            !store.put(
                static_cast<long>(i),
                value.data(),
                value.size()
            )
        ) {
            std::cerr
                << "overwrite failed\n";

            return 1;
        }
    }

    end =
        Clock::now();

    print_result(
        "overwrite",
        entries,
        end - begin
    );

    /*
     * ERASE
     */
    begin =
        Clock::now();

    for (
        std::size_t i = 0;
        i < entries;
        ++i
    ) {
        if (
            !store.erase(
                static_cast<long>(i)
            )
        ) {
            std::cerr
                << "erase failed\n";

            return 1;
        }
    }

    end =
        Clock::now();

    print_result(
        "erase",
        entries,
        end - begin
    );

    /*
     * Prevent the successful-get loop from being
     * treated as dead work.
     */
    std::cout
        << "\nchecksum = "
        << checksum
        << '\n';

    return 0;
}