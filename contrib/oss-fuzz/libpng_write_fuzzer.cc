// libgmg_write_fuzzer.cc
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#define PNG_INTERNAL
#include "png.h"

// Write-specific cleanup
#define PNG_WRITE_CLEANUP \
  if (png_handler.png_ptr) { \
    if (png_handler.row_ptr) \
      png_free(png_handler.png_ptr, png_handler.row_ptr); \
    if (png_handler.end_info_ptr) \
      png_destroy_write_struct(&png_handler.png_ptr, &png_handler.info_ptr); \
    else \
      png_destroy_write_struct(&png_handler.png_ptr, nullptr); \
  }

struct BufState {
  uint8_t* data;
  size_t capacity;
  size_t size;
};

struct PngWriteHandler {
  png_structp png_ptr = nullptr;
  png_infop info_ptr = nullptr;
  png_bytep row_ptr = nullptr;
  BufState* buf_state = nullptr;

  ~PngWriteHandler() {
    PNG_WRITE_CLEANUP
    delete buf_state;
  }
};

// Write callback for libpng
void user_write_data(png_structp png_ptr, png_bytep data, size_t length) {
  BufState* buf_state = static_cast<BufState*>(png_get_io_ptr(png_ptr));
  if (buf_state->size + length > buf_state->capacity) {
    png_error(png_ptr, "Write buffer overflow");
  }
  memcpy(buf_state->data + buf_state->size, data, length);
  buf_state->size += length;
}

void user_flush_data(png_structp) {}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 32) return 0;  // Minimal input size check

  PngWriteHandler png_handler;
  
  try {
    // Initialize write structures
    png_handler.png_ptr = png_create_write_struct(
        PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_handler.png_ptr) return 0;

    png_handler.info_ptr = png_create_info_struct(png_handler.png_ptr);
    if (!png_handler.info_ptr) return 0;

    // Set up output buffer
    png_handler.buf_state = new BufState();
    const size_t max_size = 1000000;  // Prevent OOM
    png_handler.buf_state->data = static_cast<uint8_t*>(malloc(max_size));
    png_handler.buf_state->capacity = max_size;
    png_handler.buf_state->size = 0;

    png_set_write_fn(png_handler.png_ptr, png_handler.buf_state,
                     user_write_data, user_flush_data);

    if (setjmp(png_jmpbuf(png_handler.png_ptr))) {
      return 0;
    }

    // Create synthetic image parameters from fuzzer input
    const png_uint_32 width = 64;
    const png_uint_32 height = 64;
    const int bit_depth = 8;
    const int color_type = PNG_COLOR_TYPE_RGB;

    png_set_IHDR(png_handler.png_ptr, png_handler.info_ptr,
                 width, height, bit_depth, color_type,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE,
                 PNG_FILTER_TYPE_BASE);

    // Write header
    png_write_info(png_handler.png_ptr, png_handler.info_ptr);

    // Allocate row buffer
    const png_size_t rowbytes = png_get_rowbytes(png_handler.png_ptr,
                                                png_handler.info_ptr);
    png_handler.row_ptr = png_malloc(png_handler.png_ptr, rowbytes);

    // Generate synthetic image data
    for (png_uint_32 y = 0; y < height; y++) {
      memset(png_handler.row_ptr, data[y % size], rowbytes); 
      png_write_row(png_handler.png_ptr, png_handler.row_ptr);
    }

    png_write_end(png_handler.png_ptr, nullptr);

  } catch (...) {
    PNG_WRITE_CLEANUP
    return 0;
  }

  PNG_WRITE_CLEANUP
  return 0;
}
