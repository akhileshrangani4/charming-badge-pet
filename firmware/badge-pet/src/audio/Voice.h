#pragma once
#include <stdint.h>
#include <stddef.h>
namespace audio {
constexpr uint32_t RATE = 24000;
constexpr size_t CHUNK = 960;   // 40 ms at 24 kHz
class Voice {
 public:
  void begin();
  void startRecording();
  void stopRecording();
  bool recordChunk(const int16_t*& out, size_t& n);    // next finished 40 ms chunk, if any
  void enqueuePlayback(const uint8_t* pcm, size_t bytes);
  void pumpPlayback();                                // call often: feeds the speaker from the ring buffer
  void flushPlayback();                               // play whatever is left (after say_end)
  void stopPlayback();
  bool playing() const;
  float level() const { return level_; }
 private:
  int16_t* rec_[3] = {nullptr, nullptr, nullptr}; uint8_t recIdx_ = 0, recQueued_ = 0; bool recording_ = false;
  int16_t* ring_ = nullptr; size_t ringCap_ = 0, head_ = 0, tail_ = 0;   // samples
  int16_t* play_[3] = {nullptr, nullptr, nullptr}; uint8_t playIdx_ = 0;
  bool flushing_ = false; volatile float level_ = 0;
  size_t ringCount() const { return (head_ + ringCap_ - tail_) % ringCap_; }
};
}
