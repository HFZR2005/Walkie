#pragma once
#include "AudioProcessing.hpp"
#include <memory>

class Receiver {
public:
  Receiver(const char *listenPort, AudioBridge &audioBridge);
  ~Receiver();
  Receiver(const Receiver &) = delete;
  Receiver &operator=(const Receiver &) = delete;

  void start();
  void stop();

private:
  class Impl;
  std::shared_ptr<Impl> impl;
};
