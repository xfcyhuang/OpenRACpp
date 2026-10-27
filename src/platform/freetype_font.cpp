import std;

#include <csetjmp>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>

#define FT_SIZEOF_LONG 8
#define long long long
#include <ft2build.h>
#include FT_FREETYPE_H
#undef long

static_assert(sizeof(FT_Pos) == 8);
static_assert(offsetof(FT_Glyph_Metrics, width) == 0);
static_assert(offsetof(FT_Glyph_Metrics, height) == 8);
static_assert(offsetof(FT_Glyph_Metrics, horiAdvance) == 32);
static_assert(offsetof(FT_Bitmap, pitch) == 8);
static_assert(offsetof(FT_Bitmap, buffer) == 16);
static_assert(offsetof(FT_FaceRec, glyph) == 152);
static_assert(offsetof(FT_GlyphSlotRec, metrics) == 48);
static_assert(offsetof(FT_GlyphSlotRec, bitmap) == 152);
static_assert(offsetof(FT_GlyphSlotRec, bitmap_left) == 192);
static_assert(offsetof(FT_GlyphSlotRec, bitmap_top) == 196);

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "platform/freetype_font.hpp"

namespace ora::platform {

namespace {

HMODULE hmodule_ft_ = nullptr;
FT_Library ptr_library_ = nullptr;

void* DefaultFreetypeLoad(const char* str_name) {
  if (hmodule_ft_ == nullptr)
    hmodule_ft_ = LoadLibraryA("freetype6.dll");
  if (hmodule_ft_ == nullptr)
    return nullptr;
  return reinterpret_cast<void*>(GetProcAddress(hmodule_ft_, str_name));
}

#define ORA_FT_DECL(name, sig) using PFN_##name = sig; inline PFN_##name name = nullptr;

ORA_FT_DECL(FT_Init_FreeType, FT_Error (*)(FT_Library*))
ORA_FT_DECL(FT_New_Memory_Face, FT_Error (*)(FT_Library, const FT_Byte*, FT_Long, FT_Long, FT_Face*))
ORA_FT_DECL(FT_Done_Face, FT_Error (*)(FT_Face))
ORA_FT_DECL(FT_Set_Pixel_Sizes, FT_Error (*)(FT_Face, FT_UInt, FT_UInt))
ORA_FT_DECL(FT_Load_Char, FT_Error (*)(FT_Face, FT_ULong, FT_Int32))

#undef ORA_FT_DECL

constexpr FT_Int32 kFtLoadRender = 0x04;
constexpr FT_Error kFtOk = 0;

}

FreetypeLoadResult LoadFreetype(void* (*fn_load)(const char* name)) {
  struct Entry {
    const char* str_name;
    void** pptr_fn;
  };

  const Entry arr_entries[] = {
      {"FT_Init_FreeType", reinterpret_cast<void**>(&FT_Init_FreeType)},
      {"FT_New_Memory_Face", reinterpret_cast<void**>(&FT_New_Memory_Face)},
      {"FT_Done_Face", reinterpret_cast<void**>(&FT_Done_Face)},
      {"FT_Set_Pixel_Sizes", reinterpret_cast<void**>(&FT_Set_Pixel_Sizes)},
      {"FT_Load_Char", reinterpret_cast<void**>(&FT_Load_Char)},
  };

  for (const Entry& entry : arr_entries) {
    *entry.pptr_fn = fn_load(entry.str_name);
    if (*entry.pptr_fn == nullptr)
      return {false, entry.str_name};
  }
  return {true, nullptr};
}

FreetypeLoadResult LoadFreetypeDefault() {
  return LoadFreetype(&DefaultFreetypeLoad);
}

bool FreetypeReady() {
  return FT_Init_FreeType != nullptr && FT_New_Memory_Face != nullptr &&
         FT_Done_Face != nullptr && FT_Set_Pixel_Sizes != nullptr && FT_Load_Char != nullptr;
}

FreeTypeFont::FreeTypeFont(std::span<const std::uint8_t> span_data)
    : vec_data_{span_data.begin(), span_data.end()} {
  if (ptr_library_ == nullptr && FT_Init_FreeType(&ptr_library_) != kFtOk)
    throw std::runtime_error{"Failed to initialize FreeType"};

  if (FT_New_Memory_Face(ptr_library_, vec_data_.data(),
                         static_cast<FT_Long>(vec_data_.size()), 0,
                         reinterpret_cast<FT_Face*>(&ptr_face_)) != kFtOk)
    throw std::runtime_error{"Failed to initialize font"};
}

FreeTypeFont::FreeTypeFont(FreeTypeFont&& other) noexcept
    : vec_data_{std::move(other.vec_data_)}, ptr_face_{other.ptr_face_} {
  other.ptr_face_ = nullptr;
}

FreeTypeFont::~FreeTypeFont() {
  if (ptr_face_ != nullptr) {
    FT_Done_Face(reinterpret_cast<FT_Face>(ptr_face_));
    ptr_face_ = nullptr;
  }
}

FontGlyph FreeTypeFont::CreateGlyph(std::uint32_t uint4_char_code, int int4_size,
                                    float fp4_device_scale) {
  const FT_Face ptr_face = reinterpret_cast<FT_Face>(ptr_face_);

  const FT_UInt uint4_scaled_size =
      static_cast<FT_UInt>(int4_size * fp4_device_scale);
  if (FT_Set_Pixel_Sizes(ptr_face, uint4_scaled_size, uint4_scaled_size) != kFtOk)
    return FontGlyph{};

  if (FT_Load_Char(ptr_face, uint4_char_code, kFtLoadRender) != kFtOk)
    return FontGlyph{};

  const FT_GlyphSlot ptr_glyph = ptr_face->glyph;

  const FT_Pos int8_metrics_width = ptr_glyph->metrics.width;
  const FT_Pos int8_metrics_height = ptr_glyph->metrics.height;
  const FT_Pos int8_metrics_advance = ptr_glyph->metrics.horiAdvance;

  const FT_Bitmap& bitmap = ptr_glyph->bitmap;
  const int int4_bitmap_pitch = bitmap.pitch;
  const FT_Byte* ptr_bitmap_buffer = bitmap.buffer;

  const int int4_bitmap_left = ptr_glyph->bitmap_left;
  const int int4_bitmap_top = ptr_glyph->bitmap_top;

  FontGlyph glyph;
  glyph.int4_width = static_cast<std::int32_t>(int8_metrics_width) >> 6;
  glyph.int4_height = static_cast<std::int32_t>(int8_metrics_height) >> 6;
  glyph.fp4_advance =
      static_cast<float>(static_cast<std::int32_t>(int8_metrics_advance) >> 6);
  glyph.offset = int2{int4_bitmap_left, -int4_bitmap_top};
  glyph.vec_data.assign(static_cast<std::size_t>(glyph.int4_width) *
                            static_cast<std::size_t>(glyph.int4_height),
                        0);

  const FT_Byte* ptr_row = ptr_bitmap_buffer;
  std::size_t int4_k = 0;
  for (int int4_j = 0; int4_j < glyph.int4_height; int4_j++) {
    for (int int4_i = 0; int4_i < glyph.int4_width; int4_i++)
      glyph.vec_data[int4_k++] = ptr_row[int4_i];

    ptr_row += int4_bitmap_pitch;
  }

  return glyph;
}

}
