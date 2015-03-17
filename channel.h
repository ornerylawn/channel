// Channel is a wait-free ring-buffer for inter-thread
// communication. It is safe with one sender and one receiver.

#ifndef CHANNEL_H
#define CHANNEL_H

#include <atomic>
#include <cassert>
#include <cmath>

#if ATOMIC_INT_LOCK_FREE != 2
#error No guarantee that Channel is lock-free on this platform.
#endif

template <class T>
class Channel {
public:
  // Create a channel.
  explicit Channel(int capacity) : cap_(capacity), r_(0), w_(0) {
    assert(capacity >= 0);
    // Round up to next power of 2.
    int power = floor(log(capacity)/log(2)) + 1;
    size_ = 1 << power;
    size_mask_ = size_ - 1;
    buf_ = new T[size_];
  }
  ~Channel() { delete[] buf_; }

  // The capacity passed to the constructor.
  int capacity() const { return cap_; }

  // Send attempts to put an item onto the channel. It returns true if
  // it succeeded (the channel was not already full).
  bool Send(const T &item);

  // Receive attempts to take an item from the channel. It return true
  // if it succeeded (the channel was not already empty).
  bool Receive(T* item);

private:
  int cap_, size_, size_mask_;
  T *buf_;

  std::atomic<int> r_, w_;
};

template <class T>
bool Channel<T>::Send(const T &item) {
  const int w = w_.load(std::memory_order_relaxed); // we own w_
  const int r = r_.load(std::memory_order_acquire); // observe any reads
  const int nitems = w - r;
  if (nitems == cap_ || nitems + size_ == cap_) {
    return false;
  }
  buf_[w] = item;
  w_.store((w+1) & size_mask_, std::memory_order_release); // publish the write
  return true;
}

template <class T>
bool Channel<T>::Receive(T* item) {
  const int r = r_.load(std::memory_order_relaxed); // we own r_
  const int w = w_.load(std::memory_order_acquire); // observe any writes
  if (r == w) {
    return false;
  }
  *item = buf_[r];
  r_.store((r+1) & size_mask_, std::memory_order_release); // publish the read
  return true;
}

#endif  // CHANNEL_H
