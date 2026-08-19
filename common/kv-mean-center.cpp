#include "kv-mean-center.h"

#include "log.h"

#include "ggml.h"
#include "gguf.h"
#include "llama.h"

extern "C" {
#include "hash/sha256/sha256.h"
}

#include <array>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>

static std::string kv_mean_center_hex(const unsigned char * digest, size_t len) {
    static const char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(2*len);
    for (size_t i = 0; i < len; ++i) {
        out += hex[digest[i] >> 4];
        out += hex[digest[i] & 0x0f];
    }
    return out;
}

static void kv_mean_center_hash_u64(sha256_t & sha, uint64_t value) {
    unsigned char little_endian[8];
    for (size_t i = 0; i < sizeof(little_endian); ++i) {
        little_endian[i] = (unsigned char) ((value >> (8*i)) & 0xff);
    }
    sha256_update(&sha, little_endian, sizeof(little_endian));
}

bool common_kv_mean_center_model_sha256(
        const std::string & model_path,
        const llama_model * model,
        std::string & model_sha256) {
    model_sha256.clear();
    if (model_path.empty() || model == nullptr) {
        LOG_ERR("%s: model path and loaded model are required for exact calibration binding\n", __func__);
        return false;
    }

    uint32_t split_count = 1;
    std::array<char, 32> split_count_buf{};
    const int32_t n_split_count = llama_model_meta_val_str(
            model, "split.count", split_count_buf.data(), split_count_buf.size());
    if (n_split_count >= 0) {
        char * end = nullptr;
        errno = 0;
        const unsigned long parsed = std::strtoul(split_count_buf.data(), &end, 10);
        if (errno != 0 || end == split_count_buf.data() || *end != '\0' || parsed == 0 ||
                parsed > std::numeric_limits<uint32_t>::max()) {
            LOG_ERR("%s: malformed split.count metadata '%s' in %s\n",
                    __func__, split_count_buf.data(), model_path.c_str());
            return false;
        }
        split_count = (uint32_t) parsed;
    }

    std::vector<std::string> paths;
    if (split_count == 1) {
        paths.push_back(model_path);
    } else {
        std::vector<char> prefix(32768, '\0');
        if (llama_split_prefix(prefix.data(), prefix.size(), model_path.c_str(), 0, split_count) <= 0) {
            LOG_ERR("%s: cannot derive the %u-part model prefix from %s\n",
                    __func__, split_count, model_path.c_str());
            return false;
        }
        paths.reserve(split_count);
        for (uint32_t i = 0; i < split_count; ++i) {
            std::vector<char> path(32768, '\0');
            if (llama_split_path(path.data(), path.size(), prefix.data(), i, split_count) <= 0) {
                LOG_ERR("%s: cannot derive split %u / %u from %s\n",
                        __func__, i + 1, split_count, model_path.c_str());
                return false;
            }
            paths.emplace_back(path.data());
        }
    }

    sha256_t sha;
    sha256_init(&sha);
    static const unsigned char domain[] = "arifilabs.kv-mean-center.model-files.v1";
    sha256_update(&sha, domain, sizeof(domain));
    kv_mean_center_hash_u64(sha, paths.size());

    std::vector<unsigned char> buf(8*1024*1024);
    for (const auto & path : paths) {
        std::ifstream in(path, std::ios::binary | std::ios::ate);
        if (!in) {
            LOG_ERR("%s: cannot open model file %s for calibration identity\n", __func__, path.c_str());
            return false;
        }
        const std::streamoff end = in.tellg();
        if (end < 0) {
            LOG_ERR("%s: cannot determine model file size for %s\n", __func__, path.c_str());
            return false;
        }
        kv_mean_center_hash_u64(sha, (uint64_t) end);
        in.seekg(0, std::ios::beg);
        while (in) {
            in.read((char *) buf.data(), buf.size());
            const std::streamsize got = in.gcount();
            if (got > 0) {
                sha256_update(&sha, buf.data(), (size_t) got);
            }
        }
        if (!in.eof()) {
            LOG_ERR("%s: failed while hashing model file %s\n", __func__, path.c_str());
            return false;
        }
    }

    unsigned char digest[SHA256_DIGEST_SIZE];
    sha256_final(&sha, digest);
    model_sha256 = kv_mean_center_hex(digest, sizeof(digest));
    LOG_INF("%s: exact model identity %s over %zu GGUF file(s)\n",
            __func__, model_sha256.c_str(), paths.size());
    return true;
}

bool common_kv_mean_center_write(
        const std::string & fname,
        const std::vector<common_kv_mean_center_layer> & layers,
        const std::string & model_sha256,
        bool k_rot) {
    if (model_sha256.size() != 64 || model_sha256.find_first_not_of("0123456789abcdef") != std::string::npos) {
        LOG_ERR("%s: exact 64-character lowercase model SHA-256 is required\n", __func__);
        return false;
    }
    size_t n_with_bias = 0;
    for (const auto & layer : layers) {
        if (!layer.bias.empty()) {
            n_with_bias++;
        }
    }

    if (n_with_bias == 0) {
        LOG_ERR("%s: no layers with bias data to write\n", __func__);
        return false;
    }

    size_t data_size = 0;
    for (const auto & layer : layers) {
        if (!layer.bias.empty()) {
            data_size += GGML_PAD(ggml_tensor_overhead() + sizeof(float)*layer.bias.size(), GGML_MEM_ALIGN);
        }
    }

    struct ggml_init_params params = {
        /*.mem_size   =*/ data_size,
        /*.mem_buffer =*/ NULL,
        /*.no_alloc   =*/ false,
    };

    struct ggml_context  * ctx      = ggml_init(params);
    struct gguf_context   * ctx_gguf = gguf_init_empty();

    if (!ctx || !ctx_gguf) {
        LOG_ERR("%s: failed to allocate ggml/gguf context\n", __func__);
        if (ctx)      ggml_free(ctx);
        if (ctx_gguf) gguf_free(ctx_gguf);
        return false;
    }

    gguf_set_val_str(ctx_gguf, "general.type", "kv-mean-center");
    gguf_set_val_u32(ctx_gguf, "kv_mean_center.schema_version", 1);
    gguf_set_val_str(ctx_gguf, "kv_mean_center.model_sha256", model_sha256.c_str());
    gguf_set_val_bool(ctx_gguf, "kv_mean_center.k_rot", k_rot);

    for (const auto & layer : layers) {
        if (layer.bias.empty()) {
            continue;
        }

        const std::string name = "kv_bar.blk." + std::to_string(layer.il) + ".k";

        ggml_tensor * t = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, (int64_t) layer.bias.size());
        ggml_set_name(t, name.c_str());

        memcpy(t->data, layer.bias.data(), layer.bias.size()*sizeof(float));

        gguf_add_tensor(ctx_gguf, t);
    }

    const bool ok = gguf_write_to_file(ctx_gguf, fname.c_str(), false);
    if (!ok) {
        LOG_ERR("%s: failed to write %s\n", __func__, fname.c_str());
    }

    gguf_free(ctx_gguf);
    ggml_free(ctx);

    return ok;
}
