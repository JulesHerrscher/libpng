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


void user_write_data(png_structp png_ptr, png_bytep data, size_t length) {
  BufState* buf_state = static_cast<BufState*>(png_get_io_ptr(png_ptr));
  buf_state->buffer.insert(buf_state->buffer.end(), data, data + length);
}

void user_flush_data(png_structp /*png_ptr*/) {
  // No-op
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  constexpr int width = 100;
  constexpr int height = 100;
  constexpr int channels = 4; 
  constexpr size_t row_size = width * channels;
  constexpr size_t total_bytes = row_size * height;

  if (size < total_bytes) return 0;

  // Use only as much input as needed
  const uint8_t* pixel_ptr = data;

  //PngObjectHandler png_handler;

  /*png_handler.png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png_handler.png_ptr) return 0;*/

  return 0;
}
