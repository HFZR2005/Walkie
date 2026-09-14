#include "include/Network.hpp"
#include "include/dr_wav.h"
#include "include/processAudio.hpp"
#include "include/serialise.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

Network::Network(AudioBridge &audioBridge, const char *serverPort)
    : audioBridge(audioBridge) {
  // initialise the network connection

  auto hostname = "localhost";
  int rv;
  int numbytes;

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET6; // set to AF_INET to use IPv4
  hints.ai_socktype = SOCK_DGRAM;

  rv = getaddrinfo(hostname, serverPort, &hints, &servinfo);
  if (rv != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    exit(1);
  }

  // loop through all the results and make a socket
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
      perror("talker: socket");
      continue;
    }

    break;
  }

  if (p == NULL) {
    fprintf(stderr, "talker: failed to create socket\n");
    exit(2);
  }

  sequence_number = 0;
}

Network::~Network() {
  //  flush();
  freeaddrinfo(servinfo);
  close(sockfd);
}

void Network::poll() {

  AudioPacket audioPacket;
  while (audioBridge.queuedOutput() >= kSamplesPerFrame) {
    audioPacket.sequence_number = sequence_number;

    for (int i = 0; i < kSamplesPerFrame; i++) {
      audioBridge.popOutput(audioPacket.payload[i]);
    }

    this->send(audioPacket);
    sequence_number++;
  }
}

void Network::flush() {
  AudioPacket audioPacket;
  while (audioBridge.queuedOutput() >= kSamplesPerFrame) {
    audioPacket.sequence_number = sequence_number;
    for (int i = 0; i < kSamplesPerFrame; i++) {
      audioBridge.popOutput(audioPacket.payload[i]);
    }
    this->send(audioPacket);
    sequence_number++;
    ;
  }
}

void Network::send(AudioPacket audioPacket) {
  // send audio packet over the network
  int numbytes;

  PacketBuffer buffer = serialisePacket(audioPacket);

  if ((numbytes = sendto(sockfd, &buffer, sizeof(PacketBuffer), 0, p->ai_addr,
                         p->ai_addrlen)) == -1) {
    perror("talker: sendto");
    exit(1);
  }
}
