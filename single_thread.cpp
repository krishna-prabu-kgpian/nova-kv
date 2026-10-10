#include "workload_gen.h"

#include <iostream>

#if defined(NOVA_SYNC_MUTEX) || defined(NOVA_SYNC_RWLOCK)
#error "single_thread must use NoSync"
#endif

int main() {
    KVStore trades;

    WorkloadConfig cfg;

    cfg.num_readers = 1;

    cfg.use_txn = false;
    cfg.use_join = false;

    WorkloadResults results;

    workload_run(
        trades,
        nullptr,
        cfg,
        results
    );

    print_results(
        "single_thread",
        results
    );

    return 0;
}