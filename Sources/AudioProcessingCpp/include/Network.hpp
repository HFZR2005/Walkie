#pragma once
#include "AudioProcessing.hpp"
#include <cstdint>
#include <vector>

class Network {
public:
  Network(AudioBridge &audioBridge);
  ~Network();
  void poll(); // call this periodically — no thread, caller drives the timing
  void flush();

private:
  std::vector<int16_t> output;
  AudioBridge &audioBridge;
};
