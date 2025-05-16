#ifndef CONCURRENT_UNORDERED_MAP_H
#define CONCURRENT_UNORDERED_MAP_H

#include "internal/container_base.h"
#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace concurrent {

// Implement a thread-safe unordered_map based on the base class
template <typename Key, typename Value, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>,
          typename Allocator = std::allocator<std::pair<const Key, Value>>,
          typename MutexT = std::shared_mutex>
class unordered_map
    : public internal::container_base<
          std::unordered_map<Key, Value, Hash, KeyEqual, Allocator>, MutexT> {

  using Base = internal::container_base<
      std::unordered_map<Key, Value, Hash, KeyEqual, Allocator>, MutexT>;
  using internal_type =
      std::unordered_map<Key, Value, Hash, KeyEqual, Allocator>;
  using pair_type = std::pair<const Key, Value>;

public:
  // Forward constructor to base class
  template <typename... Args>
  explicit unordered_map(Args &&...args) : Base(std::forward<Args>(args)...) {}

  // Implement thread-safe interfaces specific to unordered_map,
  // calling the execute method of the base class

  template <typename P, typename std::enable_if_t<
                            std::is_constructible_v<pair_type, P>, int> = 0>
  bool insert(P &&obj) {
    return this->execute_exclusive([&](internal_type &m) {
      return m.insert(std::forward<P>(obj)).second;
    });
  }

  void insert(const Key &key, const Value &value) {
    this->execute_exclusive([&](internal_type &m) { m[key] = value; });
  }

  void insert(Key &&key, Value &&value) {
    this->execute_exclusive(
        [&](internal_type &m) { m[std::move(key)] = std::move(value); });
  }

  std::optional<Value> find(const Key &key) const {
    return this->execute_shared(
        [&](const internal_type &m) -> std::optional<Value> {
          auto it = m.find(key);
          if (it != m.end()) {
            return it->second; // Returns a copy or moves if Value is movable
          }
          return std::nullopt;
        });
  }

  template <typename... Args> bool emplace(Args &&...args) {
    return this->execute_exclusive([&](internal_type &m) {
      return m.emplace(std::forward<Args>(args)...).second;
    });
  }

  size_t erase(const Key &key) {
    return this->execute_exclusive(
        [&](internal_type &m) { return m.erase(key); });
  }

  void clear() {
    this->execute_exclusive([](internal_type &m) { m.clear(); });
  }

  // size() and empty() inherited from base

  size_t count(const Key &key) const {
    return this->execute_shared(
        [&](const internal_type &m) { return m.count(key); });
  }

#if __cplusplus >= 202002L // Check for C++20 or later
  bool contains(const Key &key) const {
    return this->execute_shared(
        [&](const internal_type &m) { return m.contains(key); });
  }
#endif

  std::vector<std::pair<Key, Value>> snapshot() const {
    return this->execute_shared([&](const internal_type &m) {
      std::vector<std::pair<Key, Value>> data;
      data.reserve(m.size());
      for (const auto &pair : m)
        data.push_back({pair.first, pair.second});
      return data;
    });
  }

  // execute_shared and execute_exclusive inherited from base
};

} // namespace concurrent

#endif
