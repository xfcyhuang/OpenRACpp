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
