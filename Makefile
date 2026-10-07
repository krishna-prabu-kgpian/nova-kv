CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS := -I. -Iinternal
THREAD_FLAGS := -pthread

.PHONY: all test clean
.PHONY: all test sync_test clean

all: value_arena_test hash_index_test kvstore_test


value_arena_test: \
	internal/value_arena.cpp \
	tests/value_arena_test.cpp

	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@


hash_index_test: \
	internal/hash_index.cpp \
	tests/hash_index_test.cpp

	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@


kvstore_test: \
	kvstore.cpp \
	kv_error.cpp \
	internal/value_arena.cpp \
	internal/hash_index.cpp \
	kvstore_test.cpp

	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@


test: all
	./value_arena_test
	./hash_index_test
	./kvstore_test


clean:
	rm -f \
		value_arena_test \
		hash_index_test \
		kvstore_test \
		concurrency_mutex_test \
		concurrency_rwlock_test

BENCHFLAGS := -std=c++17 -O3 -DNDEBUG -march=native \
              -Wall -Wextra -Wpedantic

.PHONY: bench

baseline_bench: \
	kvstore.cpp \
	kv_error.cpp \
	internal/value_arena.cpp \
	internal/hash_index.cpp \
	bench/baseline_bench.cpp

	$(CXX) $(CPPFLAGS) $(BENCHFLAGS) $^ -o $@

bench: baseline_bench
	./baseline_bench

concurrency_mutex_test: \
	kvstore.cpp \
	kv_error.cpp \
	internal/value_arena.cpp \
	internal/hash_index.cpp \
	tests/concurrency_test.cpp

	$(CXX) \
		$(CPPFLAGS) \
		$(CXXFLAGS) \
		$(THREAD_FLAGS) \
		-DNOVA_SYNC_MUTEX \
		$^ \
		-o $@


concurrency_rwlock_test: \
	kvstore.cpp \
	kv_error.cpp \
	internal/value_arena.cpp \
	internal/hash_index.cpp \
	tests/concurrency_test.cpp

	$(CXX) \
		$(CPPFLAGS) \
		$(CXXFLAGS) \
		$(THREAD_FLAGS) \
		-DNOVA_SYNC_RWLOCK \
		$^ \
		-o $@


sync_test: \
	concurrency_mutex_test \
	concurrency_rwlock_test

	./concurrency_mutex_test
	./concurrency_rwlock_test