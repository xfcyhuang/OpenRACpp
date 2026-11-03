add_library(ora_game STATIC game.cpp support/mersenne_twister.cpp)
target_sources(ora_game PRIVATE game.hpp support/mersenne_twister.hpp)
target_include_directories(ora_game PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_game PUBLIC ora_std)
