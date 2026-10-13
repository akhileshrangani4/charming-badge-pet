#include "Voice.h"
#include <M5Unified.h>
#include <math.h>
#include <string.h>
namespace audio {
void Voice::begin() {
  for (auto& b : rec_) b = (int16_t*)heap_caps_malloc(CHUNK * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  for (auto& b : play_) b = (int16_t*)heap_caps_malloc(CHUNK * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  ringCap_ = RATE * 4;   // 4 s of reply audio
  ring_ = (int16_t*)heap_caps_malloc(ringCap_ * 2, MALLOC_CAP_SPIRAM);
  M5.Speaker.setVolume(180);
}
// The mic and speaker share one codec on this board, so only one of them runs at a time.
void Voice::startRecording() {
  stopPlayback(); M5.Speaker.end();
  M5.Mic.begin();
  recording_ = true; recIdx_ = 0; recQueued_ = 0;
}
void Voice::stopRecording() {
  recording_ = false;
  while (M5.Mic.isRecording()) delay(1);
  M5.Mic.end();
  M5.Speaker.begin();
}
bool Voice::recordChunk(const int16_t*& out, size_t& n) {
  if (!recording_) return false;
  // Keep up to two slots queued; the chunk two behind the newest is complete.
  if (recQueued_ < 3 && M5.Mic.isRecording() < 2) {
    M5.Mic.record(rec_[recIdx_ % 3], CHUNK, RATE);
    recIdx_++; recQueued_++;
  }
  if (recQueued_ >= 3) { out = rec_[(recIdx_ - 3) % 3]; n = CHUNK; recQueued_--; return true; }
  return false;
}
void Voice::enqueuePlayback(const uint8_t* pcm, size_t bytes) {
  const int16_t* s = (const int16_t*)pcm; size_t n = bytes / 2;
  for (size_t i = 0; i < n; i++) {
    const size_t next = (head_ + 1) % ringCap_;
    if (next == tail_) break;   // ring full: drop the newest samples
    ring_[head_] = s[i]; head_ = next;
  }
  flushing_ = false;
}
void Voice::pumpPlayback() {
  // Start once 120 ms is buffered (jitter cushion); refill whenever fewer than 2 chunks are queued.
  const size_t want = (M5.Speaker.isPlaying(0) ? CHUNK : CHUNK * 3);
  while (M5.Speaker.isPlaying(0) < 2 && (ringCount() >= want || (flushing_ && ringCount() > 0))) {
    int16_t* buf = play_[playIdx_ % 3];
    const size_t n = ringCount() < CHUNK ? ringCount() : CHUNK;
    double acc = 0;
    for (size_t i = 0; i < n; i++) { buf[i] = ring_[tail_]; tail_ = (tail_ + 1) % ringCap_; acc += (double)buf[i] * buf[i]; }
    level_ = fminf(1.0f, sqrtf((float)(acc / (n ? n : 1))) / 6000.0f);
    M5.Speaker.playRaw(buf, n, RATE, false, 1, 0);
    playIdx_++;
  }
  if (!M5.Speaker.isPlaying(0) && ringCount() == 0) level_ = 0;
}
void Voice::flushPlayback() { flushing_ = true; }
void Voice::stopPlayback() { M5.Speaker.stop(0); head_ = tail_ = 0; level_ = 0; flushing_ = false; }
bool Voice::playing() const { return M5.Speaker.isPlaying(0) || head_ != tail_; }
}
