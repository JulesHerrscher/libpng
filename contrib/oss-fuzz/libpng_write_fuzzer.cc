#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#define PNG_INTERNAL
#include "png.h"

#define PNG_CLEANUP \
  if (png_handler.png_ptr) { \
    if (png_handler.info_ptr) \
      png_destroy_write_struct(&png_handler.png_ptr, &png_handler.info_ptr); \
    else \
      png_destroy_write_struct(&png_handler.png_ptr, nullptr); \
    png_handler.png_ptr = nullptr; \
    png_handler.info_ptr = nullptr; \
  }

struct BufState {
  std::vector<uint8_t> buffer;
};

struct PngObjectHandler {
  png_structp png_ptr = nullptr;
  png_infop info_ptr = nullptr;
  BufState* buf_state = nullptr;

  ~PngObjectHandler() {
    if (png_ptr) {
      if (info_ptr)
        png_destroy_write_struct(&png_ptr, &info_ptr);
      else
        png_destroy_write_struct(&png_ptr, nullptr);
    }
    delete buf_state;
  }
};

// Custom write callback: stores PNG output into buffer
void user_write_data(png_structp png_ptr, png_bytep data, size_t length) {
  BufState* buf_state = static_cast<BufState*>(png_get_io_ptr(png_ptr));
  buf_state->buffer.insert(buf_state->buffer.end(), data, data + length);
}

void user_flush_data(png_structp png_ptr) {
}

void* limited_malloc(png_structp, png_alloc_size_t size) {
  if (size > 8000000)
    return nullptr;
  return malloc(size);
}

void default_free(png_structp, png_voidp ptr) {
  free(ptr);
}


extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Minimum: 100x100 RGBA = 40,000 bytes
  constexpr int width = 100;
  constexpr int height = 100;
  constexpr int channels = 4;
  constexpr size_t min_size = width * height * channels;

  if (size < min_size)
    return 0;

  PngObjectHandler png_handler;
  png_handler.png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png_handler.png_ptr) return 0;

  png_handler.info_ptr = png_create_info_struct(png_handler.png_ptr);
  if (!png_handler.info_ptr) {
    PNG_CLEANUP
    return 0;
  }

  // Optional: fail large allocations
  png_set_mem_fn(png_handler.png_ptr, nullptr, limited_malloc, default_free);

  // Set error handling
  if (setjmp(png_jmpbuf(png_handler.png_ptr))) {
    PNG_CLEANUP
    return 0;
  }

  // Set up output buffer
  png_handler.buf_state = new BufState();
  png_set_write_fn(png_handler.png_ptr, png_handler.buf_state, user_write_data, user_flush_data);

  // PNG header
  int bit_depth = 8;
  int color_type = PNG_COLOR_TYPE_RGBA;
  png_set_IHDR(png_handler.png_ptr, png_handler.info_ptr,
               width, height, bit_depth, color_type,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

  png_write_info(png_handler.png_ptr, png_handler.info_ptr);

  // Write rows from fuzzer input
  const uint8_t* pixel_ptr = data;
  for (int y = 0; y < height; ++y) {
    png_write_row(png_handler.png_ptr, const_cast<png_bytep>(pixel_ptr));
    pixel_ptr += width * channels;
  }

  png_write_end(png_handler.png_ptr, nullptr);

  PNG_CLEANUP
  return 0;
}
