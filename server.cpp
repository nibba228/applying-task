#include "socket.cpp"

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstring>
#include <netinet/in.h>
#include <stdexcept>
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <vector>
#include <thread>

Socket* server_ptr = nullptr;

void SignalHandler(int) {
  server_ptr->Close();
  exit(1);
}

int main() {
  signal(SIGINT, SignalHandler);
  int port, limit;
  std::string ip;

  std::cout << "Enter ip of the server, port and limit of clients with whitespace. For example: 0.0.0.0 8000 12\n";
  std::cin >> ip >> port >> limit;
  Socket server(ip, 8000, false, true);
  server_ptr = &server;

  int true_number;
  std::cout << "Enter number to guess\n";
  std::cin >> true_number;

  server.SetClientLimit(limit);

  std::vector<int> client_fds;
  client_fds.reserve(limit);

  int epoll_fd = epoll_create(limit);
  std::atomic<bool> stopped{false};
  std::jthread exec_thread([&server, &client_fds, &stopped, epoll_fd, limit](std::stop_token stop) {
    server.Serve([&client_fds, &stopped, stop, limit](int client_fd) -> bool {
      if (stop.stop_requested()) {
        stopped.store(false, std::memory_order_release);
        return false;
      }
      client_fds.push_back(client_fd);
      bool running = static_cast<int>(client_fds.size()) < limit;
      if (!running) {
        stopped.store(true, std::memory_order_release);
        return running;
      }

      stopped.store(false, std::memory_order_release);
      return running;
    });
  });

  exec_thread.detach();
  while (!stopped.load(std::memory_order_acquire)) {
    std::string input;
    std::cin >> input;
    if (input == "stop") {
      exec_thread.request_stop();
    }
  }

  for (size_t i = 0; i < client_fds.size(); ++i) {
      struct epoll_event epoll_ev;
      epoll_ev.events =  EPOLLIN | EPOLLPRI | EPOLLERR | EPOLLHUP | EPOLLOUT;
      epoll_ev.data.fd = client_fds[i];

      int res = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fds[i], &epoll_ev);
      if (res < 0) {
        throw std::runtime_error("error while registering client sockets!");
      }
  }
  std::string buf = "Experiment is starting!";
  std::ranges::for_each(client_fds, [&buf](int fd) {
    int res = ::write(fd, buf.c_str(), buf.size());
    if (res < 0) {
      throw std::runtime_error("error while writing");
    }
  });

  struct epoll_event events[SOMAXCONN];
  const int BUF_SIZE = 4096;
  while (true) {
    int obtained_fds = epoll_wait(epoll_fd, events, SOMAXCONN, -1);
    if (obtained_fds < 0) {
      throw std::runtime_error("epoll_wait error");
    }

    for (int i = 0; i < obtained_fds; ++i) {
      int fd = events[i].data.fd;
      std::thread thread([&events, true_number, i, fd] {
        char buf[BUF_SIZE];
        bzero(reinterpret_cast<void*>(buf), BUF_SIZE);

        int n = read(fd, buf, BUF_SIZE);
        const int ERRNO_CONNECTION_CLOSED = 104;
        if (n < 0 && errno != ERRNO_CONNECTION_CLOSED) {
          throw std::runtime_error("error reading");
        } else if (errno == ERRNO_CONNECTION_CLOSED) {
          close(fd);
          return;
        }

        int res, number = atoi(buf);
        if (number == true_number) {
          std::string msg = "Numbers are equal!";
          res = write(fd, msg.c_str(), msg.size());
        } else if (number < true_number) {
          std::string msg = "Your number is less";
          res = write(fd, msg.c_str(), msg.size());
        } else {
          std::string msg = "Your number is greater";
          res = write(fd, msg.c_str(), msg.size());
        }

        if (res < 0 && errno != ERRNO_CONNECTION_CLOSED) {
          throw std::runtime_error("error writing");
        } else if (errno == ERRNO_CONNECTION_CLOSED) {
          close(fd);
        } else {
          fsync(fd);
        }
      });
      thread.detach();
    }

  }

}
