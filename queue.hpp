#pragma once

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>

// Thread-safe queue implementation
template <typename T> class ThreadSafeQueue {
public:
  ThreadSafeQueue() {}

  void push(const T &value) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(value);
    cv_.notify_one();
  }

  void push_bulk(const std::vector<T> &data) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &v : data) {
      std::cout << v;
      queue_.push(v);
    }
    cv_.notify_one();
  }

  bool pop(T &value) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !queue_.empty(); });
    if (queue_.empty())
      return false;
    value = queue_.front();
    queue_.pop();
    return true;
  }

  bool try_pop(T &value) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty())
      return false;
    value = queue_.front();
    queue_.pop();
    return true;
  }

  bool empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
  }

private:
  std::queue<T> queue_;
  mutable std::mutex mutex_;
  std::condition_variable cv_;
};
