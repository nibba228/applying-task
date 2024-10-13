#include "socket.hpp"

#include <cerrno>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdexcept>
#include <iostream>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>


Socket::Socket(std::string_view ip, uint16_t port, bool verbose, bool blocking)
  : verbose_(verbose),
    sockfd_(socket(AF_INET, SOCK_STREAM, 0)),
    port_(port),
    ip_(ip),
    servaddr_{.sin_family = AF_INET, .sin_port = htons(port), .sin_addr = {htonl(INADDR_ANY)}} {
  if (!blocking) {
    fcntl(sockfd_, F_SETFL, O_NONBLOCK);
  }

  if (sockfd_ == -1) {
    throw std::runtime_error("socket creation failed");
  } 

  if(verbose) {
    std::cout << "Socket successfully created\n";
  }

  if (ip != "0.0.0.0") {
    servaddr_.sin_addr.s_addr = inet_addr(ip.data());
  }
}

void Socket::Serve(std::function<bool(int)> handler) {
  if ((::bind(sockfd_, reinterpret_cast<struct sockaddr*>(&servaddr_), sizeof(struct sockaddr_in))) != 0) {
    std::cout << errno;
    throw std::runtime_error("socket bind failed");
  } 

  if(verbose_) {
    std::cout << "socket successfully binded" << std::endl;
  }

  if ((listen(sockfd_, client_limit_)) != 0) {
    throw std::runtime_error("Listen failed");
  } 

  if(verbose_) {
    std::cout << "Server listening" << std::endl;
  }

  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  bool running{true};

  while (running) {
    int connfd = accept(sockfd_, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
    if (connfd < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
      throw std::runtime_error("ERROR on accept");
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
      continue;
    }

    running = handler(connfd);
  }
}

void Socket::Connect() {
  int fd = ::connect(sockfd_, reinterpret_cast<struct sockaddr*>(&servaddr_), sizeof(struct sockaddr_in));
  if (fd < 0) {
    throw std::runtime_error("ERROR connecting");
  }
}

int Socket::Read(char* buff, int len) {
  return ::read(sockfd_, buff, len);
}

int Socket::Write(const char* buff, int len) {
  return ::write(sockfd_, buff, len);
}
