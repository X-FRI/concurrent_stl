#ifndef CONCURRENT_CONTAINER_BASE_H
#define CONCURRENT_CONTAINER_BASE_H

#include <functional>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace concurrent::internal {

// Generic base template class
template <typename ContainerT, typename MutexT = std::shared_mutex>
class container_base {
protected:
  ContainerT _internal_container;
  mutable MutexT _mutex;

public:
  // Constructor: forward parameters to the underlying container's constructor
  template <typename... Args>
  explicit container_base(Args &&...args)
      : _internal_container(std::forward<Args>(args)...) {}

  // Disallow copying and assignment to avoid complex thread safety issues
  container_base(const container_base &) = delete;
  container_base &operator=(const container_base &) = delete;

  // Allow moving
  container_base(container_base &&other) noexcept
      : _internal_container(std::move(other._internal_container)),
        _mutex() // Default construct the mutex, don't move it
  {}

  container_base &operator=(container_base &&other) noexcept {
    if (this != &other) {
      // Lock both mutexes to prevent deadlock during move assignment
      // In a real-world scenario, consider a more robust locking strategy for
      // move assignment if required For simplicity here, we assume move
      // assignment is done when the container is not actively used
      std::unique_lock<MutexT> self_lock(_mutex);
      std::unique_lock<MutexT> other_lock(other._mutex);
      _internal_container = std::move(other._internal_container);
    }
    return *this;
  }

  /// Execute a read operation with a shared lock
  /// Callable func accepts const ContainerT& parameter
  template <typename Func>
  auto execute_shared(Func &&func) const
      -> decltype(func(std::declval<const ContainerT &>())) {
    std::shared_lock<MutexT> lock(_mutex);
    return func(_internal_container);
  }

  /// Execute a read-write operation with an exclusive lock
  /// Callable func accepts ContainerT& parameter
  template <typename Func>
  auto execute_exclusive(Func &&func)
      -> decltype(func(std::declval<ContainerT &>())) {
    std::unique_lock<MutexT> lock(_mutex);
    return func(_internal_container);
  }

  // You can add some common, non-container-specific interfaces here,
  // for example size() and empty(), which can be implemented directly using
  // execute_shared.
  size_t size() const {
    return execute_shared([](const ContainerT &c) { return c.size(); });
  }

  bool empty() const {
    return execute_shared([](const ContainerT &c) { return c.empty(); });
  }
};

} // namespace concurrent::internal

#endif // CONCURRENT_CONTAINER_BASE_H