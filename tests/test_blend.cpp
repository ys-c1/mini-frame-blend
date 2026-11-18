#include "blend.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

int g_checks = 0;
int g_failures = 0;

void report(bool ok, const char* expression, const char* file, int line) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::cerr << "FAIL: " << expression << " (" << file << ":" << line << ")\n";
    }
}

#define CHECK(condition) report(static_cast<bool>(condition), #condition, __FILE__, __LINE__)

bool pixel_is(const std::vector<uint8_t>& buffer, int w, int x, int y,
              uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    const std::size_t offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(w)
                                + static_cast<std::size_t>(x)) * 4U;
    return buffer[offset + 0] == r && buffer[offset + 1] == g
           && buffer[offset + 2] == b && buffer[offset + 3] == a;
}

void test_source_over_semi_transparent() {
    // Semi-transparent red over opaque blue.
    std::vector<uint8_t> src{255, 0, 0, 128};
    std::vector<uint8_t> dst{0, 0, 255, 255};

    blend_over(src.data(), dst.data(), 1, 1, 0, 0, 1);

    CHECK(dst[3] == 255);
    CHECK(dst[0] >= 120 && dst[0] <= 136);
    CHECK(dst[2] >= 119 && dst[2] <= 135);
    CHECK(dst[1] == 0);
    CHECK(dst[0] > 0 && dst[0] < 255);
    CHECK(dst[2] > 0 && dst[2] < 255);
}

void test_opaque_source_replaces_destination() {
    const std::vector<uint8_t> src{10, 20, 30, 255};
    std::vector<uint8_t> dst{200, 100, 50, 255};

    blend_over(src.data(), dst.data(), 1, 1, 0, 0, 1);

    for (int c = 0; c < 4; ++c) {
        CHECK(dst[c] == src[c]);
    }
}

void test_transparent_source_keeps_destination() {
    const std::vector<uint8_t> src{99, 88, 77, 0};
    std::vector<uint8_t> dst{1, 2, 3, 4};
    const std::vector<uint8_t> original = dst;

    blend_over(src.data(), dst.data(), 1, 1, 0, 0, 1);

    for (int c = 0; c < 4; ++c) {
        CHECK(dst[c] == original[c]);
    }
}

void test_premultiply_roundtrip() {
    int max_error = 0;
    bool alpha_preserved = true;

    // 8-bit premultiplication is inherently lossy at very small alpha.  The
    // round-trip guarantee exercised here covers alpha >= 128.
    for (int a = 128; a <= 255; ++a) {
        for (int value = 0; value < 256; ++value) {
            const uint8_t straight[4] = {
                static_cast<uint8_t>(value),
                static_cast<uint8_t>(255 - value),
                static_cast<uint8_t>((value * 73) & 0xFF),
                static_cast<uint8_t>(a),
            };
            uint8_t premultiplied[4] = {};
            uint8_t restored[4] = {};

            premultiply(straight, premultiplied, 1);
            unpremultiply(premultiplied, restored, 1);

            if (restored[3] != a) {
                alpha_preserved = false;
            }
            for (int c = 0; c < 3; ++c) {
                max_error = std::max(max_error, std::abs(static_cast<int>(restored[c])
                                                          - static_cast<int>(straight[c])));
            }
        }
    }

    CHECK(max_error <= 1);
    CHECK(alpha_preserved);
}

void test_clipping_and_guard_bytes() {
    const int w = 4;
    const int h = 4;
    const int guard_pixels = 8;
    const int frame_pixels = w * h;

    // The guard area simulates memory just past the logical w*h destination
    // frame.  A non-clipping implementation will write into it.
    std::vector<uint8_t> dst(static_cast<std::size_t>(frame_pixels + guard_pixels) * 4U, 0xAB);
    for (int p = 0; p < frame_pixels; ++p) {
        dst[p * 4 + 0] = 0;
        dst[p * 4 + 1] = 0;
        dst[p * 4 + 2] = 255;
        dst[p * 4 + 3] = 255;
    }
    std::vector<uint8_t> src(static_cast<std::size_t>(frame_pixels) * 4U);
    for (int p = 0; p < frame_pixels; ++p) {
        src[p * 4 + 0] = 255;
        src[p * 4 + 1] = 0;
        src[p * 4 + 2] = 0;
        src[p * 4 + 3] = 255;
    }

    // Offset (1, 1): only source columns/rows 0..2 fit in the destination.
    blend_over(src.data(), dst.data(), w, h, 1, 1, 1);

    int dirty_guard = 0;
    for (int p = frame_pixels; p < frame_pixels + guard_pixels; ++p) {
        for (int c = 0; c < 4; ++c) {
            if (dst[static_cast<std::size_t>(p) * 4U + c] != 0xAB) {
                ++dirty_guard;
            }
        }
    }
    CHECK(dirty_guard == 0);
    CHECK(pixel_is(dst, w, 1, 1, 255, 0, 0, 255));
    CHECK(pixel_is(dst, w, 3, 3, 255, 0, 0, 255));
    CHECK(pixel_is(dst, w, 0, 0, 0, 0, 255, 255));
    CHECK(pixel_is(dst, w, 3, 0, 0, 0, 255, 255));
    CHECK(pixel_is(dst, w, 0, 3, 0, 0, 255, 255));
}

void test_passthrough_mode() {
    const std::vector<uint8_t> src{11, 22, 33, 44};
    std::vector<uint8_t> dst{1, 2, 3, 4};

    blend_over(src.data(), dst.data(), 1, 1, 0, 0, 0);

    for (int c = 0; c < 4; ++c) {
        CHECK(dst[c] == src[c]);
    }
}

void test_add_clamps_to_255() {
    const std::vector<uint8_t> src{200, 10, 250, 255};
    std::vector<uint8_t> dst{100, 250, 20, 255};

    blend_over(src.data(), dst.data(), 1, 1, 0, 0, 2);

    CHECK(dst[0] == 255);
    CHECK(dst[1] == 255);
    CHECK(dst[2] == 255);
    CHECK(dst[3] == 255);
}

void test_multiply_clamps_to_255() {
    {
        const std::vector<uint8_t> src{200, 128, 255, 255};
        std::vector<uint8_t> dst{200, 128, 2, 255};

        blend_over(src.data(), dst.data(), 1, 1, 0, 0, 3);

        CHECK(dst[0] == 157);
        CHECK(dst[1] == 64);
        CHECK(dst[2] == 2);
        CHECK(dst[3] == 255);
    }
    {
        const std::vector<uint8_t> src{255, 255, 255, 255};
        std::vector<uint8_t> dst{255, 255, 255, 255};

        blend_over(src.data(), dst.data(), 1, 1, 0, 0, 3);

        CHECK(dst[0] == 255);
        CHECK(dst[1] == 255);
        CHECK(dst[2] == 255);
        CHECK(dst[3] == 255);
    }
}

}  // namespace

int main() {
    std::cout << "test_blend: starting\n";

    test_source_over_semi_transparent();
    test_opaque_source_replaces_destination();
    test_transparent_source_keeps_destination();
    test_premultiply_roundtrip();
    test_clipping_and_guard_bytes();
    test_passthrough_mode();
    test_add_clamps_to_255();
    test_multiply_clamps_to_255();

    std::cout << "test_blend: checks=" << g_checks
              << " failures=" << g_failures << '\n';
    return g_failures == 0 ? 0 : 1;
}
