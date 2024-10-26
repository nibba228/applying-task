#include "thread_pool.hpp"

#include <cassert>

static thread_local ThreadPool* cur_pool;

ThreadPool::ThreadPool(size_t threads)
    : threads_(threads),
      caller_thread_id_(std::this_thread::get_id()) {
  cur_pool = this;
}

void ThreadPool::Start() {
  for (size_t i = 0; i < threads_; ++i) {
    workers_.emplace_back([this] {
      cur_pool = this;
      std::optional<Task> work;
      while ((work = queue_.Pop())) {
        work.value()();
        work.reset();
      }
    });
  }
}

ThreadPool::~ThreadPool() {
  assert(stopped_ && "this->Stop() was not called!");
}

void ThreadPool::Submit(Task task) {
  queue_.Push(std::move(task));
}

ThreadPool* ThreadPool::Current() {
  if (std::this_thread::get_id() == cur_pool->caller_thread_id_) {
    return nullptr;
  }
  return cur_pool;
}

void ThreadPool::Stop() {
  queue_.Close();
  for (size_t i = 0; i < threads_; ++i) {
    workers_[i].join();
  }
  stopped_ = true;
}
