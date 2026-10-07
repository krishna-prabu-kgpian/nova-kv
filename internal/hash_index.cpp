#include "hash_index.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>

#include "hash.h"
#include "probe.h"

namespace nova::internal {

HashIndex::HashIndex(
    std::size_t initial_capacity
) noexcept {
    initial_capacity_hint_ =
        normalize_capacity(initial_capacity);

    if (initial_capacity_hint_ == 0) {
        initial_capacity_hint_ =
            kGroupWidth;
    }

    // Eager allocation is best effort.
    //
    // If this fails, prepare_upsert() will
    // retry and can return an actual error.
    (void)rehash(initial_capacity_hint_);
}

HashIndex::~HashIndex() {
    delete[] controls_;
    delete[] slots_;
}

IndexStatus HashIndex::find(
    long key,
    ValueRef& out
) const noexcept {
    if (capacity_ == 0) {
        return IndexStatus::kNotFound;
    }

    const ProbeResult result =
        probe(key);

    if (!result.found) {
        return IndexStatus::kNotFound;
    }

    out =
        slots_[result.slot].value;

    return IndexStatus::kOk;
}

IndexStatus HashIndex::prepare_upsert(
    long key,
    UpsertPosition& out
) noexcept {
    // Initial allocation may have failed during
    // construction. Retry now, where an error can
    // actually be returned to the caller.
    if (capacity_ == 0) {
        const IndexStatus status =
            rehash(initial_capacity_hint_);

        if (status != IndexStatus::kOk) {
            return status;
        }
    }

    ProbeResult result =
        probe(key);

    // Existing key: overwriting does not increase
    // table occupancy, therefore no resize needed.
    if (result.found) {
        out = UpsertPosition{
            result.slot,
            true
        };

        return IndexStatus::kOk;
    }

    const bool need_growth =
        would_exceed_load_with_one_more() ||
        !result.has_insert_slot;

    if (need_growth) {
        if (
            capacity_ >
            std::numeric_limits<std::size_t>::max() / 2
        ) {
            return IndexStatus::kSizeOverflow;
        }

        const IndexStatus status =
            rehash(capacity_ * 2);

        if (status != IndexStatus::kOk) {
            return status;
        }

        // Every slot moved during the rehash.
        // Therefore probe again.
        result = probe(key);

        if (result.found) {
            // The key was absent before rehashing, so this
            // is not expected in normal operation.
            //
            // Still, keeping this path makes the method
            // internally self-consistent.
            out = UpsertPosition{
                result.slot,
                true
            };

            return IndexStatus::kOk;
        }
    }

    if (!result.has_insert_slot) {
        return IndexStatus::kTableFull;
    }

    out = UpsertPosition{
        result.slot,
        false
    };

    return IndexStatus::kOk;
}

void HashIndex::commit_upsert(
    long key,
    ValueRef value,
    const UpsertPosition& position
) noexcept {
    if (position.existing) {
        // Key stays where it is.
        // Only its value reference changes.
        slots_[position.slot].value =
            value;

        return;
    }

    const bool reusing_deleted =
        controls_[position.slot] ==
        kDeleted;

    const std::uint64_t hash =
        hash_key(key);

    slots_[position.slot] =
        Slot{
            key,
            value
        };

    controls_[position.slot] =
        hash_h2(hash);

    ++size_;

    if (reusing_deleted) {
        --deleted_;
    }
}

IndexStatus HashIndex::erase(
    long key
) noexcept {
    if (capacity_ == 0) {
        return IndexStatus::kNotFound;
    }

    const ProbeResult result =
        probe(key);

    if (!result.found) {
        return IndexStatus::kNotFound;
    }

    controls_[result.slot] =
        kDeleted;

    --size_;
    ++deleted_;

    return IndexStatus::kOk;
}

std::size_t HashIndex::normalize_capacity(
    std::size_t requested
) noexcept {
    requested =
        std::max(
            requested,
            kGroupWidth
        );

    // Start at 16 and repeatedly double.
    //
    // Therefore capacity remains:
    //
    // - a power of two
    // - divisible by kGroupWidth
    std::size_t capacity =
        kGroupWidth;

    while (capacity < requested) {
        if (
            capacity >
            std::numeric_limits<std::size_t>::max() / 2
        ) {
            return 0;
        }

        capacity *= 2;
    }

    return capacity;
}

HashIndex::ProbeResult HashIndex::probe(
    long key
) const noexcept {
    ProbeResult result{};

    if (capacity_ == 0) {
        return result;
    }

    const std::uint64_t hash =
        hash_key(key);

    const std::uint8_t h2 =
        hash_h2(hash);

    const std::size_t mask =
        capacity_ - 1;

    const std::size_t start =
        static_cast<std::size_t>(
            hash_h1(hash)
        ) & mask;

    const std::size_t group_count =
        capacity_ / kGroupWidth;

    ProbeSequence sequence(
        start,
        capacity_
    );

    std::size_t first_deleted = 0;
    bool has_deleted = false;

    for (
        std::size_t group = 0;
        group < group_count;
        ++group
    ) {
        const std::size_t group_start =
            sequence.group_start();

        std::size_t first_empty = 0;
        bool has_empty = false;

        /*
         * Important:
         *
         * Search the ENTIRE current group for H2/key
         * matches before treating EMPTY as termination.
         *
         * H2 only identifies candidates. The long key
         * itself decides equality.
         */
        for (
            std::size_t i = 0;
            i < kGroupWidth;
            ++i
        ) {
            const std::size_t slot =
                (group_start + i) &
                mask;

            const std::uint8_t control =
                controls_[slot];

            if (
                control == h2 &&
                slots_[slot].key == key
            ) {
                return ProbeResult{
                    true,
                    true,
                    slot
                };
            }

            if (
                control == kDeleted &&
                !has_deleted
            ) {
                first_deleted = slot;
                has_deleted = true;
            }
            else if (
                control == kEmpty &&
                !has_empty
            ) {
                first_empty = slot;
                has_empty = true;
            }
        }

        /*
         * Once a probed group contains EMPTY, the key
         * cannot have been inserted into a later group.
         *
         * But for insertion, prefer a tombstone seen
         * earlier in the probe sequence.
         */
        if (has_empty) {
            result.has_insert_slot = true;

            result.slot =
                has_deleted
                    ? first_deleted
                    : first_empty;

            return result;
        }

        sequence.next();
    }

    /*
     * There was no EMPTY anywhere in the table.
     *
     * A tombstone is still a valid insertion position.
     */
    if (has_deleted) {
        result.has_insert_slot = true;
        result.slot = first_deleted;
    }

    return result;
}

bool HashIndex::would_exceed_load_with_one_more()
    const noexcept {
    if (capacity_ == 0) {
        return true;
    }

    /*
     * Provisional ~80% live-entry threshold.
     *
     * This is NOT our final tuned value.
     *
     * Avoid floating point:
     *
     *     capacity - capacity/5
     *
     * is approximately 80%.
     */
    const std::size_t max_live =
        capacity_ - capacity_ / 5;

    return size_ >= max_live;
}

IndexStatus HashIndex::rehash(
    std::size_t requested_capacity
) noexcept {
    const std::size_t new_capacity =
        normalize_capacity(
            requested_capacity
        );

    if (new_capacity == 0) {
        return IndexStatus::kSizeOverflow;
    }

    if (
        new_capacity >
        std::numeric_limits<std::size_t>::max()
            / sizeof(Slot)
    ) {
        return IndexStatus::kSizeOverflow;
    }

    auto* new_controls =
        new (std::nothrow)
        std::uint8_t[new_capacity];

    if (new_controls == nullptr) {
        return IndexStatus::kOutOfMemory;
    }

    auto* new_slots =
        new (std::nothrow)
        Slot[new_capacity];

    if (new_slots == nullptr) {
        delete[] new_controls;

        return IndexStatus::kOutOfMemory;
    }

    // Every new slot begins EMPTY.
    std::memset(
        new_controls,
        kEmpty,
        new_capacity
    );

    /*
     * Reinsert only live entries.
     *
     * EMPTY slots are ignored.
     * DELETED slots are ignored.
     *
     * Therefore any rehash also naturally removes
     * accumulated tombstones.
     */
    for (
        std::size_t i = 0;
        i < capacity_;
        ++i
    ) {
        if (!is_occupied(controls_[i])) {
            continue;
        }

        if (
            !place_rehashed(
                new_controls,
                new_slots,
                new_capacity,
                slots_[i]
            )
        ) {
            delete[] new_controls;
            delete[] new_slots;

            return IndexStatus::kTableFull;
        }
    }

    delete[] controls_;
    delete[] slots_;

    controls_ = new_controls;
    slots_ = new_slots;
    capacity_ = new_capacity;

    // All tombstones disappeared.
    deleted_ = 0;

    // size_ does not change:
    // every live entry was preserved exactly once.

    return IndexStatus::kOk;
}

bool HashIndex::place_rehashed(
    std::uint8_t* controls,
    Slot* slots,
    std::size_t capacity,
    const Slot& entry
) noexcept {
    const std::uint64_t hash =
        hash_key(entry.key);

    const std::uint8_t h2 =
        hash_h2(hash);

    const std::size_t mask =
        capacity - 1;

    const std::size_t start =
        static_cast<std::size_t>(
            hash_h1(hash)
        ) & mask;

    const std::size_t group_count =
        capacity / kGroupWidth;

    ProbeSequence sequence(
        start,
        capacity
    );

    for (
        std::size_t group = 0;
        group < group_count;
        ++group
    ) {
        const std::size_t group_start =
            sequence.group_start();

        for (
            std::size_t i = 0;
            i < kGroupWidth;
            ++i
        ) {
            const std::size_t slot =
                (group_start + i) &
                mask;

            if (
                controls[slot] ==
                kEmpty
            ) {
                slots[slot] =
                    entry;

                controls[slot] =
                    h2;

                return true;
            }
        }

        sequence.next();
    }

    return false;
}

}  // namespace nova::internal