#include "blend.h"

#include <algorithm>
#include <cstddef>

namespace {

inline uint8_t clamp_u8(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return static_cast<uint8_t>(value);
}

inline std::size_t pixel_offset(int x, int y, int w) {
    return (static_cast<std::size_t>(y) * static_cast<std::size_t>(w)
            + static_cast<std::size_t>(x)) * 4U;
}

inline void copy_pixel(const uint8_t* src, uint8_t* dst) {
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

}  // namespace

void blend_over(const uint8_t* src, const uint8_t* dst, int w, int h, int x, int y, int mode) {
    if (src == nullptr || dst == nullptr || w <= 0 || h <= 0) {
        return;
    }
    if (mode < 0 || mode > 3) {
        return;
    }

    // The public API keeps dst const for compatibility, but the documented
    // contract is that the destination frame is updated in place.
    uint8_t* out = const_cast<uint8_t*>(dst);

    // Composite the source frame at (x, y).
    const int sx0 = 0;
    const int sy0 = 0;
    const int sx1 = w;
    const int sy1 = h;

    for (int sy = sy0; sy < sy1; ++sy) {
        for (int sx = sx0; sx < sx1; ++sx) {
            const int dx = sx + x;
            const int dy = sy + y;
            const std::size_t si = pixel_offset(sx, sy, w);
            const std::size_t di = pixel_offset(dx, dy, w);

            if (mode == 0) {
                copy_pixel(src + si, out + di);
                continue;
            }

            if (mode == 1) {
                const int as = src[si + 3];
                if (as == 0) {
                    continue;
                }
                if (as == 255) {
                    copy_pixel(src + si, out + di);
                    continue;
                }

                const int ad = dst[di + 3];
                const int inv = 255 - as;
                const int out_a = as + (ad * inv + 127) / 255;
                if (out_a == 0) {
                    continue;
                }

                // Straight-alpha source-over.  Work in premultiplied units
                // internally, then divide by the composited alpha to return to
                // straight alpha.
                const int den = 255 * out_a;
                for (int c = 0; c < 3; ++c) {
                    const int num = static_cast<int>(src[si + c]) * 255 * 255
                                    + static_cast<int>(dst[di + c]) * ad * inv;
                    out[di + c] = clamp_u8((num + den / 2) / den);
                }
                out[di + 3] = static_cast<uint8_t>(out_a);
                continue;
            }

            if (mode == 2) {
                for (int c = 0; c < 3; ++c) {
                    out[di + c] = clamp_u8(static_cast<int>(src[si + c])
                                           + static_cast<int>(dst[di + c]));
                }
                out[di + 3] = std::max(src[si + 3], dst[di + 3]);
                continue;
            }

            // mode == 3: normalized multiply.
            for (int c = 0; c < 3; ++c) {
                const uint8_t product = static_cast<uint8_t>(
                    static_cast<int>(src[si + c]) * static_cast<int>(dst[di + c]));
                out[di + c] = static_cast<uint8_t>(product / 255);
            }
            out[di + 3] = std::max(src[si + 3], dst[di + 3]);
        }
    }
}

void premultiply(const uint8_t* rgba, uint8_t* out, int npixels) {
    if (rgba == nullptr || out == nullptr || npixels <= 0) {
        return;
    }

    for (int i = 0; i < npixels; ++i) {
        const std::size_t p = static_cast<std::size_t>(i) * 4U;
        const int a = rgba[p + 3];
        out[p + 0] = static_cast<uint8_t>((static_cast<int>(rgba[p + 0]) * a + 127) / 255);
        out[p + 1] = static_cast<uint8_t>((static_cast<int>(rgba[p + 1]) * a + 127) / 255);
        out[p + 2] = static_cast<uint8_t>((static_cast<int>(rgba[p + 2]) * a + 127) / 255);
        out[p + 3] = static_cast<uint8_t>(a);
    }
}

void unpremultiply(const uint8_t* rgba, uint8_t* out, int npixels) {
    if (rgba == nullptr || out == nullptr || npixels <= 0) {
        return;
    }

    for (int i = 0; i < npixels; ++i) {
        const std::size_t p = static_cast<std::size_t>(i) * 4U;
        const int a = rgba[p + 3];
        if (a == 0) {
            out[p + 0] = 0;
            out[p + 1] = 0;
            out[p + 2] = 0;
        } else {
            for (int c = 0; c < 3; ++c) {
                const int value = (static_cast<int>(rgba[p + c]) * 255 + a / 2) / a;
                out[p + c] = clamp_u8(value);
            }
        }
        out[p + 3] = static_cast<uint8_t>(a);
    }
}
