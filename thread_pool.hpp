#pragma once

#include "queue.hpp"
#include "task.hpp"

#include <thread>

class ThreadPool {
 public:
  explicit ThreadPool(size_t threads);
  ~ThreadPool();

  // Non-copyable
  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  // Non-movable
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  void Start();

  void Submit(Task task);

  static ThreadPool* Current();

  void Stop();

 private:
  bool stopped_{false};
  const size_t threads_;
  std::thread::id caller_thread_id_;
  std::vector<std::thread> workers_;
  UnboundedBlockingQueue<Task> queue_;
};
