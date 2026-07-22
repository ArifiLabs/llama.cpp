cmake_minimum_required(VERSION 3.16)

project(powerinfer-cpu C CXX)
message("Add submodule: ${PROJECT_NAME}")

FILE(GLOB_RECURSE sources             ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_library(${PROJECT_NAME} STATIC ${sources} ${moe_pipeline_sources})
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
# lane-110 Windows port: liburing is Linux-only; link it only there (the non-Linux IOUring stub in iou.hpp needs no library)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(${PROJECT_NAME} PUBLIC powerinfer-common powerinfer-perf powerinfer-disk  "-Wl,-Bstatic"  uring "-Wl,-Bdynamic" az)
else()
    target_link_libraries(${PROJECT_NAME} PUBLIC powerinfer-common powerinfer-perf powerinfer-disk az)
endif()

# lane-110 TEMPORARY debug info for crash triage (remove after) 
target_compile_options(${PROJECT_NAME} PRIVATE -g)  # lane-110: -g triage; prefer-128 inherited from subtree root
