#include "workload_gen.h"

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <string>

#ifndef NOVA_SYNC_RWLOCK
#error "rwlock.cpp must be built with -DNOVA_SYNC_RWLOCK"
#endif

namespace {

bool parse_reader_count(
    const char* text,
    int& out
) {
    errno = 0;

    char* end = nullptr;

    const long value =
        std::strtol(text, &end, 10);

    if (
        errno != 0 ||
        end == text ||
        *end != '\0' ||
        value <= 0 ||
        value > 1024
    ) {
        return false;
    }

    out = static_cast<int>(value);
    return true;
}

}  // namespace

int main(
    int argc,
    char** argv
) {
    int readers = 8;

    if (argc > 2) {
        std::cerr
            << "usage: "
            << argv[0]
            << " [num_readers]\n";

        return 1;
    }

    if (
        argc == 2 &&
        !parse_reader_count(
            argv[1],
            readers
        )
    ) {
        std::cerr
            << "invalid reader count: "
            << argv[1]
            << '\n';

        return 1;
    }

    KVStore trades;

    WorkloadConfig cfg;

    cfg.num_readers = readers;
    cfg.use_txn = false;
    cfg.txn_size = 1;
    cfg.use_join = false;

    WorkloadResults results;

    workload_run(
        trades,
        nullptr,
        cfg,
        results
    );

    print_results(
        "rwlock_" +
            std::to_string(readers) +
            "_readers",
        results
    );

    return 0;
}