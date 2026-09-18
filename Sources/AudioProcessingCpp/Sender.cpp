#include "include/Sender.hpp"
#include <cstdio>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

Sender::Sender(const Endpoint &remote) {
  struct addrinfo hints {};
  hints.ai_family = AF_INET; // IPv4 LAN addresses; discovery can still return a host:port
  hints.ai_socktype = SOCK_DGRAM;

  int rv = getaddrinfo(remote.host(), remote.port(), &hints, &servinfo);
  if (rv != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  for (p = servinfo; p != nullptr; p = p->ai_next) {
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd == -1) {
      perror("talker: socket");
      continue;
    }
    break;
  }

  if (p == nullptr) {
    fprintf(stderr, "talker: failed to create socket\n");
    exit(2);
  }
}

Sender::~Sender() {
  if (servinfo != nullptr) {
    freeaddrinfo(servinfo);
  }
  if (sockfd >= 0) {
    close(sockfd);
  }
}

void Sender::send(const AudioPacket &packet) {
  AudioPacket outgoing = packet;
  outgoing.sequence_number = sequence_number;
  PacketBuffer buffer = serialisePacket(outgoing);

  if (sendto(sockfd, buffer.data(), buffer.size(), 0, p->ai_addr, p->ai_addrlen) ==
      -1) {
    perror("talker: sendto");
    return;
  }
  sequence_number++;
}
