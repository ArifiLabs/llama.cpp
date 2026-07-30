#include "moe_sparse_pipeline/config.hpp"
#include "moe_sparse_pipeline/expert_bundle.hpp"
#include "moe_sparse_pipeline/packed_kernel.hpp"

namespace moe_sparse_pipeline {

ExpertBundleBuilder::ExpertBundleBuilder(const std::string &out_path) :
    file(out_path, std::ios::binary)
{}

void ExpertBundleBuilder::append(const void *data, size_t ne0, size_t ne1,bool repack) {
    size_t size = powerinfer_row_size(POWERINFER_TYPE_Q4_0, ne0) * ne1;
    if (size % io_alignment != 0) {
        size += io_alignment - size % io_alignment;
    }

    
    if(repack)
    {
        buf.resize(size);
        repack_q4_0_to_q4_0_4_bl(buf.data(), ne0, ne1, 4, data, size);
        file.write(buf.data(), static_cast<ssize_t>(buf.size()));
    }
    else
    file.write(static_cast<const char*>(data),size);

    // advance by what was actually WRITTEN, not by buf.size(). On the repack=false path -- the only
    // path bundle generation uses (llama-model.cpp:1734 and :1736 both pass false) -- `buf` is never
    // resized, so this counter advanced by a stale value, and by zero on a builder that had never
    // taken the repack branch. The written bytes were always correct because file.write() is given
    // `size` directly, so no bundle on disk is affected; current_offset is a public member that
    // nothing in the tree reads yet. Fixed now rather than left as a trap for whoever adds an offset
    // index to the bundle format and reasonably assumes this counter tracks the file position.
    current_offset += size;
}

void ExpertBundleBuilder::append_zero(size_t ne0, size_t ne1) {
    size_t size = powerinfer_row_size(POWERINFER_TYPE_Q4_0, ne0) * ne1;
    if (size % io_alignment != 0) {
        size += io_alignment - size % io_alignment;
    }

    buf.resize(size, 0);
    file.write(buf.data(), static_cast<ssize_t>(buf.size()));

    current_offset += buf.size();
}

void ExpertBundleBuilder::flush() {
    file.flush();
}

}
