llama_add_compile_flags()

#
# libraries
#

# llama

file(GLOB LLAMA_MODELS_SOURCES "models/*.cpp")

set(LLAMA_CORE_SOURCES
    llama.cpp
    llama-adapter.cpp
    llama-arch.cpp
    llama-batch.cpp
    llama-chat.cpp
    llama-context.cpp
    llama-cparams.cpp
    llama-grammar.cpp
    llama-graph.cpp
    llama-hparams.cpp
    llama-impl.cpp
    llama-io.cpp
    llama-kv-cache.cpp
    llama-kv-cache-iswa.cpp
    llama-kv-cache-dsa.cpp
    llama-kv-cache-dsa-iswa.cpp
    llama-kv-cache-msa.cpp
    llama-kv-cache-dsv4.cpp
    llama-memory.cpp
    llama-memory-hybrid.cpp
    llama-memory-hybrid-iswa.cpp
    llama-memory-hybrid-idx.cpp
    llama-memory-recurrent.cpp
    llama-mmap.cpp
    llama-model-loader.cpp
    llama-model-saver.cpp
    llama-model.cpp
    llama-quant.cpp
    llama-sampler.cpp
    llama-vocab.cpp
    unicode-data.cpp
    unicode.cpp
)

add_library(llama
            ../include/llama.h
            ${LLAMA_CORE_SOURCES}
            unicode.h
            ${LLAMA_MODELS_SOURCES}
            )

set_target_properties(llama PROPERTIES
    VERSION ${LLAMA_VERSION_BASE}
    SOVERSION ${LLAMA_VERSION_MAJOR}
    MACHO_CURRENT_VERSION 0 # keep macOS linker from seeing oversized version number
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE 16
)

# exclude non-model sources from unity build
set_source_files_properties(${LLAMA_CORE_SOURCES} ../include/llama.h unicode.h
    PROPERTIES SKIP_UNITY_BUILD_INCLUSION ON)

configure_file(llama-version.h.in ${CMAKE_CURRENT_BINARY_DIR}/llama-version.h @ONLY)

if (GGML_ARIFI_KV_MEANCENTER)
    target_compile_definitions(llama PUBLIC GGML_ARIFI_KV_MEANCENTER)
endif()

# R46b: the deterministic checkpoint fault-injection seam (LLAMA_R46_CKPT_FAIL_ALLOC / _COPY) is
# compiled in only when the dedicated option is explicitly turned ON. The option defaults to OFF and
# is independent of LLAMA_BUILD_IS_DEV and LLAMA_BUILD_TESTS, so an ordinary source build - dev or
# release, tests on or off - compiles no environment read and no injected failure branch. PUBLIC so
# the tests that drive the seam see the same definition and can never disagree with the library
# about whether it exists.
if (LLAMA_TEST_CHECKPOINT_FAULT_INJECTION)
    target_compile_definitions(llama PUBLIC LLAMA_TEST_FAULT_INJECTION)
    message(STATUS "checkpoint fault-injection seam: ON (LLAMA_TEST_CHECKPOINT_FAULT_INJECTION=ON, test builds only)")
else()
    message(STATUS "checkpoint fault-injection seam: OFF")
endif()
target_include_directories(llama PRIVATE . ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(llama PUBLIC ../include)
target_compile_features   (llama PRIVATE cxx_std_17) # don't bump

# -- PowerInfer (lane-110 M3 graft)
target_link_libraries(llama PRIVATE powerinfer-${POWERINFER_GROUP_SIZE})
target_link_libraries(llama PUBLIC ggml)

if (BUILD_SHARED_LIBS)
    set_target_properties(llama PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_compile_definitions(llama PRIVATE LLAMA_BUILD)
    target_compile_definitions(llama PUBLIC  LLAMA_SHARED)
endif()
