add_library(ora_mods_common STATIC common.cpp wav_loader.cpp ima_adpcm.cpp)
target_sources(ora_mods_common PRIVATE common.hpp wav_loader.hpp ima_adpcm.hpp)
target_include_directories(ora_mods_common PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_mods_common PUBLIC ora_std)
