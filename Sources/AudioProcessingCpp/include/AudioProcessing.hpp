#pragma once
#include <cstdint>
#include <memory>

using std::array;
using std::shared_ptr;

class AudioBridge {
public:
  AudioBridge();
  ~AudioBridge();
  int processFrame();
  // Process every complete near+far pair currently queued. Returns frames
  // written.
  int processAvailableFrames();
  // Skip WebRTC so queue/copy bugs can be tested without AEC/AGC changing
  // samples.
  void setPassthrough(bool enabled);
  bool pushNearEnd(const int16_t *sample, int sampleCount);
  bool pushFarEnd(const int16_t *sample, int sampleCount);
  bool pushPlayback(const int16_t *sample, int sampleCount);
  bool popOutput(int16_t &out);
  bool popPlayback(int16_t &out);
  int queuedNearEnd();
  int queuedFarEnd();
  int queuedOutput();
  int queuedPlayback();
  void start();
  void stop();

private:
  class Impl;
  shared_ptr<Impl> impl;
};
