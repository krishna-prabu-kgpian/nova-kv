#pragma once

#include <mutex>
#include <shared_mutex>

namespace nova::internal {

/*
 * No synchronization.
 *
 * This preserves the behavior of our current
 * single-threaded KVStore.
 */
class NoSync {
public:
    class ReadGuard {
    public:
        ReadGuard() noexcept = default;
    };

    class WriteGuard {
    public:
        WriteGuard() noexcept = default;
    };

    [[nodiscard]]
    ReadGuard read_guard() const noexcept {
        return {};
    }

    [[nodiscard]]
    WriteGuard write_guard() noexcept {
        return {};
    }
};


/*
 * Global mutex.
 *
 * Reads and writes both acquire the exact same
 * exclusive mutex.
 */
class MutexSync {
public:
    using ReadGuard =
        std::unique_lock<std::mutex>;

    using WriteGuard =
        std::unique_lock<std::mutex>;

    [[nodiscard]]
    ReadGuard read_guard() const {
        return ReadGuard(mutex_);
    }

    [[nodiscard]]
    WriteGuard write_guard() {
        return WriteGuard(mutex_);
    }

private:
    mutable std::mutex mutex_;
};


/*
 * Reader-writer lock.
 *
 * Multiple readers may coexist.
 * Writers acquire exclusive ownership.
 */
class RWLockSync {
public:
    using ReadGuard =
        std::shared_lock<std::shared_mutex>;

    using WriteGuard =
        std::unique_lock<std::shared_mutex>;

    [[nodiscard]]
    ReadGuard read_guard() const {
        return ReadGuard(mutex_);
    }

    [[nodiscard]]
    WriteGuard write_guard() {
        return WriteGuard(mutex_);
    }

private:
    mutable std::shared_mutex mutex_;
};


/*
 * Each executable selects its synchronization
 * strategy at compile time.
 *
 * No macro:
 *     NoSync
 *
 * -DNOVA_SYNC_MUTEX:
 *     MutexSync
 *
 * -DNOVA_SYNC_RWLOCK:
 *     RWLockSync
 */

#if defined(NOVA_SYNC_MUTEX) && defined(NOVA_SYNC_RWLOCK)

#error "Only one NOVA synchronization policy may be selected"

#endif

#if defined(NOVA_SYNC_MUTEX)

using ActiveSyncPolicy = MutexSync;

#elif defined(NOVA_SYNC_RWLOCK)

using ActiveSyncPolicy = RWLockSync;

#else

using ActiveSyncPolicy = NoSync;

#endif

}  // namespace nova::internal