#define DR_WAV_IMPLEMENTATION
#include "constants.h"
#include "dr_wav.h"
#include "modules/audio_processing/include/audio_processing.h"
#include <algorithm>
#include <cstdint>
#include <iostream>

using namespace webrtc;

std::vector<int16_t> loadWav(const char *filename) {
  drwav wav;

  if (!drwav_init_file(&wav, filename, NULL)) {
    std::cerr << "Error opening file " << "'" << filename << "'";
    exit(1);
  }
  std::vector<int16_t> v(wav.totalPCMFrameCount * wav.channels);
  std::cout << "Loading Wav \n";
  drwav_read_pcm_frames_s16(&wav, wav.totalPCMFrameCount, v.data());
  drwav_uninit(&wav);
  return v;
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

bool process_audio(std::vector<int16_t> &audio_near_end,
                   std::vector<int16_t> &audio_far_end, AudioProcessing *apm,
                   const StreamConfig &streamConfig) {
  size_t samplesPerFrame =
      (streamConfig.num_channels() * streamConfig.sample_rate_hz()) /
      100; // 1s is sample_rate samples. Divide by 100 for 10ms
  size_t numFrames =
      std::min(audio_far_end.size(), audio_near_end.size()) / samplesPerFrame;

  bool success = true;
  for (size_t i = 0; i < numFrames; i++) {
    int16_t *nearEndFrame = audio_near_end.data() + i * samplesPerFrame;
    int16_t *farEndFrame = audio_far_end.data() + i * samplesPerFrame;

    int err = apm->ProcessReverseStream(farEndFrame, streamConfig, streamConfig,
                                        farEndFrame);

    if (err != 0) {
      std::cerr << "ProcessReverseStream error " << err << " at frame " << i
                << "\n";
      success = false;
    }

    err = apm->ProcessStream(nearEndFrame, streamConfig, streamConfig,
                             nearEndFrame);

    if (err != 0) {
      std::cerr << "ProcessStream error " << err << " at frame " << i << "\n";
      success = false;
    }
  }

  return success;
}

int main(int argc, char **argv) {
  AudioProcessing::Config config;

  config.pre_amplifier.enabled = true;
  config.gain_controller1.mode =
      AudioProcessing::Config::GainController1::kAdaptiveAnalog;
  config.gain_controller1.analog_level_minimum = 0;
  config.gain_controller1.analog_level_maximum = 255;
  config.gain_controller2.enabled = true;
  config.high_pass_filter.enabled = true;
  config.voice_detection.enabled = true;
  config.echo_canceller.enabled = true;

  rtc::scoped_refptr<AudioProcessing> apm(AudioProcessingBuilder().Create());
  apm->ApplyConfig(config);

  auto input_near_end = loadWav("test_data/near_end.wav");
  auto input_far_end = loadWav("test_data/far_end.wav");
  StreamConfig streamConfig;
  streamConfig.set_sample_rate_hz(kSampleRate);
  streamConfig.set_num_channels(kNumChannels);
  process_audio(input_near_end, input_far_end, apm, streamConfig);
  auto output = writeWav("output.wav", input_near_end);

  return 0;
}
