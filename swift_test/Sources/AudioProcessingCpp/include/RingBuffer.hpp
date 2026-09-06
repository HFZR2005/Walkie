#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <array>
#include <atomic>
#include <cstddef>

using std::array;
using std::atomic;
using std::size_t;

template <typename T, size_t N> class RingBuffer {
  static_assert((N & (N - 1)) == 0, "N must be a power of 2");

public:
  bool push(const T &element) {
    size_t h = head.load(std::memory_order_relaxed);
    size_t next = (h + 1) & (N - 1);

    if (next == tail.load(std::memory_order_acquire)) {
      return false;
    }

    buffer[h] = element;
    head.store(next, std::memory_order_release);
    return true;
  }

  bool pop(T &out) {
    size_t t = tail.load(std::memory_order_relaxed);
    size_t next = (t + 1) & (N - 1);

    if (t == head.load(std::memory_order_acquire)) {
      return false;
    }

    out = buffer[t];
    tail.store(next, std::memory_order_release);
    return true;
  }

  size_t size() {
    size_t h = head.load(std::memory_order_acquire);
    size_t t = tail.load(std::memory_order_acquire);
    return ((h - t) & (N - 1));
  }

private:
  alignas(64) atomic<size_t> head = 0;
  alignas(64) atomic<size_t> tail = 0;
  array<T, N> buffer;
};

#endif
