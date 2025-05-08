#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#define PNG_INTERNAL
#include "png.h"

#define PNG_CLEANUP \
  if(png_handler.png_ptr) \
  { \
    if (png_handler.row_ptr) \
      png_free(png_handler.png_ptr, png_handler.row_ptr); \
    if (png_handler.end_info_ptr) \
      png_destroy_write_struct(&png_handler.png_ptr, &png_handler.end_info_ptr); \
    else \
      png_destroy_write_struct(&png_handler.png_ptr, nullptr); \
    png_handler.png_ptr = nullptr; \
    png_handler.row_ptr = nullptr; \
    png_handler.end_info_ptr = nullptr; \
  }

struct BufState {
  uint8_t* data;
  size_t bytes_left;
};

struct PngObjectHandler {
  png_infop end_info_ptr = nullptr;
  png_structp png_ptr = nullptr;
  png_voidp row_ptr = nullptr;
  BufState* buf_state = nullptr;

  ~PngObjectHandler() {
    if (row_ptr)
      png_free(png_ptr, row_ptr);
    if (end_info_ptr)
      png_destroy_write_struct(&png_ptr, &end_info_ptr);
    else
      png_destroy_write_struct(&png_ptr, nullptr);
    delete buf_state;
  }
};

void user_write_data(png_structp png_ptr, png_bytep data, size_t length) {
  BufState* buf_state = static_cast<BufState*>(png_get_io_ptr(png_ptr));
  if (length > buf_state->bytes_left) {
    png_error(png_ptr, "write error");
  }
  memcpy(buf_state->data, data, length);
  buf_state->bytes_left -= length;
  buf_state->data += length;
}

void* limited_malloc(png_structp, png_alloc_size_t size) {
  // Similar to the reader, limit memory allocation to avoid OOM.
  if (size > 8000000)
    return nullptr;

  return malloc(size);
}

void default_free(png_structp, png_voidp ptr) {
  return free(ptr);
}

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 8) {
    return 0;
  }

  std::vector<unsigned char> v(data, data + size);
  
  PngObjectHandler png_handler;
  png_handler.png_ptr = nullptr;
  png_handler.row_ptr = nullptr;
  png_handler.end_info_ptr = nullptr;

  png_handler.png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png_handler.png_ptr) {
    return 0;
  }

  png_handler.end_info_ptr = png_create_info_struct(png_handler.png_ptr);
  if (!png_handler.end_info_ptr) {
    PNG_CLEANUP
    return 0;
  }

  // Use a custom allocator that fails for large allocations to avoid OOM.
  png_set_mem_fn(png_handler.png_ptr, nullptr, limited_malloc, default_free);

  // Set output for writing to memory (can be adapted to write to a file instead).
  png_handler.buf_state = new BufState();
  png_handler.buf_state->data = (uint8_t*)malloc(size);  // Allocate memory for output data
  png_handler.buf_state->bytes_left = size;
  png_set_write_fn(png_handler.png_ptr, png_handler.buf_state, user_write_data);

  if (setjmp(png_jmpbuf(png_handler.png_ptr))) {
    PNG_CLEANUP
    return 0;
  }

  // Set up basic PNG information (create a small image for testing).
  int width = 256, height = 256;
  int bit_depth = 8;
  int color_type = PNG_COLOR_TYPE_RGB;

  png_set_IHDR(png_handler.png_ptr, png_handler.end_info_ptr, width, height, bit_depth, color_type,
               PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

  // Start writing the PNG data.
  png_write_info(png_handler.png_ptr, png_handler.end_info_ptr);

  // Write image rows (random or simple test data).
  png_handler.row_ptr = png_malloc(png_handler.png_ptr, png_get_rowbytes(png_handler.png_ptr, png_handler.end_info_ptr));

  for (int y = 0; y < height; ++y) {
    // Fill the row with random test data (this could be random bytes or anything).
    for (int x = 0; x < width; ++x) {
      png_bytep row = static_cast<png_bytep>(png_handler.row_ptr);
      row[x * 3] = static_cast<png_byte>(rand() % 256);       // R
      row[x * 3 + 1] = static_cast<png_byte>(rand() % 256);   // G
      row[x * 3 + 2] = static_cast<png_byte>(rand() % 256);   // B
    }
    png_write_row(png_handler.png_ptr, static_cast<png_bytep>(png_handler.row_ptr));
  }

  png_write_end(png_handler.png_ptr, png_handler.end_info_ptr);

  PNG_CLEANUP

  return 0;
}
