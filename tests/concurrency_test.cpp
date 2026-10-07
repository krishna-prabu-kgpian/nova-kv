#include "../kvstore.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

namespace {

constexpr long kKey = 42;

constexpr std::size_t kWords = 16;

constexpr std::uint64_t kWriterIterations =
    100'000;

constexpr int kReaderCount = 8;

struct Record {
    std::array<std::uint64_t, kWords> words;
};

Record make_record(
    std::uint64_t version
) {
    Record record{};

    for (auto& word : record.words) {
        word = version;
    }

    return record;
}

bool valid_record(
    const Record& record
) {
    const std::uint64_t expected =
        record.words[0];

    for (const auto word : record.words) {
        if (word != expected) {
            return false;
        }
    }

    return true;
}

}  // namespace


int main() {
    KVStore store;

    /*
     * Put an initial valid value before readers begin.
     * Therefore a reader should never see KEY_NOT_FOUND.
     */
    const Record initial =
        make_record(0);

    if (
        !store.put(
            kKey,
            &initial,
            sizeof(initial)
        )
    ) {
        std::cerr
            << "initial put failed\n";

        return 1;
    }

    std::atomic<bool> start{false};
    std::atomic<bool> writer_done{false};

    std::atomic<std::uint64_t>
        read_count{0};

    std::atomic<std::uint64_t>
        error_count{0};


    /*
     * Writer continually replaces the whole value.
     *
     * Every individual version is:
     *
     *     [x x x x x ... x]
     *
     * A reader may legally observe any complete version.
     *
     * It must never observe:
     *
     *     [7 7 7 8 8 8 ...]
     */
    std::thread writer(
        [&]() {
            while (
                !start.load(
                    std::memory_order_acquire
                )
            ) {
                std::this_thread::yield();
            }

            for (
                std::uint64_t version = 1;
                version <= kWriterIterations;
                ++version
            ) {
                const Record record =
                    make_record(version);

                if (
                    !store.put(
                        kKey,
                        &record,
                        sizeof(record)
                    )
                ) {
                    error_count.fetch_add(
                        1,
                        std::memory_order_relaxed
                    );

                    break;
                }
            }

            writer_done.store(
                true,
                std::memory_order_release
            );
        }
    );


    std::vector<std::thread> readers;

    readers.reserve(kReaderCount);

    for (
        int i = 0;
        i < kReaderCount;
        ++i
    ) {
        readers.emplace_back(
            [&]() {
                while (
                    !start.load(
                        std::memory_order_acquire
                    )
                ) {
                    std::this_thread::yield();
                }

                Record output{};

                while (
                    !writer_done.load(
                        std::memory_order_acquire
                    )
                ) {
                    if (
                        !store.get(
                            kKey,
                            &output,
                            sizeof(output)
                        )
                    ) {
                        error_count.fetch_add(
                            1,
                            std::memory_order_relaxed
                        );

                        continue;
                    }

                    if (!valid_record(output)) {
                        error_count.fetch_add(
                            1,
                            std::memory_order_relaxed
                        );
                    }

                    read_count.fetch_add(
                        1,
                        std::memory_order_relaxed
                    );
                }

                /*
                 * Perform a few extra reads after the
                 * writer exits as an additional check.
                 */
                for (int j = 0; j < 1000; ++j) {
                    if (
                        !store.get(
                            kKey,
                            &output,
                            sizeof(output)
                        ) ||
                        !valid_record(output)
                    ) {
                        error_count.fetch_add(
                            1,
                            std::memory_order_relaxed
                        );
                    }

                    read_count.fetch_add(
                        1,
                        std::memory_order_relaxed
                    );
                }
            }
        );
    }


    start.store(
        true,
        std::memory_order_release
    );

    writer.join();

    for (auto& reader : readers) {
        reader.join();
    }


    const auto errors =
        error_count.load();

    const auto reads =
        read_count.load();

    std::cout
        << "reads  = "
        << reads
        << '\n';

    std::cout
        << "errors = "
        << errors
        << '\n';


    if (errors != 0) {
        std::cerr
            << "FAILED: observed invalid/torn reads\n";

        return 1;
    }

    std::cout
        << "PASS: no torn reads observed\n";

    return 0;
}