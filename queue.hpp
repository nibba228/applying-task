#pragma once

#include <mutex>
#include <condition_variable>

#include <optional>
#include <deque>
#include <mutex>

// Unbounded blocking multi-producers/multi-consumers (MPMC) queue

template <typename T>
class UnboundedBlockingQueue {
 public:
  void Push(T elem) {
    std::lock_guard lock{mutex_};
    dq_.emplace_back(std::move(elem));
    is_empty_.notify_one();
  }

  std::optional<T> Pop() {
    std::unique_lock lock{mutex_};
    while (dq_.empty()) {
      if (closed_) {
        return std::nullopt;
      }
      is_empty_.wait(lock);
    }
    auto ret_val = std::make_optional<T>(std::move(dq_.front()));
    dq_.pop_front();
    return ret_val;
  }

  void Close() {
    std::lock_guard lock{mutex_};
    closed_ = true;
    is_empty_.notify_all();
  }

 private:
  bool closed_{false};
  std::mutex mutex_;
  std::condition_variable is_empty_;
  std::deque<T> dq_;
};
