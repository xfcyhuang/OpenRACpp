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
