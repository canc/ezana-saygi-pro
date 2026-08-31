#pragma once

namespace adhan {

// Linear interpolation. t is 0..1. Returns `to` when t >= 1.
float fade_volume(float from, float to, double elapsed_ms, double duration_ms, bool* done);

}  // namespace adhan
