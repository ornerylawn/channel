// This is a .wav file player, which illustrates the use of a
// Channel. On an audio thread, you want to avoid any operations that
// may take a non-deterministic amount of time, and in doing so avoid
// skipping audio. Which means the only thing we can really do is read
// and write from shared memory, and let another thread do all of the
// non-deterministic stuff. To do it safely without blocking, you need
// to use memory barriers and atomic operations. A Channel
// encapsulates that idea into an easy to use interface.

#include <iostream>

#include "portaudio.h"
#include "sndfile.h"
#include "../channel.h"

using std::cout;
using std::endl;

const int kFramesPerBuffer = 2048;
const int kSampleRate = 44100;
const int kChannels = 2; // stereo sound

const int kChunks = 10;

// We don't want the audio thread to block, so we read chunks of audio
// from a file on the main thread, and then send them to the audio
// thread over this channel.
Channel<float*> filledChunks(kChunks);

// To reuse the memory (and to not block on the delete operator), the
// chunk is sent back to the main thread over this channel. The
// channel must be big enough to hold all of the chunks in flight
// because we want the audio thread to always be able to send on the
// first try.
Channel<float*> emptyChunks(kChunks);

int callback(const void* input, void* output, unsigned long frames,
             const PaStreamCallbackTimeInfo* timeInfo,
             PaStreamCallbackFlags flags, void *data) {
  float* out = reinterpret_cast<float*>(output);
  // Make sure the audio library is using the same size buffers as us.
  assert(frames == kFramesPerBuffer);
  // Get the next chunk from the channel.
  float* chunk;
  if (filledChunks.Receive(&chunk)) {
    // Copy into the sound card's buffer.
    memcpy(out, chunk, frames * kChannels * sizeof(float));
    // Send the chunk back to the main thread for reuse.
    emptyChunks.Send(chunk);
  } else {
    // The channel was empty, write zeros instead.
    memset(out, 0.0, frames * kChannels * sizeof(float));
  }
  return paContinue;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    cout << "Usage: " << argv[0] << " foo.wav\n";
    return 1;
  }
  // Open the audio file.
  SF_INFO info;
  info.format = 0;
  SNDFILE* f = sf_open(argv[1], SFM_READ, &info);
  if (f == NULL) {
    cout << sf_strerror(NULL) << endl;
    return 1;
  }
  // Initialize the audio library.
  PaError err = Pa_Initialize();
  if (err != paNoError) {
    cout << Pa_GetErrorText(err) << endl;
    return 1;
  }
  // Initialize the emptyChunks channel with a number of reusable
  // chunks.
  for (int i = 0; i < kChunks; i++) {
    float *chunk = new float[kFramesPerBuffer * kChannels];
    memset(chunk, 0, kFramesPerBuffer * kChannels * sizeof(float));
    emptyChunks.Send(chunk);
  }
  // Open an output stream.
  PaStream *stream;
  err = Pa_OpenDefaultStream(&stream, 0, kChannels, paFloat32, kSampleRate,
                             kFramesPerBuffer, callback, NULL);
  if (err != paNoError) {
    cout << Pa_GetErrorText(err) << endl;
    return 1;
  }
  // Start the stream.
  err = Pa_StartStream(stream);
  if (err != paNoError) {
    cout << Pa_GetErrorText(err) << endl;
    return 1;
  }
  // Copy from the file to the channel.
  int n;
  do {
    // Wait for a reusable chunk.
    float *chunk;
    while (!emptyChunks.Receive(&chunk)) {
      Pa_Sleep(10);
    }
    // Read next chunk from file.
    n = sf_readf_float(f, chunk, kFramesPerBuffer);
    // Fill anything left over with zeros.
    int left = kFramesPerBuffer - n;
    memset(chunk + (n * kChannels), 0, left * kChannels * sizeof(float));
    // Send to the audio thread.
    while (!filledChunks.Send(chunk)) {
      Pa_Sleep(10);
    }
  } while (n == kFramesPerBuffer);
  // Allow audio to play a little longer.
  Pa_Sleep(1000);
  // Clean up.
  for (int i = 0; i < kChunks; i++) {
    float* chunk;
    while (!emptyChunks.Receive(&chunk)) {
      Pa_Sleep(10);
    }
    delete[] chunk;
  }
  Pa_StopStream(stream);
  Pa_CloseStream(stream);
  Pa_Terminate();
  sf_close(f);
  return 0;
}
