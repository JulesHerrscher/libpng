#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <vector>
#include <sstream>

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
  std::ostringstream buffer;
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
  buf_state->buffer.write(reinterpret_cast<const char*>(data), length);
}

void user_flush_data(png_structp png_ptr) {
  // No-op for this example.
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
  fprintf(stderr, "[DEBUG] Called with size = %zu, data = %p\n", size, data);
  if (size < 8) return 0;  // need enough input to fill one frame

  PngObjectHandler png_handler;
  png_handler.png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png_handler.png_ptr) return 0;

  png_handler.info_ptr = png_create_info_struct(png_handler.png_ptr);
  if (!png_handler.info_ptr) {
    PNG_CLEANUP
    return 0;
  }

  png_handler.buf_state = new BufState();
  png_set_write_fn(png_handler.png_ptr, png_handler.buf_state, user_write_data, user_flush_data);

  if (setjmp(png_jmpbuf(png_handler.png_ptr))) {
    PNG_CLEANUP
    return 0;
  }

  png_uint_32 width = 100;
  png_uint_32 height = 100;
  int bit_depth = 8;
  int color_type = PNG_COLOR_TYPE_RGBA;

  png_set_IHDR(png_handler.png_ptr, png_handler.info_ptr, width, height,
               bit_depth, color_type, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

  png_write_info(png_handler.png_ptr, png_handler.info_ptr);

  const uint8_t* ptr = data;
  for (png_uint_32 y = 0; y < height; ++y) {
    png_write_row(png_handler.png_ptr, const_cast<png_bytep>(ptr));
    ptr += width * 4;
  }

  png_write_end(png_handler.png_ptr, nullptr);
  PNG_CLEANUP
  return 0;
}
