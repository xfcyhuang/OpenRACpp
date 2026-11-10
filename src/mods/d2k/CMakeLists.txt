add_library(ora_mods_d2k STATIC d2k.cpp)
target_sources(ora_mods_d2k PUBLIC FILE_SET cxx_modules TYPE CXX_MODULES FILES d2k.cppm)
target_include_directories(ora_mods_d2k PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_mods_d2k PUBLIC ora_std)
