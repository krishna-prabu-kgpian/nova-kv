#pragma once

#include <cstddef>

#include "internal/hash_index.h"
#include "internal/sync_policy.h"
#include "internal/value_arena.h"

class KVStore {
public:
    KVStore();
    ~KVStore();

    KVStore(const KVStore&) = delete;
    KVStore& operator=(const KVStore&) = delete;

    bool put(
        long key,
        const void* value,
        std::size_t value_len
    );

    bool get(
        long key,
        void* value_out,
        std::size_t value_out_len
    ) const;

    bool erase(long key);

    #ifdef NOVA_WORKLOAD_COMPAT

    bool create_index(
        std::size_t field_offset,
        std::size_t field_len
    );

    int index_get(
        const void* field_value,
        std::size_t field_len,
        long* keys_out,
        int max_keys
    ) const;

#endif

private:
    nova::internal::ActiveSyncPolicy sync_;

    nova::internal::HashIndex index_;
    nova::internal::ValueArena arena_;
};

#ifdef NOVA_WORKLOAD_COMPAT

class Transaction {
public:
    explicit Transaction(KVStore& store);
    ~Transaction();

    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    void put(
        long key,
        const void* value,
        std::size_t value_len
    );

    bool commit();

private:
    KVStore& store_;
};

#endif