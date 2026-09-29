# OpenRAC — OpenRA 的 C23 移植(见 PORTING_PLAN.md)
#
# 工具链基线(锁定,见计划 §Phase 0):
#   CMake >= 3.28  +  Clang(本机基线 23.1.0,llvm-mingw/UCRT,x86_64-w64-windows-gnu) + Ninja
#   语言标准:gnu23(CMAKE_C_EXTENSIONS ON → -std=gnu23)
#
# 配置示例:
#   cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build
#   ctest --test-dir build --output-on-failure

cmake_minimum_required(VERSION 3.28)

project(OpenRAC
    VERSION 0.1.0
    DESCRIPTION "OpenRA real-time strategy engine, ported to C23"
    LANGUAGES C)

# —— 语言标准:gnu23,且禁止 CMake 在缺支持时静默降级 ——
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS ON)          # gnu23(GNU 方言)
set(CMAKE_C_STANDARD_REQUIRED ON)

# —— 确定性基线(计划 §4.4,不可协商):
#    -fwrapv            对齐 C# unchecked 有符号整数回绕语义,规避 signed 溢出 UB
#    -fno-strict-aliasing 大量 void*/字符缓冲的字节级访问(网络包/资产解码)
add_compile_options(
    -fwrapv
    -fno-strict-aliasing
    -Wall -Wextra -Wconversion -Wshadow
    -Wno-unused-parameter
)

if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
else()
    message(WARNING "OpenRAC 基线工具链为 Clang,当前编译器为 ${CMAKE_C_COMPILER_ID},仅作探索性构建")
endif()

# —— Sanitizers(计划 §8:CI 全程 ASan+UBSan)——
option(ORA_SANITIZE "Enable ASan+UBSan" OFF)
if(ORA_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

# —— c23_probe:Phase 0 验收物,验证特性白名单与确定性前提 ——
add_executable(c23_probe tools/c23_probe.c)

# —— golden_core:Phase 0 定点原语/MersenneTwister 黄金对拍(与 C# 版逐行比对)——
add_executable(golden_core tests/golden_core.c)
target_include_directories(golden_core PRIVATE src/core)

enable_testing()
add_test(NAME c23_probe COMMAND c23_probe)
add_test(NAME golden_core COMMAND golden_core ${CMAKE_SOURCE_DIR}/tests/golden_core.txt)
