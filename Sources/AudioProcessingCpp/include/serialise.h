#ifndef SERIALISE_H
#define SERIALISE_H

#include <array>
#include <cstdint>
using std::array;

struct __attribute__((packed)) AudioPacket {
  int sequence_number;
  array<int16_t, 160> payload;
};

using PacketBuffer = array<int8_t, sizeof(AudioPacket)>;

PacketBuffer serialisePacket(const AudioPacket &packet);
AudioPacket deSerialisePacket(const PacketBuffer &buffer);

#endif
