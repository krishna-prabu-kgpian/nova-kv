CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS := -I. -Iinternal

.PHONY: all test clean

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
		kvstore_test