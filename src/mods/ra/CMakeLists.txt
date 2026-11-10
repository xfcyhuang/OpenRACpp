add_library(ora_mods_ra STATIC ra.cpp)
target_sources(ora_mods_ra PUBLIC FILE_SET cxx_modules TYPE CXX_MODULES FILES ra.cppm)
target_include_directories(ora_mods_ra PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_mods_ra PUBLIC ora_std)
