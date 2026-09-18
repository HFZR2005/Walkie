#include "include/Network.hpp"
#include "include/Receiver.hpp"
#include "include/Sender.hpp"
#include "include/constants.hpp"
#include "include/serialise.h"
#include <memory>

class Network::Impl {
public:
  Impl(AudioBridge &audioBridge, const char *listenPort, const Endpoint &remote)
      : audioBridge(audioBridge), sender(remote),
        receiver(listenPort, audioBridge) {}

  void send() {
    AudioPacket audioPacket{};
    while (audioBridge.queuedOutput() >= kSamplesPerFrame) {
      for (int i = 0; i < kSamplesPerFrame; i++) {
        audioBridge.popOutput(audioPacket.payload[static_cast<size_t>(i)]);
      }
      sender.send(audioPacket);
    }
  }

  void start() { receiver.start(); }
  void stop() { receiver.stop(); }

  AudioBridge &audioBridge;
  Sender sender;
  Receiver receiver;
};

Network::Network(AudioBridge &audioBridge, const char *listenPort,
                 const Endpoint &remote)
    : impl(std::make_shared<Impl>(audioBridge, listenPort, remote)) {}

Network::~Network() = default;

void Network::send() { impl->send(); }
void Network::start() { impl->start(); }
void Network::stop() { impl->stop(); }
