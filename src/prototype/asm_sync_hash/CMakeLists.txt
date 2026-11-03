# PROTOTYPE（wayfinder 工单 #23）—— 一次性原型，独立成团不污染主线目标，勿并入主分支。
add_library(ora_asm_sync_hash_proto STATIC sync_hash_proto.S)
target_include_directories(ora_asm_sync_hash_proto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_executable(ora_asm_sync_hash_proto_test ${CMAKE_SOURCE_DIR}/tests/prototype_asm_sync_hash.cpp)
target_link_libraries(ora_asm_sync_hash_proto_test PRIVATE ora_asm_sync_hash_proto ora_std)
target_compile_features(ora_asm_sync_hash_proto_test PRIVATE cxx_std_26)

add_test(NAME asm_sync_hash_proto COMMAND ora_asm_sync_hash_proto_test)
