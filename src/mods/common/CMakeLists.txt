add_library(ora_mods_common STATIC common.cpp wav_loader.cpp ima_adpcm.cpp)
target_sources(ora_mods_common PUBLIC FILE_SET cxx_modules TYPE CXX_MODULES FILES common.cppm)
target_include_directories(ora_mods_common PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_mods_common PUBLIC ora_std)
