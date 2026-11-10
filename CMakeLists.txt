cmake_minimum_required(VERSION 3.28)

project(OpenRACpp LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

option(ORA_HARDENED "GCC -fhardened 旗标等价集（fortify/栈保护/PIE）" OFF)

add_compile_options(
  -Wall
  -Wextra
  -Wpedantic
  -fwrapv
  -fno-strict-aliasing
  -ffunction-sections
  -fdata-sections
)

add_link_options(-Wl,--gc-sections)

if(ORA_HARDENED)
  add_compile_options(-D_FORTIFY_SOURCE=3 -fstack-protector-strong -fPIE)
  add_link_options(-Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va)
endif()

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)
