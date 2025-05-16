#ifndef CONCURRENT_UNORDERED_MAP_H
#define CONCURRENT_UNORDERED_MAP_H

#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace concurrent {

template <typename Key, typename Value, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>,
          typename Allocator = std::allocator<std::pair<const Key, Value>>>
class unordered_map {
public:
  using key_type = Key;
  using value_type = Value;
  using mapped_type = Value;
  using size_type = typename std::unordered_map<Key, Value, Hash, KeyEqual,
                                                Allocator>::size_type;
  using hasher = Hash;
  using key_equal = KeyEqual;
  using allocator_type = Allocator;
  using internal_type =
      std::unordered_map<Key, Value, Hash, KeyEqual, Allocator>;
  using pair_type = std::pair<const Key, Value>;

  // Constructors - forwarding to underlying map constructor
  // This allows using custom hashers, comparers, allocators etc.
  template <typename... Args>
  explicit unordered_map(Args &&...args)
      : _internal_map(std::forward<Args>(args)...) {}

  /// Insert an element using a type convertible to value_type (pair<const Key,
  /// Value>). Uses a unique_lock for exclusive access. Uses perfect forwarding
  /// and type traits for strict parameter type checking.
  template <typename P, typename std::enable_if_t<
                            std::is_constructible_v<pair_type, P>, int> = 0>
  bool insert(P &&obj) {
    std::unique_lock<std::shared_mutex> lock(_shrd_mutx);
    // _internal_map.insert returns std::pair<iterator, bool>
    // We only return the bool part for thread safety reasons (iterator
    // validity)
    return _internal_map.insert(std::forward<P>(obj)).second;
  }

  /// Insert an element (copy version). Uses a unique_lock for exclusive access.
  void insert(const Key &key, const Value &value) {
    std::unique_lock<std::shared_mutex> lock(_shrd_mutx);
    _internal_map[key] = value; // Or _internal_map.insert({key, value});
  }

  /// Insert an element (move version). Uses a unique_lock for exclusive access.
  void insert(Key &&key, Value &&value) {
    std::unique_lock<std::shared_mutex> lock(_shrd_mutx);
    _internal_map[std::move(key)] = std::move(
        value); // Or _internal_map.insert({std::move(key), std::move(value)});
  }

  /// Find an element. Uses a shared_lock for concurrent read access.
  /// Returns std::optional<Value> to indicate if the key was found.
  /// Returning std::optional<Value> is type-safe as it returns a copy/moved
  /// value, preventing dangling references after the shared_lock is released.
  std::optional<Value> find(const Key &key) const {
    std::shared_lock<std::shared_mutex> lock(_shrd_mutx);
    auto it = _internal_map.find(key);
    if (it != _internal_map.end()) {
      // Returns a copy or moves if Value is movable.
      // This is crucial for thread safety after the lock is released.
      return it->second;
    }
    return std::nullopt;
  }

  /// Emplace an element. Constructs the element in-place within the map.
  /// Uses variadic templates and perfect forwarding for efficiency (zero
  /// overhead in parameter passing). The element is constructed directly in the
  /// map's memory under lock. Returns bool indicating if insertion took place
  /// (false if key already existed). Does NOT return an iterator for thread
  /// safety reasons (iterator validity).
  template <typename... Args> bool emplace(Args &&...args) {
    std::unique_lock<std::shared_mutex> lock(_shrd_mutx);
    // _internal_map.emplace returns std::pair<iterator, bool>
    // We only return the bool part for thread safety reasons (iterator
    // validity)
    return _internal_map.emplace(std::forward<Args>(args)...).second;
  }

  /// Erase element by key. Uses unique_lock. Returns number of elements removed
  /// (0 or 1).
  size_type erase(const Key &key) {
    std::unique_lock<std::shared_mutex> lock(_shrd_mutx);
    return _internal_map.erase(key);
  }

  /// Clear all elements. Uses unique_lock.
  void clear() {
    std::unique_lock<std::shared_mutex> lock(_shrd_mutx);
    _internal_map.clear();
  }

  /// Get number of elements. Uses shared_lock.
  size_type size() const {
    std::shared_lock<std::shared_mutex> lock(_shrd_mutx);
    return _internal_map.size();
  }

  /// Check if map is empty. Uses shared_lock.
  bool empty() const {
    std::shared_lock<std::shared_mutex> lock(_shrd_mutx);
    return _internal_map.empty();
  }

  /// Count elements with a specific key. Uses shared_lock.
  /// For unordered_map, this is 0 or 1.
  size_type count(const Key &key) const {
    std::shared_lock<std::shared_mutex> lock(_shrd_mutx);
    return _internal_map.count(key);
  }

// C++20: Check if key exists. Uses shared_lock.
#if __cplusplus >= 202002L // Check for C++20 or later
  bool contains(const Key &key) const {
    std::shared_lock<std::shared_mutex> lock(_shrd_mutx);
    return _internal_map.contains(key);
  }
#endif

  /// Creates a snapshot of the map's contents in a std::vector.
  /// This allows safe iteration and use with STL algorithms on the snapshot.
  /// Uses a shared_lock for concurrent read access during snapshot creation.
  /// Note: This creates a copy, which might have performance implications for
  /// large maps. The keys in the snapshot are not const to allow sorting.
  std::vector<std::pair<Key, Value>> snapshot() const {
    std::shared_lock<std::shared_mutex> lock(_shrd_mutx);
    // Copy the contents under lock. Copy pair<const Key, Value> to pair<Key,
    // Value>
    std::vector<std::pair<Key, Value>> data;
    data.reserve(
        _internal_map
            .size()); // Reserve space to potentially avoid reallocations
    for (const auto &pair : _internal_map)
      data.push_back({pair.first, pair.second}); // Copy data, non-const key
    return data;                                 // Return the copy
  }

  /// Executes a callable with a const reference to the underlying map, holding
  /// a shared_lock.
  /// Useful for performing multiple read operations atomically or using
  /// read-only STL algorithms within a thread-safe context. The callable should
  /// accept a const reference to internal_type. IMPORTANT: The
  /// callable should NOT return references or pointers to elements within the
  /// map if those references/pointers are intended to be used AFTER this
  /// `execute_shared` method returns, as the lock will be released.
  template <typename Func>
  auto execute_shared(Func &&func) const
      -> decltype(func(std::declval<const internal_type &>())) {
    std::shared_lock<std::shared_mutex> lock(_shrd_mutx);
    // Execute the function while holding the shared lock
    return func(_internal_map);
  }

  /// Executes a callable with a mutable reference to the underlying map,
  /// holding a unique_lock.
  /// Useful for performing multiple read/write operations atomically or using
  /// modifying STL algorithms within a thread-safe context. The callable should
  /// accept a mutable reference to internal_type. IMPORTANT: The
  /// callable should NOT return references or pointers to elements within the
  /// map if those references/pointers are intended to be used AFTER this
  /// `execute_exclusive` method returns, as the lock will be released.
  template <typename Func>
  auto execute_exclusive(Func &&func)
      -> decltype(func(std::declval<internal_type &>())) {
    std::unique_lock<std::shared_mutex> lock(_shrd_mutx);
    // Execute the function while holding the unique lock
    return func(_internal_map);
  }

  // Avoid providing operator[] that returns references, as they become dangling
  // immediately after the lock is released. The find method provides a safe
  // way to retrieve values by copy or move.

private:
  std::unordered_map<Key, Value, Hash, KeyEqual, Allocator> _internal_map;
  mutable std::shared_mutex _shrd_mutx;
};

} // namespace concurrent

#endif
