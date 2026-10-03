# OpenRACpp — OpenRA → C++26 语义重写(PORTING_PLAN Phase 0)
# 工具链基线:clang(llvm-mingw) + Ninja;-std=c++26 + import std;(std.cppm 预编译 PCM)
# 确定性保障(PORTING_PLAN §4.7):-fwrapv 复刻 C# unchecked 整数回绕;-fno-strict-aliasing
cmake_minimum_required(VERSION 3.28)
project(OpenRACpp LANGUAGES CXX)

enable_testing()

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -fwrapv -fno-strict-aliasing)

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

# ———— 引擎头文件库(Phase 0 全部为 constexpr 头文件实现)
add_library(ora_core INTERFACE)
target_include_directories(ora_core INTERFACE ${CMAKE_SOURCE_DIR}/src)
target_compile_options(ora_core INTERFACE
  -fmodule-file=std=${CMAKE_BINARY_DIR}/cxx/std.pcm)

# ———— 工具与测试
add_executable(cpp26_probe tools/cpp26_probe.cpp)
target_link_libraries(cpp26_probe PRIVATE ora_core)
ora_link_std(cpp26_probe)
add_test(NAME cpp26_probe COMMAND cpp26_probe)

add_executable(golden_core tests/golden_core.cpp)
target_link_libraries(golden_core PRIVATE ora_core)
ora_link_std(golden_core)
add_test(NAME golden_core COMMAND golden_core ${CMAKE_SOURCE_DIR}/tests/golden_core.txt)
