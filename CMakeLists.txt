# OpenRACpp — OpenRA → C++26 语义重写(PORTING_PLAN Phase 0)
# 工具链基线:clang(llvm-mingw) + Ninja;-std=c++26 + import std;(std.cppm 预编译 PCM)
# 确定性保障(PORTING_PLAN §4.7):-fwrapv 复刻 C# unchecked 整数回绕;-fno-strict-aliasing
cmake_minimum_required(VERSION 3.28)
project(OpenRACpp LANGUAGES CXX C)

enable_testing()

# C 编译器默认指向同链 clang(miniz 等第三方 C 库用)
# Default the C compiler to the same-chain clang (for third-party C libs).
if(NOT CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER clang CACHE STRING "C compiler")
endif()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# 函数级裁剪 / Function-level dead-code elimination:
# 每个函数/数据项独立成段,链接期回收未引用段(ld.lld --gc-sections),
# 二进制只保留实际到达的代码路径。
# Each function/data object is placed in its own section; the linker then
# garbage-collects unreferenced sections (ld.lld --gc-sections), so the
# binary keeps only the reachable code paths.
add_compile_options(-Wall -Wextra -Wpedantic -fwrapv -fno-strict-aliasing
  -ffunction-sections -fdata-sections)
add_link_options(-Wl,--gc-sections)

option(ORA_SANITIZE "ASan + UBSan 构建开关" OFF)
if(ORA_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# ———— GCC -fhardened 的旗标等价集(双编译排错的第二编排;见 readme.md
#      工程门禁与 ../cpp26.md 工具链表:本机 gcc.exe 为 llvm-mingw 的
#      clang 壳,-fhardened 原旗标不可执行 —— 按 GCC 文档逐项展开等价:
#      _FORTIFY_SOURCE=3 / stack-protector-strong / PIE + pie 链接;
#      Windows/MinGW 下 stack-clash 防护与影子调用栈不可用面照实略去并
#      登记)————
# ———— The flag-equivalent set of GCC's -fhardened (the second leg of the
#      dual-compile debugging; see the readme.md gates and the ../cpp26.md
#      toolchain table: this machine's gcc.exe is llvm-mingw's clang shim,
#      so the literal -fhardened is unrunnable — expanded per the GCC docs
#      into the equivalents: _FORTIFY_SOURCE=3 / stack-protector-strong /
#      PIE + pie linking; the faces unavailable under Windows/MinGW
#      (stack-clash protection / the shadow call stack) are dropped and
#      registered).
option(ORA_HARDENED "GCC -fhardened 等价硬化构建开关" OFF)
if(ORA_HARDENED)
  add_compile_definitions(_FORTIFY_SOURCE=3)
  add_compile_options(-fstack-protector-strong -fPIE)
  add_link_options(-fPIE -pie)
endif()

# ———— import std; 支持:定位并预编译 libc++ 的 std 模块(本机实测 D:/software/LLVM/share/libc++/v1/std.cppm)
if(NOT ORA_STD_CPPM)
  execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} -print-resource-dir
    OUTPUT_VARIABLE res_dir_ora
    OUTPUT_STRIP_TRAILING_WHITESPACE)
  cmake_path(SET llvm_root_ora NORMALIZE "${res_dir_ora}")  # .../lib/clang/<ver>
  cmake_path(GET llvm_root_ora PARENT_PATH llvm_root_ora)   # .../lib/clang
  cmake_path(GET llvm_root_ora PARENT_PATH llvm_root_ora)   # .../lib
  cmake_path(GET llvm_root_ora PARENT_PATH llvm_root_ora)   # LLVM 根
  find_file(ORA_STD_CPPM NAMES std.cppm
    PATHS "${llvm_root_ora}/share/libc++/v1"
    NO_DEFAULT_PATH)
endif()

if(NOT ORA_STD_CPPM)
  message(FATAL_ERROR
    "未找到 libc++ 的 std.cppm(import std; 依赖)。可用 -DORA_STD_CPPM=<路径> 显式指定,"
    "或参考 PORTING_PLAN §R5 回退方案(整体改用传统 #include)。")
endif()
message(STATUS "import std; 模块源: ${ORA_STD_CPPM}")

add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/cxx/std.pcm
  # PCM 须与使用方共享语义相关选项(-fwrapv/-fno-strict-aliasing;
  # 硬化构建的 -fstack-protector-strong 同入 —— 配置失配即拒载),否则配置不匹配被拒载
  # The PCM must share every semantics-relevant option with its consumers
  # (-fwrapv/-fno-strict-aliasing; the hardened build's
  # -fstack-protector-strong joins in — a configuration mismatch refuses
  # the load).
  COMMAND ${CMAKE_CXX_COMPILER} -std=c++26 -fwrapv -fno-strict-aliasing
          $<IF:$<BOOL:${ORA_HARDENED}>,-fstack-protector-strong,>
          --precompile ${ORA_STD_CPPM} -o ${CMAKE_BINARY_DIR}/cxx/std.pcm
  DEPENDS ${ORA_STD_CPPM}
  COMMENT "预编译 std 模块 PCM"
  VERBATIM)
add_custom_target(ora_std_pcm DEPENDS ${CMAKE_BINARY_DIR}/cxx/std.pcm)

# 所有目标统一挂接 import std;(PCM 与 -std=c++26 一致,勿单独改标准级别)
function(ora_link_std target_name)
  target_compile_options(${target_name} PRIVATE
    -fmodule-file=std=${CMAKE_BINARY_DIR}/cxx/std.pcm)
  add_dependencies(${target_name} ora_std_pcm)
endfunction()

# ———— 引擎库(Phase 0:core 全为 constexpr 头文件;Phase 1 起含编译单元)
add_library(ora_core INTERFACE)
target_include_directories(ora_core INTERFACE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(ora_core INTERFACE
  -fmodule-file=std=${CMAKE_BINARY_DIR}/cxx/std.pcm)

# MiniYaml 解析器(PORTING_PLAN §Phase 1)/ MiniYaml parser (PORTING_PLAN §Phase 1)
add_library(ora_yaml STATIC src/yaml/mini_yaml.cpp)
target_link_libraries(ora_yaml PUBLIC ora_core)
ora_link_std(ora_yaml)

# 第三方:miniz(C 库,zip 读写;import std 门禁白名单内)/
# Third party: miniz (C library for zip I/O; whitelisted by the import std gate)
add_library(ora_miniz STATIC third_party/miniz/miniz.c)
target_include_directories(ora_miniz PUBLIC ${CMAKE_SOURCE_DIR}/third_party/miniz)
target_compile_options(ora_miniz PRIVATE -w)  # 第三方告警不外溢 / third-party warnings stay local

# 虚拟文件系统(PORTING_PLAN §Phase 1)/ Virtual file system (PORTING_PLAN §Phase 1)
add_library(ora_fs STATIC
  src/fs/folder.cpp
  src/fs/zip_file.cpp
  src/fs/file_system.cpp
  src/fs/yaml_load.cpp)
target_link_libraries(ora_fs PUBLIC ora_yaml PRIVATE ora_miniz)
ora_link_std(ora_fs)

# 元数据框架:FieldLoader/FieldSaver 等价 + 类型注册表 + 变量表达式
# (PORTING_PLAN §Phase 2)
# Metadata framework: the FieldLoader/FieldSaver equivalents + the type
# registry + variable expressions (PORTING_PLAN §Phase 2).
add_library(ora_meta STATIC
  src/meta/parse.cpp
  src/meta/variable_expression.cpp
  src/meta/type_registry.cpp
  src/meta/generic_record.cpp
  src/meta/field_loader.cpp
  src/meta/dump_format.cpp
  src/meta/field_saver.cpp)
target_link_libraries(ora_meta PUBLIC ora_yaml)
ora_link_std(ora_meta)

# schema_dumper --gen 生成源(描述表/默认值/枚举/BitSet 标签;勿手改,
# 由 tools/schema_dumper --gen 再生)
# schema_dumper --gen output (descriptor/default/enum/BitSet tables; do not
# edit, regenerated by tools/schema_dumper --gen).
add_library(ora_gen STATIC
  gen/enums_gen.cpp
  gen/bitsets_gen.cpp
  gen/types_gen.cpp
  gen/gen_all.cpp)
target_link_libraries(ora_gen PUBLIC ora_meta)
ora_link_std(ora_gen)

# 游戏加载链:Manifest/Ruleset/ActorInfo/WeaponInfo + 24 个 LoadUsing loader
# (PORTING_PLAN §Phase 2)
# The game loading chain: Manifest/Ruleset/ActorInfo/WeaponInfo + the 24
# LoadUsing loaders (PORTING_PLAN §Phase 2).
add_library(ora_terrain STATIC
  src/terrain/map_grid.cpp
  src/terrain/terrain_info.cpp)
target_link_libraries(ora_terrain PUBLIC ora_gen)
ora_link_std(ora_terrain)

add_library(ora_game STATIC
  src/game/platform.cpp
  src/game/manifest.cpp
  src/game/actor_info.cpp
  src/game/game_records.cpp
  src/game/game_speed.cpp
  src/game/ruleset.cpp
  src/game/mod_data.cpp
  src/game/loaders.cpp
  src/game/settings.cpp)
target_link_libraries(ora_game PUBLIC ora_gen ora_terrain PRIVATE ora_fs ora_formats)
ora_link_std(ora_game)

# 仿真核心:Actor/World/TraitDictionary/Activity/条件系统/SyncHash/Effects
# (PORTING_PLAN §Phase 3)
# The simulation core: Actor/World/TraitDictionary/Activity/condition
# system/SyncHash/Effects (PORTING_PLAN §Phase 3).
add_library(ora_map STATIC
  src/map/map.cpp
  src/map/map_players.cpp
  src/map/map_cache.cpp)
target_link_libraries(ora_map PUBLIC ora_game)
ora_link_std(ora_map)

add_library(ora_sim STATIC
  gen/sync_gen.cpp
  src/sim/trait_dictionary.cpp
  src/sim/sync_hash.cpp
  src/sim/type_dictionary.cpp
  src/sim/activity.cpp
  src/sim/actor_init.cpp
  src/sim/actor.cpp
  src/sim/target.cpp
  src/sim/effects.cpp
  src/sim/spatially_partitioned.cpp
  src/sim/screen_map.cpp
  src/sim/selection.cpp
  src/sim/shroud.cpp
  src/sim/frozen_actor_layer.cpp
  src/sim/actor_map.cpp
  src/sim/control_groups.cpp
  src/sim/player.cpp
  src/sim/trait_registry.cpp
  src/sim/conditional_trait.cpp
  src/sim/weapons.cpp
  src/sim/world.cpp)
# gen/interfaces_gen.h 位于仓库根(gen/ 目录),TypeId 头需根 include
# gen/interfaces_gen.h lives at the repo root (gen/); the TypeId header
# needs the root on the include path.
target_include_directories(ora_sim PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(ora_sim PUBLIC ora_gen ora_map PRIVATE ora_gfx)
ora_link_std(ora_sim)

# 网络层:Order 序列化/OrderIO 线协议/OrderManager 锁步/EchoConnection
# (PORTING_PLAN §Phase 3)
# The network layer: Order serialization / the OrderIO wire protocol / the
# OrderManager lockstep / EchoConnection (PORTING_PLAN §Phase 3).
add_library(ora_net STATIC
  src/net/order.cpp
  src/net/order_io.cpp
  src/net/connection.cpp
  src/net/order_manager.cpp
  src/net/unit_orders.cpp
  src/net/replay_recorder.cpp
  src/net/sync_report.cpp)
target_link_libraries(ora_net PUBLIC ora_sim)
ora_link_std(ora_net)

# ———— Phase 4 第一批:平台层 + 渲染命令缓冲 ————
# Phase 4 first batch: the platform layer + the render command buffer.

# SDL2 2.32.10 官方 MinGW x64 开发包(third_party/SDL2,见其 LICENSE.txt)
# The official SDL2 2.32.10 MinGW x64 dev package (third_party/SDL2; see its LICENSE.txt).
add_library(ora_sdl2 INTERFACE)
target_include_directories(ora_sdl2 INTERFACE ${CMAKE_SOURCE_DIR}/third_party/SDL2/include)
target_link_libraries(ora_sdl2 INTERFACE ${CMAKE_SOURCE_DIR}/third_party/SDL2/lib/libSDL2.dll.a)

# OpenAL Soft 1.24.3 头 + freetype 2.14.1 头(third_party/OpenAL、
# third_party/FreeType,官方包逐字节;运行时 DLL 经 LoadLibrary 直连,
# 无链接期依赖)。第十一批:OpenAL 引擎/FreeType 字体(OPT-B2 真结构体)。
# The OpenAL Soft 1.24.3 headers + the FreeType 2.14.1 headers
# (third_party/OpenAL, third_party/FreeType; verbatim official packages.
# Runtime DLLs connect via LoadLibrary — no link-time dependency). Batch 11:
# the OpenAL engine / FreeType fonts (OPT-B2 real structs).
add_library(ora_openal INTERFACE)
target_include_directories(ora_openal INTERFACE ${CMAKE_SOURCE_DIR}/third_party/OpenAL/include)
add_library(ora_freetype INTERFACE)
target_include_directories(ora_freetype INTERFACE ${CMAKE_SOURCE_DIR}/third_party/FreeType/include)

# 平台层:GL 类型/常量 + 加载器(OPT-A6)+ SDL2 窗口与原子几何快照(OPT-B5)
# + 输入泵(Keycode 枚举/修饰符/多击/motion 合并)+ OpenAL 引擎与接口族
# (池 256/实例限 3/jthread 流式)+ FreeType 字体(OPT-B2)+ SDL2 硬件光标
# The platform layer: GL types/constants + loader (OPT-A6), the SDL2
# window with the atomic geometry snapshot (OPT-B5), the input pump (the
# Keycode enum/modifiers/multi-tap/motion coalescing), the OpenAL engine and
# interface family (the 256 pool / the 3-instance cap / jthread streaming),
# the FreeType font (OPT-B2), and the SDL2 hardware cursor.
add_library(ora_platform STATIC
  src/platform/gl_loader.cpp
  src/platform/sdl2_input.cpp
  src/platform/sdl2_window.cpp
  src/platform/al_loader.cpp
  src/platform/openal_sound_engine.cpp
  src/platform/freetype_font.cpp
  src/platform/sdl2_hardware_cursor.cpp)
target_link_libraries(ora_platform PUBLIC ora_core ora_sdl2 ora_openal ora_freetype)
ora_link_std(ora_platform)

# 渲染核心:值类型命令 + SPSC 无锁队列(OPT-A5)+ 渲染线程(状态 diff,O6)
# + Shader/Texture 封装(OPT-A6:uniform 整数化 + RAII 纹理句柄)
# + Sprite/Sheet/SheetBuilder/Palette 家族 + HardwarePalette(OPT-A7:调色板
# dirty 行)+ 顶点/四边形生成(第三批)
# + WorldRenderer/渲染收集(OPT-A7 SOA + 帧 arena:RenderItem POD/排序分段/
# 三列表复用)+ 四引擎 renderable + TerrainSpriteLayer(第十二批;
# 依赖 ora_sim 的 World/Actor 面)
# The render core: value-type commands + the SPSC lock-free ring (OPT-A5),
# the render thread (state diff, OPT-A6), the Shader/Texture wrappers
# (OPT-A6: integer uniforms + RAII texture handles), and — third batch — the
# Sprite/Sheet/SheetBuilder/Palette family + HardwarePalette (OPT-A7:
# palette dirty rows) + vertex/quad generation; twelfth batch — the
# WorldRenderer/render collection (OPT-A7 SOA + frame arena: the RenderItem
# POD, sort/segmentation, three reused lists) + the four engine renderables
# + TerrainSpriteLayer (depends on ora_sim's World/Actor faces).
add_library(ora_gfx STATIC
  src/gfx/render_thread.cpp
  src/gfx/shader.cpp
  src/gfx/texture.cpp
  src/gfx/gfx_util.cpp
  src/gfx/sheet.cpp
  src/gfx/palette.cpp
  src/gfx/hardware_palette.cpp
  src/gfx/vertex_buffer.cpp
  src/gfx/frame_buffer.cpp
  src/gfx/sprite_renderer.cpp
  src/gfx/renderer.cpp
  src/gfx/renderable.cpp
  src/gfx/world_renderer.cpp
  src/gfx/terrain_sprite_layer.cpp
  src/gfx/sprite_loader.cpp
  src/gfx/sprite_cache.cpp
  src/gfx/sequence_set.cpp
  src/gfx/animation.cpp
  src/gfx/cursor_manager.cpp
  src/gfx/chrome_provider.cpp
  src/gfx/viewport.cpp)
target_link_libraries(ora_gfx PUBLIC ora_platform ora_sim PRIVATE ora_formats)
ora_link_std(ora_gfx)

# mods 序列系统(第十四批):Default/TilesetSpecific/Classic/ClassicTileset/
# D2k 五变体 + SpriteSequenceFormat 名分派工厂(d2k 头含 formats/r8_loader)
# The mods sequence system (the fourteenth batch): the five Default/
# TilesetSpecific/Classic/ClassicTileset/D2k variants + the
# SpriteSequenceFormat name-dispatch factory (the d2k header includes
# formats/r8_loader).
add_library(ora_mods STATIC
  src/mods/default_sprite_sequence.cpp
  src/mods/tileset_specific_sprite_sequence.cpp
  src/mods/classic_sprite_sequence.cpp
  src/mods/classic_tileset_specific_sprite_sequence.cpp
  src/mods/d2k_sprite_sequence.cpp
  src/mods/sequence_loader_factory.cpp
  src/mods/create_map_players.cpp
  src/mods/health.cpp
  src/mods/armament.cpp
  src/mods/body_orientation.cpp
  src/mods/mobile.cpp
  src/mods/move_activities.cpp
  src/mods/attack_activity.cpp
  src/mods/attack_base.cpp
  src/mods/auto_target.cpp
  src/mods/affects_shroud.cpp
  src/mods/armor.cpp
  src/mods/blocks_projectiles.cpp
  src/mods/hit_shape.cpp
  src/mods/hit_shapes.cpp
  src/mods/projectiles.cpp
  src/mods/missile_projectiles.cpp
  src/mods/smudge_layer.cpp
  src/mods/building.cpp
  src/mods/building_influence.cpp
  src/mods/frozen_under_fog.cpp
  src/mods/production_support.cpp
  src/mods/production.cpp
  src/mods/player_resources.cpp
  src/mods/targetable.cpp
  src/mods/warheads.cpp
  src/mods/unit_order_generator.cpp
  src/mods/pathfinding/cell_info_layer_pool.cpp
  src/mods/pathfinding/dense_path_graph.cpp
  src/mods/pathfinding/path_search.cpp
  src/mods/pathfinding/locomotor.cpp
  src/mods/pathfinding/hierarchical_path_finder.cpp
  src/mods/pathfinding/path_finder.cpp
  src/mods/turreted.cpp
  src/mods/attack_follow.cpp
  src/mods/power.cpp
  src/mods/resource_layer.cpp
  src/mods/dock_client.cpp
  src/mods/dock_host.cpp
  src/mods/dock_activities.cpp
  src/mods/stores_resources.cpp
  src/mods/harvester.cpp
  src/mods/harvest_activities.cpp
  src/mods/refinery.cpp
  src/mods/conditions.cpp
  src/mods/external_condition.cpp
  src/mods/cloak.cpp
  src/mods/experience.cpp
  src/mods/capture.cpp
  src/mods/selectable.cpp
  src/mods/spawn_map_actors.cpp
  src/mods/trait_registry_batch7.cpp
  src/mods/render_sprites.cpp
  src/mods/with_sprite_body.cpp
  src/mods/with_infantry_body.cpp
  src/mods/with_sprite_turret.cpp
  src/mods/with_make_animation.cpp
  src/mods/proximity_capturable.cpp
  src/mods/trait_registry_batch8.cpp
  src/mods/sprite_effect.cpp
  src/mods/with_death_animation.cpp
  src/mods/with_damage_overlay.cpp
  src/mods/with_decoration.cpp
  src/mods/with_decoration_pips.cpp
  src/mods/selection_decorations.cpp
  src/mods/production_bar.cpp
  src/mods/trait_registry_batch9.cpp)
target_link_libraries(ora_mods PUBLIC ora_gfx PRIVATE ora_formats)
ora_link_std(ora_mods)

# 声音门面(第十四批):格式链分派 + Sound 调度/音乐/视频 + MusicInfo.Load
# The sound facade (the fourteenth batch): the format-chain dispatch +
# Sound's scheduling/music/video + MusicInfo::Load.
add_library(ora_sound STATIC
  src/sound/sound_loader.cpp
  src/sound/sound.cpp)
target_link_libraries(ora_sound PUBLIC ora_sim ora_game ora_platform PRIVATE ora_fs ora_formats)
ora_link_std(ora_sound)

# UI 框架(第十五批):Widget 树/Ui 门面/ChromeMetrics/WidgetLoader(名字分派
# 注册表;gfx 的 Viewport 静态 + meta 表达式)
# The UI framework (the fifteenth batch): the Widget tree / the Ui facade /
# ChromeMetrics / WidgetLoader (name-dispatch registries; gfx's Viewport
# statics + meta's expressions).
add_library(ora_ui STATIC
  src/ui/widget.cpp
  src/ui/ui.cpp
  src/ui/chrome_metrics.cpp
  src/ui/widget_loader.cpp)
target_link_libraries(ora_ui PUBLIC ora_gfx ora_net ora_meta PRIVATE ora_fs)
ora_link_std(ora_ui)

# 游戏主循环骨架(第十五批):Game 的 Run/Loop/LogicTick/RenderTick/延迟动作
# (依赖 ora_sound 而 ora_sound 依赖 ora_game —— 引擎装配层拆环)
# The game main-loop skeleton (the fifteenth batch): Game's Run/Loop/
# LogicTick/RenderTick/delayed actions (depends on ora_sound, which depends
# on ora_game — the engine-assembly layer breaks the cycle).
add_library(ora_engine STATIC
  src/game/game.cpp)
target_link_libraries(ora_engine PUBLIC ora_sound ora_ui ora_gfx ora_net ora_game)
ora_link_std(ora_engine)

# Westwood 文件格式族(PORTING_PLAN §Phase 4 formats 第一编 + 第二编 +
# 第三编 Mix/Blowfish):字节读取器 + 压缩编解码器(LCW/XOR/LZO/RLE0)+
# CRC32 + SHP TD/D2/TS + TMP TD/RA/TS 加载器 + PNG(自研,zlib 走 miniz)
# + Tga/DDS(Pfim 逐语义)+ ShpRemastered(zip 容器)+
# EmbeddedSpritePalette;帧契约(ISpriteFrame)在 gfx 层。Mix 包件
# (package_entry/mix_file)也编入本库:mix 依赖 Blowfish/Xcc(本库),
# 而本库经 ShpRemastered 依赖 ora_fs —— 在此拆环。
# The Westwood file-format family (PORTING_PLAN §Phase 4, the first,
# second, and third formats batches — the third adds Mix/Blowfish): the
# byte readers + compression codecs (LCW/XOR/LZO/RLE0) + CRC32 + the SHP
# TD/D2/TS and TMP TD/RA/TS loaders + PNG (in-house, zlib via miniz) +
# Tga/DDS (Pfim semantics) + ShpRemastered (zip container) +
# EmbeddedSpritePalette; the frame contract (ISpriteFrame) lives in the
# gfx layer. The mix package pieces (package_entry/mix_file) also compile
# into this library: mix depends on this library's Blowfish/Xcc while the
# library depends on ora_fs through ShpRemastered — the cycle breaks here.
add_library(ora_formats STATIC
  src/formats/lcw.cpp
  src/formats/xor_delta.cpp
  src/formats/rle_zeros.cpp
  src/formats/lzo.cpp
  src/formats/shp_td.cpp
  src/formats/shp_d2.cpp
  src/formats/shp_ts.cpp
  src/formats/tmp_td.cpp
  src/formats/tmp_ra.cpp
  src/formats/tmp_ts.cpp
  src/formats/png.cpp
  src/formats/targa.cpp
  src/formats/dds.cpp
  src/formats/shp_remastered.cpp
  src/formats/blowfish.cpp
  src/formats/blowfish_key_provider.cpp
  src/formats/xcc_database.cpp
  src/formats/ima_adpcm.cpp
  src/formats/westwood_compressed.cpp
  src/formats/aud_reader.cpp
  src/formats/wav_reader.cpp
  src/formats/vqa_video.cpp
  src/formats/wsa_video.cpp
  src/formats/vxl_reader.cpp
  src/formats/hva_reader.cpp
  src/formats/idx_reader.cpp
  src/formats/voc_loader.cpp
  src/formats/ogg_loader.cpp
  src/formats/mp3_loader.cpp
  src/formats/r8_loader.cpp
  src/formats/png_sheet_loader.cpp
  src/fs/package_entry.cpp
  src/fs/mix_file.cpp
  src/fs/d2k_sound_resources.cpp)
# ogg/mp3 解码后端(stb_vorbis/minimp3 单文件,告警不外溢)+ png_sheet 的
# FieldLoader 值入口(meta)
# The ogg/mp3 decode backends (the single-file stb_vorbis/minimp3; warnings
# stay local) + png_sheet's FieldLoader value entries (meta).
target_include_directories(ora_formats PRIVATE
  ${CMAKE_SOURCE_DIR}/third_party/StbVorbis
  ${CMAKE_SOURCE_DIR}/third_party/Minimp3)
set_source_files_properties(src/formats/ogg_loader.cpp PROPERTIES COMPILE_OPTIONS "-w")
set_source_files_properties(src/formats/mp3_loader.cpp PROPERTIES COMPILE_OPTIONS "-w")
target_link_libraries(ora_formats PUBLIC ora_core PRIVATE ora_miniz ora_fs ora_meta)
ora_link_std(ora_formats)

# ———— 工具与测试 / Tools & tests
# tools/ 已移出仓库跟踪(.gitignore):本地开发工具(探针/校验器/oracle 生成器)
# 不再随库分发;引用其源文件的目标在文件存在时才构建,克隆环境自动跳过
# tools/ is no longer tracked (.gitignore): local development tools (probes/
# validators/oracle generators) are not distributed with the repo; targets
# referencing their sources build only when the files exist, so fresh clones
# simply skip them.
if(EXISTS ${CMAKE_SOURCE_DIR}/tools/cpp26_probe.cpp)
  add_executable(cpp26_probe tools/cpp26_probe.cpp)
  target_link_libraries(cpp26_probe PRIVATE ora_core)
  ora_link_std(cpp26_probe)
  add_test(NAME cpp26_probe COMMAND cpp26_probe)
endif()

add_executable(golden_core tests/golden_core.cpp)
target_link_libraries(golden_core PRIVATE ora_core)
ora_link_std(golden_core)
add_test(NAME golden_core COMMAND golden_core ${CMAKE_SOURCE_DIR}/tests/golden_core.txt)

# core 基础设施单测(percent_modifiers 的 decimal 语义 + arena 内存分区;
# docs/OPTIMIZATION_TRACKER.md OPT-A1/OPT-INFRA)
# Core infrastructure unit tests (the decimal semantics of
# percent_modifiers + the arena memory regions; see
# docs/OPTIMIZATION_TRACKER.md OPT-A1/OPT-INFRA).
add_executable(core_test tests/core_test.cpp)
target_link_libraries(core_test PRIVATE ora_core)
ora_link_std(core_test)
add_test(NAME core_test COMMAND core_test)

add_executable(mini_yaml_test tests/mini_yaml_test.cpp)
target_link_libraries(mini_yaml_test PRIVATE ora_yaml)
ora_link_std(mini_yaml_test)
add_test(NAME mini_yaml_test COMMAND mini_yaml_test)

# MiniYaml 黄金对拍(§Phase 1 第一关卡);上游仓库根取自 UPSTREAM.baseline 的 repo=
# Golden differential gate for MiniYaml (§Phase 1); upstream repo root comes from repo= in UPSTREAM.baseline
file(READ ${CMAKE_SOURCE_DIR}/UPSTREAM.baseline _ora_baseline_txt)
string(REGEX MATCH "repo=([^# \r\n]+)" _ora_repo_match "${_ora_baseline_txt}")
if(NOT _ora_repo_match)
  message(FATAL_ERROR "UPSTREAM.baseline 缺少 repo= 记录(golden_yaml 对拍输入)")
endif()
set(ORA_UPSTREAM_ROOT "${CMAKE_MATCH_1}" CACHE PATH "上游 OpenRA 仓库根(黄金对拍输入)")
add_executable(golden_yaml tests/golden_yaml.cpp)
target_link_libraries(golden_yaml PRIVATE ora_yaml)
ora_link_std(golden_yaml)
add_test(NAME golden_yaml COMMAND golden_yaml
  ${CMAKE_SOURCE_DIR}/tests/golden_yaml.txt ${ORA_UPSTREAM_ROOT})

add_executable(fs_test tests/fs_test.cpp)
target_link_libraries(fs_test PRIVATE ora_fs ora_miniz)  # 测试直接用 miniz 造 zip 夹具 / the test drives miniz directly for zip fixtures
ora_link_std(fs_test)
add_test(NAME fs_test COMMAND fs_test)

# 元数据框架单测(PORTING_PLAN §Phase 2)/ Metadata-framework unit tests (§Phase 2)
add_executable(meta_test tests/meta_test.cpp)
target_link_libraries(meta_test PRIVATE ora_meta)
ora_link_std(meta_test)
add_test(NAME meta_test COMMAND meta_test)

# 三代表 trait 端到端断言(§Phase 2 R2 对冲;期望值人工对照上游源码)
# The three-representative-trait end-to-end assertions (the §Phase 2 R2
# hedge; expected values hand-verified against upstream sources).
add_executable(map_test tests/map_test.cpp)
target_link_libraries(map_test PRIVATE
  ora_engine ora_ui ora_sound ora_mods ora_formats ora_net ora_sim ora_map
  ora_game ora_terrain ora_gen ora_gfx ora_platform ora_meta ora_fs ora_yaml
  ora_miniz ora_sdl2 ora_openal ora_freetype)
ora_link_std(map_test)
add_test(NAME map_test COMMAND map_test ${ORA_UPSTREAM_ROOT})

add_executable(game_test tests/game_test.cpp)
target_link_libraries(game_test PRIVATE ora_game)
ora_link_std(game_test)
add_test(NAME game_test COMMAND game_test ${ORA_UPSTREAM_ROOT})

# Phase 5 第二批验收:寻路套件(A*/双向/HPF 面)+ ActorMap/ControlGroups/
# Player 关系面/ResolveFaction(纯逻辑 + 真实 ra 地形全链)
# The Phase 5 batch-2 acceptance: the pathfinding suite (A*/bidirectional/
# HPF faces) + the ActorMap/ControlGroups/Player relationship faces/
# ResolveFaction (pure logic + the real-ra-terrain full chain).
add_executable(path_test tests/path_test.cpp)
target_link_libraries(path_test PRIVATE
  ora_engine ora_ui ora_sound ora_mods ora_formats ora_net ora_sim ora_map
  ora_game ora_terrain ora_gen ora_gfx ora_platform ora_meta ora_fs ora_yaml
  ora_miniz ora_sdl2 ora_openal ora_freetype)
ora_link_std(path_test)
# 运行库就近拷贝(map_test 同型)| runtime-DLL copies (the map_test shape).
add_custom_command(TARGET path_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/third_party/SDL2/bin/SDL2.dll $<TARGET_FILE_DIR:path_test>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/third_party/OpenAL/bin/OpenAL32.dll $<TARGET_FILE_DIR:path_test>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/third_party/FreeType/bin/freetype6.dll $<TARGET_FILE_DIR:path_test>)
# Phase 5 第三批验收:Mobile/BodyOrientation/Move 活动族/Armament 开火链
# (纯逻辑矩阵 + 真实 ra 规则/地图全链)
# The Phase 5 batch-3 acceptance: Mobile/BodyOrientation/the move activity
# family/the Armament firing chain (pure-logic matrices + the real ra
# rules/map full chain).
add_executable(move_test tests/move_test.cpp)
target_link_libraries(move_test PRIVATE
  ora_engine ora_ui ora_sound ora_mods ora_formats ora_net ora_sim ora_map
  ora_game ora_terrain ora_gen ora_gfx ora_platform ora_meta ora_fs ora_yaml
  ora_miniz ora_sdl2 ora_openal ora_freetype)

# Phase 5 第四批验收:Shroud/FrozenActorLayer/AttackBase 族/AutoTarget/
# 弹丸/战头(真 ra 全链)
# The Phase 5 batch-4 acceptance: Shroud/FrozenActorLayer/the AttackBase
# family/AutoTarget/projectiles/warheads (the real ra full chain).
add_executable(production_test tests/production_test.cpp)
ora_link_std(production_test)
target_link_libraries(production_test PRIVATE
  ora_engine ora_ui ora_sound ora_mods ora_formats ora_net ora_sim ora_map
  ora_game ora_terrain ora_gen ora_gfx ora_platform ora_meta ora_fs ora_yaml
  ora_miniz ora_sdl2 ora_openal ora_freetype)
ora_link_std(production_test)

add_executable(attack_test tests/attack_test.cpp)
ora_link_std(move_test)
target_link_libraries(attack_test PRIVATE
  ora_engine ora_ui ora_sound ora_mods ora_formats ora_net ora_sim ora_map
  ora_game ora_terrain ora_gen ora_gfx ora_platform ora_meta ora_fs ora_yaml
  ora_miniz ora_sdl2 ora_openal ora_freetype)
ora_link_std(attack_test)
add_custom_command(TARGET move_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/third_party/SDL2/bin/SDL2.dll $<TARGET_FILE_DIR:move_test>
          ${CMAKE_SOURCE_DIR}/third_party/OpenAL/bin/OpenAL32.dll $<TARGET_FILE_DIR:move_test>
          ${CMAKE_SOURCE_DIR}/third_party/FreeType/bin/freetype6.dll $<TARGET_FILE_DIR:move_test>)
add_test(NAME move_test COMMAND move_test ${ORA_UPSTREAM_ROOT})
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/condition_test.cpp)
add_executable(condition_test tests/condition_test.cpp)
ora_link_std(condition_test)
target_link_libraries(condition_test PRIVATE
  ora_engine ora_ui ora_sound ora_mods ora_formats ora_net ora_sim ora_map
  ora_game ora_terrain ora_gen ora_gfx ora_platform ora_meta ora_fs ora_yaml
  ora_miniz ora_sdl2 ora_openal ora_freetype)
ora_link_std(condition_test)
add_test(NAME condition_test COMMAND condition_test ${ORA_UPSTREAM_ROOT})
set_tests_properties(condition_test PROPERTIES ENVIRONMENT "ORA_UPSTREAM_ROOT=${ORA_UPSTREAM_ROOT}")
endif()

if(EXISTS ${CMAKE_SOURCE_DIR}/tests/render_test.cpp)
add_executable(render_test tests/render_test.cpp)
ora_link_std(render_test)
target_link_libraries(render_test PRIVATE
  ora_engine ora_ui ora_sound ora_mods ora_formats ora_net ora_sim ora_map
  ora_game ora_terrain ora_gen ora_gfx ora_platform ora_meta ora_fs ora_yaml
  ora_miniz ora_sdl2 ora_openal ora_freetype)
ora_link_std(render_test)
add_custom_command(TARGET render_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/third_party/SDL2/bin/SDL2.dll $<TARGET_FILE_DIR:render_test>
          ${CMAKE_SOURCE_DIR}/third_party/OpenAL/bin/OpenAL32.dll $<TARGET_FILE_DIR:render_test>
          ${CMAKE_SOURCE_DIR}/third_party/FreeType/bin/freetype6.dll $<TARGET_FILE_DIR:render_test>)
add_test(NAME render_test COMMAND render_test ${ORA_UPSTREAM_ROOT})
set_tests_properties(render_test PROPERTIES ENVIRONMENT "ORA_UPSTREAM_ROOT=${ORA_UPSTREAM_ROOT}")
endif()

add_executable(resource_test tests/resource_test.cpp)
ora_link_std(resource_test)
target_link_libraries(resource_test PRIVATE
  ora_engine ora_ui ora_sound ora_mods ora_formats ora_net ora_sim ora_map
  ora_game ora_terrain ora_gen ora_gfx ora_platform ora_meta ora_fs ora_yaml
  ora_miniz ora_sdl2 ora_openal ora_freetype)
ora_link_std(resource_test)

add_test(NAME resource_test COMMAND resource_test ${ORA_UPSTREAM_ROOT})
add_test(NAME production_test COMMAND production_test ${ORA_UPSTREAM_ROOT})
set_tests_properties(production_test PROPERTIES ENVIRONMENT "ORA_UPSTREAM_ROOT=${ORA_UPSTREAM_ROOT}")
add_test(NAME attack_test COMMAND attack_test ${ORA_UPSTREAM_ROOT})

add_test(NAME path_test COMMAND path_test ${ORA_UPSTREAM_ROOT})

# 仿真核心单测(PORTING_PLAN §Phase 3 验收:TraitDictionary/Activity/条件
# 系统/Order 字节级序列化/OrderIO/EchoConnection 锁步 10⁶ tick)
# The sim-core unit tests (the PORTING_PLAN §Phase 3 acceptance:
# TraitDictionary/Activity/condition system/byte-level Order serialization/
# OrderIO/EchoConnection lockstep with 10⁶ ticks).
add_executable(sim_test tests/sim_test.cpp)
target_link_libraries(sim_test PRIVATE ora_net)
ora_link_std(sim_test)
add_test(NAME sim_test COMMAND sim_test)

# 规则深度解析对拍(§Phase 2 验收关卡;黄金文件由 schema_dumper --dump 再生)
# The deep-parse rules comparison (the §Phase 2 gate; regenerate goldens with
# schema_dumper --dump).
add_executable(golden_rules tests/golden_rules.cpp)
target_link_libraries(golden_rules PRIVATE ora_game)
ora_link_std(golden_rules)
foreach(ora_mod ra cnc d2k)
  if(EXISTS ${CMAKE_SOURCE_DIR}/tests/golden_rules_${ora_mod}.txt)
    add_test(NAME golden_rules_${ora_mod} COMMAND golden_rules ${ora_mod}
             ${ORA_UPSTREAM_ROOT}
             ${CMAKE_SOURCE_DIR}/tests/golden_rules_${ora_mod}.txt)
  endif()
endforeach()

# Phase 4 第一批验收:SPSC 命令队列(纯逻辑)+ 桌面 GL 集成(OPT-A5/A6/B1/B5;
# 无桌面环境自动 SKIP)。SDL2.dll 就近拷贝,保证 ctest 直接可跑。
# Phase 4 first-batch acceptance: the SPSC command queue (pure logic) plus the
# desktop-GL integration (OPT-A5/A6/B1/B5; auto-SKIP on headless hosts).
# SDL2.dll is copied alongside so ctest runs directly.
add_executable(platform_test tests/platform_test.cpp)
target_link_libraries(platform_test PRIVATE ora_gfx ora_sdl2 ora_meta)
ora_link_std(platform_test)
add_custom_command(TARGET platform_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/third_party/SDL2/bin/SDL2.dll $<TARGET_FILE_DIR:platform_test>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/third_party/OpenAL/bin/OpenAL32.dll $<TARGET_FILE_DIR:platform_test>
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/third_party/FreeType/bin/freetype6.dll $<TARGET_FILE_DIR:platform_test>)
# 第十一批:字形黄金对拍(golden_fonts.txt 由 tools/golden_gen --fonts 再生;
# 字体资产取自上游 mods)
# Batch 11: the glyph golden differential (golden_fonts.txt regenerates via
# tools/golden_gen --fonts; the font assets come from upstream mods).
if(EXISTS ${CMAKE_SOURCE_DIR}/tests/golden_fonts.txt)
  add_test(NAME platform_test COMMAND platform_test
           ${ORA_UPSTREAM_ROOT} ${CMAKE_SOURCE_DIR}/tests/golden_fonts.txt)
else()
  add_test(NAME platform_test COMMAND platform_test)
endif()

# Phase 4 第三批验收:Sheet/SheetBuilder/Sprite 几何 + Palette 家族 +
# HardwarePalette dirty 行(OPT-A7)纯逻辑断言,及 GL 集成上传读回
# (Sheet 子区域/dirty region、调色板 OPT-C5 增量 == 全量;无桌面自动 SKIP)。
# Phase 4 third-batch acceptance: pure-logic assertions on the
# Sheet/SheetBuilder/Sprite geometry, the Palette family, and the
# HardwarePalette dirty rows (OPT-A7), plus GL-integration upload readbacks
# (Sheet sub-regions/dirty regions, and the palette OPT-C5 incremental ==
# full equivalence; auto-SKIP on headless hosts).
add_executable(gfx_test tests/gfx_test.cpp)
target_link_libraries(gfx_test PRIVATE ora_gfx ora_sdl2)
target_compile_definitions(gfx_test PRIVATE ORA_GLSL_DIR="${CMAKE_SOURCE_DIR}/glsl")
ora_link_std(gfx_test)
add_custom_command(TARGET gfx_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/third_party/SDL2/bin/SDL2.dll $<TARGET_FILE_DIR:gfx_test>)
add_test(NAME gfx_test COMMAND gfx_test)

# Phase 4 formats 第一编验收:编解码黄金向量 + mods 全量 .shp/.pal 资产
# 解码对拍(黄金文件由 tools/golden_gen -- fmt 再生)+ 合成 tmp 夹具 +
# SheetBuilder::Add(ISpriteFrame) 接线断言。
# Phase 4 first-formats-batch acceptance: codec golden vectors + the
# all-mods .shp/.pal asset decode differential (the golden file
# regenerates via tools/golden_gen -- fmt) + the synthetic tmp fixtures +
# the SheetBuilder::Add(ISpriteFrame) wiring assertions.
add_executable(formats_test tests/formats_test.cpp)
target_link_libraries(formats_test PRIVATE ora_formats ora_gfx ora_mods ora_meta ora_miniz)  # ora_mods = 第 21 段序列矩阵 | section 21's sequence matrix  # 测试直接 mz_uncompress 展开 Save 的 IDAT | the test drives mz_uncompress directly for Save's IDAT
ora_link_std(formats_test)
add_custom_command(TARGET formats_test POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${CMAKE_SOURCE_DIR}/third_party/SDL2/bin/SDL2.dll $<TARGET_FILE_DIR:formats_test>)
add_test(NAME formats_test COMMAND formats_test
         ${ORA_UPSTREAM_ROOT} ${CMAKE_SOURCE_DIR}/tests/golden_formats.txt)

# 第十四批验收:mods 序列族端到端(合成 shpTD + 临时 FileSystem + 真实
# SpriteCache 流水;Classic/Tileset/D2k 变体与异常文本矩阵)。
# The fourteenth-batch acceptance: the mods sequence family end to end
# (synthetic shpTD + a temp FileSystem + the real SpriteCache pipeline; the
# Classic/Tileset/D2k variants and the exception-text matrix).
add_executable(seq_test tests/seq_test.cpp)
target_link_libraries(seq_test PRIVATE ora_mods ora_formats)
ora_link_std(seq_test)
add_test(NAME seq_test COMMAND seq_test)

# 第十四批验收:声音门面(格式链分派/调度/通知池/音乐面;Dummy 引擎 +
# 上游 mods 散装 .aud 实资产)。
# The fourteenth-batch acceptance: the sound facade (the format-chain
# dispatch / scheduling / notification pools / the music faces; the Dummy
# engine + real loose .aud assets from upstream mods).
add_executable(sound_test tests/sound_test.cpp)
target_link_libraries(sound_test PRIVATE ora_sound ora_fs)
ora_link_std(sound_test)
add_test(NAME sound_test COMMAND sound_test ${ORA_UPSTREAM_ROOT})

# 第十五批验收:UI 框架(ActionQueue/Widget 树/Ui 窗口栈/Mediator/
# ChromeMetrics/WidgetLoader 异常矩阵)+ Game 主循环骨架(时钟注入下的
# Loop 节拍/JoinLocal/延迟动作/Run 清理)。
# The fifteenth-batch acceptance: the UI framework (ActionQueue/the Widget
# tree/the Ui window stack/Mediator/ChromeMetrics/the WidgetLoader exception
# matrix) + the Game main-loop skeleton (the Loop pacing under an injected
# clock/JoinLocal/delayed actions/Run cleanup).
add_executable(ui_test tests/ui_test.cpp)
target_link_libraries(ui_test PRIVATE ora_engine)
ora_link_std(ui_test)
add_test(NAME ui_test COMMAND ui_test)
