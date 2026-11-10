add_library(ora_utility STATIC utility.cpp)
target_sources(ora_utility PUBLIC FILE_SET cxx_modules TYPE CXX_MODULES FILES utility.cppm)
target_include_directories(ora_utility PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(ora_utility PUBLIC ora_std)
