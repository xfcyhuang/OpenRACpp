add_library(ora_mods_common STATIC common.cpp)
target_sources(ora_mods_common PRIVATE common.hpp)
target_include_directories(ora_mods_common PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_mods_common PUBLIC ora_std)
