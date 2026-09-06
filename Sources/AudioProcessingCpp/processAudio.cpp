#define DR_WAV_IMPLEMENTATION
#include "include/processAudio.hpp"
#include "include/RingBuffer.hpp"
#include "include/constants.hpp"
#include "include/dr_wav.h"
#include "modules/audio_processing/include/audio_processing.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>

using std::array;
using namespace webrtc;

bool process_audio(array<int16_t, kSamplesPerFrame> &audio_near_end,
                   array<int16_t, kSamplesPerFrame> &audio_far_end,
                   array<int16_t, kSamplesPerFrame> &output,
                   AudioProcessing *apm, const StreamConfig &streamConfig) {
  size_t samplesPerFrame =
      (streamConfig.num_channels() * streamConfig.sample_rate_hz()) /
      100; // 1s is sample_rate samples. Divide by 100 for 10ms
  size_t numFrames =
      std::min(audio_far_end.size(), audio_near_end.size()) / samplesPerFrame;

  bool success = true;
  for (size_t i = 0; i < numFrames; i++) {
    int16_t *nearEndFrame = audio_near_end.data() + i * samplesPerFrame;
    int16_t *farEndFrame = audio_far_end.data() + i * samplesPerFrame;
    int16_t *outputFrame = output.data() + i * samplesPerFrame;

    int err = apm->ProcessReverseStream(farEndFrame, streamConfig, streamConfig,
                                        farEndFrame);

    if (err != 0) {
      std::cerr << "ProcessReverseStream error " << err << " at frame " << i
                << "\n";
      success = false;
    }

    // Echo canceller requires a delay estimate each capture frame. Copy
    // near-end into dest first so a ProcessStream failure cannot emit
    // uninitialized samples.
    apm->set_stream_delay_ms(50);
    std::memcpy(outputFrame, nearEndFrame, samplesPerFrame * sizeof(int16_t));

    err = apm->ProcessStream(nearEndFrame, streamConfig, streamConfig,
                             outputFrame);

    if (err != 0) {
      std::cerr << "ProcessStream error " << err << " at frame " << i << "\n";
      success = false;
    }
  }

  return success;
}

bool writeWav(const char *filename, const std::vector<int16_t> &audio) {
  drwav_data_format format;
  format.container =
      drwav_container_riff; // <-- drwav_container_riff = normal WAV files,
                            // drwav_container_w64 = Sony Wave64.
  format.format = DR_WAVE_FORMAT_PCM; // <-- Any of the DR_WAVE_FORMAT_* codes.
  format.channels = kNumChannels;
  format.sampleRate = kSampleRate;
  format.bitsPerSample = kBitsPerSample;

  drwav wav;
  if (!drwav_init_file_write(&wav, filename, &format, NULL)) {
    std::cerr << "Error occured while initialising write";
    exit(1);
  }

  drwav_uint64 framesWritten = drwav_write_pcm_frames(
      &wav, audio.size() / format.channels, audio.data());

  drwav_uninit(&wav);

  return true;
}
