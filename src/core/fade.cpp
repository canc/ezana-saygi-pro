#include "fade.h"

namespace adhan {

float fade_volume(float from, float to, double elapsed_ms, double duration_ms, bool* done) {
  if (duration_ms <= 0) {
    if (done) *done = true;
    return to;
  }
  if (elapsed_ms >= duration_ms) {
    if (done) *done = true;
    return to;
  }
  if (elapsed_ms <= 0) {
    if (done) *done = false;
    return from;
  }
  double t = elapsed_ms / duration_ms;
  if (done) *done = false;
  return static_cast<float>(from + (to - from) * t);
}

}  // namespace adhan
