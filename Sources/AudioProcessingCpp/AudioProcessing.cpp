#include "include/AudioProcessing.hpp"
#include "include/RingBuffer.hpp"
#include "include/constants.hpp"
#include "include/dr_wav.h"
#include "include/processAudio.hpp"
#include "modules/audio_processing/include/audio_processing.h"
#include <memory>
#include <thread>

using namespace webrtc;

class AudioBridge::Impl {

public:
  Impl(rtc::scoped_refptr<AudioProcessing> apm) : apm(apm) {
    streamConfig.set_num_channels(kNumChannels);
    streamConfig.set_sample_rate_hz(kSampleRate);
  }
  rtc::scoped_refptr<AudioProcessing> apm;

  StreamConfig streamConfig;

  RingBuffer<int16_t, 16384> nearEndBuffer; // Mic -> [nearEndBuffer] --> AEC3
  RingBuffer<int16_t, 16384> farEndBuffer;  // Network -> [farEndBuffer] -> AEC3
  RingBuffer<int16_t, 16384> outputBuffer;  // AEC3 -> [outputBuffer] -> Network
  RingBuffer<int16_t, 16384> pbackBuffer; // Network -> [pbackBuffer] -> Speaker

  std::atomic<bool> running{false};
  std::atomic<bool> passthrough{false};
  std::thread worker;

  ~Impl() {
    if (worker.joinable()) {
      worker.join();
    }
  }

  int processAvailableFrames() {
    int frames = 0;
    while (nearEndBuffer.size() >= kSamplesPerFrame &&
           farEndBuffer.size() >= kSamplesPerFrame) {
      array<int16_t, kSamplesPerFrame> nearEndArray{};
      array<int16_t, kSamplesPerFrame> farEndArray{};
      array<int16_t, kSamplesPerFrame> outputArray{};

      for (size_t i = 0; i < kSamplesPerFrame; i++) {
        nearEndBuffer.pop(nearEndArray[i]);
        farEndBuffer.pop(farEndArray[i]);
      }

      if (passthrough.load(std::memory_order_acquire)) {
        outputArray = nearEndArray;
      } else {
        process_audio(nearEndArray, farEndArray, outputArray, apm,
                      streamConfig);
      }

      for (size_t i = 0; i < kSamplesPerFrame; i++) {
        outputBuffer.push(outputArray[i]);
      }
      frames++;
    }
    return frames;
  }

  void processingLoop() {
    while (running.load(std::memory_order_acquire)) {
      if (processAvailableFrames() == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }
  }

  void start() {
    running.store(true, std::memory_order_release);
    worker = std::thread(&Impl::processingLoop, this);
  }
  void stop() {
    running.store(false, std::memory_order_release);
    if (worker.joinable()) {
      worker.join();
    };
  }
};

AudioBridge::AudioBridge() {
  // Set up apm
  AudioProcessing::Config config;
  config.pre_amplifier.enabled = true;
  config.gain_controller1.enabled = true;
  config.gain_controller1.mode =
      AudioProcessing::Config::GainController1::kAdaptiveDigital;
  config.gain_controller2.enabled = true;
  config.high_pass_filter.enabled = true;
  config.voice_detection.enabled = true;
  config.echo_canceller.enabled = true;

  rtc::scoped_refptr<AudioProcessing> apm(AudioProcessingBuilder().Create());
  apm->ApplyConfig(config);
  impl = std::make_shared<Impl>(apm);
}

AudioBridge::~AudioBridge() = default;

int AudioBridge::processFrame() {
  impl->processingLoop();
  return 0;
}

int AudioBridge::processAvailableFrames() {
  return impl->processAvailableFrames();
}

void AudioBridge::setPassthrough(bool enabled) {
  impl->passthrough.store(enabled, std::memory_order_release);
}

int AudioBridge::queuedNearEnd() {
  return static_cast<int>(impl->nearEndBuffer.size());
}

int AudioBridge::queuedFarEnd() {
  return static_cast<int>(impl->farEndBuffer.size());
}

int AudioBridge::queuedOutput() {
  return static_cast<int>(impl->outputBuffer.size());
}

bool AudioBridge::pushNearEnd(const int16_t *sample, int sampleCount) {
  if (!sample || sampleCount <= 0) {
    return false;
  }
  for (int i = 0; i < sampleCount; i++) {
    impl->nearEndBuffer.push(sample[i]);
  }
  return true;
}

bool AudioBridge::pushFarEnd(const int16_t *sample, int sampleCount) {
  if (!sample || sampleCount <= 0) {
    return false;
  }
  for (int i = 0; i < sampleCount; i++) {
    impl->farEndBuffer.push(sample[i]);
  }
  return true;
}

bool AudioBridge::popOutput(int16_t &out) {
  return impl->outputBuffer.pop(out);
}

void AudioBridge::stop() { impl->stop(); }
void AudioBridge::start() { impl->start(); }
