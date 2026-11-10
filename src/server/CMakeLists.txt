add_library(ora_server STATIC server.cpp)
target_sources(ora_server PUBLIC FILE_SET cxx_modules TYPE CXX_MODULES FILES server.cppm)
target_include_directories(ora_server PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_server PUBLIC ora_std)
