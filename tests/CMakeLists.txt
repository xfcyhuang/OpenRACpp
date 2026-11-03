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
