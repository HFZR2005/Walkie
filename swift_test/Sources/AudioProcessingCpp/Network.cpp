#include "include/Network.hpp"
#include "include/dr_wav.h"
#include "include/processAudio.hpp"

#include <iostream>

Network::Network(AudioBridge &audioBridge) : audioBridge(audioBridge) {}

Network::~Network() { flush(); }

void Network::poll() {
  int16_t sample;
  while (audioBridge.popOutput(sample)) {
    output.push_back(sample);
  }
}

void Network::flush() {
  poll(); // catch any final samples still sitting in outputBuffer
  std::cout << "Captured " << output.size() << " samples";
  writeWav("output.wav", output);
}
