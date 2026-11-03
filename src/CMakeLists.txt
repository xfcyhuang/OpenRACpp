add_library(ora_std)
target_sources(ora_std PUBLIC FILE_SET cxx_modules TYPE CXX_MODULES FILES std.cppm)
target_compile_features(ora_std PUBLIC cxx_std_26)
target_compile_options(ora_std PRIVATE -Wno-reserved-module-identifier)

add_subdirectory(game)
add_subdirectory(mods)
add_subdirectory(platforms)
add_subdirectory(server)
add_subdirectory(utility)
# PROTOTYPE（wayfinder #23）：一次性汇编原型，仅存在于 prototype/asm-sync-hash 分支
add_subdirectory(prototype)
