add_library(ora_platforms STATIC platforms.cpp)
target_sources(ora_platforms PRIVATE platforms.hpp)
target_include_directories(ora_platforms PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_platforms PUBLIC ora_std)
