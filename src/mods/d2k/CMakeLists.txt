add_library(ora_mods_d2k STATIC d2k.cpp)
target_sources(ora_mods_d2k PRIVATE d2k.hpp)
target_include_directories(ora_mods_d2k PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_mods_d2k PUBLIC ora_std)
