#ifndef MINI_FRAME_BLEND_BLEND_H
#define MINI_FRAME_BLEND_BLEND_H

#include <cstdint>

// RGBA8 frame compositing helpers.
//
// All pixel buffers are tightly packed RGBA8 (4 bytes per pixel) and use
// straight (non-premultiplied) alpha.  A frame is w * h pixels.  blend_over()
// places src at offset (x, y) on top of a destination frame that has the same
// w * h dimensions, clipping any part that falls outside the destination.
//
// mode:
//   0  passthrough: copy src pixels to dst
//   1  source-over: straight-alpha "over" compositing
//   2  add:         saturating per-channel addition (alpha = max)
//   3  multiply:    normalized per-channel multiply (alpha = max)
void blend_over(const uint8_t* src, const uint8_t* dst, int w, int h, int x, int y, int mode);

// Convert npixels straight-alpha RGBA pixels to premultiplied RGBA.
void premultiply(const uint8_t* rgba, uint8_t* out, int npixels);

// Convert npixels premultiplied RGBA pixels back to straight alpha.
void unpremultiply(const uint8_t* rgba, uint8_t* out, int npixels);

#endif  // MINI_FRAME_BLEND_BLEND_H
