add_library(ora_platforms STATIC platforms.cpp openal_loader.cpp sound_engine.cpp)
target_sources(ora_platforms PRIVATE platforms.hpp openal_loader.hpp sound_engine.hpp)
target_include_directories(ora_platforms PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_platforms PUBLIC ora_std)
