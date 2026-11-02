add_library(ora_server STATIC server.cpp)
target_sources(ora_server PRIVATE server.hpp)
target_include_directories(ora_server PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_server PUBLIC ora_std)
