#pragma once

#include <cstddef>
#include <cstdint>

#include "config.h"
#include "value_ref.h"

namespace nova::internal {

enum class IndexStatus : std::uint8_t {
    kOk = 0,
    kNotFound,
    kOutOfMemory,
    kTableFull,
    kSizeOverflow,
};

struct UpsertPosition {
    std::size_t slot{0};
    bool existing{false};
};

class HashIndex {
public:
    explicit HashIndex(
        std::size_t initial_capacity =
            kInitialTableCapacity
    ) noexcept;

    ~HashIndex();

    HashIndex(const HashIndex&) = delete;
    HashIndex& operator=(const HashIndex&) = delete;
    HashIndex(HashIndex&&) = delete;
    HashIndex& operator=(HashIndex&&) = delete;

    IndexStatus find(
        long key,
        ValueRef& out
    ) const noexcept;

    // Performs all parts of an upsert that can fail:
    //
    // - searching
    // - deciding whether key exists
    // - resizing
    // - selecting a final insertion slot
    IndexStatus prepare_upsert(
        long key,
        UpsertPosition& out
    ) noexcept;

    // Must only be called with a position obtained
    // from prepare_upsert().
    //
    // No allocation happens here.
    void commit_upsert(
        long key,
        ValueRef value,
        const UpsertPosition& position
    ) noexcept;

    IndexStatus erase(
        long key
    ) noexcept;

    std::size_t size() const noexcept {
        return size_;
    }

    std::size_t capacity() const noexcept {
        return capacity_;
    }

    std::size_t deleted_count() const noexcept {
        return deleted_;
    }

private:
    struct Slot {
        long key;
        ValueRef value;
    };

    struct ProbeResult {
        bool found{false};
        bool has_insert_slot{false};
        std::size_t slot{0};
    };

    static constexpr std::uint8_t kEmpty =
        0x80;

    static constexpr std::uint8_t kDeleted =
        0xFE;

    static bool is_occupied(
        std::uint8_t control
    ) noexcept {
        return control <= 0x7F;
    }

    static std::size_t normalize_capacity(
        std::size_t requested
    ) noexcept;

    ProbeResult probe(
        long key
    ) const noexcept;

    bool would_exceed_load_with_one_more()
        const noexcept;

    IndexStatus rehash(
        std::size_t new_capacity
    ) noexcept;

    static bool place_rehashed(
        std::uint8_t* controls,
        Slot* slots,
        std::size_t capacity,
        const Slot& entry
    ) noexcept;

    std::uint8_t* controls_{nullptr};
    Slot* slots_{nullptr};

    std::size_t capacity_{0};
    std::size_t size_{0};
    std::size_t deleted_{0};

    std::size_t initial_capacity_hint_{
        kGroupWidth
    };
};

}  // namespace nova::internal