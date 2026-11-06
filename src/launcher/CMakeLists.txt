add_library(ora_launcher_core STATIC cli_args.cpp sandbox_recipe.cpp game_session.cpp)
target_sources(ora_launcher_core PRIVATE cli_args.hpp sandbox_recipe.hpp game_session.hpp)
target_include_directories(ora_launcher_core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_launcher_core PUBLIC ora_std ora_game)

# console 子系统（非 GUI）：ora 是 CLI 工具（--headless 事件日志/退出码/Ctrl+C
# 控制台事件可达）。SDL2 mingw 官方 config 给 SDL2::SDL2 接口注入 -mwindows
# （windows 子系统 → GUI 进程不附加控制台，SetConsoleCtrlHandler 永远收不到
# 事件——实测 CTRL_BREAK 送不达实锤），SDL2_NO_MWINDOWS 是 config 自带开关。
# 同时不链 SDL2::SDL2main（其接口 -Wl,--undefined=WinMain 强拉 GUI 入口）。
set(SDL2_NO_MWINDOWS ON)
find_package(SDL2 CONFIG QUIET)
if(SDL2_FOUND)
  add_executable(ora main.cpp)
  target_link_libraries(ora PRIVATE ora_std ora_launcher_core SDL2::SDL2)
  target_include_directories(ora PRIVATE ${SDL2_INCLUDE_DIRS})
  add_custom_command(TARGET ora POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL2_DIR}/../../../bin/SDL2.dll" $<TARGET_FILE_DIR:ora>)
  add_custom_command(TARGET ora POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/data/temperat.yaml"
    "$<TARGET_FILE_DIR:ora>/data/temperat.yaml")
  message(STATUS "SDL2 launcher enabled: ${SDL2_DIR}")
else()
  message(STATUS "SDL2 未找到（-DSDL2_DIR=<...>/lib/cmake/SDL2）——ora 可执行 target 跳过")
endif()
