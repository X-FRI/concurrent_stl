# Concurrent STL

This project provides thread-safe wrapper classes for standard C++ containers. 
The goal is to offer easy-to-use concurrent versions of STL containers while maintaining a familiar interface where possible.

> It's important to note that this method might not be the most performant solution for all cases, and its suitability depends heavily on the application scenario.

## `concurrent::unordered_map`

The `concurrent::unordered_map` class wraps `std::unordered_map` and provides thread-safe access to its elements.

### Thread Safety Implementation

Thread safety is achieved by using a `std::shared_mutex` to protect the underlying `std::unordered_map`.

*   **Write Operations** (e.g., `insert`, `emplace`, `erase`, `clear`) acquire a `std::unique_lock` for exclusive access. This ensures that only one thread can modify the map at a time.
*   **Read Operations** (e.g., `find`, `size`, `empty`, `count`, `contains`) acquire a `std::shared_lock` for shared access. This allows multiple threads to read from the map concurrently.

### Special Methods for Thread-Safe Access

While individual read/write operations are protected, performing multiple operations atomically or iterating safely requires specific mechanisms. This library provides the following methods for such scenarios:

#### `snapshot()`

This method creates a thread-safe copy of the entire map's contents into a `std::vector<std::pair<Key, Value>>` while holding a `std::shared_lock`.

```cpp
concurrent::unordered_map<int, std::string> my_map;
// ... insert elements ...

// Get a thread-safe copy for iteration or processing
std::vector<std::pair<int, std::string>> current_data = my_map.snapshot();

// Now you can safely iterate over current_data outside the lock
for (const auto& pair : current_data) {
    std::cout << pair.first << ": " << pair.second << std::endl;
}
// The lock is released as soon as snapshot() returns.
```

Use `snapshot()` when you need to iterate over the map's contents or perform operations on a fixed state without blocking other threads for the entire duration of your processing. Be mindful that this creates a copy, which may have performance implications for very large maps.

#### `execute_shared()`

This method allows you to execute a provided callable (like a lambda function) while holding a `std::shared_lock` on the internal map. The callable receives a `const` reference to the underlying `std::unordered_map`.

```cpp
concurrent::unordered_map<int, std::string> my_map;
// ... insert elements ...

// Execute a read-only operation under a shared lock
my_map.execute_shared([](const auto& internal_map) {
    // This lambda is executed while the shared_lock is held.
    // You can perform multiple read operations atomically here.
    // e.g., check existence of multiple keys, calculate sum of values, etc.
    if (internal_map.count(1) > 0) {
        std::cout << "Key 1 found inside execute_shared." << std::endl;
    }
    // Do NOT return references/pointers to internal_map elements if
    // they are intended to be used after this lambda finishes,
    // as the lock is released upon return.
});
```

Use `execute_shared()` when you need to perform multiple atomic read operations or use read-only algorithms on the underlying map directly without creating a full copy like `snapshot()`.

#### `execute_exclusive()`

This method allows you to execute a provided callable while holding a `std::unique_lock` on the internal map. The callable receives a mutable reference to the underlying `std::unordered_map`.

```cpp
concurrent::unordered_map<int, std::string> my_map;
// ... insert elements ...

// Execute a read/write operation under a unique lock
my_map.execute_exclusive([](auto& internal_map) {
    // This lambda is executed while the unique_lock is held.
    // You can perform multiple read/write operations atomically here.
    // e.g., insert if not exists, update value based on current value, etc.
    auto it = internal_map.find(5);
    if (it != internal_map.end()) {
        it->second = "updated value";
        std::cout << "Updated value for key 5 inside execute_exclusive." << std::endl;
    } else {
        internal_map[5] = "new value";
        std::cout << "Inserted key 5 inside execute_exclusive." << std::endl;
    }
    // Do NOT return references/pointers to internal_map elements if
    // they are intended to be used after this lambda finishes,
    // as the lock is released upon return.
});
```

Use `execute_exclusive()` when you need to perform multiple atomic read/write operations or use modifying algorithms on the underlying map directly. This grants exclusive access, blocking all other readers and writers.

## Building and Testing

The project uses [xmake](https://xmake.io) as the build system.

1.  Ensure xmake is installed.
2.  Clone the repository.
3.  Navigate to the project root.
4.  Build the project (it's header-only, so this mostly configures):
    ```bash
    xmake
    ```
5.  Run the tests located in the `tests/` directory:
    ```bash
    xmake run tests/test_unordered_map
    ```


## [LICENSE](./LICENSE)

```
Copyright (c) 2025 Somhairle H. Marisol

All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name of Fringer nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```
