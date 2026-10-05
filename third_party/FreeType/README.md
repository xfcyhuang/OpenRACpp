# FreeType 2.14.1(头文件 + 运行时)| FreeType 2.14.1 (headers + runtime)

- `include/`(ft2build.h + freetype/**):FreeType **2.14.1** 官方源码包
  (https://freetype.org,tag VER-2-14-1)的 `include/` 子树,逐字节未改。
- `bin/freetype6.dll`:上游 OpenRA 仓库 `bin/freetype6.dll` 的副本(x64,
  版本资源 2.14.1)—— **与 C# oracle(golden_gen)实际加载的是同一份二进制**,
  这是字形黄金逐字节对拍成立的前提(两语言对同一 DLL 的相同调用序列)。
- 许可证:`LICENSE.TXT`(FreeType License,BSD 风格,GPL 兼容)。

C++ 侧 OPT-B2:直接 `#include <ft2build.h>` 用**真结构体**(FT_FaceRec/
FT_GlyphSlotRec/FT_Bitmap)取代上游 FreeTypeFont.cs L27-36 的手算偏移
HACK;5 个入口(FT_Init_FreeType/FT_New_Memory_Face/FT_Done_Face/
FT_Set_Pixel_Sizes/FT_Load_Char)经 LoadLibraryA("freetype6.dll") 运行时
直连。测试经 CMake 把 `bin/freetype6.dll` 拷到可执行文件旁。

- `include/` (ft2build.h + freetype/**): the `include/` subtree of the
  official FreeType **2.14.1** source package (https://freetype.org, tag
  VER-2-14-1), verbatim.
- `bin/freetype6.dll`: a copy of the upstream OpenRA repo's
  `bin/freetype6.dll` (x64, version resource 2.14.1) — **the very binary the
  C# oracle (golden_gen) loads**, which is what makes the byte-exact glyph
  golden differential possible (identical call sequences into the same DLL
  from both languages).
- License: `LICENSE.TXT` (the FreeType License, BSD-style, GPL-compatible).

C++ side OPT-B2: real structs (FT_FaceRec/FT_GlyphSlotRec/FT_Bitmap) via a
direct `#include <ft2build.h>`, replacing the hand-computed-offset HACK of
FreeTypeFont.cs L27-36; the five entry points (FT_Init_FreeType/
FT_New_Memory_Face/FT_Done_Face/FT_Set_Pixel_Sizes/FT_Load_Char) connect at
runtime through LoadLibraryA("freetype6.dll"). CMake copies
`bin/freetype6.dll` next to the test executables.
