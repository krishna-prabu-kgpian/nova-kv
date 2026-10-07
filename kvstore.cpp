#include "kvstore.h"

#include <cassert>

#include "internal/config.h"
#include "kv_error.h"

namespace {

KVError map_index_error(
    nova::internal::IndexStatus status
) noexcept {
    using nova::internal::IndexStatus;

    switch (status) {
        case IndexStatus::kOk:
            return KVError::NONE;

        case IndexStatus::kNotFound:
            return KVError::KEY_NOT_FOUND;

        case IndexStatus::kOutOfMemory:
            return KVError::OUT_OF_MEMORY;

        case IndexStatus::kTableFull:
            return KVError::TABLE_FULL;

        case IndexStatus::kSizeOverflow:
            return KVError::SIZE_OVERFLOW;
    }

    return KVError::INVALID_ARGUMENT;
}

KVError map_arena_error(
    nova::internal::ArenaStatus status
) noexcept {
    using nova::internal::ArenaStatus;

    switch (status) {
        case ArenaStatus::kOk:
            return KVError::NONE;

        case ArenaStatus::kInvalidArgument:
            return KVError::INVALID_ARGUMENT;

        case ArenaStatus::kOutOfMemory:
            return KVError::OUT_OF_MEMORY;

        case ArenaStatus::kSizeOverflow:
            return KVError::SIZE_OVERFLOW;
    }

    return KVError::INVALID_ARGUMENT;
}

}  // namespace

KVStore::KVStore()
    : index_(
          nova::internal::kInitialTableCapacity
      ),
      arena_(
          nova::internal::kInitialArenaBytes
      ) {}

KVStore::~KVStore() = default;

bool KVStore::put(
    long key,
    const void* value,
    std::size_t value_len
) {
    /*
     * Public API policy:
     *
     * - nullptr rejected
     * - zero-length values rejected
     */
    if (
        value == nullptr ||
        value_len == 0
    ) {
        nova::detail::set_last_error(
            KVError::INVALID_ARGUMENT
        );

        return false;
    }

    [[maybe_unused]]
    auto guard =
        sync_.write_guard();

    /*
     * First make sure the index is capable of committing
     * this operation.
     *
     * prepare_upsert() may:
     *
     * - locate an existing key
     * - locate a free/tombstone slot
     * - resize the table
     * - fail allocation
     *
     * No logical mapping changes yet.
     */
    nova::internal::UpsertPosition position{};

    const auto index_status =
        index_.prepare_upsert(
            key,
            position
        );

    if (
        index_status !=
        nova::internal::IndexStatus::kOk
    ) {
        nova::detail::set_last_error(
            map_index_error(index_status)
        );

        return false;
    }

    /*
     * Now append the new bytes.
     *
     * For an overwrite, the old bytes remain in the
     * arena and become stale after commit.
     */
    nova::internal::ValueRef new_value{};

    const auto arena_status =
        arena_.append(
            value,
            value_len,
            new_value
        );

    if (
        arena_status !=
        nova::internal::ArenaStatus::kOk
    ) {
        nova::detail::set_last_error(
            map_arena_error(arena_status)
        );

        return false;
    }

    /*
     * Logical commit point.
     *
     * This does not allocate and does not fail.
     */
    index_.commit_upsert(
        key,
        new_value,
        position
    );

    nova::detail::set_last_error(
        KVError::NONE
    );

    return true;
}

bool KVStore::get(
    long key,
    void* value_out,
    std::size_t value_out_len
) const {
    if (value_out == nullptr) {
        nova::detail::set_last_error(
            KVError::INVALID_ARGUMENT
        );

        return false;
    }

    [[maybe_unused]]
    auto guard =
        sync_.read_guard();

    nova::internal::ValueRef ref{};

    const auto index_status =
        index_.find(
            key,
            ref
        );

    if (
        index_status !=
        nova::internal::IndexStatus::kOk
    ) {
        nova::detail::set_last_error(
            map_index_error(index_status)
        );

        return false;
    }

    /*
     * We chose all-or-nothing copy semantics.
     *
     * No partial prefix copy.
     */
    if (value_out_len < ref.length) {
        nova::detail::set_last_error(
            KVError::BUFFER_TOO_SMALL
        );

        return false;
    }

    const auto arena_status =
        arena_.copy_out(
            ref,
            value_out
        );

    /*
     * A ValueRef stored by HashIndex must have originated
     * from this arena.
     *
     * If this assertion fails, our own internal data
     * structures are inconsistent.
     */
    assert(
        arena_status ==
        nova::internal::ArenaStatus::kOk
    );

    if (
        arena_status !=
        nova::internal::ArenaStatus::kOk
    ) {
        nova::detail::set_last_error(
            map_arena_error(arena_status)
        );

        return false;
    }

    nova::detail::set_last_error(
        KVError::NONE
    );

    return true;
}

bool KVStore::erase(
    long key
) {
    [[maybe_unused]]
    auto guard =
        sync_.write_guard();
    const auto status =
        index_.erase(key);

    if (
        status !=
        nova::internal::IndexStatus::kOk
    ) {
        nova::detail::set_last_error(
            map_index_error(status)
        );

        return false;
    }

    /*
     * Only the hash-table mapping disappears.
     *
     * The old arena bytes deliberately remain stale
     * during Part A.
     */
    nova::detail::set_last_error(
        KVError::NONE
    );

    return true;
}