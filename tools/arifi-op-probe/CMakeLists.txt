set(TARGET arifi-op-probe)
add_executable(${TARGET} op-probe.cpp)
target_link_libraries(${TARGET} PRIVATE ggml ${CMAKE_THREAD_LIBS_INIT})
target_compile_features(${TARGET} PRIVATE cxx_std_17)

add_executable(arifi-op-capture capture.cpp)
target_link_libraries(arifi-op-capture PRIVATE llama ggml ${CMAKE_THREAD_LIBS_INIT})
target_compile_features(arifi-op-capture PRIVATE cxx_std_17)

add_executable(arifi-fa-replay fa-replay.cpp)
target_link_libraries(arifi-fa-replay PRIVATE ggml ${CMAKE_THREAD_LIBS_INIT})
target_compile_features(arifi-fa-replay PRIVATE cxx_std_17)

add_executable(arifi-det-probe det-probe.cpp)
target_link_libraries(arifi-det-probe PRIVATE llama-common llama ggml ${CMAKE_THREAD_LIBS_INIT})
target_compile_features(arifi-det-probe PRIVATE cxx_std_17)
