#pragma once

#include <atomic>
#include <string_view>
#include <functional>
#include <netinet/in.h>
#include <unistd.h>


class Socket {
public:
  Socket(std::string_view ip = "0.0.0.0",
         uint16_t port = 8080,
         bool verbose = false,
         bool blocking = true);

  ~Socket() { close(sockfd_); }

  void SetClientLimit(int limit) { client_limit_ = limit; }
  int GetClientLimit() const { return client_limit_; }
  void Close() const { close(sockfd_); }
  void Fsync() const { fsync(sockfd_); }

  void Serve(std::function<bool(int)> handler, std::atomic_bool& is_cancelled);

  void Connect();

  int Read(char* buff, int len);
  int Write(const char* buff, int len);

private:
  bool verbose_;
  int sockfd_;
  int client_limit_{128};
  uint16_t port_;
  struct sockaddr_in servaddr_;
  std::string_view ip_;
};
