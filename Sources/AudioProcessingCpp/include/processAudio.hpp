
#pragma once
#include "constants.hpp"
#include <array>
#include <cstring>
#include <vector>

namespace webrtc {
class AudioProcessing;
class StreamConfig;
} // namespace webrtc

using std::array;
using std::vector;

using namespace webrtc;
bool process_audio(array<int16_t, kSamplesPerFrame> &audio_near_end,
                   array<int16_t, kSamplesPerFrame> &audio_far_end,
                   array<int16_t, kSamplesPerFrame> &output,
                   AudioProcessing *apm, const StreamConfig &streamConfig);

bool writeWav(const char *filename, const std::vector<int16_t> &audio);
