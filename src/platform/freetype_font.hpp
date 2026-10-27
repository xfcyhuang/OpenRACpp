#pragma once
import std;

#include "core/int2.hpp"
#include "platform/ifont.hpp"

namespace ora::platform {

class FreeTypeFont : public IFont {
 public:

  explicit FreeTypeFont(std::span<const std::uint8_t> span_data);

  FreeTypeFont(const FreeTypeFont&) = delete;
  FreeTypeFont& operator=(const FreeTypeFont&) = delete;
  FreeTypeFont(FreeTypeFont&&) noexcept;
  FreeTypeFont& operator=(FreeTypeFont&&) = delete;
  ~FreeTypeFont();

  [[nodiscard]] FontGlyph CreateGlyph(std::uint32_t uint4_char_code, int int4_size,
                                      float fp4_device_scale) override;

 private:
  std::vector<std::uint8_t> vec_data_;
  void* ptr_face_ = nullptr;
};

struct FreetypeLoadResult {
  bool b_ok;
  const char* str_missing;
};

FreetypeLoadResult LoadFreetype(void* (*fn_load)(const char* name));

FreetypeLoadResult LoadFreetypeDefault();

[[nodiscard]] bool FreetypeReady();

}
