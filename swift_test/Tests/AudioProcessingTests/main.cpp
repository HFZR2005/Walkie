#include "AudioProcessing.hpp"
#include "RingBuffer.hpp"
#include "constants.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// How to add a test
//
// 1. Add a void testYourName() { ... } function in this file (or a new .cpp
//    next to it, then declare it here and add it to the registry in main).
// 2. Use CHECK / CHECK_EQ. On failure they print file:line and fail the run.
// 3. Register it:  run("your_name", testYourName);
// 4. Run from the swift_test directory:
//      swift run AudioProcessingTests
//
// Pick the right mode
//   • setPassthrough(true)  — WebRTC is off. Output must equal the near-end
//     samples. Use this for queue, copy, and lockstep bugs.
//   • passthrough off       — full APM. Only assert invariants (length, not
//     a constant, not random noise), not exact sample values.
//
// Use processAvailableFrames(), not start(). start() is a background thread
// and will flake. processAvailableFrames() is synchronous and returns how
// many 10 ms frames were consumed.
//
// Fixture that would have caught the original bugs
//   push a ramp {0,1,2,...,n-1} on near (and far if you need a pair)
//   processAvailableFrames()
//   pop everything
//   CHECK_EQ(output, that ramp)
// If someone writes push(*sample) again, output becomes {0,0,0,...}.
// If they process near without far, queued counts change and output appears
// too early.
// -----------------------------------------------------------------------------

namespace {

int g_failures = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::cerr << __FILE__ << ":" << __LINE__ << " CHECK failed: " << #cond   \
                << "\n";                                                       \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

#define CHECK_EQ(a, b)                                                         \
  do {                                                                         \
    const auto &_va = (a);                                                     \
    const auto &_vb = (b);                                                     \
    if (_va != _vb) {                                                          \
      std::cerr << __FILE__ << ":" << __LINE__ << " CHECK_EQ failed: " << #a   \
                << " != " << #b << " (" << _va << " vs " << _vb << ")\n";      \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

std::vector<int16_t> ramp(size_t count, int16_t start = 0) {
  std::vector<int16_t> samples(count);
  for (size_t i = 0; i < count; ++i) {
    samples[i] = static_cast<int16_t>(start + static_cast<int16_t>(i));
  }
  return samples;
}

bool pushNear(AudioBridge &bridge, const std::vector<int16_t> &samples) {
  return bridge.pushNearEnd(samples.data(), static_cast<int>(samples.size()));
}

bool pushFar(AudioBridge &bridge, const std::vector<int16_t> &samples) {
  return bridge.pushFarEnd(samples.data(), static_cast<int>(samples.size()));
}

std::vector<int16_t> popAll(AudioBridge &bridge) {
  std::vector<int16_t> samples;
  int16_t sample = 0;
  while (bridge.popOutput(sample)) {
    samples.push_back(sample);
  }
  return samples;
}

void run(const char *name, void (*fn)()) {
  const int before = g_failures;
  fn();
  if (g_failures == before) {
    std::cout << "  pass  " << name << "\n";
  } else {
    std::cout << "  FAIL  " << name << "\n";
  }
}

// MARK: RingBuffer — ignored push/pop bools and FIFO order

void testRingBufferFifoAndEmptyPop() {
  RingBuffer<int16_t, 8> buffer;
  int16_t out = 123;

  CHECK(buffer.pop(out) == false);
  CHECK_EQ(out, 123); // empty pop must not write garbage or change out
  CHECK_EQ(buffer.size(), 0u);

  CHECK(buffer.push(1));
  CHECK(buffer.push(2));
  CHECK(buffer.push(3));
  CHECK_EQ(buffer.size(), 3u);

  CHECK(buffer.pop(out));
  CHECK_EQ(out, 1);
  CHECK(buffer.pop(out));
  CHECK_EQ(out, 2);
  CHECK(buffer.pop(out));
  CHECK_EQ(out, 3);
  CHECK(buffer.pop(out) == false);
  CHECK_EQ(out, 3);
}

void testRingBufferOverflowDoesNotCorrupt() {
  RingBuffer<int16_t, 8> buffer; // capacity is 7 (one slot reserved)
  for (int16_t i = 0; i < 7; ++i) {
    CHECK(buffer.push(i));
  }
  CHECK(buffer.push(99) == false);
  CHECK_EQ(buffer.size(), 7u);

  int16_t out = 0;
  CHECK(buffer.pop(out));
  CHECK_EQ(out, 0); // overflow must drop the new sample, not rotate the FIFO
}

// MARK: AudioBridge — the bugs that garbled output

void testPushNearEndPreservesRamp() {
  // Old bug: push(*sample) wrote sample[0] once per callback.
  AudioBridge bridge;
  bridge.setPassthrough(true);

  const auto near = ramp(kSamplesPerFrame);
  const auto far = std::vector<int16_t>(kSamplesPerFrame, 99);

  CHECK(pushNear(bridge, near));
  CHECK(pushFar(bridge, far));
  CHECK_EQ(bridge.processAvailableFrames(), 1);

  const auto output = popAll(bridge);
  CHECK(output == near);
}

void testPushFarEndPreservesRampInQueue() {
  // Leftover far samples after one frame must be the tail of the ramp,
  // not more copies of far[0].
  AudioBridge bridge;
  bridge.setPassthrough(true);

  const auto near = ramp(kSamplesPerFrame);
  const auto far = ramp(kSamplesPerFrame + 40, 500);

  CHECK(pushNear(bridge, near));
  CHECK(pushFar(bridge, far));
  CHECK_EQ(bridge.processAvailableFrames(), 1);
  CHECK_EQ(bridge.queuedFarEnd(), 40);
  CHECK(popAll(bridge) == near);

  CHECK(pushNear(bridge, ramp(kSamplesPerFrame, 2000)));
  CHECK(pushFar(bridge, std::vector<int16_t>(kSamplesPerFrame - 40, 0)));
  CHECK_EQ(bridge.processAvailableFrames(), 1);
  CHECK(popAll(bridge) == ramp(kSamplesPerFrame, 2000));
}

void testWaitsForBothQueuesBeforeProcessing() {
  // Old bug: waited on near-end only, then popped far-end (often empty).
  AudioBridge bridge;
  bridge.setPassthrough(true);

  const auto near = ramp(kSamplesPerFrame);
  CHECK(pushNear(bridge, near));
  CHECK_EQ(bridge.processAvailableFrames(), 0);
  CHECK_EQ(bridge.queuedNearEnd(), kSamplesPerFrame);
  CHECK_EQ(bridge.queuedOutput(), 0);
  CHECK(popAll(bridge).empty());

  CHECK(pushFar(bridge, ramp(kSamplesPerFrame / 2, 1000)));
  CHECK_EQ(bridge.processAvailableFrames(), 0);
  CHECK_EQ(bridge.queuedNearEnd(), kSamplesPerFrame);
  CHECK_EQ(bridge.queuedFarEnd(), kSamplesPerFrame / 2);
  CHECK(popAll(bridge).empty());

  CHECK(pushFar(bridge, ramp(kSamplesPerFrame / 2, 1080)));
  CHECK_EQ(bridge.processAvailableFrames(), 1);
  CHECK_EQ(bridge.queuedNearEnd(), 0);
  CHECK_EQ(bridge.queuedFarEnd(), 0);
  CHECK(popAll(bridge) == near);
}

void testTwoFramesStayInOrder() {
  AudioBridge bridge;
  bridge.setPassthrough(true);

  const auto near = ramp(kSamplesPerFrame * 2);
  const auto far = ramp(kSamplesPerFrame * 2, 3000);

  CHECK(pushNear(bridge, near));
  CHECK(pushFar(bridge, far));
  CHECK_EQ(bridge.processAvailableFrames(), 2);
  CHECK(popAll(bridge) == near);
}

void testRejectsInvalidPushes() {
  AudioBridge bridge;
  bridge.setPassthrough(true);

  CHECK(bridge.pushNearEnd(nullptr, kSamplesPerFrame) == false);
  CHECK(bridge.pushFarEnd(nullptr, kSamplesPerFrame) == false);
  CHECK(bridge.pushNearEnd(ramp(1).data(), 0) == false);
  CHECK(bridge.pushFarEnd(ramp(1).data(), 0) == false);
  CHECK_EQ(bridge.queuedNearEnd(), 0);
  CHECK_EQ(bridge.queuedFarEnd(), 0);
}

void testProcessedOutputIsNotUninitializedGarbage() {
  // Old bug: ProcessStream could fail and leave dest untouched; dest was
  // an uninitialized array. Passthrough is off so this goes through APM.
  AudioBridge bridge;
  bridge.setPassthrough(false);

  std::vector<int16_t> near(kSamplesPerFrame);
  for (int i = 0; i < kSamplesPerFrame; ++i) {
    near[i] = static_cast<int16_t>(i * 200);
  }
  const auto far = std::vector<int16_t>(kSamplesPerFrame, 0);

  CHECK(pushNear(bridge, near));
  CHECK(pushFar(bridge, far));
  CHECK_EQ(bridge.processAvailableFrames(), 1);

  const auto output = popAll(bridge);
  CHECK_EQ(output.size(), static_cast<size_t>(kSamplesPerFrame));

  bool all_same = true;
  for (size_t i = 1; i < output.size(); ++i) {
    if (output[i] != output[0]) {
      all_same = false;
      break;
    }
  }
  CHECK(all_same == false);

  int64_t abs_sum = 0;
  for (int16_t sample : output) {
    abs_sum += std::abs(static_cast<int>(sample));
  }
  const int mean_abs =
      static_cast<int>(abs_sum / static_cast<int64_t>(output.size()));
  // Random int16 has typical |sample| around 16k. A speech-scale ramp
  // through APM should not look like that.
  CHECK(mean_abs < 8000);
}

} // namespace

int main() {
  std::cout << "AudioProcessingTests\n";

  run("ring_buffer_fifo_and_empty_pop", testRingBufferFifoAndEmptyPop);
  run("ring_buffer_overflow_does_not_corrupt",
      testRingBufferOverflowDoesNotCorrupt);
  run("push_near_end_preserves_ramp", testPushNearEndPreservesRamp);
  run("push_far_end_preserves_ramp_in_queue", testPushFarEndPreservesRampInQueue);
  run("waits_for_both_queues_before_processing",
      testWaitsForBothQueuesBeforeProcessing);
  run("two_frames_stay_in_order", testTwoFramesStayInOrder);
  run("rejects_invalid_pushes", testRejectsInvalidPushes);
  run("processed_output_is_not_uninitialized_garbage",
      testProcessedOutputIsNotUninitializedGarbage);

  if (g_failures == 0) {
    std::cout << "All tests passed.\n";
    return 0;
  }
  std::cout << g_failures << " check(s) failed.\n";
  return 1;
}
