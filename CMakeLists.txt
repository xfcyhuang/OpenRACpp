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
  # PCM 须与使用方共享语义相关选项(-fwrapv/-fno-strict-aliasing),否则配置不匹配被拒载
  COMMAND ${CMAKE_CXX_COMPILER} -std=c++26 -fwrapv -fno-strict-aliasing
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

# ———— 工具与测试 / Tools & tests
add_executable(cpp26_probe tools/cpp26_probe.cpp)
target_link_libraries(cpp26_probe PRIVATE ora_core)
ora_link_std(cpp26_probe)
add_test(NAME cpp26_probe COMMAND cpp26_probe)

add_executable(golden_core tests/golden_core.cpp)
target_link_libraries(golden_core PRIVATE ora_core)
ora_link_std(golden_core)
add_test(NAME golden_core COMMAND golden_core ${CMAKE_SOURCE_DIR}/tests/golden_core.txt)

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
