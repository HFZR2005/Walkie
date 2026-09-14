#include "include/serialise.h"
#include <cstring>

PacketBuffer serialisePacket(const AudioPacket &packet) {
  PacketBuffer buffer;
  std::memcpy(buffer.data(), &packet, sizeof(AudioPacket));
  return buffer;
}

AudioPacket deSerialisePacket(const PacketBuffer &buffer) {
  AudioPacket tmp;
  std::memcpy(&tmp, buffer.data(), sizeof(AudioPacket));
  return tmp;
}
