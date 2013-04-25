// Channel is an inter-thread communication queue. It is lock-free to
// ensure that all operations are time-deterministic. It is only safe
// with one sender and one receiver.

// Note: The capacity must be specified as one less than a power of 2
// (e.g. 31). This allows for some optimization because the buffer's
// size is one more than the capacity.

#ifndef CHANNEL_H
#define CHANNEL_H

#include <cassert>

template <class T>
class Channel {
public:
  // Create a channel with capacity n. The channel actually has n + 1
  // elements but one of them is a dummy.
  explicit Channel(int n)
    : n_(n + 1), size_mask_(n), buf_(new T[n + 1]), r_(0), w_(0) {
    assert((n & (n + 1)) == 0);
  }
  ~Channel() { delete[] buf_; }

  // The capacity passed to the constructor.
  int capacity() const { return n_ - 1; }

  // Returns true if the item was put onto the channel (the channel
  // was not already full).
  bool Send(const T &item);

  // Returns true if an item was taken from the channel (the channel
  // was not already empty).
  bool Receive(T* item);

private:
  int n_;
  int size_mask_;
  T *buf_;
  // r_ and w_ are volatile here to ensure that the writer doesn't use
  // cached values of r_ and the reader doesn't use cached values of w_.
  // Is it really necessary? Who knows?
  volatile int r_;
  volatile int w_;
};

template <class T>
bool Channel<T>::Send(const T &t) {
  int w = w_;
  int next = (w + 1) & size_mask_;
  int r = r_;
  // We need a read-acquire here to prevent the following check from
  // using old values of w_ and r_.
  __sync_synchronize();
  // Is the buffer full?
  if (next == r) {
    return false;
  }
  buf_[w] = t;
  // We need a write-release here to prevent the other thread from
  // seeing w_ change before buf_.
  __sync_synchronize();
  // We need an atomic write here to prevent the other thread from
  // seeing a half-updated w_. This should always work, first try :)
  while (!__sync_bool_compare_and_swap(&w_, w, next)) ;
  return true;
}

template <class T>
bool Channel<T>::Receive(T* item) {
  int r = r_;
  int w = w_;
  // We need a read-acquire here to prevent the following check from
  // using old values of w_ and r_.
  __sync_synchronize();
  // Is the buffer empty?
  if (r == w) {
    return false;
  }
  *item = buf_[r];
  int next = (r + 1) & size_mask_;
  // We need a full memory barrier here to prevent the other thread
  // from seeing r_ change before the read from buf_ is done.
  __sync_synchronize();
  // We need an atomic write here to prevent the other thread from
  // seeing a half-updated r_. This should always work, first try :)
  while (!__sync_bool_compare_and_swap(&r_, r, next));
  return true;
}

#endif  // CHANNEL_H
