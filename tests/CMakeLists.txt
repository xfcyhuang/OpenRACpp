add_executable(ora_smoke smoke.cpp)
target_link_libraries(ora_smoke PRIVATE
  ora_std
  ora_game
  ora_mods_common
  ora_mods_cnc
  ora_mods_d2k
  ora_mods_ra
  ora_platforms
  ora_server
  ora_utility
)

add_test(NAME smoke COMMAND ora_smoke)

add_executable(ora_prng prng.cpp)
target_link_libraries(ora_prng PRIVATE
  ora_std
  ora_game
)
target_compile_definitions(ora_prng PRIVATE ORA_PRNG_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/prng")

add_test(NAME prng COMMAND ora_prng)

add_executable(ora_miniyaml_unit miniyaml_unit.cpp)
target_link_libraries(ora_miniyaml_unit PRIVATE ora_std ora_game)
target_compile_definitions(ora_miniyaml_unit PRIVATE
  ORA_MINIYAML_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/miniyaml")
add_test(NAME miniyaml_unit COMMAND ora_miniyaml_unit)

add_executable(ora_miniyaml_roundtrip miniyaml_roundtrip.cpp)
target_link_libraries(ora_miniyaml_roundtrip PRIVATE ora_std ora_game)
target_compile_definitions(ora_miniyaml_roundtrip PRIVATE
  ORA_MINIYAML_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/miniyaml")
add_test(NAME miniyaml_roundtrip COMMAND ora_miniyaml_roundtrip)

add_executable(ora_rules_load rules_load.cpp)
target_link_libraries(ora_rules_load PRIVATE ora_std ora_game)
target_compile_definitions(ora_rules_load PRIVATE
  ORA_RULES_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/rules"
  ORA_MINIYAML_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/miniyaml")
add_test(NAME rules_load COMMAND ora_rules_load)

add_executable(ora_map_read map_read.cpp)
target_link_libraries(ora_map_read PRIVATE ora_std ora_game)
target_compile_definitions(ora_map_read PRIVATE
  ORA_MAPS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/maps")
add_test(NAME map_read COMMAND ora_map_read)

add_executable(ora_world_tick world_tick.cpp)
target_link_libraries(ora_world_tick PRIVATE ora_std ora_game)
add_test(NAME world_tick COMMAND ora_world_tick)

add_executable(ora_render_decode render_decode.cpp)
target_link_libraries(ora_render_decode PRIVATE ora_std ora_game)
target_compile_definitions(ora_render_decode PRIVATE
  ORA_RENDER_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/render"
  ORA_MAPS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/maps")
add_test(NAME render_decode COMMAND ora_render_decode)
set_tests_properties(render_decode PROPERTIES LABELS "render")

set(ORA_CONTENT_TEMPERAT_DIR "" CACHE PATH
  "EA 内容包抽取目录（ore-lord 58 .tem + 1tnk.shp，OpenRA.Utility --extract 产物）；空则测试 SKIP")
add_executable(ora_render_assets render_assets.cpp)
target_link_libraries(ora_render_assets PRIVATE ora_std ora_game)
target_compile_definitions(ora_render_assets PRIVATE
  ORA_CONTENT_TEMPERAT_DIR="${ORA_CONTENT_TEMPERAT_DIR}"
  ORA_RENDER_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/render")
add_test(NAME render_assets COMMAND ora_render_assets)
set_tests_properties(render_assets PROPERTIES LABELS "render" SKIP_RETURN_CODE 77)

set(SDL2_DIR "" CACHE PATH "SDL2 cmake 配置目录（SDL2-devel-mingw 包内 lib/cmake/SDL2）；空则跳过渲染探针 target")
find_package(SDL2 CONFIG QUIET)
if(SDL2_FOUND)
  add_executable(ora_input_probe input_probe.cpp)
  target_link_libraries(ora_input_probe PRIVATE ora_std ora_game SDL2::SDL2main SDL2::SDL2)
  target_include_directories(ora_input_probe PRIVATE ${SDL2_INCLUDE_DIRS})
  add_custom_command(TARGET ora_input_probe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${SDL2_DIR}/../../../bin/SDL2.dll" $<TARGET_FILE_DIR:ora_input_probe>)
  add_test(NAME input_probe COMMAND ora_input_probe
    WORKING_DIRECTORY $<TARGET_FILE_DIR:ora_input_probe>)
  set_tests_properties(input_probe PROPERTIES LABELS "render" SKIP_RETURN_CODE 77)
  message(STATUS "SDL2 input probe enabled: ${SDL2_DIR}")

  add_executable(ora_render_probe render_probe.cpp)
  target_link_libraries(ora_render_probe PRIVATE ora_std ora_game SDL2::SDL2main SDL2::SDL2)
  target_include_directories(ora_render_probe PRIVATE ${SDL2_INCLUDE_DIRS})
  target_compile_definitions(ora_render_probe PRIVATE
    ORA_CONTENT_TEMPERAT_DIR="${ORA_CONTENT_TEMPERAT_DIR}"
    ORA_MAPS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/maps"
    ORA_RENDER_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/render")
  add_custom_command(TARGET ora_render_probe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${SDL2_DIR}/../../../bin/SDL2.dll" $<TARGET_FILE_DIR:ora_render_probe>)
  add_test(NAME render_probe COMMAND ora_render_probe
    WORKING_DIRECTORY $<TARGET_FILE_DIR:ora_render_probe>)
  set_tests_properties(render_probe PROPERTIES LABELS "render" SKIP_RETURN_CODE 77)
  message(STATUS "SDL2 render probe enabled: ${SDL2_DIR}")
else()
  message(STATUS "SDL2 未找到（-DSDL2_DIR=<...>/lib/cmake/SDL2）——render_probe target 跳过")
endif()
add_executable(ora_orders orders.cpp)
target_link_libraries(ora_orders PRIVATE ora_std ora_game)
target_compile_definitions(ora_orders PRIVATE
  ORA_ORDERS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/orders")
add_test(NAME orders COMMAND ora_orders)

add_executable(ora_pathfind pathfind.cpp)
target_link_libraries(ora_pathfind PRIVATE ora_std ora_game)
target_compile_definitions(ora_pathfind PRIVATE
  ORA_PATHFINDING_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/pathfinding"
  ORA_MAPS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/maps")
add_test(NAME pathfind COMMAND ora_pathfind)

add_executable(ora_pathfind_move pathfind_move.cpp)
target_link_libraries(ora_pathfind_move PRIVATE ora_std ora_game)
target_compile_definitions(ora_pathfind_move PRIVATE
  ORA_PATHFINDING_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/pathfinding"
  ORA_MAPS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/maps")
add_test(NAME pathfind_move COMMAND ora_pathfind_move)

add_executable(ora_combat combat.cpp)
target_link_libraries(ora_combat PRIVATE ora_std ora_game)
target_compile_definitions(ora_combat PRIVATE
  ORA_COMBAT_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/combat"
  ORA_MAPS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/maps")
add_test(NAME combat COMMAND ora_combat)

add_executable(ora_production production.cpp)
target_link_libraries(ora_production PRIVATE ora_std ora_game)
target_compile_definitions(ora_production PRIVATE
  ORA_PRODUCTION_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/production")
add_test(NAME production COMMAND ora_production)

add_executable(ora_economy economy.cpp)
target_link_libraries(ora_economy PRIVATE ora_std ora_game)
target_compile_definitions(ora_economy PRIVATE
  ORA_ECONOMY_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/economy"
  ORA_MAPS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/maps")
add_test(NAME economy COMMAND ora_economy)

add_executable(ora_input input_logic.cpp)
target_link_libraries(ora_input PRIVATE ora_std ora_game)
add_test(NAME input COMMAND ora_input)

add_executable(ora_cli_game cli_game.cpp)
target_link_libraries(ora_cli_game PRIVATE ora_std ora_game ora_launcher_core)
add_test(NAME cli_game COMMAND ora_cli_game)

add_executable(ora_replay replay.cpp)
target_link_libraries(ora_replay PRIVATE ora_std ora_game)
target_compile_definitions(ora_replay PRIVATE
  ORA_REPLAY_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/replay")
add_test(NAME replay COMMAND ora_replay)

add_executable(ora_replay_oracle replay_oracle.cpp)
target_link_libraries(ora_replay_oracle PRIVATE ora_std ora_game)
target_compile_definitions(ora_replay_oracle PRIVATE
  ORA_REPLAY_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/replay"
  ORA_MAPS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/maps")
add_test(NAME replay_oracle COMMAND ora_replay_oracle)

add_executable(ora_audio audio.cpp)
target_link_libraries(ora_audio PRIVATE
  ora_std
  ora_game
  ora_mods_common
  ora_platforms
)
target_compile_definitions(ora_audio PRIVATE
  ORA_AUDIO_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/audio")
add_test(NAME audio COMMAND ora_audio)

set(ORA_SOFT_OAL_DLL "" CACHE FILEPATH
  "soft_oal.dll 路径（nuget 缓存 openra-openal-cs/1.0.23/native/win-x64 或 openal-soft 发布包）；空则跳过设备探针 target")
if(ORA_SOFT_OAL_DLL)
  add_executable(ora_audio_probe audio_probe.cpp)
  target_link_libraries(ora_audio_probe PRIVATE ora_std ora_game ora_platforms)
  add_custom_command(TARGET ora_audio_probe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ORA_SOFT_OAL_DLL}"
    $<TARGET_FILE_DIR:ora_audio_probe>)
  add_test(NAME audio_probe COMMAND ora_audio_probe
    WORKING_DIRECTORY $<TARGET_FILE_DIR:ora_audio_probe>)
  set_tests_properties(audio_probe PROPERTIES LABELS "audio" SKIP_RETURN_CODE 77)
  message(STATUS "OpenAL audio probe enabled: ${ORA_SOFT_OAL_DLL}")
else()
  message(STATUS "ORA_SOFT_OAL_DLL 未设置（-DORA_SOFT_OAL_DLL=<soft_oal.dll 路径>）——audio_probe target 跳过")
endif()

add_executable(ora_hotkeys hotkeys.cpp)
target_link_libraries(ora_hotkeys PRIVATE ora_std ora_game)
target_compile_definitions(ora_hotkeys PRIVATE
  ORA_HOTKEYS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/hotkeys")
add_test(NAME hotkeys COMMAND ora_hotkeys)

add_executable(ora_fonts fonts.cpp)
target_link_libraries(ora_fonts PRIVATE ora_std ora_game)
target_compile_definitions(ora_fonts PRIVATE
  ORA_FONTS_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden/fonts")
add_test(NAME fonts COMMAND ora_fonts)
