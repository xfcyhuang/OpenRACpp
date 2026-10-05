// UPSTREAM: OpenRA.Platforms.Default/FreeTypeFont.cs @b6fc03f L20-143(实现)
// OPT-B2:真结构体直取(third_party/FreeType/include 官方 2.14.1 头);入口经
// LoadLibraryA 运行时直连。进程级 FT_Library 一次初始化(上游 static
// library 字段的等价物)。
//
// ABI 校正(随包 freetype6.dll 的实证布局):上游 FreeTypeFont.cs L27-36 的
// 手算偏移(glyph@152/metrics@48/bitmap@152/bitmap_left@192/bitmap_top@196,
// 64 位分支)与 LLP64 头(long=4,glyph@120)不符 —— 该 DLL 按 long=8 的
// LP64 字长编译。真结构体路线下,以 `#define long long long` 宏窗把官方头
// 的 long 提升为 long long,布局即与 DLL 一致(等价于用与 DLL 同字长的编
// 译器编这份头),并以 static_assert 锁死上游六个偏移 + FT_Pos==8 —— 偏移
// 漂移在编译期即炸,上游 HACK 常量由此从"运行期魔法数"变为"编译期契约"。
// 系统头(stddef/limits/string/stdio/stdlib/setjmp/stdarg)先于宏窗经典引入
// (include guard 已置),宏窗内只剩 FreeType 自有头。
// OPT-B2: fields read straight through the real structs (the official 2.14.1
// headers of third_party/FreeType/include); the entries connect at runtime
// via LoadLibraryA. The process-wide FT_Library initializes once (the
// equivalent of upstream's static library field).
//
// ABI correction (the bundled freetype6.dll's empirically-observed layout):
// the hand-computed offsets of upstream FreeTypeFont.cs L27-36 (glyph@152/
// metrics@48/bitmap@152/bitmap_left@192/bitmap_top@196, the 64-bit branch)
// do not match the LLP64 headers (long = 4, glyph@120) — the DLL was built
// with LP64-sized longs (8 bytes). On the real-struct route, a
// `#define long long long` macro window promotes the official headers' long
// to long long, aligning the layout with the DLL (equivalent to compiling
// these headers with the DLL's own long size); static_asserts then pin the
// six upstream offsets plus FT_Pos == 8 — any layout drift explodes at
// compile time, turning the upstream HACK constants from runtime magic
// numbers into a compile-time contract. The system headers (stddef/limits/
// string/stdio/stdlib/setjmp/stdarg) are classically included before the
// macro window (their include guards are set), so only FreeType's own
// headers are seen inside it.
import std;

#include <csetjmp>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <climits>

#define FT_SIZEOF_LONG 8  // LP64 分支(与宏窗一致的预处理判定)| the LP64 branch (the preprocess decision matching the macro window)
#define long long long  // LP64 字长窗(见上)| the LP64 size window (see above)
#include <ft2build.h>
#include FT_FREETYPE_H
#undef long

// 编译期 ABI 契约:与上游 FreeTypeFont.cs L27-36 的手算偏移逐值相等
// (64 位分支),即随包 freetype6.dll 的实际布局。
// Compile-time ABI contract: value-for-value equal to the hand-computed
// offsets of FreeTypeFont.cs L27-36 (the 64-bit branch) — the bundled
// freetype6.dll's actual layout.
static_assert(sizeof(FT_Pos) == 8);
static_assert(offsetof(FT_Glyph_Metrics, width) == 0);       // MetricsWidthOffset
static_assert(offsetof(FT_Glyph_Metrics, height) == 8);      // MetricsHeightOffset
static_assert(offsetof(FT_Glyph_Metrics, horiAdvance) == 32);  // MetricsAdvanceOffset
static_assert(offsetof(FT_Bitmap, pitch) == 8);              // BitmapPitchOffset
static_assert(offsetof(FT_Bitmap, buffer) == 16);            // BitmapBufferOffset
static_assert(offsetof(FT_FaceRec, glyph) == 152);           // FaceRecGlyphOffset
static_assert(offsetof(FT_GlyphSlotRec, metrics) == 48);     // GlyphSlotMetricsOffset
static_assert(offsetof(FT_GlyphSlotRec, bitmap) == 152);     // GlyphSlotBitmapOffset
static_assert(offsetof(FT_GlyphSlotRec, bitmap_left) == 192);   // GlyphSlotBitmapLeftOffset
static_assert(offsetof(FT_GlyphSlotRec, bitmap_top) == 196);    // GlyphSlotBitmapTopOffset

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "platform/freetype_font.hpp"

namespace ora::platform {

namespace {

HMODULE hmodule_ft_ = nullptr;  // freetype6.dll(进程级)| freetype6.dll (process-lifetime)
FT_Library ptr_library_ = nullptr;  // 进程级 FT_Library(static library 字段)| the process FT_Library (the static library field)

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

constexpr FT_Int32 kFtLoadRender = 0x04;  // FT_LOAD_RENDER(FreeTypeFont.cs L25)
constexpr FT_Error kFtOk = 0;             // FreeTypeFont.cs L24 的 OK

}  // namespace

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
      static_cast<FT_UInt>(int4_size * fp4_device_scale);  // (uint)(size * deviceScale)
  if (FT_Set_Pixel_Sizes(ptr_face, uint4_scaled_size, uint4_scaled_size) != kFtOk)
    return FontGlyph{};

  if (FT_Load_Char(ptr_face, uint4_char_code, kFtLoadRender) != kFtOk)
    return FontGlyph{};

  // 提取关注的字形数据 —— 直接真结构体字段(OPT-B2:上游手算偏移 HACK 删除)
  // Extract the glyph data we care about — straight real-struct fields
  // (OPT-B2: upstream's hand-computed-offset HACK deleted).
  const FT_GlyphSlot ptr_glyph = ptr_face->glyph;

  const FT_Pos int8_metrics_width = ptr_glyph->metrics.width;
  const FT_Pos int8_metrics_height = ptr_glyph->metrics.height;
  const FT_Pos int8_metrics_advance = ptr_glyph->metrics.horiAdvance;

  const FT_Bitmap& bitmap = ptr_glyph->bitmap;
  const int int4_bitmap_pitch = bitmap.pitch;
  const FT_Byte* ptr_bitmap_buffer = bitmap.buffer;

  const int int4_bitmap_left = ptr_glyph->bitmap_left;
  const int int4_bitmap_top = ptr_glyph->bitmap_top;

  // FreeType 的 26.6 定点 → 整数:丢弃小数位(右移)
  // Convert FreeType's 26.6 fixed point to integers by discarding the
  // fractional bits (right shift).
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

}  // namespace ora::platform
