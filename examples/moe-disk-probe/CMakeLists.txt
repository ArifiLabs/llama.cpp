add_executable(llama-moe-disk-probe moe-disk-probe.cpp)
target_include_directories(llama-moe-disk-probe PRIVATE ../../src ../../ggml/src)
target_link_libraries(llama-moe-disk-probe PRIVATE llama)
target_compile_features(llama-moe-disk-probe PRIVATE cxx_std_17)

add_executable(llama-moe-batch-probe moe-batch-probe.cpp)
target_include_directories(llama-moe-batch-probe PRIVATE ../../src ../../ggml/src)
target_link_libraries(llama-moe-batch-probe PRIVATE llama)
target_compile_features(llama-moe-batch-probe PRIVATE cxx_std_17)

add_executable(llama-moe-replica-probe moe-replica-probe.cpp)
target_include_directories(llama-moe-replica-probe PRIVATE ../../src ../../ggml/src)
target_link_libraries(llama-moe-replica-probe PRIVATE llama)
target_compile_features(llama-moe-replica-probe PRIVATE cxx_std_17)
