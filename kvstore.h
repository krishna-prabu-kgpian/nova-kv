#pragma once

#include <cstddef>

#include "internal/hash_index.h"
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

private:
    nova::internal::HashIndex index_;
    nova::internal::ValueArena arena_;
};