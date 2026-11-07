# T09 渲染素材与 SDL2 依赖获取

工单 #10（T09 渲染窗口与地形/精灵绘制最小面）的外部依赖获取方式。
EA 内容包素材与 SDL2 开发库**不入库**（授权与体积），运行时/构建时按以下方式探测。

## EA 内容包素材（.tem / .shp）

地形 tile（TmpRA）与单位精灵（ShpTD）原版素材在 EA 免费 redistribution 的
原版 RA 内容包（mix 容器）里，上游仓库 `bits/` 不含 ore-lord 所需条目。

1. 安装内容包（OpenRA 内容安装器产物）：`%APPDATA%/OpenRA/Content/ra/v2/`
   （含 temperat.mix / conquer.mix / snow.mix / interior.mix，未加密；
   local.mix 等为 blowfish 加密，本票不需要）。
2. 用上游 utility 无头抽取（在目标目录执行，`ENGINE_DIR` 指向上游检出）：

```bash
mkdir -p <deps>/content-temperat && cd <deps>/content-temperat
ENGINE_DIR="<openra 检出路径>" <openra 检出>/bin/OpenRA.Utility.exe ra \
  --extract clear1.tem d01.tem d02.tem d03.tem d04.tem d05.tem d06.tem d07.tem \
  d08.tem d09.tem d10.tem d11.tem d12.tem d13.tem d14.tem d15.tem d16.tem d17.tem \
  d18.tem d19.tem d20.tem d21.tem d22.tem d23.tem d44.tem d45.tem falls1.tem \
  falls2.tem p03.tem p13.tem p14.tem rf05.tem rf11.tem rv01.tem rv02.tem rv03.tem \
  rv04.tem rv05.tem rv06.tem rv07.tem rv08.tem rv09.tem rv10.tem rv11.tem rv12.tem \
  rv13.tem rv14.tem rv15.tem s01.tem s04.tem s05.tem s06.tem s07.tem s09.tem s10.tem \
  s11.tem s13.tem s29.tem 1tnk.shp
```

清单 = ore-lord（64×64 TEMPERAT）全部 58 个地形图文件 + 1tnk.shp（单位精灵验收样本）。

- 本机稳定缓存：`Z:/source_code/.ora-deps/content-temperat/`
- 测试/探针探测顺序：环境变量 `ORA_CONTENT_TEMPERAT` → CMake cache 变量
  `ORA_CONTENT_TEMPERAT_DIR`（configure 时 `-D` 注入编译定义）。
- 目录缺失时 `render_assets` / `render_probe` 以 **ctest SKIP（退出码 77）** 优雅跳过，
  无头测试不因此 FAIL。
- 调色板 `temperat.pal` 不走内容包（local.mix 加密）——用仓库内 GPL 干净副本
  `tests/golden/render/temperat.pal`（上游 `mods/ra/maps/chernobyl/` 同源，
  与内容包字节恒等，见 COPYING）。

## SDL2 开发库（窗口探针）

- 来源：官方预编译 `SDL2-devel-2.32.10-mingw.zip`
  （GCC x86_64 构建，与 llvm-mingw clang 23 + lld 完全兼容，ASan/hardened 混链实测通过）。
- 下载：`https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-mingw.zip`
  （网络受限环境可走代理；14MB 二进制不 modified 入库）。
- 本机稳定缓存：`Z:/source_code/.ora-deps/sdl2/SDL2-2.32.10/`
- 构建接线：`cmake --preset base -DSDL2_DIR=<...>/x86_64-w64-mingw32/lib/cmake/SDL2`。
  `find_package(SDL2 CONFIG QUIET)` 失败时 `ora_render_probe` target 与 `render_probe`
  测试整体跳过（STATUS 提示），其余测试不受影响。

## soft_oal.dll（音频设备层，T18）

- 来源：nuget 缓存 `openra-openal-cs/1.0.23` 包内 `native/win-x64/soft_oal.dll`
  （openal-soft 1.23.x，LGPL-2.1，动态加载合规；本机路径
  `C:/Users/xfcyhuang/.nuget/packages/openra-openal-cs/1.0.23/native/win-x64/soft_oal.dll`，
  PE 校验 + ctypes/al 探针实测通过）。**不入库**（同 SDL2 红线）。
- 也可用 openal-soft 官方发布包的 soft_oal.dll（版本 ≥1.23 均可）。
- 构建接线：`cmake --preset base -DORA_SOFT_OAL_DLL=<soft_oal.dll 路径>`。
  变量为空时 `ora_audio_probe` target 与 `audio_probe` 测试整体跳过（STATUS 提示）；
  变量非空时 POST_BUILD `copy_if_different` 到探针 exe 旁（探针按 exe 目录优先探测）。
- 运行时探测顺序（src/platforms/openal_loader.cpp）：环境变量缺失前提下
  exe 目录 `soft_oal.dll` → exe 目录 `openal32.dll` → LoadLibrary 默认搜索
  （`soft_oal.dll`、`openal32.dll`）；任一命中后 25 个 AL/ALC 符号全量
  GetProcAddress，任一缺失整库作废 → 回落 Dummy 引擎。
- `ora_audio_probe` 无声卡/设备打开失败时以 **ctest SKIP（退出码 77）** 跳过；
  Dummy 回落路径在 `ora_audio`（无外部依赖，三编排必跑）。
