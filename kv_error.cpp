#include "kv_error.h"

namespace {

thread_local KVError g_last_error =
    KVError::NONE;

}  // namespace

KVError kv_last_error() noexcept {
    return g_last_error;
}

const char* kv_error_string(
    KVError error
) noexcept {
    switch (error) {
        case KVError::NONE:
            return "no error";

        case KVError::KEY_NOT_FOUND:
            return "key not found";

        case KVError::INVALID_ARGUMENT:
            return "invalid argument";

        case KVError::BUFFER_TOO_SMALL:
            return "output buffer too small";

        case KVError::OUT_OF_MEMORY:
            return "out of memory";

        case KVError::TABLE_FULL:
            return "hash table full";

        case KVError::SIZE_OVERFLOW:
            return "size arithmetic overflow";
    }

    return "unknown KV error";
}

namespace nova::detail {

void set_last_error(
    KVError error
) noexcept {
    g_last_error = error;
}

}  // namespace nova::detail