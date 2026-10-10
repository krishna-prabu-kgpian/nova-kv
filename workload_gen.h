#pragma once

#include "kvstore.h"

#include <string>

struct WorkloadConfig {
    int m = 1000;
    int n = 100;

    int hot_every = 100;

    int num_readers = 8;
    int duration_sec = 5;

    bool use_txn = false;
    int txn_size = 1;

    bool use_join = false;
};

struct WorkloadResults {
    long total_ops = 0;

    double rps = 0;

    double p50_us = 0;
    double p99_us = 0;
    double p999_us = 0;
};

void workload_run(
    KVStore& trades,
    KVStore* instruments,
    const WorkloadConfig& cfg,
    WorkloadResults& out
);

void print_results(
    const std::string& label,
    const WorkloadResults& r
);