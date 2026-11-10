add_library(ora_platforms STATIC platforms.cpp openal_loader.cpp sound_engine.cpp)
target_sources(ora_platforms PUBLIC FILE_SET cxx_modules TYPE CXX_MODULES FILES platforms.cppm)
target_include_directories(ora_platforms PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_platforms PUBLIC ora_std)
