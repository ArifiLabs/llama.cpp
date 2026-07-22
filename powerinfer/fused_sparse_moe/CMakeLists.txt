add_library(powerinfer-fused-sparse-moe)

target_sources(powerinfer-fused-sparse-moe PRIVATE
    "fused_sparse_moe.cpp"
)
target_include_directories(powerinfer-fused-sparse-moe PUBLIC .)
target_link_libraries(powerinfer-fused-sparse-moe PUBLIC az)
target_link_libraries(powerinfer-fused-sparse-moe PRIVATE powerinfer-cpu)

# lane-110 TEMPORARY debug info for crash triage (remove after)
target_compile_options(powerinfer-fused-sparse-moe PRIVATE -g)  # lane-110: -g triage; prefer-128 inherited from subtree root
