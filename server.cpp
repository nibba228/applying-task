#include "socket.hpp"
#include "thread_pool.hpp"


#include <algorithm>
#include <atomic>
#include <cmath>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdexcept>
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <vector>
#include <thread>

ThreadPool* pool_ptr = nullptr;

void SignalHandler(int) {
  if (pool_ptr != nullptr) {
    pool_ptr->Stop();
  }
  exit(1);
}

int main() {
  signal(SIGINT, SignalHandler);
  int port, limit;
  std::string ip;

  std::cout << "Enter port of the 0.0.0.0 and limit of clients with whitespace. For example: 8000 2\n";
  std::cin >> port >> limit;
  Socket server("0.0.0.0", port, true, false);

  int true_number;
  std::cout << "Enter number to guess\n";
  std::cin >> true_number;

  server.SetClientLimit(limit);

  std::vector<int> client_fds;
  client_fds.reserve(limit);

  int epoll_fd = epoll_create(limit);
  std::atomic_bool stopped{false};
  std::thread exec_thread([&server, &client_fds, &stopped, limit] {
    server.Serve([&client_fds, &stopped, limit](int client_fd) -> bool {
      client_fds.push_back(client_fd);

      bool running = static_cast<int>(client_fds.size()) < limit;
      bool failure = false;
      stopped.compare_exchange_strong(failure, !running, std::memory_order_release);
      return running;
    }, stopped);
  });

  while (!stopped.load(std::memory_order_acquire)) {
    const std::string kStop = "stop";
    std::string input;
    std::cin >> input;
    if (input == kStop) {
      stopped.store(true, std::memory_order_release);
      break;
    }
  }

  exec_thread.join();

  std::ranges::for_each(client_fds, [epoll_fd](int client_fd) {
      struct epoll_event epoll_ev;
      epoll_ev.events =  EPOLLIN | EPOLLOUT;
      epoll_ev.data.fd = client_fd;

      int res = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &epoll_ev);
      if (res < 0) {
        throw std::runtime_error("error while registering client sockets!");
      }
  });

  const std::string kStarting = "Experiment is starting!\n";
  std::ranges::for_each(client_fds, [&kStarting](int fd) {
    int res = ::write(fd, kStarting.c_str(), kStarting.size());
    if (res < 0) {
      throw std::runtime_error("error while writing");
    }
    fsync(fd);
  });

  ThreadPool pool{std::min(client_fds.size(), static_cast<size_t>(std::thread::hardware_concurrency()))};
  pool.Start();
  pool_ptr = &pool;
  struct epoll_event events[SOMAXCONN];
  const int BUF_SIZE = 4096;

  while (!client_fds.empty()) {
    int obtained_fds = epoll_wait(epoll_fd, events, SOMAXCONN, -1);
    if (obtained_fds < 0) {
      throw std::runtime_error("epoll_wait error");
    }

    for (int i = 0; i < obtained_fds; ++i) {
      int fd = events[i].data.fd;
      if ((events[i].events & EPOLLIN) && (events[i].events & EPOLLOUT)) {
        pool.Submit([true_number, fd] {
          char buf[BUF_SIZE];
          bzero(reinterpret_cast<void*>(buf), BUF_SIZE);

          fcntl(fd, F_SETFL, O_NONBLOCK);
          int n = read(fd, buf, BUF_SIZE);
          const int ERRNO_CONNECTION_CLOSED = 104;
          if (n == 0 || n < 0 && errno != ERRNO_CONNECTION_CLOSED) {
            return;
          } else if (n < 0 && errno == ERRNO_CONNECTION_CLOSED) {
            close(fd);
            return;
          }

          int res, number = atoi(buf);
          if (number == true_number) {
            const char* msg = "Numbers are equal!\n";
            res = write(fd, msg, std::strlen(msg));
          } else if (number < true_number) {
            const char* msg = "Your number is less\n";
            res = write(fd, msg, std::strlen(msg));
          } else {
            const char* msg = "Your number is greater\n";
            res = write(fd, msg, std::strlen(msg));
          }

          if (res < 0 && errno != ERRNO_CONNECTION_CLOSED) {
            return;
          } else if (errno == ERRNO_CONNECTION_CLOSED) {
            close(fd);
          } else {
            fsync(fd);
          }
        });
      }
    }
  }
  pool.Stop();
  pool_ptr = nullptr;
}
