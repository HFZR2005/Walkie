#include "include/Receiver.hpp"
#include "include/constants.hpp"
#include "include/serialise.h"
#include <atomic>
#include <cerrno>
#include <iostream>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

class Receiver::Impl {
public:
  Impl(const char *listenPort, AudioBridge &audioBridge)
      : audioBridge(audioBridge) {
    struct addrinfo hints {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *servinfo = nullptr;
    int rv = getaddrinfo(nullptr, listenPort, &hints, &servinfo);
    if (rv != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      exit(1);
    }

    struct addrinfo *p = nullptr;
    for (p = servinfo; p != nullptr; p = p->ai_next) {
      sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if (sockfd == -1) {
        perror("listener: socket");
        continue;
      }
      if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
        close(sockfd);
        sockfd = -1;
        perror("listener: bind");
        continue;
      }
      break;
    }

    freeaddrinfo(servinfo);

    if (p == nullptr) {
      std::cerr << "Couldn't bind to socket.\n";
      exit(2);
    }

    struct timeval tv {};
    tv.tv_sec = 0;
    tv.tv_usec = 200000;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  }

  ~Impl() { stop(); }

  void receive() {
    PacketBuffer buf{};
    struct sockaddr_storage their_addr {};
    socklen_t addr_len = sizeof(their_addr);

    while (running.load(std::memory_order_acquire)) {
      ssize_t numbytes =
          recvfrom(sockfd, buf.data(), buf.size(), 0,
                   reinterpret_cast<struct sockaddr *>(&their_addr), &addr_len);
      if (numbytes < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
          continue;
        }
        if (!running.load(std::memory_order_acquire)) {
          break;
        }
        perror("recvfrom");
        break;
      }
      if (numbytes != static_cast<ssize_t>(sizeof(AudioPacket))) {
        continue;
      }

      AudioPacket audio = deSerialisePacket(buf);
      audioBridge.pushPlayback(audio.payload.data(), kSamplesPerFrame);
    }
  }

  void start() {
    bool expected = false;
    if (!running.compare_exchange_strong(expected, true)) {
      return;
    }
    worker = std::thread(&Impl::receive, this);
  }

  void stop() {
    running.store(false, std::memory_order_release);
    if (sockfd >= 0) {
      shutdown(sockfd, SHUT_RDWR);
    }
    if (worker.joinable()) {
      worker.join();
    }
    if (sockfd >= 0) {
      close(sockfd);
      sockfd = -1;
    }
  }

  AudioBridge &audioBridge;
  std::thread worker;
  std::atomic<bool> running{false};
  int sockfd = -1;
};

Receiver::Receiver(const char *listenPort, AudioBridge &audioBridge) {
  impl = std::make_shared<Impl>(listenPort, audioBridge);
}

Receiver::~Receiver() = default;
void Receiver::start() { impl->start(); }
void Receiver::stop() { impl->stop(); }
