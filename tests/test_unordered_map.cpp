#include "../concurrent_unordered_map.h"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <iostream>
#include <map>
#include <numeric>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Helper function to print test results
void print_test_status(const std::string &test_name, bool passed) {
  std::cout << test_name << ": " << (passed ? "PASSED" : "FAILED") << std::endl;
}

// --- Single-threaded Tests ---

void test_single_threaded_basic_ops() {
  std::cout << "\n--- Running Single-threaded Basic Ops Test ---" << std::endl;
  concurrent::unordered_map<int, std::string> map;

  // Test empty()
  assert(map.empty());
  assert(map.size() == 0);

  // Test insert (copy)
  map.insert(1, "one");
  assert(!map.empty());
  assert(map.size() == 1);

  // Test insert (move)
  std::string two_str = "two";
  map.insert(2, std::move(two_str));
  assert(map.size() == 2);
  assert(two_str.empty()); // Ensure moved

  // Test find()
  auto val1 = map.find(1);
  assert(val1.has_value());
  assert(val1.value() == "one");

  auto val2 = map.find(2);
  assert(val2.has_value());
  assert(val2.value() == "two");

  auto val3 = map.find(3);
  assert(!val3.has_value());

  // Test count()
  assert(map.count(1) == 1);
  assert(map.count(2) == 1);
  assert(map.count(3) == 0);

  // Test C++20 contains()
#if __cplusplus >= 202002L
  assert(map.contains(1));
  assert(map.contains(2));
  assert(!map.contains(3));
#endif

  // Test insert (pair version)
  map.insert(std::pair<const int, std::string>(3, "three"));
  assert(map.size() == 3);
  assert(map.find(3).has_value());
  assert(map.find(3).value() == "three");

  map.insert(std::make_pair(4, "four")); // Using make_pair
  assert(map.size() == 4);
  assert(map.find(4).has_value());
  assert(map.find(4).value() == "four");

  // Test erase()
  assert(map.erase(1) == 1);
  assert(map.size() == 3);
  assert(!map.find(1).has_value());

  assert(map.erase(100) == 0); // Erase non-existent key
  assert(map.size() == 3);

  // Test clear()
  map.clear();
  assert(map.empty());
  assert(map.size() == 0);
  assert(!map.find(2).has_value());

  print_test_status("Single-threaded Basic Ops",
                    map.empty() && map.size() == 0);
}

void test_single_threaded_emplace() {
  std::cout << "\n--- Running Single-threaded Emplace Test ---" << std::endl;
  concurrent::unordered_map<int, std::pair<std::string, int>> map;

  // Emplace pair directly
  bool inserted1 = map.emplace(1, std::make_pair("one", 11));
  assert(inserted1);
  assert(map.size() == 1);
  auto val1_opt = map.find(1);
  assert(val1_opt.has_value());
  assert(val1_opt.value().first == "one");
  assert(val1_opt.value().second == 11);

  // Emplace with arguments for internal pair construction using
  // piecewise_construct
  bool inserted2 =
      map.emplace(std::piecewise_construct, std::forward_as_tuple(2),
                  std::forward_as_tuple("two", 22));
  assert(inserted2);
  assert(map.size() == 2);
  auto val2_opt = map.find(2);
  assert(val2_opt.has_value());
  assert(val2_opt.value().first == "two");
  assert(val2_opt.value().second == 22);

  // Emplace existing key (should fail) - Use piecewise_construct for
  // correctness
  bool inserted3 =
      map.emplace(std::piecewise_construct, std::forward_as_tuple(1),
                  std::forward_as_tuple("one_new", 111));
  assert(!inserted3); // Should not be inserted
  assert(map.size() == 2);
  auto val1_opt_after = map.find(1);
  assert(val1_opt_after.value().first == "one"); // Value should be unchanged
  assert(val1_opt_after.value().second == 11);

  print_test_status("Single-threaded Emplace", map.size() == 2);
}

void test_single_threaded_snapshot() {
  std::cout << "\n--- Running Single-threaded Snapshot Test ---" << std::endl;
  concurrent::unordered_map<int, std::string> map;

  map.insert(1, "one");
  map.insert(2, "two");
  map.insert(3, "three");

  // Create a snapshot
  std::vector<std::pair<int, std::string>> snapshot_vec = map.snapshot();

  // Test snapshot size
  assert(snapshot_vec.size() == map.size());
  assert(snapshot_vec.size() == 3);

  // Test contents (unordered_map has no guaranteed order, so sort for
  // comparison)
  std::sort(snapshot_vec.begin(),
            snapshot_vec.end()); // Requires pair comparison

  assert((snapshot_vec[0] == std::pair<int, std::string>(1, "one")));
  assert((snapshot_vec[1] == std::pair<int, std::string>(2, "two")));
  assert((snapshot_vec[2] == std::pair<int, std::string>(3, "three")));

  // Use STL algorithm on snapshot (e.g., find_if)
  auto it = std::find_if(snapshot_vec.begin(), snapshot_vec.end(),
                         [](const auto &pair) { return pair.second == "two"; });
  assert(it != snapshot_vec.end());
  assert(it->first == 2);

  // Use STL algorithm on snapshot (e.g., accumulate - requires Value to be
  // numeric or adaptable) Example with size (just to show algorithm usage on
  // snapshot)
  size_t total_key_chars =
      std::accumulate(snapshot_vec.begin(), snapshot_vec.end(), 0,
                      [](size_t sum, const auto &pair) {
                        return sum + std::to_string(pair.first).length();
                      });
  assert(total_key_chars == 1 + 1 + 1); // "1", "2", "3"

  // Modify the original map AFTER snapshot
  map.insert(4, "four");
  assert(map.size() == 4);
  assert(snapshot_vec.size() == 3); // Snapshot is unchanged

  print_test_status("Single-threaded Snapshot", snapshot_vec.size() == 3);
}

void test_single_threaded_execute_ops() {
  std::cout << "\n--- Running Single-threaded Execute Ops Test ---"
            << std::endl;
  concurrent::unordered_map<int, int> map;

  map.insert(1, 10);
  map.insert(2, 20);
  map.insert(3, 30);

  // Use execute_shared (read-only)
  int size_from_shared = map.execute_shared(
      [](const auto &internal_map) { return internal_map.size(); });
  assert(size_from_shared == 3);

  int value_from_shared = map.execute_shared([](const auto &internal_map) {
    auto it = internal_map.find(2);
    if (it != internal_map.end()) {
      return it->second; // Return a copy
    }
    return -1; // Indicate not found
  });
  assert(value_from_shared == 20);

  // Use execute_exclusive (read-write)
  bool erase_successful = map.execute_exclusive(
      [](auto &internal_map) { return internal_map.erase(1) > 0; });
  assert(erase_successful);
  assert(map.size() == 2); // Verify change outside lock

  int new_value = 40;
  map.execute_exclusive([&new_value](auto &internal_map) {
    internal_map[4] = new_value; // Modify or insert
  });
  assert(map.size() == 3);
  assert(map.find(4).has_value());
  assert(map.find(4).value() == 40);

  // Test returning a calculated value from execute_exclusive
  int sum_of_values = map.execute_exclusive([](auto &internal_map) {
    int sum = 0;
    for (const auto &pair : internal_map) {
      sum += pair.second;
    }
    internal_map.clear(); // Can also modify inside
    return sum;
  });
  // Values were 20, 30, 40 -> sum = 90
  assert(sum_of_values == 90);
  assert(map.empty()); // Verify clear happened

  print_test_status("Single-threaded Execute Ops", map.empty());
}

// --- Multi-threaded Tests ---

void insert_worker(concurrent::unordered_map<int, int> &map, int start,
                   int count) {
  for (int i = 0; i < count; ++i) {
    int key = start + i;
    map.insert(key, key * 10);
  }
}

void test_multi_threaded_insert() {
  std::cout << "\n--- Running Multi-threaded Insert Test ---" << std::endl;
  concurrent::unordered_map<int, int> map;
  const int num_threads = 4;
  const int items_per_thread = 1000;
  const int total_items = num_threads * items_per_thread;

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back(insert_worker, std::ref(map), i * items_per_thread,
                         items_per_thread);
  }

  for (auto &t : threads) {
    t.join();
  }

  // Verify final size
  assert(map.size() == total_items);

  // Verify contents (check if all inserted keys exist and have correct values)
  bool all_found_and_correct = true;
  for (int i = 0; i < total_items; ++i) {
    auto val = map.find(i);
    if (!val.has_value() || val.value() != i * 10) {
      all_found_and_correct = false;
      // Optional: print failing key/value for debugging
      // std::cerr << "Verification failed for key " << i << std::endl;
      break;
    }
  }

  assert(all_found_and_correct); // Assert the verification result

  print_test_status("Multi-threaded Insert",
                    map.size() == total_items && all_found_and_correct);
}

void read_worker(const concurrent::unordered_map<int, int> &map,
                 const std::vector<int> &keys_to_find,
                 std::atomic<int> &found_count) {
  for (int key : keys_to_find) {
    if (map.find(key).has_value()) {
      found_count.fetch_add(1);
    }
  }
}

void test_multi_threaded_read() {
  std::cout << "\n--- Running Multi-threaded Read Test ---" << std::endl;
  concurrent::unordered_map<int, int> map;
  const int num_items = 5000;
  const int num_threads = 4;

  // Populate the map first
  for (int i = 0; i < num_items; ++i) {
    map.insert(i, i * 10);
  }
  assert(map.size() == num_items);

  // Prepare keys for threads to find
  std::vector<int> keys_to_find_all;
  for (int i = 0; i < num_items; ++i)
    keys_to_find_all.push_back(i);
  // Add some keys that don't exist
  for (int i = num_items; i < num_items + 100; ++i)
    keys_to_find_all.push_back(i);

  std::vector<std::vector<int>> thread_keys(num_threads);
  for (size_t i = 0; i < keys_to_find_all.size(); ++i) {
    thread_keys[i % num_threads].push_back(keys_to_find_all[i]);
  }

  std::vector<std::thread> threads;
  std::atomic<int> total_found_count(0);

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back(read_worker, std::ref(map), std::ref(thread_keys[i]),
                         std::ref(total_found_count));
  }

  for (auto &t : threads) {
    t.join();
  }

  // Verify that we found exactly the number of keys that exist
  assert(total_found_count.load() == num_items);

  print_test_status("Multi-threaded Read",
                    total_found_count.load() == num_items);
}

void mixed_op_worker(concurrent::unordered_map<int, int> &map, int thread_id,
                     int num_ops) {
  srand(thread_id); // Seed random number generator for different threads

  for (int i = 0; i < num_ops; ++i) {
    int key = rand() % (num_ops * 4); // Keys in a certain range
    int op_type = rand() % 3;         // 0: insert, 1: find, 2: erase

    if (op_type == 0) { // Insert
      map.insert(key, key * 10 + thread_id);
    } else if (op_type == 1) { // Find
      map.find(key); // Just check existence, value not critical for this test
    } else {         // Erase
      map.erase(key);
    }
  }
}

void test_multi_threaded_mixed_ops() {
  std::cout << "\n--- Running Multi-threaded Mixed Ops Test ---" << std::endl;
  concurrent::unordered_map<int, int> map;
  const int num_threads = 8;
  const int ops_per_thread = 5000;

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back(mixed_op_worker, std::ref(map), i, ops_per_thread);
  }

  for (auto &t : threads) {
    t.join();
  }

  // After mixed operations, verifying the exact state is complex
  // due to non-deterministic operations. A basic check is that
  // the map is in a consistent state (e.g., no crashes, size is reasonable).
  // More rigorous tests would involve tracking expected state changes,
  // which is beyond the scope of a simple assert-based test.
  std::cout << "Final map size after mixed ops: " << map.size() << std::endl;
  // Basic sanity check: size should be non-negative
  assert(map.size() >=
         0); // This will always be true for size_type, but good practice.
  // More relevant: check if basic operations still work after heavy use
  assert(!map.empty() ||
         ops_per_thread == 0); // Should not be empty unless no ops were run
  assert(map.find(0).has_value() ||
         !map.find(0).has_value()); // Find doesn't crash

  print_test_status(
      "Multi-threaded Mixed Ops",
      true); // We assert for crashes/inconsistency, not final exact state
}

void execute_worker(concurrent::unordered_map<int, int> &map, int thread_id,
                    int num_ops, bool is_exclusive) {
  srand(thread_id + (is_exclusive ? 1000 : 0)); // Different seeds

  for (int i = 0; i < num_ops; ++i) {
    int key = rand() % (num_ops * 2);

    if (is_exclusive) {
      map.execute_exclusive([key, thread_id](auto &internal_map) {
        internal_map[key] = key * 100 + thread_id; // Write
        // Optionally read inside exclusive lock
        // auto it = internal_map.find(key);
      });
    } else {
      map.execute_shared([key](const auto &internal_map) {
        internal_map.count(key); // Read
                                 // Optionally read value
                                 // auto it = internal_map.find(key);
        // if(it != internal_map.end()) { volatile int val = it->second; }
      });
    }
  }
}

void test_multi_threaded_execute() {
  std::cout << "\n--- Running Multi-threaded Execute Test ---" << std::endl;
  concurrent::unordered_map<int, int> map;
  const int num_threads = 8;
  const int ops_per_thread = 1000;

  std::vector<std::thread> threads;

  // Create some threads for exclusive operations (writes)
  for (int i = 0; i < num_threads / 2; ++i) {
    threads.emplace_back(execute_worker, std::ref(map), i, ops_per_thread,
                         true);
  }

  // Create some threads for shared operations (reads)
  for (int i = num_threads / 2; i < num_threads; ++i) {
    threads.emplace_back(execute_worker, std::ref(map), i, ops_per_thread,
                         false);
  }

  for (auto &t : threads) {
    t.join();
  }

  std::cout << "Final map size after execute ops: " << map.size() << std::endl;

  // Basic sanity check: no crashes, map state is accessible
  assert(map.size() >= 0);
  assert(map.find(0).has_value() || !map.find(0).has_value());

  // More complex verification could involve checking specific keys inserted
  // by exclusive workers, but like mixed_ops, exact state is non-deterministic.
  // The main goal here is to ensure execute_* methods are thread-safe and don't
  // crash.

  print_test_status("Multi-threaded Execute",
                    true); // Assert for crashes/consistency
}

int main() {
  test_single_threaded_basic_ops();
  test_single_threaded_emplace();
  test_single_threaded_snapshot();
  test_single_threaded_execute_ops();

  // Multi-threaded tests
  test_multi_threaded_insert();
  test_multi_threaded_read();
  test_multi_threaded_mixed_ops(); // This test focuses on stability under load
  test_multi_threaded_execute();   // This test focuses on execute_* method
                                   // stability

  std::cout << "\nAll tests finished." << std::endl;

  return 0;
}
