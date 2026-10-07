#include "../internal/hash_index.h"
#include "../internal/probe.h"

#include <array>
#include <climits>
#include <cstddef>
#include <iostream>
#include <set>

using nova::internal::HashIndex;
using nova::internal::IndexStatus;
using nova::internal::ProbeSequence;
using nova::internal::UpsertPosition;
using nova::internal::ValueRef;
using nova::internal::kGroupWidth;

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

bool upsert(
    HashIndex& index,
    long key,
    ValueRef value
) {
    UpsertPosition position{};

    if (
        index.prepare_upsert(
            key,
            position
        ) != IndexStatus::kOk
    ) {
        return false;
    }

    index.commit_upsert(
        key,
        value,
        position
    );

    return true;
}

bool basic_insert_find_overwrite_erase() {
    HashIndex index(16);

    CHECK(
        upsert(
            index,
            42,
            {100, 8}
        )
    );

    CHECK(index.size() == 1);

    ValueRef out{};

    CHECK(
        index.find(
            42,
            out
        ) == IndexStatus::kOk
    );

    CHECK(out.offset == 100);
    CHECK(out.length == 8);

    // Overwrite same key.
    CHECK(
        upsert(
            index,
            42,
            {200, 16}
        )
    );

    // It must still be exactly one table entry.
    CHECK(index.size() == 1);

    CHECK(
        index.find(
            42,
            out
        ) == IndexStatus::kOk
    );

    CHECK(out.offset == 200);
    CHECK(out.length == 16);

    CHECK(
        index.erase(42) ==
        IndexStatus::kOk
    );

    CHECK(index.size() == 0);

    CHECK(
        index.find(
            42,
            out
        ) == IndexStatus::kNotFound
    );

    CHECK(
        index.erase(42) ==
        IndexStatus::kNotFound
    );

    return true;
}

bool tombstone_does_not_break_lookup_and_is_reused() {
    // Capacity 16 = exactly one logical group.
    // This makes collision/tombstone behavior easy to test.
    HashIndex index(16);

    CHECK(
        upsert(index, 1, {10, 1})
    );

    CHECK(
        upsert(index, 2, {20, 1})
    );

    CHECK(
        upsert(index, 3, {30, 1})
    );

    CHECK(
        index.erase(1) ==
        IndexStatus::kOk
    );

    CHECK(
        index.deleted_count() == 1
    );

    ValueRef out{};

    // Tombstone must NOT make later keys disappear.
    CHECK(
        index.find(2, out) ==
        IndexStatus::kOk
    );

    CHECK(
        index.find(3, out) ==
        IndexStatus::kOk
    );

    // New insertion should reuse the deleted slot.
    CHECK(
        upsert(index, 4, {40, 1})
    );

    CHECK(
        index.deleted_count() == 0
    );

    CHECK(
        index.find(4, out) ==
        IndexStatus::kOk
    );

    return true;
}

bool resize_preserves_entries_and_cleans_tombstones() {
    HashIndex index(16);

    // Enough inserts to force the initial tiny table to grow.
    for (long key = 0; key < 20; ++key) {
        CHECK(
            upsert(
                index,
                key,
                {
                    static_cast<std::size_t>(
                        key * 10
                    ),
                    4
                }
            )
        );
    }

    CHECK(
        index.capacity() > 16
    );

    // Every old mapping must survive rehash.
    for (long key = 0; key < 20; ++key) {
        ValueRef out{};

        CHECK(
            index.find(
                key,
                out
            ) == IndexStatus::kOk
        );

        CHECK(
            out.offset ==
            static_cast<std::size_t>(
                key * 10
            )
        );
    }

    // Create a tombstone.
    CHECK(
        index.erase(5) ==
        IndexStatus::kOk
    );

    CHECK(
        index.deleted_count() == 1
    );

    const auto before =
        index.capacity();

    // Keep inserting until another resize happens.
    for (
        long key = 20;
        index.capacity() == before;
        ++key
    ) {
        CHECK(
            upsert(
                index,
                key,
                {
                    static_cast<std::size_t>(
                        key * 10
                    ),
                    4
                }
            )
        );
    }

    // Rehash copies only live entries.
    CHECK(
        index.deleted_count() == 0
    );

    return true;
}

bool extreme_long_keys_work() {
    HashIndex index(16);

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
        CHECK(
            upsert(
                index,
                keys[i],
                {
                    i * 100,
                    i + 1
                }
            )
        );
    }

    for (
        std::size_t i = 0;
        i < keys.size();
        ++i
    ) {
        ValueRef out{};

        CHECK(
            index.find(
                keys[i],
                out
            ) == IndexStatus::kOk
        );

        CHECK(
            out.offset == i * 100
        );

        CHECK(
            out.length == i + 1
        );
    }

    return true;
}

bool triangular_probe_visits_every_group() {
    constexpr std::size_t capacity =
        64;

    constexpr std::size_t group_count =
        capacity / kGroupWidth;

    const std::size_t start =
        7;

    ProbeSequence probe(
        start,
        capacity
    );

    std::set<std::size_t> starts;

    for (
        std::size_t i = 0;
        i < group_count;
        ++i
    ) {
        starts.insert(
            probe.group_start()
        );

        probe.next();
    }

    // No group-start was repeated early.
    CHECK(
        starts.size() ==
        group_count
    );

    // Every start has the same offset within its
    // 16-slot logical group partition.
    for (
        const auto group_start :
        starts
    ) {
        CHECK(
            (group_start - start)
                % kGroupWidth == 0
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
            "basic insert/find/overwrite/erase",
            basic_insert_find_overwrite_erase
        },
        {
            "tombstone lookup/reuse",
            tombstone_does_not_break_lookup_and_is_reused
        },
        {
            "resize preserves entries",
            resize_preserves_entries_and_cleans_tombstones
        },
        {
            "extreme long keys",
            extreme_long_keys_work
        },
        {
            "probe covers all groups",
            triangular_probe_visits_every_group
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