#pragma once
#include "AudioProcessing.hpp"
#include "serialise.h"
#include <arpa/inet.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

using std::array;

class Network {
public:
  Network(AudioBridge &audioBridge, const char *serverPort);
  ~Network();
  void poll(); // call this periodically — no thread, caller drives the timing
  void flush();
  void send(AudioPacket audioPacket);

  int sockfd;
  int sequence_number;
  struct addrinfo hints, *servinfo, *p;

private:
  std::vector<int16_t> output;
  AudioBridge &audioBridge;
};
