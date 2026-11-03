add_library(ora_game STATIC game.cpp miniyaml.cpp miniyaml_merge.cpp ruleset.cpp
  map_bin.cpp map_read.cpp tileset.cpp
  support/mersenne_twister.cpp support/sha1.cpp filesystem/inflate.cpp filesystem/zip_read.cpp)
target_sources(ora_game PRIVATE game.hpp miniyaml.hpp miniyaml_merge.hpp ruleset.hpp
  map_bin.hpp map_read.hpp tileset.hpp
  support/mersenne_twister.hpp support/sha1.hpp filesystem/inflate.hpp filesystem/zip_read.hpp)
target_include_directories(ora_game PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_game PUBLIC ora_std)
