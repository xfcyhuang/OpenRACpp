# OpenAL(OpenAL Soft 1.24.3 运行时与头文件)| OpenAL (OpenAL Soft 1.24.3 runtime & headers)

- `include/AL/{al.h,alc.h,efx.h}`:官方发行包 `openal-soft-1.24.3-bin.zip` 的头文件
  (https://github.com/kcat/openal-soft/releases/tag/1.24.3),逐字节未改。
- `bin/OpenAL32.dll`:同包 `bin/Win64/soft_oal.dll` 的更名副本(x64;Windows 惯例:
  应用按 `OpenAL32.dll` 名加载 OpenAL,OpenAL Soft 以此名随程序分发以替代系统路由)。
- 许可证:`COPYING`(GNU LGPL 2.1+,与本项目 GPL-3.0 兼容的随包义务见其文)。

C++ 侧经 `src/platform/al_loader` 以 `LoadLibraryA("OpenAL32.dll")` + `GetProcAddress`
表驱动直连(OPT-A6 同形态;上游的 OpenRA-OpenAL-CS P/Invoke 包装层消失)。
测试经 CMake 把 `bin/OpenAL32.dll` 拷到可执行文件旁(与 SDL2.dll 同款)。

- `include/AL/{al.h,alc.h,efx.h}`: verbatim headers from the official
  `openal-soft-1.24.3-bin.zip` release
  (https://github.com/kcat/openal-soft/releases/tag/1.24.3).
- `bin/OpenAL32.dll`: a renamed copy of that package's `bin/Win64/soft_oal.dll`
  (x64; Windows convention loads OpenAL under the name `OpenAL32.dll`, and
  OpenAL Soft ships under that name so it substitutes the system router).
- License: `COPYING` (GNU LGPL 2.1+; see it for the bundling obligations,
  compatible with this project's GPL-3.0).

The C++ side connects table-driven via `LoadLibraryA("OpenAL32.dll")` +
`GetProcAddress` in `src/platform/al_loader` (the same shape as OPT-A6; the
upstream OpenRA-OpenAL-CS P/Invoke wrapper disappears). CMake copies
`bin/OpenAL32.dll` next to the test executables (the same arrangement as
SDL2.dll).
