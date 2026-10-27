// SPDX-License-Identifier: GPL-3.0-or-later
#include "quick_picture.h"
#include "amiibo_file.h"
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>

namespace {
// A PNG can advertise a small IHDR but expand a huge compressed stream. Bound the decoder's
// actual aggregate allocations too, rather than rely only on dimensions or compressed bytes.
constexpr size_t kDecodeBudget = 12 * 1024 * 1024;
thread_local size_t allocated{};
struct alignas(std::max_align_t) Allocation { size_t size; };
void* PngAllocate(size_t size) {
    if (size > kDecodeBudget - allocated) return nullptr;
    auto* block = static_cast<Allocation*>(std::malloc(sizeof(Allocation) + size));
    if (!block) return nullptr;
    block->size = size; allocated += size;
    return block + 1;
}
void PngFree(void* pointer) {
    if (!pointer) return;
    auto* block = static_cast<Allocation*>(pointer) - 1;
    allocated -= block->size;
    std::free(block);
}
void* PngResize(void* pointer, size_t size) {
    if (!pointer) return PngAllocate(size);
    auto* block = static_cast<Allocation*>(pointer) - 1;
    const auto old_size = block->size;
    if (size > kDecodeBudget - (allocated - old_size)) return nullptr;
    auto* resized = static_cast<Allocation*>(std::realloc(block, sizeof(Allocation) + size));
    if (!resized) return nullptr;
    resized->size = size; allocated = allocated - old_size + size;
    return resized + 1;
}
} // namespace

// Private copy: do not change Eden's unrelated cover/texture decoders or export stb symbols.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 1024
#define STBI_MALLOC(size) PngAllocate(size)
#define STBI_REALLOC(pointer, size) PngResize(pointer, size)
#define STBI_FREE(pointer) PngFree(pointer)
#include <stb_image.h>

namespace Eden {
std::shared_ptr<const QuickPicture> LoadQuickPicture(const std::string& path) try {
    if (path.empty()) return {};
    const int fd = OpenAmiiboPath(path, O_RDONLY);
    if (fd < 0) return {};
    struct Close { int fd; ~Close() { close(fd); } } close_fd{fd};
    struct stat info {};
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 33 ||
        info.st_size > 4 * 1024 * 1024) return {};
    std::vector<unsigned char> encoded(static_cast<size_t>(info.st_size));
    size_t done = 0;
    while (done < encoded.size()) {
        const auto count = pread(fd, encoded.data() + done, encoded.size() - done, done);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return {};
        done += static_cast<size_t>(count);
    }
    constexpr std::array<unsigned char, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
    if (!std::equal(signature.begin(), signature.end(), encoded.begin())) return {};
    int width{}, height{}, channels{};
    const auto count = static_cast<int>(encoded.size());
    if (!stbi_info_from_memory(encoded.data(), count, &width, &height, &channels) ||
        width <= 0 || height <= 0 || width > 1024 || height > 1024) return {};
    auto* pixels = stbi_load_from_memory(encoded.data(), count, &width, &height, &channels, 4);
    struct Free { unsigned char* pixels; ~Free() { stbi_image_free(pixels); } } free_pixels{pixels};
    if (!pixels) return {};
    auto picture = std::make_shared<QuickPicture>();
    const double scale = static_cast<double>(QuickPicture::kSide) / std::max(width, height);
    const int out_w = std::max(1, static_cast<int>(std::lround(width * scale)));
    const int out_h = std::max(1, static_cast<int>(std::lround(height * scale)));
    const int left = (QuickPicture::kSide - out_w) / 2, top = (QuickPicture::kSide - out_h) / 2;
    // Alpha-weighted bilinear filtering preserves transparent edges on the existing dark panel.
    for (int y = 0; y < out_h; ++y) {
        const double sy = std::max(0.0, (y + 0.5) * height / out_h - 0.5);
        const int y0 = std::min(height - 1, static_cast<int>(sy)), y1 = std::min(height - 1, y0 + 1);
        const double fy = sy - y0;
        for (int x = 0; x < out_w; ++x) {
            const double sx = std::max(0.0, (x + 0.5) * width / out_w - 0.5);
            const int x0 = std::min(width - 1, static_cast<int>(sx)), x1 = std::min(width - 1, x0 + 1);
            const double fx = sx - x0;
            const double weights[]{(1 - fx) * (1 - fy), fx * (1 - fy), (1 - fx) * fy, fx * fy};
            const int positions[]{(y0 * width + x0) * 4, (y0 * width + x1) * 4,
                                  (y1 * width + x0) * 4, (y1 * width + x1) * 4};
            double sums[4]{};
            for (int n = 0; n < 4; ++n) {
                const double alpha = weights[n] * pixels[positions[n] + 3];
                sums[3] += alpha;
                for (int c = 0; c < 3; ++c) sums[c] += alpha * pixels[positions[n] + c];
            }
            auto* target = picture->rgba.data() + ((y + top) * QuickPicture::kSide + x + left) * 4;
            target[3] = static_cast<uint8_t>(std::lround(sums[3]));
            for (int c = 0; c < 3; ++c)
                target[c] = sums[3] > 0 ? static_cast<uint8_t>(std::lround(sums[c] / sums[3])) : 0;
        }
    }
    return picture;
} catch (const std::bad_alloc&) {
    return {}; // an optional preview must not end a guest session when memory is tight
}
} // namespace Eden
