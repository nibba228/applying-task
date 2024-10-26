#include "socket.cpp"

#include <csignal>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <strings.h>
#include <iostream>

int main() {
  int port;
  std::cin >> port;
  Socket client("0.0.0.0", port, true);
  const int BUF_SIZE = 4096;
  client.Connect();

  while (true) {
    char buf[BUF_SIZE];
    bzero(buf, BUF_SIZE);
    int res = client.Read(buf, BUF_SIZE);
    if (res <= 0) {
      throw std::runtime_error("error reading");
    }
    std::cout << buf;
    fflush(stdout);

    std::string number;
    std::cin >> number;
    res = client.Write(number.c_str(), number.size());
    if (res < 0) {
      throw std::runtime_error("error writing");
    }
    client.Fsync();
  }
}
