#pragma once
#include "AudioProcessing.hpp"
#include "Endpoint.hpp"
#include <memory>

class Network {
public:
  // listenPort is local. remote is whoever we send to — CLI today, discovery later.
  Network(AudioBridge &audioBridge, const char *listenPort,
          const Endpoint &remote);
  ~Network();

  void send(); // caller drives timing; no send thread
  void start();
  void stop();

private:
  class Impl;
  std::shared_ptr<Impl> impl;
};
