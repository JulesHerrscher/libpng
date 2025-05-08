// libpng_write_fuzzer.cc
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include "png.h"

// Maximum dimensions to prevent excessive memory usage
constexpr uint32_t kMaxWidth = 1024;
constexpr uint32_t kMaxHeight = 1024;

// Custom write function to store output in memory
void user_write_data(png_structp png_ptr, png_bytep data, png_size_t length) {
    auto* output = static_cast<std::vector<uint8_t>*>(png_get_io_ptr(png_ptr));
    output->insert(output->end(), data, data + length);
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Minimum input size to extract initial parameters
    if (size < 10) return 0;

    // Extract initial parameters from input
    uint32_t width = *reinterpret_cast<const uint32_t*>(data);
    uint32_t height = *reinterpret_cast<const uint32_t*>(data + 4);
    int color_type = data[8] % 6;  // 0-5 corresponding to valid color types
    int bit_depth = data[9] % 17;  // 0-16 (valid values vary by color type)
    data += 10;
    size -= 10;

    // Clamp dimensions to prevent OOM
    width = std::min(width & 0x7FFFFFFF, kMaxWidth);  // Clear high bit to avoid negative
    height = std::min(height & 0x7FFFFFFF, kMaxHeight);
    if (width == 0) width = 1;
    if (height == 0) height = 1;

    // Validate and adjust color type
    switch (color_type) {
        case 0: color_type = PNG_COLOR_TYPE_GRAY; break;
        case 2: color_type = PNG_COLOR_TYPE_RGB; break;
        case 3: color_type = PNG_COLOR_TYPE_PALETTE; break;
        case 4: color_type = PNG_COLOR_TYPE_GRAY_ALPHA; break;
        case 6: color_type = PNG_COLOR_TYPE_RGBA; break;
        default: color_type = PNG_COLOR_TYPE_RGB;
    }

    // Validate and adjust bit depth
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE) {
        const int valid[] = {1, 2, 4, 8, 16};
        bit_depth = valid[bit_depth % 5];
    } else if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_RGBA) {
        bit_depth = (bit_depth & 8) ? 8 : 16;
    } else {  // GRAY_ALPHA
        bit_depth = 8;
    }

    // Initialize PNG write structures
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) return 0;

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, nullptr);
        return 0;
    }

    // Set up error handling
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return 0;
    }

    // Set up memory output
    std::vector<uint8_t> output;
    png_set_write_fn(png_ptr, &output, user_write_data, nullptr);

    // Set IHDR with validated parameters
    png_set_IHDR(png_ptr, info_ptr, width, height, bit_depth, color_type,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    // Add dummy PLTE for palette images
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_color palette[256];
        for (int i = 0; i < 256; ++i) {
            palette[i].red = palette[i].green = palette[i].blue = i;
        }
        png_set_PLTE(png_ptr, info_ptr, palette, 256);
    }

    // Write header
    png_write_info(png_ptr, info_ptr);

    // Prepare row buffer
    const png_size_t row_bytes = png_get_rowbytes(png_ptr, info_ptr);
    std::vector<png_byte> row(row_bytes);

    // Generate row data from input (cycle if needed)
    for (png_uint_32 y = 0; y < height; ++y) {
        for (size_t i = 0; i < row_bytes; ++i) {
            if (size == 0) {
                data = data - size;  // Reset to start of pixel data
                size = row_bytes * height;
            }
            row[i] = size ? *data++ : 0;
            if (size) --size;
        }
        png_write_row(png_ptr, row.data());
    }

    // Finalize write
    png_write_end(png_ptr, info_ptr);

    // Cleanup
    png_destroy_write_struct(&png_ptr, &info_ptr);

    return 0;
}