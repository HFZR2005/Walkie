#pragma once
#include "Endpoint.hpp"
#include "serialise.h"
#include <netdb.h>

class Sender {
public:
  explicit Sender(const Endpoint &remote);
  ~Sender();
  Sender(const Sender &) = delete;
  Sender &operator=(const Sender &) = delete;
  void send(const AudioPacket &packet);

private:
  int sockfd = -1;
  int sequence_number = 0;
  struct addrinfo *servinfo = nullptr;
  struct addrinfo *p = nullptr;
};
