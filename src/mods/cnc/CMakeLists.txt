add_library(ora_mods_cnc STATIC cnc.cpp)
target_sources(ora_mods_cnc PUBLIC FILE_SET cxx_modules TYPE CXX_MODULES FILES cnc.cppm)
target_include_directories(ora_mods_cnc PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_mods_cnc PUBLIC ora_std)
