// This file defines tests for various GGML ops and backends.
// For the forward pass it asserts that the results of multiple backends computing the same GGML ops are consistent.
// For the backward pass it asserts that the gradients from backpropagation are consistent
// with the gradients obtained via the method of finite differences ("grad" mode, this is optional).
// It is also possible to check the performance ("perf" mode).
//
// this file has three sections: Section 1 does general setup, section 2 defines the GGML ops to be tested,
// and section 3 defines which tests to run.
// Quick start for adding a new GGML op: Go to section 2 and create a struct that inherits from test_case,
// then go to section 3 and add an instantiation of your struct.


// ##############################
// ## Section 1: General Setup ##
// ##############################


#include "ggml.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"
#include "ggml-cpp.h"

#include <algorithm>
#include <atomic>
#include <array>
#include <cfloat>
#include <cinttypes>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <future>
#include <fstream>
#include <memory>
#include <mutex>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <unordered_map>

#ifdef __EMSCRIPTEN__
#   define N_THREADS 1
#else
#   define N_THREADS std::thread::hardware_concurrency()
#endif

static void init_tensor_uniform(ggml_tensor * tensor, float min = -1.0f, float max = 1.0f) {
    if (ggml_is_empty(tensor)) {
        return;
    }

    size_t nels = ggml_nelements(tensor);
    std::vector<float> data(nels);
    {
        // parallel initialization
        static const size_t n_threads = std::max<size_t>(1, std::min<size_t>(nels/1024, std::min<size_t>(4, N_THREADS/2)));

        auto init_thread = [&](size_t start, size_t end) {
            thread_local std::default_random_engine gen(std::random_device{}());
            std::uniform_real_distribution<float> distribution(min, max);
            for (size_t i = start; i < end; i++) {
                data[i] = distribution(gen);
            }
        };

        if (n_threads == 1) {
            init_thread(0, nels);
        } else {
            std::vector<std::future<void>> tasks;
            tasks.reserve(n_threads);
            for (size_t i = 0; i < n_threads; i++) {
                size_t start =     i*nels/n_threads;
                size_t end   = (i+1)*nels/n_threads;
                tasks.push_back(std::async(std::launch::async, init_thread, start, end));
            }
            for (auto & t : tasks) {
                t.get();
            }
        }
    }

    if (tensor->type == GGML_TYPE_F32 || tensor->type == GGML_TYPE_I32) {
        ggml_backend_tensor_set(tensor, data.data(), 0, nels * sizeof(float));
    } else if (ggml_is_quantized(tensor->type) || tensor->type == GGML_TYPE_F16 || tensor->type == GGML_TYPE_BF16) {
        GGML_ASSERT(nels % ggml_blck_size(tensor->type) == 0);

         // dummy importance matrix
        std::vector<float> imatrix(tensor->ne[0], 1.0f);
        const float * im = imatrix.data();
        if (!ggml_quantize_requires_imatrix(tensor->type)) {
            // when the imatrix is optional, we want to test both quantization with and without imatrix
            // use one of the random numbers to decide
            if (data[0] > 0.5f*(min + max)) {
                im = nullptr;
            }
        }

        std::vector<uint8_t> dataq(ggml_row_size(tensor->type, nels));
        {
            // parallel quantization by block
            size_t blck_size = ggml_blck_size(tensor->type);
            size_t n_blocks = nels / blck_size;

            auto quantize_thread = [&](size_t start, size_t end) {
                ggml_quantize_chunk(tensor->type, data.data(), dataq.data(),
                    start * blck_size, end - start, blck_size, im);
            };

            const size_t min_blocks_per_thread = 1;
            const size_t n_quant_threads = std::min<size_t>(std::max<size_t>(N_THREADS/2, 1),
                                                            std::max<size_t>(1, n_blocks / min_blocks_per_thread));

            if (n_quant_threads == 1) {
                // single-threaded quantization: do all blocks in the current thread
                quantize_thread(0, n_blocks);
            } else {
                std::vector<std::future<void>> tasks;
                tasks.reserve(n_quant_threads);
                for (size_t i = 0; i < n_quant_threads; i++) {
                    size_t start =     i*n_blocks/n_quant_threads;
                    size_t end   = (i+1)*n_blocks/n_quant_threads;
                    tasks.push_back(std::async(std::launch::async, quantize_thread, start, end));
                }
                for (auto & t : tasks) {
                    t.get();
                }
            }
        }
        // A non-contiguous tensor (e.g. a k_v view whose rows are strided over a
        // wider base) must be written row by row. ggml_backend_tensor_set copies
        // `size` bytes contiguously from tensor->data, so a single packed write
        // lays row i at i*row_size instead of i*nb[1], leaving the tail of the
        // logical extent holding whatever was there before.
        if (!ggml_is_contiguous(tensor) && ggml_n_dims(tensor) >= 2) {
            const size_t row_sz = ggml_row_size(tensor->type, tensor->ne[0]);
            const int64_t nrows = ggml_nrows(tensor);
            for (int64_t r = 0; r < nrows; r++) {
                const int64_t i3 =  r / (tensor->ne[1]*tensor->ne[2]);
                const int64_t i2 = (r / tensor->ne[1]) % tensor->ne[2];
                const int64_t i1 =  r % tensor->ne[1];
                const size_t off = i1*tensor->nb[1] + i2*tensor->nb[2] + i3*tensor->nb[3];
                ggml_backend_tensor_set(tensor, dataq.data() + r*row_sz, off, row_sz);
            }
        } else {
            ggml_backend_tensor_set(tensor, dataq.data(), 0, dataq.size());
        }
    } else if (tensor->type == GGML_TYPE_I8 || tensor->type == GGML_TYPE_I16) {
        // This is going to create some weird integers though.
        ggml_backend_tensor_set(tensor, data.data(), 0, nels * ggml_type_size(tensor->type));
    } else if (tensor->type == GGML_TYPE_I64) {
        // Integers with a size of 8 bytes can be set by mirroring the float data, the specific values are again not really meaningful.
        const size_t nbytes_half = nels * sizeof(float);
        ggml_backend_tensor_set(tensor, data.data(), 0*nbytes_half, nbytes_half);
        ggml_backend_tensor_set(tensor, data.data(), 1*nbytes_half, nbytes_half);
    } else {
        GGML_ABORT("fatal error");
    }
}

// generate an F16 mask where certain blocks are randomly masked with -INF value
static void init_tensor_kq_mask(ggml_tensor * tensor, float min = -1.0f, float max = 1.0f) {
    GGML_ASSERT(tensor->type == GGML_TYPE_F16);

    GGML_TENSOR_LOCALS( int32_t, ne, tensor, ne);

    std::vector<float>       data_f32(ne0*ne1*ne2*ne3);
    std::vector<ggml_fp16_t> data_f16(ne0*ne1*ne2*ne3);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(min, max);

    for (size_t i = 0; i < data_f32.size(); i++) {
        data_f32[i] = dis(gen);
    }

    // block size
    const int blck0 = 128;
    const int blck1 = 64;

    // number of INF/zero blocks
    const int n_inf_zero_blocks = 0.2*(ne0*ne1*ne2*ne3)/(blck0*blck1);

    for (int b = 0; b < n_inf_zero_blocks; b++) {
        const int p3 = (rd() % ne3);
        const int p2 = (rd() % ne2);
        const int p1 = (rd() % ne1);
        const int p0 = (rd() % ne0);

        bool inf = rd() & 1;

        for (int i1 = 0; i1 < blck1 && p1 + i1 < ne1; i1++) {
            const int idx = p3*ne2*ne1*ne0 + p2*ne1*ne0 + (p1 + i1)*ne0 + p0;

            for (int i0 = 0; i0 < blck0 && p0 + i0 < ne0; i0++) {
                data_f32[idx + i0] = inf ? -INFINITY : 0.0f;
            }
        }
    }

    ggml_fp32_to_fp16_row(data_f32.data(), data_f16.data(), ne0*ne1*ne2*ne3);

    ggml_backend_tensor_set(tensor, data_f16.data(), 0, data_f16.size()*sizeof(ggml_fp16_t));
}

static void init_tensor_kq_mask_sparse(ggml_tensor * tensor, int64_t n_kv_max) {
    GGML_ASSERT(tensor->type == GGML_TYPE_F16);
    GGML_ASSERT(n_kv_max > 0 && n_kv_max <= tensor->ne[0]);

    const int64_t ne0 = tensor->ne[0];
    const int64_t nrows = ggml_nrows(tensor);
    std::vector<float> data_f32(ggml_nelements(tensor), -INFINITY);
    std::vector<ggml_fp16_t> data_f16(ggml_nelements(tensor));
    std::vector<int32_t> order(ne0);
    for (int64_t i = 0; i < ne0; ++i) {
        order[i] = i;
    }

    std::mt19937 gen(0x5A17);
    for (int64_t row = 0; row < nrows; ++row) {
        std::shuffle(order.begin(), order.end(), gen);
        const int64_t count = n_kv_max - row % std::min<int64_t>(n_kv_max, 17);
        std::sort(order.begin(), order.begin() + count);
        for (int64_t i = 0; i < count; ++i) {
            data_f32[row*ne0 + order[i]] = -0.03125f * (1 + (i + row) % 7);
        }
    }

    ggml_fp32_to_fp16_row(data_f32.data(), data_f16.data(), data_f16.size());
    ggml_backend_tensor_set(tensor, data_f16.data(), 0, data_f16.size()*sizeof(ggml_fp16_t));
}

// generate a lower triangular matrix
static void init_tensor_tril(ggml_tensor * tensor, float min = -1.0f, float max = 1.0f) {
    GGML_ASSERT(tensor->type == GGML_TYPE_F32);
    GGML_ASSERT(tensor->ne[0] == tensor->ne[1]);

    GGML_TENSOR_LOCALS(int32_t, ne, tensor, ne);
    GGML_TENSOR_LOCALS(size_t, nb, tensor, nb);

    std::vector<float> data_f32(ne0*ne1*ne2*ne3);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(min, max);

    for (int64_t i3 = 0; i3 < ne3; i3++) {
        for (int64_t i2 = 0; i2 < ne2; i2++) {
            for (int64_t i1 = 0; i1 < ne1; i1++) {
                for (int64_t i0 = 0; i0 < ne0; i0++) {
                    int64_t idx = (i0 * nb0 + i1 * nb1 + i2 * nb2 + i3 * nb3) / sizeof(float);
                    if (i0 <= i1) {
                        data_f32[idx] = dis(gen);
                    } else {
                        data_f32[idx] = 0.0f;
                    }
                }
            }
        }
    }

    ggml_backend_tensor_set(tensor, data_f32.data(), 0, ggml_nbytes(tensor));
}

static std::vector<float> tensor_to_float(const ggml_tensor * t) {
    std::vector<float> tv;
    tv.reserve(ggml_nelements(t));

    std::vector<uint8_t> buf(ggml_nbytes(t));
    ggml_backend_tensor_get(t, buf.data(), 0, ggml_nbytes(t));

    const auto * tt = ggml_get_type_traits(t->type);
    size_t bs = ggml_blck_size(t->type);
    std::vector<float> vq(ggml_blck_size(t->type));
    bool quantized = ggml_is_quantized(t->type);

    // access elements by index to avoid gaps in views
    for (int64_t i3 = 0; i3 < t->ne[3]; i3++) {
        for (int64_t i2 = 0; i2 < t->ne[2]; i2++) {
            for (int64_t i1 = 0; i1 < t->ne[1]; i1++) {
                for (int64_t i0 = 0; i0 < t->ne[0]; i0 += bs) {
                    size_t i = i3*t->nb[3] + i2*t->nb[2] + i1*t->nb[1] + i0/bs*t->nb[0];
                    if (t->type == GGML_TYPE_F16) {
                        tv.push_back(ggml_fp16_to_fp32(*(ggml_fp16_t*)&buf[i]));
                    } else if (t->type == GGML_TYPE_BF16) {
                        tv.push_back(ggml_bf16_to_fp32(*(ggml_bf16_t*)&buf[i]));
                    } else if (t->type == GGML_TYPE_F32) {
                        tv.push_back(*(float *) &buf[i]);
                    } else if (t->type == GGML_TYPE_I64) {
                        tv.push_back((float)*(int64_t *) &buf[i]);
                    } else if (t->type == GGML_TYPE_I32) {
                        tv.push_back((float)*(int32_t *) &buf[i]);
                    } else if (t->type == GGML_TYPE_I16) {
                        tv.push_back((float)*(int16_t *) &buf[i]);
                    } else if (t->type == GGML_TYPE_I8) {
                        tv.push_back((float)*(int8_t *) &buf[i]);
                    } else if (quantized) {
                        tt->to_float(&buf[i], vq.data(), bs);
                        tv.insert(tv.end(), vq.begin(), vq.end());
                    } else {
                        GGML_ABORT("fatal error");
                    }
                }
            }
        }
    }

    return tv;
}

// normalized mean squared error = mse(a, b) / mse(a, 0)
static double nmse(const float * a, const float * b, size_t n) {
    double mse_a_b = 0.0;
    double mse_a_0 = 0.0;

    for (size_t i = 0; i < n; i++) {
        float a_i = a[i];
        float b_i = b[i];

        mse_a_b += (a_i - b_i) * (a_i - b_i);
        mse_a_0 += a_i * a_i;
    }

    return mse_a_b / mse_a_0;
}

// difference between 2 sets (Jaccard distance, 0 - no difference, 1 - no overlap)
template <typename T>
static double jdst(const T * a, const T * b, size_t n) {
    std::unordered_map<T, size_t> set_a;
    std::unordered_map<T, size_t> set_b;

    for (size_t i = 0; i < n; ++i) {
        set_a[a[i]]++;
        set_b[b[i]]++;
    }

    size_t diff = 0;

    for (const auto & p : set_a) {
        const int64_t na = p.second;
        const int64_t nb = set_b.find(p.first) != set_b.end() ? set_b.at(p.first) : 0;

        diff += std::abs(na - nb);
    }

    for (const auto & p : set_b) {
        if (set_a.find(p.first) == set_a.end()) {
            diff += p.second;
        }
    }

    return (double) diff / (2*n);
}

// maximum absolute asymmetry between a and b
// asymmetry: (a - b) / (a + b)
// This is more stable than relative error if one of the values fluctuates towards zero.
// n: number of values to compare.
// expected_vals: optional vector of expected values for a. If expected_vals is not empty, filter out all comparisons where
//     a does not match any of the expected values. Needed for noncontinuous gradients where the numerical calculation can fail.
static double mean_abs_asymm(const float * a, const float * b, const size_t n, const std::vector<float> & expected_vals) {
    double sum = 0.0f;

    size_t nvalid = 0;
    for (size_t i = 0; i < n; i++) {
        if (!expected_vals.empty()) {
            bool matches_any = false;
            for (const float & ev : expected_vals) {
                if (fabsf(a[i] - ev) < 1e-3f) {
                    matches_any = true;
                    break;
                }
            }
            if (!matches_any) {
                continue;
            }
        }

        const float asymm = (a[i] - b[i]) / (a[i] + b[i]);

        sum += fabsf(asymm);
        nvalid++;
    }

    return sum/nvalid;
}

// utils for printing the variables of the test cases

static std::string var_to_str(const std::string & x) {
    return x;
}

template<typename T>
static std::string var_to_str(const T & x) {
    return std::to_string(x);
}

template<typename T, size_t N>
static std::string var_to_str(const T (&x)[N]) {
    std::string s = "[";
    for (size_t i = 0; i < N; i++) {
        if (i > 0) {
            s += ",";
        }
        s += var_to_str(x[i]);
    }
    s += "]";
    return s;
}

template<typename T, size_t N>
static std::string var_to_str(const std::array<T, N> & x) {
    std::string s = "[";
    for (size_t i = 0; i < N; i++) {
        if (i > 0) {
            s += ",";
        }
        s += var_to_str(x[i]);
    }
    s += "]";
    return s;
}

static std::string var_to_str(ggml_type type) {
    return ggml_type_name(type);
}

static std::string var_to_str(ggml_prec prec) {
    return prec == GGML_PREC_F32 ? "f32" : "def";
}

static std::string var_to_str(ggml_op_pool pool) {
    switch (pool) {
        case GGML_OP_POOL_AVG:  return "avg";
        case GGML_OP_POOL_MAX:  return "max";
        default:                return std::to_string(pool);
    }
}

static std::string var_to_str(ggml_scale_mode mode) {
    std::string str;
    switch (mode & 0xFF) {
        case GGML_SCALE_MODE_NEAREST:  str = "nearest"; break;
        case GGML_SCALE_MODE_BILINEAR: str = "bilinear"; break;
        case GGML_SCALE_MODE_BICUBIC:  str = "bicubic"; break;
        default:                       str = std::to_string(mode); break;
    }
    if (mode & GGML_SCALE_FLAG_ALIGN_CORNERS) {
        str += "|align_corners";
    }
    if (mode & GGML_SCALE_FLAG_ANTIALIAS) {
        str += "|antialias";
    }
    return str;
}

#define VAR_TO_STR(x) (#x "=" + var_to_str(x))

#define VARS_TO_STR1(a) VAR_TO_STR(a)
#define VARS_TO_STR2(a, b) VAR_TO_STR(a) + "," + VAR_TO_STR(b)
#define VARS_TO_STR3(a, b, c) VAR_TO_STR(a) + "," + VARS_TO_STR2(b, c)
#define VARS_TO_STR4(a, b, c, d) VAR_TO_STR(a) + "," + VARS_TO_STR3(b, c, d)
#define VARS_TO_STR5(a, b, c, d, e) VAR_TO_STR(a) + "," + VARS_TO_STR4(b, c, d, e)
#define VARS_TO_STR6(a, b, c, d, e, f) VAR_TO_STR(a) + "," + VARS_TO_STR5(b, c, d, e, f)
#define VARS_TO_STR7(a, b, c, d, e, f, g) VAR_TO_STR(a) + "," + VARS_TO_STR6(b, c, d, e, f, g)
#define VARS_TO_STR8(a, b, c, d, e, f, g, h) VAR_TO_STR(a) + "," + VARS_TO_STR7(b, c, d, e, f, g, h)
#define VARS_TO_STR9(a, b, c, d, e, f, g, h, i) VAR_TO_STR(a) + "," + VARS_TO_STR8(b, c, d, e, f, g, h, i)
#define VARS_TO_STR10(a, b, c, d, e, f, g, h, i, j) VAR_TO_STR(a) + "," + VARS_TO_STR9(b, c, d, e, f, g, h, i, j)
#define VARS_TO_STR11(a, b, c, d, e, f, g, h, i, j, k) VAR_TO_STR(a) + "," + VARS_TO_STR10(b, c, d, e, f, g, h, i, j, k)
#define VARS_TO_STR12(a, b, c, d, e, f, g, h, i, j, k, l) VAR_TO_STR(a) + "," + VARS_TO_STR11(b, c, d, e, f, g, h, i, j, k, l)
#define VARS_TO_STR13(a, b, c, d, e, f, g, h, i, j, k, l, m) VAR_TO_STR(a) + "," + VARS_TO_STR12(b, c, d, e, f, g, h, i, j, k, l, m)
#define VARS_TO_STR14(a, b, c, d, e, f, g, h, i, j, k, l, m, n) VAR_TO_STR(a) + "," + VARS_TO_STR13(b, c, d, e, f, g, h, i, j, k, l, m, n)
#define VARS_TO_STR15(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o) VAR_TO_STR(a) + "," + VARS_TO_STR14(b, c, d, e, f, g, h, i, j, k, l, m, n, o)
#define VARS_TO_STR16(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p) VAR_TO_STR(a) + "," + VARS_TO_STR15(b, c, d, e, f, g, h, i, j, k, l, m, n, o, p)
#define VARS_TO_STR17(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q) VAR_TO_STR(a) + "," + VARS_TO_STR16(b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q)

// accept FLT_MAX as infinity
static bool isinf_or_max(float f) {
    return std::isinf(f) || f == FLT_MAX || f == -FLT_MAX;
}

static bool ggml_is_view_op(enum ggml_op op) {
    return op == GGML_OP_VIEW || op == GGML_OP_RESHAPE || op == GGML_OP_PERMUTE || op == GGML_OP_TRANSPOSE;
}

static bool backend_has_feature(ggml_backend_t backend, const char * feature_name) {
    ggml_backend_dev_t dev = ggml_backend_get_device(backend);
    ggml_backend_reg_t reg = ggml_backend_dev_backend_reg(dev);

    auto get_features = (ggml_backend_get_features_t) ggml_backend_reg_get_proc_address(reg, "ggml_backend_get_features");
    if (!get_features) {
        return false;
    }

    const ggml_backend_feature * features = get_features(reg);
    if (!features) {
        return false;
    }

    for (const ggml_backend_feature * f = features; f->name; ++f) {
        if (strcmp(f->name, feature_name) == 0 && strcmp(f->value, "1") == 0) {
            return true;
        }
    }
    return false;
}

enum test_mode {
    MODE_TEST,
    MODE_PERF,
    MODE_GRAD,
    MODE_SUPPORT,
    MODE_SX8REF,   // ArifiLabs lane-230 / R46b B2: S-X8 vs an independent reference
};

// Output format support similar to llama-bench
enum output_formats { CONSOLE, SQL, CSV };

static const char * output_format_str(output_formats format) {
    switch (format) {
        case CONSOLE:
            return "console";
        case SQL:
            return "sql";
        case CSV:
            return "csv";
        default:
            GGML_ABORT("invalid output format");
    }
}

static bool output_format_from_str(const std::string & s, output_formats & format) {
    if (s == "console") {
        format = CONSOLE;
    } else if (s == "sql") {
        format = SQL;
    } else if (s == "csv") {
        format = CSV;
    } else {
        return false;
    }
    return true;
}

static std::string test_time_now() {
    time_t t = time(NULL);
    struct tm tm_buf;
#ifdef _WIN32
    if (gmtime_s(&tm_buf, &t) != 0) {
        return "";
    }
#else
    if (gmtime_r(&t, &tm_buf) == nullptr) {
        return "";
    }
#endif
    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%FT%TZ", &tm_buf) == 0) {
        return "";
    }
    return buf;
}

// Test result structure for SQL output
struct test_result {
    std::string test_time;
    std::string build_commit;
    std::string backend_name;
    std::string op_name;
    std::string op_params;
    std::string test_mode;
    bool        supported;
    bool        passed;
    std::string error_message;
    double      time_us;
    double      flops;
    double      bandwidth_gb_s;
    size_t      memory_kb;
    int         n_runs;
    std::string device_description;
    std::string backend_reg_name;

    test_result() {
        // Initialize with default values
        time_us        = 0.0;
        flops          = 0.0;
        bandwidth_gb_s = 0.0;
        memory_kb      = 0;
        n_runs         = 0;
        supported      = false;
        passed         = false;

        test_time = test_time_now();

        // Set build info
        build_commit = ggml_commit();
    }

    test_result(const std::string & backend_name, const std::string & op_name, const std::string & op_params,
                const std::string & test_mode, bool supported, bool passed, const std::string & error_message = "",
                double time_us = 0.0, double flops = 0.0, double bandwidth_gb_s = 0.0, size_t memory_kb = 0,
                int n_runs = 0, const std::string & device_description = "", const std::string & backend_reg_name = "") :
        backend_name(backend_name),
        op_name(op_name),
        op_params(op_params),
        test_mode(test_mode),
        supported(supported),
        passed(passed),
        error_message(error_message),
        time_us(time_us),
        flops(flops),
        bandwidth_gb_s(bandwidth_gb_s),
        memory_kb(memory_kb),
        n_runs(n_runs),
        device_description(device_description),
        backend_reg_name(backend_reg_name) {
        test_time = test_time_now();

        // Set build info
        build_commit = ggml_commit();
    }

    static const std::vector<std::string> & get_fields() {
        static const std::vector<std::string> fields = {
            "test_time", "build_commit",  "backend_name", "op_name", "op_params",      "test_mode", "supported",
            "passed",    "error_message", "time_us",      "flops",   "bandwidth_gb_s", "memory_kb", "n_runs",
            "device_description", "backend_reg_name"
        };
        return fields;
    }

    enum field_type { STRING, BOOL, INT, FLOAT };

    static field_type get_field_type(const std::string & field) {
        if (field == "supported" || field == "passed") {
            return BOOL;
        }
        if (field == "memory_kb" || field == "n_runs") {
            return INT;
        }
        if (field == "time_us" || field == "flops" || field == "bandwidth_gb_s") {
            return FLOAT;
        }
        return STRING;
    }

    std::vector<std::string> get_values() const {
        return { test_time,
                 build_commit,
                 backend_name,
                 op_name,
                 op_params,
                 test_mode,
                 std::to_string(supported),
                 std::to_string(passed),
                 error_message,
                 std::to_string(time_us),
                 std::to_string(flops),
                 std::to_string(bandwidth_gb_s),
                 std::to_string(memory_kb),
                 std::to_string(n_runs),
                 device_description,
                 backend_reg_name };
    }
};

// Printer classes for different output formats
enum class test_status_t { NOT_SUPPORTED, OK, FAIL, SKIPPED };

struct test_operation_info {
    std::string   op_name;
    std::string   op_params;
    std::string   backend_name;
    test_status_t status = test_status_t::OK;
    std::string   failure_reason;

    // Additional information fields that were previously in separate structs
    std::string error_component;
    std::string error_details;

    // Gradient info
    int64_t     gradient_index = -1;
    std::string gradient_param_name;
    float       gradient_value = 0.0f;

    // MAA error info
    double maa_error     = 0.0;
    double maa_threshold = 0.0;

    // Flags for different types of information
    bool has_error            = false;
    bool has_gradient_info    = false;
    bool has_maa_error        = false;
    bool is_compare_failure   = false;
    bool is_large_tensor_skip = false;

    test_operation_info() = default;

    test_operation_info(const std::string & op_name, const std::string & op_params, const std::string & backend_name,
                        test_status_t status = test_status_t::OK, const std::string & failure_reason = "") :
        op_name(op_name),
        op_params(op_params),
        backend_name(backend_name),
        status(status),
        failure_reason(failure_reason) {}

    // Set error information
    void set_error(const std::string & component, const std::string & details) {
        has_error       = true;
        error_component = component;
        error_details   = details;
        if (status == test_status_t::OK) {
            status = test_status_t::FAIL;
        }
    }

    // Set gradient information
    void set_gradient_info(int64_t index, const std::string & param_name, float value) {
        has_gradient_info   = true;
        gradient_index      = index;
        gradient_param_name = param_name;
        gradient_value      = value;
        if (status == test_status_t::OK) {
            status = test_status_t::FAIL;
        }
    }

    // Set MAA error information
    void set_maa_error(double error, double threshold) {
        has_maa_error = true;
        maa_error     = error;
        maa_threshold = threshold;
        if (status == test_status_t::OK) {
            status = test_status_t::FAIL;
        }
    }

    // Set compare failure
    void set_compare_failure() {
        is_compare_failure = true;
        if (status == test_status_t::OK) {
            status = test_status_t::FAIL;
        }
    }

    // Set large tensor skip
    void set_large_tensor_skip() { is_large_tensor_skip = true; }
};

struct test_summary_info {
    size_t tests_passed;
    size_t tests_total;
    bool   is_backend_summary = false;  // true for backend summary, false for test summary
    // lane-149 / failure-ledger F-112: cases the backend DECLINED never reach tests_total, so a run
    // in which every case was declined printed "0/0 tests passed" and then "OK" — a green banner
    // over zero executed coverage. They are counted here and named on their own line; the
    // "%zu/%zu tests passed" line above is left byte-identical because sweep_table.py parses it.
    size_t tests_not_supported = 0;     // graph contained a node the backend does not support
    size_t tests_filtered      = 0;     // did not match -o/-t, never intended to run

    test_summary_info() = default;

    test_summary_info(size_t tests_passed, size_t tests_total, bool is_backend_summary = false) :
        tests_passed(tests_passed),
        tests_total(tests_total),
        is_backend_summary(is_backend_summary) {}
};

struct testing_start_info {
    size_t device_count;

    testing_start_info() = default;

    testing_start_info(size_t device_count) : device_count(device_count) {}
};

struct backend_init_info {
    size_t      device_index;
    size_t      total_devices;
    std::string device_name;
    bool        skipped = false;
    std::string skip_reason;
    std::string description;
    size_t      memory_total_mb = 0;
    size_t      memory_free_mb  = 0;
    bool        has_memory_info = false;

    backend_init_info() = default;

    backend_init_info(size_t device_index, size_t total_devices, const std::string & device_name, bool skipped = false,
                      const std::string & skip_reason = "", const std::string & description = "",
                      size_t memory_total_mb = 0, size_t memory_free_mb = 0, bool has_memory_info = false) :
        device_index(device_index),
        total_devices(total_devices),
        device_name(device_name),
        skipped(skipped),
        skip_reason(skip_reason),
        description(description),
        memory_total_mb(memory_total_mb),
        memory_free_mb(memory_free_mb),
        has_memory_info(has_memory_info) {}
};

struct backend_status_info {
    std::string   backend_name;
    test_status_t status;

    backend_status_info() = default;

    backend_status_info(const std::string & backend_name, test_status_t status) :
        backend_name(backend_name),
        status(status) {}
};

struct overall_summary_info {
    size_t backends_passed;
    size_t backends_total;
    bool   all_passed;

    overall_summary_info() = default;

    overall_summary_info(size_t backends_passed, size_t backends_total, bool all_passed) :
        backends_passed(backends_passed),
        backends_total(backends_total),
        all_passed(all_passed) {}
};

struct printer {
    virtual ~printer() {}

    FILE * fout = stdout;

    virtual void print_header() {}

    virtual void print_test_result(const test_result & result) = 0;

    virtual void print_footer() {}

    virtual void print_operation(const test_operation_info & info) { (void) info; }

    virtual void print_summary(const test_summary_info & info) { (void) info; }

    virtual void print_testing_start(const testing_start_info & info) { (void) info; }

    virtual void print_backend_init(const backend_init_info & info) { (void) info; }

    virtual void print_backend_status(const backend_status_info & info) { (void) info; }

    virtual void print_overall_summary(const overall_summary_info & info) { (void) info; }

    virtual void print_failed_tests(const std::vector<std::string> & failed_tests) { (void) failed_tests; }
};

struct console_printer : public printer {
    void print_test_result(const test_result & result) override {
        if (result.test_mode == "test") {
            print_test_console(result);
        } else if (result.test_mode == "perf") {
            print_perf_console(result);
        } else if (result.test_mode == "support") {
            print_support_console(result);
        }
        fflush(stdout);
    }

    void print_operation(const test_operation_info & info) override {
        printf("  %s(%s): ", info.op_name.c_str(), info.op_params.c_str());
        fflush(stdout);

        // Handle large tensor skip first
        if (info.is_large_tensor_skip) {
            printf("skipping large tensors for speed \n");
            fflush(stdout);
            return;
        }

        // Handle not supported status
        if (info.status == test_status_t::NOT_SUPPORTED) {
            if (!info.failure_reason.empty()) {
                printf("not supported [%s]\n", info.failure_reason.c_str());
            } else {
                printf("not supported [%s]\n", info.backend_name.c_str());
            }
            fflush(stdout);
            return;
        }

        // Handle errors and additional information
        if (info.has_error) {
            if (info.error_component == "allocation") {
                fprintf(stderr, "failed to allocate tensors [%s] ", info.backend_name.c_str());
            } else if (info.error_component == "backend") {
                fprintf(stderr, "  Failed to initialize %s backend\n", info.backend_name.c_str());
            } else {
                fprintf(stderr, "Error in %s: %s\n", info.error_component.c_str(), info.error_details.c_str());
            }
        }

        // Handle gradient info
        if (info.has_gradient_info) {
            printf("[%s] nonfinite gradient at index %" PRId64 " (%s=%f) ", info.op_name.c_str(), info.gradient_index,
                   info.gradient_param_name.c_str(), info.gradient_value);
        }

        // Handle MAA error
        if (info.has_maa_error) {
            printf("[%s] MAA = %.9f > %.9f ", info.op_name.c_str(), info.maa_error, info.maa_threshold);
        }

        // Handle compare failure
        if (info.is_compare_failure) {
            printf("compare failed ");
        }

        // Print final status
        if (info.status == test_status_t::OK) {
            printf("\033[1;32mOK\033[0m\n");
        } else {
            printf("\033[1;31mFAIL\033[0m\n");
        }
        fflush(stdout);
    }

    void print_summary(const test_summary_info & info) override {
        if (info.is_backend_summary) {
            printf("%zu/%zu backends passed\n", info.tests_passed, info.tests_total);
        } else {
            printf("  %zu/%zu tests passed\n", info.tests_passed, info.tests_total);
            // F-112: name what did NOT run. Silence here is what let SET_ROWS_TURBO4 look green
            // for 21 cases while executing none of them.
            if (info.tests_not_supported > 0) {
                printf("  %zu case(s) NOT SUPPORTED by the backend — DECLINED, never executed\n",
                       info.tests_not_supported);
            }
            if (info.tests_total == 0 && info.tests_not_supported > 0) {
                printf("  \033[1;31mNO CASES EXECUTED\033[0m: every case matching the filter was "
                       "declined. A pass banner here would mean 'nothing failed', not 'something ran'.\n");
            }
        }
    }

    void print_backend_status(const backend_status_info & info) override {
        printf("  Backend %s: ", info.backend_name.c_str());
        if (info.status == test_status_t::OK) {
            printf("\033[1;32mOK\033[0m\n");
        } else {
            printf("\033[1;31mFAIL\033[0m\n");
        }
    }

    void print_testing_start(const testing_start_info & info) override {
        printf("Testing %zu devices\n\n", info.device_count);
    }

    void print_backend_init(const backend_init_info & info) override {
        printf("Backend %zu/%zu: %s\n", info.device_index + 1, info.total_devices, info.device_name.c_str());

        if (info.skipped) {
            printf("  %s\n", info.skip_reason.c_str());
            return;
        }

        if (!info.description.empty()) {
            printf("  Device description: %s\n", info.description.c_str());
        }

        if (info.has_memory_info) {
            printf("  Device memory: %zu MB (%zu MB free)\n", info.memory_total_mb, info.memory_free_mb);
        }

        printf("\n");
    }

    void print_overall_summary(const overall_summary_info & info) override {
        printf("%zu/%zu backends passed\n", info.backends_passed, info.backends_total);
        if (info.all_passed) {
            printf("\033[1;32mOK\033[0m\n");
        } else {
            printf("\033[1;31mFAIL\033[0m\n");
        }
    }

    void print_failed_tests(const std::vector<std::string> & failed_tests) override {
        if (failed_tests.empty()) {
            return;
        }

        printf("\nFailing tests:\n");
        for (const auto & test_name : failed_tests) {
            printf("  %s\n", test_name.c_str());
        }
    }

  private:
    void print_test_console(const test_result & result) {
        printf("  %s(%s): ", result.op_name.c_str(), result.op_params.c_str());
        fflush(stdout);

        if (!result.supported) {
            printf("not supported [%s] ", result.backend_name.c_str());
            printf("\n");
            return;
        }

        if (result.passed) {
            printf("\033[1;32mOK\033[0m\n");
        } else {
            printf("\033[1;31mFAIL\033[0m\n");
        }
    }

    void print_perf_console(const test_result & result) {
        int len = printf("  %s(%s): ", result.op_name.c_str(), result.op_params.c_str());
        fflush(stdout);

        if (!result.supported) {
            printf("not supported\n");
            return;
        }

        // align while also leaving some margin for variations in parameters
        int align = 8;
        int last  = (len + align - 1) / align * align;
        if (last - len < 5) {
            last += align;
        }
        printf("%*s", last - len, "");

        printf("    %8d runs - %8.2f us/run - ", result.n_runs, result.time_us);

        if (result.flops > 0) {
            auto format_flops = [](double flops) -> std::string {
                char buf[256];
                if (flops >= 1e12) {
                    snprintf(buf, sizeof(buf), "%6.2f TFLOP", flops / 1e12);
                } else if (flops >= 1e9) {
                    snprintf(buf, sizeof(buf), "%6.2f GFLOP", flops / 1e9);
                } else if (flops >= 1e6) {
                    snprintf(buf, sizeof(buf), "%6.2f MFLOP", flops / 1e6);
                } else {
                    snprintf(buf, sizeof(buf), "%6.2f kFLOP", flops / 1e3);
                }
                return buf;
            };
            uint64_t op_flops_per_run = result.flops * result.time_us / 1e6;
            printf("%s/run - \033[1;34m%sS\033[0m", format_flops(op_flops_per_run).c_str(),
                   format_flops(result.flops).c_str());
        } else {
            printf("%8zu kB/run - \033[1;34m%7.2f GB/s\033[0m", result.memory_kb, result.bandwidth_gb_s);
        }
        printf("\n");
    }

    void print_support_console(const test_result & result) {
        printf("  %s(%s): ", result.op_name.c_str(), result.op_params.c_str());
        fflush(stdout);

        if (result.supported) {
            printf("\033[1;32mSUPPORTED\033[0m\n");
        } else {
            printf("\033[1;31mNOT SUPPORTED\033[0m\n");
        }
    }
};

struct sql_printer : public printer {
    static std::string get_sql_field_type(const std::string & field) {
        switch (test_result::get_field_type(field)) {
            case test_result::STRING:
                return "TEXT";
            case test_result::BOOL:
            case test_result::INT:
                return "INTEGER";
            case test_result::FLOAT:
                return "REAL";
            default:
                GGML_ABORT("invalid field type");
        }
    }

    void print_header() override {
        std::vector<std::string> fields = test_result::get_fields();
        fprintf(fout, "CREATE TABLE IF NOT EXISTS test_backend_ops (\n");
        for (size_t i = 0; i < fields.size(); i++) {
            fprintf(fout, "  %s %s%s\n", fields[i].c_str(), get_sql_field_type(fields[i]).c_str(),
                    i < fields.size() - 1 ? "," : "");
        }
        fprintf(fout, ");\n\n");
    }

    void print_test_result(const test_result & result) override {
        fprintf(fout, "INSERT INTO test_backend_ops (");
        std::vector<std::string> fields = test_result::get_fields();
        for (size_t i = 0; i < fields.size(); i++) {
            fprintf(fout, "%s%s", fields[i].c_str(), i < fields.size() - 1 ? ", " : "");
        }
        fprintf(fout, ") VALUES (");
        std::vector<std::string> values = result.get_values();
        for (size_t i = 0; i < values.size(); i++) {
            fprintf(fout, "'%s'%s", values[i].c_str(), i < values.size() - 1 ? ", " : "");
        }
        fprintf(fout, ");\n");
    }
};

struct csv_printer : public printer {
    void print_header() override {

        std::vector<std::string> fields     = test_result::get_fields();
        std::vector<std::string> fields_csv = get_fields_csv();
        for (size_t i = 0; i < fields.size(); i++) {
            if (std::find(std::begin(fields_csv), std::end(fields_csv), fields[i]) == std::end(fields_csv)) {
                continue;
            }
            printf("\"%s\"%s", fields[i].c_str(), i < fields.size() - 1 ? "," : "");
        }
        printf("\n");
    }

    void print_test_result(const test_result & result) override {

        std::vector<std::string> values     = result.get_values();
        std::vector<std::string> fields     = test_result::get_fields();
        std::vector<std::string> fields_csv = get_fields_csv();

        for (size_t i = 0; i < values.size(); i++) {

            if (std::find(std::begin(fields_csv), std::end(fields_csv), fields[i]) == std::end(fields_csv)) {
                continue;
            }

            // Escape quotes and wrap in quotes for CSV
            std::string escaped_value = values[i];
            size_t pos = 0;
            while ((pos = escaped_value.find("\"", pos)) != std::string::npos) {
                escaped_value.replace(pos, 1, "\"\"");
                pos += 2;
            }
            printf("\"%s\"%s", escaped_value.c_str(), i < values.size() - 1 ? "," : "");
        }
        printf("\n");
    }

    static std::vector<std::string> get_fields_csv() {
        return {
            "op_name",
            "op_params",
            "supported",
            "error_message",
            "test_mode",
            "backend_reg_name",
            "backend_name",
        };
    }

};

static std::unique_ptr<printer> create_printer(output_formats format) {
    switch (format) {
        case CONSOLE:
            return std::make_unique<console_printer>();
        case SQL:
            return std::make_unique<sql_printer>();
        case CSV:
            return std::make_unique<csv_printer>();
    }
    GGML_ABORT("invalid output format");
}

static std::mutex g_test_output_mutex;

static void print_test_result_locked(printer * output_printer, const test_result & result) {
    if (output_printer == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> guard(g_test_output_mutex);
    output_printer->print_test_result(result);
}

// Splits the -o filter into comma separated entries. Commas inside parentheses
// (i.e. inside a full test case string) are not treated as separators.
static std::vector<std::string_view> op_filter_entries(const char * op_names_filter) {
    std::vector<std::string_view> entries;
    if (op_names_filter == nullptr) {
        return entries;
    }
    std::string_view filter(op_names_filter);
    while (!filter.empty()) {
        auto comma_pos = filter.find_first_of(',');
        const auto lparen_pos = filter.find_first_of('(');
        if (lparen_pos < comma_pos) {
            const auto rparen_pos = filter.find_first_of(')');
            comma_pos = filter.find_first_of(',', rparen_pos);
        }
        entries.push_back(filter.substr(0, comma_pos));
        filter = comma_pos != std::string_view::npos ? filter.substr(comma_pos + 1) : "";
    }
    return entries;
}

// An entry from the -o filter matches an op if it is either
//   * an exact op name as given by ggml_op_desc() (e.g. "ADD"), or
//   * a regex that matches the op name (e.g. "DSV4.*")
static bool op_filter_entry_matches(std::string_view entry, std::string_view op_name) {
    if (entry == op_name) {
        return true;
    }
    // plain op names are matched exactly, anything else is treated as a regex
    if (std::regex_match(std::string(entry), std::regex("[A-Z0-9_]+"))) {
        return false;
    }
    std::regex re;
    try {
        re = std::regex(std::string(entry));
    } catch (const std::regex_error &) {
        return false;
    }
    return std::regex_search(op_name.data(), op_name.data() + op_name.size(), re);
}

struct test_case {
    virtual ~test_case() {}

    virtual std::string op_desc(ggml_tensor * t) {
        return ggml_op_desc(t);
    }

    virtual std::string vars() {
        return "";
    }

    virtual ggml_tensor * build_graph(ggml_context * ctx) = 0;
    virtual ggml_tensor * build_graph(ggml_context * ctx, ggml_context * ctx_weights) {
        GGML_UNUSED(ctx_weights);
        return build_graph(ctx);
    }

    virtual double max_nmse_err() {
        return 1e-7;
    }

    virtual double max_nmse_err(ggml_backend_t backend) {
        ggml_backend_reg_t reg = ggml_backend_dev_backend_reg(ggml_backend_get_device(backend));
        // See https://github.com/ggml-org/llama.cpp/pull/22976 for explanation.
        if (contains_f16 && strcmp(ggml_backend_reg_name(reg), "WebGPU") == 0) {
            return std::max(max_nmse_err(), 1e-6);
        }
        return max_nmse_err();
    }

    virtual double max_maa_err() {
        return 1e-4;
    }

    virtual double max_err() {
        return max_nmse_err();
    }

    virtual double max_err(ggml_backend_t backend) {
        return max_nmse_err(backend);
    }

    virtual double err(const float * a, const float * b, size_t n) {
        return nmse(a, b, n);
    }

    virtual float grad_eps() {
        return 1e-1f;
    }

    // If false, estimate gradient with 2 points, neglects 3rd order derivative and higher.
    // If true,  estimate gradient with 4 points, neglects 5th order derivative and higher.
    virtual bool grad_precise() {
        return false;
    }

    // Skip gradient checks if total number of gradients to be checked is larger than this (to speed up the tests).
    virtual int64_t grad_nmax() {
        return 10000;
    }

    // No effect if empty.
    // If not empty, skip all gradient checks where the numerical result does not match any of the values.
    // Needed for dealing with noncontinuous gradients (e.g. ReLU) where estimation using finite differences is unreliable.
    virtual std::vector<float> grad_expect() {
        return {};
    }

    virtual void initialize_tensors(ggml_context * ctx) {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != nullptr; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t);
        }
    }

    // re-draw data-dependent inputs between timed perf iterations
    virtual void reinit_perf_iter(ggml_context * ctx) {
        GGML_UNUSED(ctx);
    }

    virtual size_t op_size(ggml_tensor * t) {
        size_t size = ggml_nbytes(t);
        // add source tensors
        for (int i = 0; i < GGML_MAX_SRC; i++) {
            if (t->src[i] != NULL) {
                size += ggml_nbytes(t->src[i]);
            }
        }
        return size;
    }

    virtual uint64_t op_flops(ggml_tensor * t) {
        GGML_UNUSED(t);
        return 0;
    }

    virtual bool run_whole_graph() { return false; }
    virtual std::vector<ggml_tensor *> fusion_test_nodes() { return {}; }
    virtual bool use_weight_context() { return false; }

    ggml_cgraph * gf = nullptr;
    ggml_cgraph * gb = nullptr;

    static const int sentinel_size = 1024;

    test_mode mode;

    std::vector<ggml_tensor *> sentinels;

    std::string current_op_name;
    bool contains_f16 = false;

    // Used by the WebGPU backend to relax error thresholds on ops on f16 tensors
    void check_for_f16_tensor(ggml_context * ctx) {
        contains_f16 = false;
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != nullptr; t = ggml_get_next_tensor(ctx, t)) {
            if (t->type == GGML_TYPE_F16) {
                contains_f16 = true;
                break;
            }
        }
    }

    void add_sentinel(ggml_context * ctx) {
        if (mode == MODE_PERF || mode == MODE_GRAD || mode == MODE_SUPPORT) {
            return;
        }
        ggml_tensor * sentinel = ::ggml_new_tensor_1d(ctx, GGML_TYPE_F32, sentinel_size);
        ggml_format_name(sentinel, "sent_%zu", sentinels.size());
        sentinels.push_back(sentinel);
    }

    // hijack ggml_new_tensor to add sentinels after each tensor to check for overflows in the backend

    ggml_tensor * ggml_new_tensor(ggml_context * ctx, ggml_type type, int n_dims, const int64_t * ne) {
        ggml_tensor * t = ::ggml_new_tensor(ctx, type, n_dims, ne);
        add_sentinel(ctx);
        return t;
    }

    ggml_tensor * ggml_new_tensor_1d(ggml_context * ctx, ggml_type type, int64_t ne0) {
        ggml_tensor * t = ::ggml_new_tensor_1d(ctx, type, ne0);
        add_sentinel(ctx);
        return t;
    }

    ggml_tensor * ggml_new_tensor_2d(ggml_context * ctx, ggml_type type, int64_t ne0, int64_t ne1) {
        ggml_tensor * t = ::ggml_new_tensor_2d(ctx, type, ne0, ne1);
        add_sentinel(ctx);
        return t;
    }

    ggml_tensor * ggml_new_tensor_3d(ggml_context * ctx, ggml_type type, int64_t ne0, int64_t ne1, int64_t ne2) {
        ggml_tensor * t = ::ggml_new_tensor_3d(ctx, type, ne0, ne1, ne2);
        add_sentinel(ctx);
        return t;
    }

    ggml_tensor * ggml_new_tensor_4d(ggml_context * ctx, ggml_type type, int64_t ne0, int64_t ne1, int64_t ne2, int64_t ne3) {
        ggml_tensor * t = ::ggml_new_tensor_4d(ctx, type, ne0, ne1, ne2, ne3);
        add_sentinel(ctx);
        return t;
    }

    // Checks an op against the test filter, which is a comma separated list of OP names, regexes, or specific variations
    bool matches_filter(ggml_tensor * op, const char * op_names_filter) {
        if (op_names_filter == nullptr) {
            return true;
        }
        const auto op_name = op_desc(op);
        const auto op_full_name = op_name + "(" + vars() + ")";
        for (const auto & entry : op_filter_entries(op_names_filter)) {
            if (entry.find_first_of('(') != std::string_view::npos) {
                // a full test case string, matched exactly
                if (entry == op_full_name) {
                    return true;
                }
            } else if (op_filter_entry_matches(entry, op_name)) {
                return true;
            }
        }
        return false;
    }

    test_status_t eval(ggml_backend_t backend1,
                       ggml_backend_t backend2,
                       const char *   op_names_filter,
                       printer *      output_printer) {
        mode = MODE_TEST;

        ggml_init_params params = {
            /* .mem_size = */ ggml_tensor_overhead()*128 + ggml_graph_overhead(),
            /* .mem_base = */ NULL,
            /* .no_alloc = */ true,
        };
        const bool use_weights = use_weight_context();

        ggml_context_ptr ctx(ggml_init(params));
        GGML_ASSERT(ctx);
        ggml_context_ptr ctx_weights(use_weights ? ggml_init(params) : nullptr);
        GGML_ASSERT(!use_weights || ctx_weights);

        gf = ggml_new_graph(ctx.get());

        // pre-graph sentinel
        add_sentinel(ctx.get());
        if (ctx_weights) {
            add_sentinel(ctx_weights.get());
        }

        ggml_tensor * out = build_graph(ctx.get(), ctx_weights.get());
        current_op_name   = op_desc(out);
        check_for_f16_tensor(ctx.get());

        if (!matches_filter(out, op_names_filter)) {
            //printf("  %s: skipping\n", op_desc(out).c_str());
            return test_status_t::SKIPPED;
        }

        // check if the backends support the ops
        bool supported = true;
        std::string unsupported_str;
        for (ggml_backend_t backend : {backend1, backend2}) {
            for (ggml_tensor * t = ggml_get_first_tensor(ctx.get()); t != NULL; t = ggml_get_next_tensor(ctx.get(), t)) {
                if (!ggml_backend_supports_op(backend, t)) {
                    supported = false;
                    if (unsupported_str.empty()) {
                        unsupported_str = std::string(ggml_backend_name(backend));
                    } else {
                        unsupported_str += ", " + std::string(ggml_backend_name(backend));
                    }
                }
            }
        }

        if (!supported) {
            test_result result(unsupported_str, current_op_name, vars(), "test",
                             false, false, "not supported");

            print_test_result_locked(output_printer, result);

            return test_status_t::NOT_SUPPORTED;
        }

        // post-graph sentinel
        add_sentinel(ctx.get());
        if (ctx_weights) {
            add_sentinel(ctx_weights.get());
        }

        ggml_backend_buffer_ptr buf_weights(nullptr);
        if (ctx_weights) {
            buf_weights.reset(ggml_backend_alloc_ctx_tensors(ctx_weights.get(), backend1));
            if (buf_weights == NULL) {
                printf("failed to allocate weight tensors [%s] ", ggml_backend_name(backend1));
                return test_status_t::FAIL;
            }
            ggml_backend_buffer_set_usage(buf_weights.get(), GGML_BACKEND_BUFFER_USAGE_WEIGHTS);
        }

        // allocate
        ggml_backend_buffer_ptr buf(ggml_backend_alloc_ctx_tensors(ctx.get(), backend1));

        if (buf == NULL) {
            printf("failed to allocate tensors [%s] ", ggml_backend_name(backend1));
            return test_status_t::FAIL;
        }

        // build graph
        ggml_build_forward_expand(gf, out);

        // add sentinels as graph nodes so that they are checked in the callback
        for (ggml_tensor * sentinel : sentinels) {
            ggml_graph_add_node(gf, sentinel);
        }

        // randomize tensors
        initialize_tensors(ctx.get());
        if (ctx_weights) {
            initialize_tensors(ctx_weights.get());
        }

        // compare
        struct callback_userdata {
            bool   ok;
            test_case * tc;
            ggml_backend_t backend1;
            ggml_backend_t backend2;
        };

        callback_userdata ud {
            true,
            this,
            backend1,
            backend2,
        };

        auto callback = [](int index, ggml_tensor * t1, ggml_tensor * t2, void * user_data) -> bool {
            callback_userdata * ud = (callback_userdata *) user_data;
            const char * bn1 = ggml_backend_name(ud->backend1);
            const char * bn2 = ggml_backend_name(ud->backend2);

            if (t1->op == GGML_OP_NONE) {
                // sentinels must be unchanged
                std::vector<uint8_t> t1_data(ggml_nbytes(t1));
                std::vector<uint8_t> t2_data(ggml_nbytes(t2));
                ggml_backend_tensor_get(t1, t1_data.data(), 0, ggml_nbytes(t1));
                ggml_backend_tensor_get(t2, t2_data.data(), 0, ggml_nbytes(t2));

                if (memcmp(t1_data.data(), t2_data.data(), ggml_nbytes(t1)) != 0) {
                    printf("sentinel mismatch: %s ", t1->name);
                    ud->ok = false;
                    return true;
                }
            }

            std::vector<float> f1 = tensor_to_float(t1);
            std::vector<float> f2 = tensor_to_float(t2);

            for (size_t i = 0; i < f1.size(); i++) {
                // check for nans
                if (std::isnan(f1[i]) || std::isnan(f2[i])) {
                    printf("[%s] NaN at index %zu (%s=%f %s=%f) ", ggml_op_desc(t1), i, bn1, f1[i], bn2, f2[i]);
                    ud->ok = false;
                    return true;
                }
                // check for infs: both must be inf of the same sign, or both must be finite
                if (isinf_or_max(f1[i]) || isinf_or_max(f2[i])) {
                    if (isinf_or_max(f1[i]) && isinf_or_max(f2[i])) {
                        if (std::signbit(f1[i]) != std::signbit(f2[i])) {
                            printf("[%s] inf sign mismatch: %s=%f %s=%f ", ggml_op_desc(t1), bn1, f1[i], bn2, f2[i]);
                            ud->ok = false;
                            return true;
                        }
                    } else {
                        printf("[%s] inf mismatch: %s=%f %s=%f ", ggml_op_desc(t1), bn1, f1[i], bn2, f2[i]);
                        ud->ok = false;
                        return true;
                    }
                }
            }

            double err = ud->tc->err(f1.data(), f2.data(), f1.size());
            if (err > ud->tc->max_err(ud->backend1)) {
                printf("[%s] ERR = %.9f > %.9f ", ggml_op_desc(t1), err, ud->tc->max_err(ud->backend1));
                //for (int i = 0; i < (int) f1.size(); i++) {
                //    printf("%5d %9.6f %9.6f, diff = %9.6f\n", i, f1[i], f2[i], f1[i] - f2[i]);
                //}
                //printf("\n");
                //exit(1);
                ud->ok = false;
            }
            return true;

            GGML_UNUSED(index);
        };

        std::vector<ggml_tensor *> fused_nodes_to_verify = fusion_test_nodes();
        if (fused_nodes_to_verify.size() == 0 && run_whole_graph()) {
            fused_nodes_to_verify.push_back(out);
        }
        const bool cmp_ok = ggml_backend_compare_graph_backend(backend1, backend2, gf, callback, &ud,
                                                               run_whole_graph() ? fused_nodes_to_verify.data() : nullptr,
                                                               fused_nodes_to_verify.size());

        // Create test result
        bool        test_passed = ud.ok && cmp_ok;
        std::string error_msg   = test_passed ? "" : (!cmp_ok ? "compare failed" : "test failed");
        test_result result(ggml_backend_name(backend1), current_op_name, vars(), "test", supported, test_passed,
                           error_msg);

        print_test_result_locked(output_printer, result);

        return test_passed ? test_status_t::OK : test_status_t::FAIL;
    }

    bool eval_perf(ggml_backend_t backend, const char * op_names_filter, printer * output_printer) {
        mode = MODE_PERF;

        static const size_t graph_nodes = 8192;

        ggml_init_params params = {
            /* .mem_size = */ ggml_tensor_overhead()*128 + ggml_graph_overhead_custom(graph_nodes, false),
            /* .mem_base = */ NULL,
            /* .no_alloc = */ true,
        };
        const bool use_weights = use_weight_context();

        ggml_context_ptr ctx(ggml_init(params)); // smart ptr
        GGML_ASSERT(ctx);
        ggml_context_ptr ctx_weights(use_weights ? ggml_init(params) : nullptr);
        GGML_ASSERT(!use_weights || ctx_weights);

        ggml_tensor * out             = build_graph(ctx.get(), ctx_weights.get());
        current_op_name               = op_desc(out);
        if (!matches_filter(out, op_names_filter)) {
            //printf("  %s: skipping\n", op_desc(out).c_str());
            return true;
        }

        if (!ggml_backend_supports_op(backend, out)) {
            // Create test result for unsupported performance test
            test_result result(ggml_backend_name(backend), current_op_name, vars(), "perf", false, false,
                               "not supported");

            output_printer->print_test_result(result);

            return true;
        }

        ggml_backend_buffer_ptr buf_weights(nullptr);
        if (ctx_weights) {
            buf_weights.reset(ggml_backend_alloc_ctx_tensors(ctx_weights.get(), backend));
            if (buf_weights == NULL) {
                printf("failed to allocate weight tensors\n");
                return false;
            }
            ggml_backend_buffer_set_usage(buf_weights.get(), GGML_BACKEND_BUFFER_USAGE_WEIGHTS);
        }

        // allocate
        ggml_backend_buffer_ptr buf(ggml_backend_alloc_ctx_tensors(ctx.get(), backend)); // smart ptr

        if (buf == NULL) {
            printf("failed to allocate tensors\n");
            return false;
        }

        // randomize tensors
        initialize_tensors(ctx.get());
        if (ctx_weights) {
            initialize_tensors(ctx_weights.get());
        }

        // build graph
        ggml_cgraph * gf = ggml_new_graph_custom(ctx.get(), graph_nodes, false);
        ggml_build_forward_expand(gf, out);

        // warmup run
        ggml_status status = ggml_backend_graph_compute(backend, gf);
        if (status != GGML_STATUS_SUCCESS) {
            fprintf(stderr, "%s: ggml_backend_graph_compute failed. status=%s \n", __func__, ggml_status_to_string(status));
            return false;
        }

        // determine number of runs
        int n_runs;
        bool is_cpu = ggml_backend_dev_type(ggml_backend_get_device(backend)) == GGML_BACKEND_DEVICE_TYPE_CPU;
        if (op_flops(out) > 0) {
            // based on flops
            const uint64_t GFLOP = 1000 * 1000 * 1000;
            const uint64_t target_flops_cpu =   8ULL * GFLOP;
            const uint64_t target_flops_gpu = 100ULL * GFLOP;
            uint64_t target_flops = is_cpu ? target_flops_cpu : target_flops_gpu;
            n_runs = (int)std::min<int64_t>(ggml_graph_size(gf) - ggml_graph_n_nodes(gf), target_flops / op_flops(out)) + 1;
        } else {
            // based on memory size
            const size_t GB = 1ULL << 30;
            const size_t target_size_cpu =  8 * GB;
            const size_t target_size_gpu = 32 * GB;
            size_t target_size = is_cpu ? target_size_cpu : target_size_gpu;
            n_runs = (int)std::min<int64_t>(ggml_graph_size(gf) - ggml_graph_n_nodes(gf), target_size / op_size(out)) + 1;
        }

        // duplicate the op
        for (int i = 1; i < n_runs; i++) {
            ggml_graph_add_node(gf, out);
        }

        // calculate memory
        size_t mem = n_runs * op_size(out);
        auto tensor_op_size = [](ggml_tensor * t) {
            size_t size = ggml_nbytes(t);
            // add source tensors
            for (int i = 0; i < GGML_MAX_SRC; i++) {
                if (t->src[i] != NULL) {
                    size += ggml_nbytes(t->src[i]);
                }
            }
            return size;
        };
        for (int i = 0; i < ggml_graph_n_nodes(gf); ++i) {
            if (ggml_is_view_op(ggml_graph_node(gf, i)->op) || ggml_graph_node(gf, i) == out) {
                continue;
            }
            mem += tensor_op_size(ggml_graph_node(gf, i));
        }

        // run
        int64_t total_time_us = 0;
        int64_t total_mem = 0;
        int total_runs = 0;
        do {
            int64_t start_time = ggml_time_us();
            ggml_status status = ggml_backend_graph_compute(backend, gf);
            if (status != GGML_STATUS_SUCCESS) {
                fprintf(stderr, "%s: ggml_backend_graph_compute failed. status=%s \n", __func__, ggml_status_to_string(status));
                return false;
            }
            int64_t end_time = ggml_time_us();

            total_time_us += end_time - start_time;
            total_mem += mem;
            total_runs += n_runs;

            // re-draw any data-dependent inputs (expert ids) outside the timed region
            reinit_perf_iter(ctx.get());
        } while (total_time_us < 1000*1000); // run for at least 1 second

        // Create test result
        double avg_time_us      = (double) total_time_us / total_runs;
        double calculated_flops = (op_flops(out) > 0) ? (op_flops(out) * total_runs) / (total_time_us / 1e6) : 0.0;
        double calculated_bandwidth =
            (op_flops(out) == 0) ? total_mem / (total_time_us / 1e6) / 1024.0 / 1024.0 / 1024.0 : 0.0;
        size_t calculated_memory_kb = op_size(out) / 1024;

        test_result result(ggml_backend_name(backend), current_op_name, vars(), "perf", true, true, "", avg_time_us,
                           calculated_flops, calculated_bandwidth, calculated_memory_kb, total_runs);

        if (output_printer) {
            output_printer->print_test_result(result);
        }

        return true;
    }

    bool eval_support(ggml_backend_t backend, const char * op_names_filter, printer * output_printer) {
        mode = MODE_SUPPORT;

        static const size_t graph_nodes = 8192;

        ggml_init_params params = {
            /* .mem_size = */ ggml_tensor_overhead()*128 + ggml_graph_overhead_custom(graph_nodes, false),
            /* .mem_base = */ NULL,
            /* .no_alloc = */ true,
        };
        ggml_context_ptr ctx(ggml_init(params)); // smart ptr
        GGML_ASSERT(ctx);

        gf = ggml_new_graph_custom(ctx.get(), graph_nodes, false);

        ggml_tensor * out = build_graph(ctx.get());
        current_op_name   = op_desc(out);

        if (!matches_filter(out, op_names_filter)) {
            return true;
        }

        bool supported = ggml_backend_supports_op(backend, out);

        std::string device_desc = ggml_backend_dev_description(ggml_backend_get_device(backend));
        std::string backend_reg_name = ggml_backend_reg_name(ggml_backend_dev_backend_reg(ggml_backend_get_device(backend)));

        test_result result(ggml_backend_name(backend), current_op_name, vars(), "support", supported, supported,
                           supported ? "yes" : "no", 0.0, 0.0, 0.0, 0, 0, device_desc, backend_reg_name);

        output_printer->print_test_result(result);

        return true;
    }

    bool eval_grad(ggml_backend_t backend, const char * op_names_filter, printer * output_printer) {
        mode = MODE_GRAD;
        const std::vector<float> expect = grad_expect();

        ggml_init_params params = {
            /* .mem_size = */ ggml_tensor_overhead()*128 + 2*ggml_graph_overhead_custom(GGML_DEFAULT_GRAPH_SIZE, true),
            /* .mem_base = */ NULL,
            /* .no_alloc = */ true,
        };
        ggml_context_ptr ctx(ggml_init(params)); // smart ptr
        GGML_ASSERT(ctx);

        gf = ggml_new_graph_custom(ctx.get(), GGML_DEFAULT_GRAPH_SIZE, true);
        gb = ggml_new_graph_custom(ctx.get(), GGML_DEFAULT_GRAPH_SIZE, true);

        ggml_tensor * out = build_graph(ctx.get());

        if (!matches_filter(out, op_names_filter) || out->op == GGML_OP_OPT_STEP_ADAMW) {
            return true;
        }

        if (out->type != GGML_TYPE_F32) {
            output_printer->print_operation(test_operation_info(op_desc(out), vars(), ggml_backend_name(backend),
                                                                test_status_t::NOT_SUPPORTED,
                                                                out->name + std::string("->type != FP32")));
            return true;
        }

        // Print operation info first
        output_printer->print_operation(test_operation_info(op_desc(out), vars(), ggml_backend_name(backend)));

        // check if the backend supports the ops
        bool        supported  = true;
        bool        any_params = false;
        std::string failure_reason;

        for (ggml_tensor * t = ggml_get_first_tensor(ctx.get()); t != NULL; t = ggml_get_next_tensor(ctx.get(), t)) {
            if (!ggml_backend_supports_op(backend, t)) {
                supported      = false;
                failure_reason = ggml_backend_name(backend);
                break;
            }
            if ((t->flags & GGML_TENSOR_FLAG_PARAM)) {
                any_params = true;
                if (t->type != GGML_TYPE_F32) {
                    supported      = false;
                    failure_reason = std::string(t->name) + "->type != FP32";
                    break;
                }
            }
        }
        if (!any_params) {
            supported      = false;
            failure_reason = op_desc(out);
        }

        if (!supported) {
            output_printer->print_operation(test_operation_info(op_desc(out), vars(), ggml_backend_name(backend),
                                                                test_status_t::NOT_SUPPORTED, failure_reason));
            return true;
        }

        int64_t ngrads = 0;
        for (ggml_tensor * t = ggml_get_first_tensor(ctx.get()); t != NULL; t = ggml_get_next_tensor(ctx.get(), t)) {
            if (t->flags & GGML_TENSOR_FLAG_PARAM) {
                ngrads += ggml_nelements(t);
            }
        }
        if (ngrads > grad_nmax()) {
            test_operation_info info(op_desc(out), vars(), ggml_backend_name(backend));
            info.set_large_tensor_skip();
            output_printer->print_operation(info);
            return true;
        }


        if (!ggml_is_scalar(out)) {
            out = ggml_sum(ctx.get(), out);
            ggml_set_name(out, "sum_of_out");
        }
        ggml_set_loss(out);

        ggml_build_forward_expand(gf, out);
        ggml_graph_cpy(gf, gb);
        ggml_build_backward_expand(ctx.get(), gb, nullptr);
        if (expect.size() != 1 || expect[0] != 0.0f) {
            GGML_ASSERT(ggml_graph_n_nodes(gb) > ggml_graph_n_nodes(gf));
            for (ggml_tensor * t = ggml_get_first_tensor(ctx.get()); t != NULL; t = ggml_get_next_tensor(ctx.get(), t)) {
                GGML_ASSERT(!(t->flags & GGML_TENSOR_FLAG_PARAM) || ggml_graph_get_grad(gb, t)->op != GGML_OP_NONE);
            }
        }

        for (ggml_tensor * t = ggml_get_first_tensor(ctx.get()); t != NULL; t = ggml_get_next_tensor(ctx.get(), t)) {
            if (!ggml_backend_supports_op(backend, t)) {
                output_printer->print_operation(test_operation_info(op_desc(out), vars(), ggml_backend_name(backend),
                                                                    test_status_t::NOT_SUPPORTED,
                                                                    ggml_backend_name(backend)));
                supported = false;
                break;
            }
            if ((t->flags & GGML_TENSOR_FLAG_PARAM) && t->type != GGML_TYPE_F32) {
                output_printer->print_operation(test_operation_info(op_desc(out), vars(), ggml_backend_name(backend),
                                                                    test_status_t::NOT_SUPPORTED,
                                                                    std::string(t->name) + "->type != FP32"));
                supported = false;
                break;
            }
        }
        if (!supported) {
            return true;
        }

        // allocate
        ggml_backend_buffer_ptr buf(ggml_backend_alloc_ctx_tensors(ctx.get(), backend)); // smart ptr
        if (buf == NULL) {
            test_operation_info info(op_desc(out), vars(), ggml_backend_name(backend));
            info.set_error("allocation", "");
            output_printer->print_operation(info);
            return false;
        }

        initialize_tensors(ctx.get()); // Randomizes all tensors (including gradients).
        ggml_graph_reset(gb);    // Sets gradients to 1 if loss, 0 otherwise.

        ggml_status status = ggml_backend_graph_compute(backend, gf);
        if (status != GGML_STATUS_SUCCESS) {
            fprintf(stderr, "%s: ggml_backend_graph_compute failed. status=%s \n", __func__, ggml_status_to_string(status));
            return false;
        }
        status = ggml_backend_graph_compute(backend, gb);
        if (status != GGML_STATUS_SUCCESS) {
            fprintf(stderr, "%s: ggml_backend_graph_compute failed. status=%s \n", __func__, ggml_status_to_string(status));
            return false;
        }

        bool ok = true;
        for (struct ggml_tensor * t = ggml_get_first_tensor(ctx.get()); t != nullptr; t = ggml_get_next_tensor(ctx.get(), t)) {
            if (!(t->flags & GGML_TENSOR_FLAG_PARAM)) {
                continue;
            }

            const char * bn = ggml_backend_name(backend);
            const int64_t ne = ggml_nelements(t);

            std::vector<float> ga;
            struct ggml_tensor * grad = ggml_graph_get_grad(gb, t);
            if (grad) {
                ga = tensor_to_float(grad);
            } else {
                ga.resize(ne); // default value is 0.0f
            }

            for (int64_t i = 0; i < ne; ++i) { // gradient algebraic
                // check for nans
                if (!std::isfinite(ga[i])) {
                    test_operation_info info(op_desc(out), vars(), ggml_backend_name(backend));
                    info.set_gradient_info(i, bn, ga[i]);
                    output_printer->print_operation(info);
                    ok = false;
                    break;
                }
            }
            if (!ok) {
                break;
            }

            std::vector<float> gn(ne); // gradient numeric
            GGML_ASSERT(ga.size() == gn.size());

            std::vector<float> x0 = tensor_to_float(t); // original t data
            GGML_ASSERT(ggml_is_scalar(out));
            GGML_ASSERT(out->type == GGML_TYPE_F32);

            const float eps = grad_eps();
            for (int64_t i = 0; i < ne; ++i) {
                const float xiu  = x0[i] + 1.0f*eps; // x, index i, up
                const float xiuh = x0[i] + 0.5f*eps; // x, index i, up half
                const float xidh = x0[i] - 0.5f*eps; // x, index i, down half
                const float xid  = x0[i] - 1.0f*eps; // x, index i, down

                float fu, fuh, fdh, fd; // output values for xiu, xiuh, xid, xidh

                ggml_backend_tensor_set(t, &xiu, i*sizeof(float), sizeof(float));
                status = ggml_backend_graph_compute(backend, gf);
                if (status != GGML_STATUS_SUCCESS) {
                    fprintf(stderr, "%s: ggml_backend_graph_compute failed. status=%s \n", __func__, ggml_status_to_string(status));
                    return false;
                }
                ggml_backend_tensor_get(out, &fu, 0, ggml_nbytes(out));

                ggml_backend_tensor_set(t, &xid, i*sizeof(float), sizeof(float));
                status = ggml_backend_graph_compute(backend, gf);
                if (status != GGML_STATUS_SUCCESS) {
                    fprintf(stderr, "%s: ggml_backend_graph_compute failed. status=%s \n", __func__, ggml_status_to_string(status));
                    return false;
                }
                ggml_backend_tensor_get(out, &fd, 0, ggml_nbytes(out));

                if (grad_precise()) {
                    ggml_backend_tensor_set(t, &xiuh, i*sizeof(float), sizeof(float));
                    status = ggml_backend_graph_compute(backend, gf);
                    if (status != GGML_STATUS_SUCCESS) {
                        fprintf(stderr, "%s: ggml_backend_graph_compute failed. status=%s \n", __func__, ggml_status_to_string(status));
                        return false;
                    }
                    ggml_backend_tensor_get(out, &fuh, 0, ggml_nbytes(out));

                    ggml_backend_tensor_set(t, &xidh, i*sizeof(float), sizeof(float));
                    status = ggml_backend_graph_compute(backend, gf);
                    if (status != GGML_STATUS_SUCCESS) {
                        fprintf(stderr, "%s: ggml_backend_graph_compute failed. status=%s \n", __func__, ggml_status_to_string(status));
                        return false;
                    }
                    ggml_backend_tensor_get(out, &fdh, 0, ggml_nbytes(out));

                    gn[i] = (8.0*(double)fuh + (double)fd - (8.0*(double)fdh + (double)fu)) / (6.0*(double)eps);
                } else {
                    gn[i] = (fu - fd) / (2.0f*eps);
                }

                ggml_backend_tensor_set(t, x0.data(), 0, ggml_nbytes(t));
            }

            const double err = mean_abs_asymm(gn.data(), ga.data(), gn.size(), expect);
            if (err > max_maa_err()) {
                test_operation_info info(op_desc(out), vars(), ggml_backend_name(backend));
                info.set_maa_error(err, max_maa_err());
                output_printer->print_operation(info);
                ok = false;
                break;
            }
            if (!ok) {
                break;
            }
        }

        // Create final test result
        test_operation_info final_info(op_desc(out), vars(), ggml_backend_name(backend));
        if (!ok) {
            final_info.set_compare_failure();
        }
        final_info.status = ok ? test_status_t::OK : test_status_t::FAIL;
        output_printer->print_operation(final_info);

        if (ok) {
            return true;
        }

        return false;
    }
};


// ####################################
// ## Section 2: GGML Op Definitions ##
// ####################################


// The following is an example showing the bare minimum for creating a test for a GGML op.

// GGML_OP_EXAMPLE
struct test_example : public test_case {
    // Always define these 2 or variants thereof:
    const ggml_type type; // The type of the input tensors.
    const std::array<int64_t, 4> ne; // The shape of the input tensors.
    // For some ops it's necessary to define multiple types or shapes for the inputs.
    // Or they may need additional parameters.

    // Put all parameters needed to fully define the test into one of the VARS_TO_STR macros.
    // In most cases these are just the properties of the struct that you defined above.
    // This is needed for info prints.
    std::string vars() override {
        return VARS_TO_STR2(type, ne);
    }

    // Define a constructor for the struct.
    // In most cases it will be sufficient to have the same arguments as the struct has properties
    // and just use initializer lists.
    test_example(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 5, 4, 3})
        : type(type), ne(ne) {}

    // Define how a simple GGML compute graph can be constructed for the new GGML op.
    ggml_tensor * build_graph(ggml_context * ctx) override {
        // Step 1: create input tensors that don't depend on any other tensors:
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(a, "a"); // Setting names is optional but it's useful for debugging.

        ggml_tensor * b = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(b, "b");

        // Step 2: use the op that you want to test in the GGML compute graph.
        ggml_tensor * out = ggml_add(ctx, a, b); // For this example we're just doing a simple addition.
        ggml_set_name(out, "out");

        // Step 3: return the output tensor.
        return out;
    }
    // In order to also check the gradients for your op, add calls like ggml_set_param(a)
    // immediately after you create the tensors.
    // This is optional and only makes sense if a backward pass has actually been implemented for the new op.
};


// GGML_OP_UNARY
struct test_unary : public test_case {
    const ggml_unary_op op;
    const ggml_type type;
    const std::array<int64_t, 4> ne_a;
    int v; // view (1 : non-contiguous a)

    std::string vars() override {
        return VARS_TO_STR3(type, ne_a, v);
    }

    test_unary(ggml_unary_op op,
            ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne_a = {128, 2, 2, 2},
            int v = 0)
        : op(op), type(type), ne_a(ne_a), v(v) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        const bool grad_supported = op == GGML_UNARY_OP_ABS || op == GGML_UNARY_OP_SGN || op == GGML_UNARY_OP_NEG ||
            op == GGML_UNARY_OP_STEP || op == GGML_UNARY_OP_RELU || op == GGML_UNARY_OP_SILU ||
            op == GGML_UNARY_OP_EXPM1 || op == GGML_UNARY_OP_SOFTPLUS;

        ggml_tensor * a;
        if (v & 1) {
            auto ne = ne_a;
            ne[0] *= 3;
            ne[1] *= 2;
            ne[2] *= 5;
            ne[3] *= 4;
            a = ggml_new_tensor(ctx, type, 4, ne.data());
            if (grad_supported) {
                ggml_set_param(a);
            }
            ggml_set_name(a, "a");

            a = ggml_view_4d(ctx, a, ne_a[0], ne_a[1], ne_a[2], ne_a[3], a->nb[1], a->nb[2], a->nb[3], 0);
            ggml_set_name(a, "view_of_a");
        } else {
            a = ggml_new_tensor(ctx, type, 4, ne_a.data());
            if (grad_supported) {
                ggml_set_param(a);
            }
            ggml_set_name(a, "a");
        }

        ggml_tensor * out = ggml_unary(ctx, a, op);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        float min = -150.f;
        float max =  150.f;

        // Keep FP16 exp/expm1 inputs in-range so all backends stay finite instead of
        // disagreeing on whether overflow saturates to max-F16 or produces +inf.
        if (type == GGML_TYPE_F16 && (op == GGML_UNARY_OP_EXP || op == GGML_UNARY_OP_EXPM1)) {
            min = -10.f;
            max =  10.f;
        }

        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            // test extended range of values to check for NaNs in GELU
            init_tensor_uniform(t, min, max);
        }
    }

    float grad_eps() override {
        return 15.0f;
    }

    std::vector<float> grad_expect() override {
        if (op == GGML_UNARY_OP_ABS) {
            return {-1.0f, 1.0f};
        }
        if (op == GGML_UNARY_OP_SGN || op == GGML_UNARY_OP_STEP) {
            return {0.0f};
        }
        if (op == GGML_UNARY_OP_RELU) {
            return {0.0f, 1.0f};
        }
        return {};
    }

};

// GGML_OP_GLU
struct test_glu : public test_case {
    const ggml_glu_op op;
    const ggml_type type;
    const std::array<int64_t, 4> ne_a;
    int v; // view (1 : non-contiguous a)
    bool swapped;

    std::string vars() override {
        return VARS_TO_STR4(type, ne_a, v, swapped);
    }

    test_glu(ggml_glu_op op,
            ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne_a = {128, 2, 2, 2},
            int v = 0,
            bool swapped = false)
        : op(op), type(type), ne_a(ne_a), v(v), swapped(swapped) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a;
        if (v & 1) {
            auto ne = ne_a; ne[0] *= 3;
            a = ggml_new_tensor(ctx, type, 4, ne.data());
            ggml_set_name(a, "a");

            a = ggml_view_4d(ctx, a, ne_a[0], ne_a[1], ne_a[2], ne_a[3], a->nb[1], a->nb[2], a->nb[3], 0);
            ggml_set_name(a, "view_of_a");
        } else {
            a = ggml_new_tensor(ctx, type, 4, ne_a.data());
            ggml_set_name(a, "a");
        }

        ggml_tensor * out = ggml_glu(ctx, a, op, swapped);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            // test extended range of values to check for NaNs in GELU
            init_tensor_uniform(t, -150.f, 150.f);
        }
    }
};

struct test_glu_split : public test_case {
    const ggml_glu_op op;
    const ggml_type type;
    const std::array<int64_t, 4> ne_a;
    int v; // view (1 : non-contiguous a)

    std::string vars() override {
        return VARS_TO_STR3(type, ne_a, v) + ",split";
    }

    test_glu_split(ggml_glu_op op,
            ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne_a = {128, 2, 2, 2},
            int v = 0)
        : op(op), type(type), ne_a(ne_a), v(v) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a;
        ggml_tensor * b;
        if (v & 1) {
            auto ne = ne_a; ne[0] *= 3;
            a = ggml_new_tensor(ctx, type, 4, ne.data());
            ggml_set_param(a);
            ggml_set_name(a, "a");

            a = ggml_view_4d(ctx, a, ne_a[0], ne_a[1], ne_a[2], ne_a[3], a->nb[1], a->nb[2], a->nb[3], 0);
            ggml_set_name(a, "view_of_a");

            b = ggml_new_tensor(ctx, type, 4, ne.data());
            ggml_set_param(b);
            ggml_set_name(b, "b");

            b = ggml_view_4d(ctx, b, ne_a[0], ne_a[1], ne_a[2], ne_a[3], b->nb[1], b->nb[2], b->nb[3], 0);
            ggml_set_name(a, "view_of_b");
        } else {
            a = ggml_new_tensor(ctx, type, 4, ne_a.data());
            ggml_set_param(a);
            ggml_set_name(a, "a");

            b = ggml_new_tensor(ctx, type, 4, ne_a.data());
            ggml_set_param(b);
            ggml_set_name(b, "b");
        }

        ggml_tensor * out = ggml_glu_split(ctx, a, b, op);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            // test extended range of values to check for NaNs in GELU
            init_tensor_uniform(t, -150.f, 150.f);
        }
    }
};

struct test_swiglu_oai : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne_a;
    int v; // view (1 : non-contiguous a)
    float alpha;
    float limit;

    std::string vars() override {
        return VARS_TO_STR5(type, ne_a, v, alpha, limit);
    }

    test_swiglu_oai(ggml_type type = GGML_TYPE_F32,
                    std::array<int64_t, 4> ne_a = {128, 2, 2, 2},
                    int v = 0,
                    float alpha = 1.702f,
                    float limit = 7.0f)
        : type(type), ne_a(ne_a), v(v), alpha(alpha), limit(limit) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a;
        ggml_tensor * b;
        if (v & 1) {
            auto ne = ne_a; ne[0] *= 3;
            a = ggml_new_tensor(ctx, type, 4, ne.data());
            ggml_set_param(a);
            ggml_set_name(a, "a");

            a = ggml_view_4d(ctx, a, ne_a[0], ne_a[1], ne_a[2], ne_a[3], a->nb[1], a->nb[2], a->nb[3], 0);
            ggml_set_name(a, "view_of_a");

            b = ggml_new_tensor(ctx, type, 4, ne.data());
            ggml_set_param(b);
            ggml_set_name(b, "b");

            b = ggml_view_4d(ctx, b, ne_a[0], ne_a[1], ne_a[2], ne_a[3], b->nb[1], b->nb[2], b->nb[3], 0);
            ggml_set_name(a, "view_of_b");
        } else {
            a = ggml_new_tensor(ctx, type, 4, ne_a.data());
            ggml_set_param(a);
            ggml_set_name(a, "a");

            b = ggml_new_tensor(ctx, type, 4, ne_a.data());
            ggml_set_param(b);
            ggml_set_name(b, "b");
        }

        ggml_tensor * out = ggml_swiglu_oai(ctx, a, b, alpha, limit);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            // test extended range of values to check for NaNs in GELU
            init_tensor_uniform(t, -150.f, 150.f);
        }
    }
};

struct test_swiglu_clamp : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne_a;
    int v; // view (1 : non-contiguous a)
    float limit;

    std::string vars() override {
        return VARS_TO_STR4(type, ne_a, v, limit);
    }

    test_swiglu_clamp(ggml_type type = GGML_TYPE_F32,
                      std::array<int64_t, 4> ne_a = {128, 2, 2, 2},
                      int v = 0,
                      float limit = 7.0f)
        : type(type), ne_a(ne_a), v(v), limit(limit) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a;
        ggml_tensor * b;
        if (v & 1) {
            auto ne = ne_a; ne[0] *= 3;
            a = ggml_new_tensor(ctx, type, 4, ne.data());
            ggml_set_param(a);
            ggml_set_name(a, "a");

            a = ggml_view_4d(ctx, a, ne_a[0], ne_a[1], ne_a[2], ne_a[3], a->nb[1], a->nb[2], a->nb[3], 0);
            ggml_set_name(a, "view_of_a");

            b = ggml_new_tensor(ctx, type, 4, ne.data());
            ggml_set_param(b);
            ggml_set_name(b, "b");

            b = ggml_view_4d(ctx, b, ne_a[0], ne_a[1], ne_a[2], ne_a[3], b->nb[1], b->nb[2], b->nb[3], 0);
            ggml_set_name(b, "view_of_b");
        } else {
            a = ggml_new_tensor(ctx, type, 4, ne_a.data());
            ggml_set_param(a);
            ggml_set_name(a, "a");

            b = ggml_new_tensor(ctx, type, 4, ne_a.data());
            ggml_set_param(b);
            ggml_set_name(b, "b");
        }

        ggml_tensor * out = ggml_swiglu_clamp(ctx, a, b, limit);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, -150.f, 150.f);
        }
    }
};

// GGML_OP_GET_ROWS
struct test_get_rows : public test_case {
    const ggml_type type;
    const int n; // cols
    const int m; // rows
    const int r; // rows to get
    const int be1; // batch size
    const int be2; // batch size
    const bool v; // view src1
    const bool vs0; // view src0
    const int offset_cols; // // column offset of the view src0

    std::string vars() override {
        return VARS_TO_STR9(type, n, m, r, be1, be2, v, vs0, offset_cols);
    }

    test_get_rows(ggml_type type = GGML_TYPE_F32, int n = 10, int m = 5, int r = 3, int be1 = 1, int be2 = 1, bool v = false, bool vs0 = false, int offset_cols = 0)
        : type(type), n(n), m(m), r(r), be1(be1), be2(be2), v(v), vs0(vs0), offset_cols(offset_cols) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * in;
        if (vs0) {
            const int offset_rows = 3;
            const int padded_m = m + offset_rows;
            const int padded_n = n + offset_cols;
            ggml_tensor * in_padded = ggml_new_tensor_4d(ctx, type, padded_n, padded_m, be1, be2);
            ggml_set_name(in_padded, "in_padded");
            in = ggml_view_4d(ctx, in_padded, n, m, be1, be2,
                              in_padded->nb[1], in_padded->nb[2], in_padded->nb[3],
                            offset_cols * in_padded->nb[0] + offset_rows * in_padded->nb[1]);
            ggml_set_name(in, "in_view");
        } else {
            in = ggml_new_tensor_4d(ctx, type, n, m, be1, be2);
            ggml_set_name(in, "in");
        }

        ggml_tensor * rows = ggml_new_tensor_3d(ctx, GGML_TYPE_I32, v ? r + 1 : r, be1, be2);
        ggml_set_name(rows, "rows");
        if (v) {
            rows = ggml_view_3d(ctx, rows, r/2, be1, be2, rows->nb[1], rows->nb[2], rows->nb[0]);
            ggml_set_name(rows, "view_of_rows");
        }

        const bool grad_supported = !vs0 && ggml_is_matrix(in) && ggml_is_vector(rows);
        if (grad_supported) {
            ggml_set_param(in);
            // rows is a constant input -> no gradients
        }

        ggml_tensor * out = ggml_get_rows(ctx, in, rows);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (ggml_is_view_op(t->op)) {
                continue;
            }
            if (t->type == GGML_TYPE_I32) {
                // rows
                std::vector<int> data(ggml_nelements(t));
                for (size_t i = 0; i < data.size(); i++) {
                    data[i] = rand() % m;
                }
                ggml_backend_tensor_set(t, data.data(), 0, data.size() * sizeof(int));
            } else {
                init_tensor_uniform(t);
            }
        }
    }
};

// GGML_OP_GET_ROWS_BACK
struct test_get_rows_back : public test_case {
    const ggml_type type;
    const int n; // cols
    const int m; // rows
    const int r; // rows to get
    const int b; // batch size
    const bool v; // view (non-contiguous src1)

    std::string vars() override {
        return VARS_TO_STR6(type, n, m, r, b, v);
    }

    test_get_rows_back(ggml_type type = GGML_TYPE_F32, int n = 10, int m = 5, int r = 3, int b = 1, bool v = false)
        : type(type), n(n), m(m), r(r), b(b), v(v) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * in_forward = ggml_new_tensor_3d(ctx, type, n, m, b);
        ggml_set_name(in_forward, "in_forward");

        ggml_tensor * rows = ggml_new_tensor_2d(ctx, GGML_TYPE_I32, r, b);
        ggml_set_name(rows, "rows");
        if (v) {
            rows = ggml_view_2d(ctx, rows, r/2, b, rows->nb[1], 0);
            ggml_set_name(rows, "view_of_rows");
        }

        ggml_tensor * grad = ggml_new_tensor_3d(ctx, type, n, r, b);
        ggml_set_name(grad, "grad");

        ggml_tensor * out = ggml_get_rows_back(ctx, grad, rows, in_forward);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (t->type == GGML_TYPE_I32) {
                if (ggml_is_view_op(t->op)) { continue; }
                // rows
                std::vector<int> data(r*b);
                for (int i = 0; i < r*b; i++) {
                    data[i] = rand() % m;
                }
                ggml_backend_tensor_set(t, data.data(), 0, r * b * sizeof(int));
            } else {
                init_tensor_uniform(t);
            }
        }
    }
};

static void init_set_rows_row_ids(ggml_tensor * t, int num_rows) {
    std::random_device rd;
    std::default_random_engine rng(rd());
    for (int i2 = 0; i2 < t->ne[2]; i2++) {
        for (int i1 = 0; i1 < t->ne[1]; i1++) {
            // generate a shuffled subset of row indices
            std::vector<int64_t> data(num_rows);
            for (int i = 0; i < num_rows; i++) {
                data[i] = i;
            }
            std::shuffle(data.begin(), data.end(), rng);
            data.resize(t->ne[0]);

            const size_t offs = i1*t->nb[1] + i2*t->nb[2];
            if (t->type == GGML_TYPE_I32) {
                // TODO: Make a template or something
                std::vector<int32_t> data_i32(t->ne[0]);
                for (int i = 0; i < t->ne[0]; i++) {
                    data_i32[i] = static_cast<int32_t>(data[i]);
                }
                ggml_backend_tensor_set(t, data_i32.data(), offs, t->ne[0]*sizeof(int32_t));
            } else {
                ggml_backend_tensor_set(t, data.data(), offs, t->ne[0]*sizeof(int64_t));
            }
        }
    }
}

// GGML_OP_SET_ROWS
struct test_set_rows : public test_case {
    const ggml_type type_src;
    const ggml_type type_dst;
    const ggml_type type_idx;
    const std::array<int64_t, 4> ne;
    const std::array<int, 2> nr23; // broadcast only dims 2 and 3
    const int r; // rows to set
    const bool v; // view (non-contiguous src1)

    std::string vars() override {
        return VARS_TO_STR7(type_src, type_dst, type_idx, ne, nr23, r, v);
    }

    test_set_rows(ggml_type type_src,
            ggml_type type_dst,
            ggml_type type_idx,
            std::array<int64_t, 4> ne,
            std::array<int, 2> nr23,
            int r, bool v = false)
        : type_src(type_src), type_dst(type_dst), type_idx(type_idx), ne(ne), nr23(nr23), r(r), v(v) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * dst = ggml_new_tensor_4d(ctx, type_dst, ne[0], ne[1], ne[2]*nr23[0], ne[3]*nr23[1]);
        ggml_set_name(dst, "dst");

        ggml_tensor * src = ggml_new_tensor_4d(ctx, type_src, ne[0], r,     ne[2]*nr23[0], ne[3]*nr23[1]);
        ggml_set_name(src, "src");

        ggml_tensor * row_idxs = ggml_new_tensor_3d(ctx, type_idx, r, ne[2], ne[3]);
        ggml_set_name(row_idxs, "row_idxs");

        if (v) {
            src      = ggml_view_4d(ctx, src, ne[0], r/2, ne[2]*nr23[0], ne[3]*nr23[1], src->nb[1], src->nb[2], src->nb[3], 0);
            row_idxs = ggml_view_3d(ctx, row_idxs, r/2, ne[2], ne[3], row_idxs->nb[1], row_idxs->nb[2], 0);
            ggml_set_name(row_idxs, "view_of_rows");
        }

        ggml_tensor * out = ggml_set_rows(ctx, dst, src, row_idxs);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (ggml_is_view_op(t->op)) {
                continue;
            }
            if (t->type == GGML_TYPE_I64 || t->type == GGML_TYPE_I32) {
                init_set_rows_row_ids(t, ne[1]);
            } else {
                init_tensor_uniform(t);
            }
        }
    }

    double max_nmse_err() override {
        if (type_dst == GGML_TYPE_Q2_0 || type_dst == GGML_TYPE_Q4_0 || type_dst == GGML_TYPE_Q4_1 ||
            type_dst == GGML_TYPE_IQ4_NL ||
            type_dst == GGML_TYPE_Q5_0 || type_dst == GGML_TYPE_Q5_1 || type_dst == GGML_TYPE_Q8_0) {
            // estimate what the max nmse error would be if one quantized value is
            // off by one. The test values are distributed in [-1,1], so it'll be
            // roughly (2.0 / 2^bits)^2, divided by the mean square value of the reference,
            // which is roughly 0.25 times the number of elements.
            double err_estimate = 1.0f/8.0f;
            if (type_src == GGML_TYPE_F16 && type_dst == GGML_TYPE_Q2_0) {
                err_estimate *= 4.0f;
            }
            if (type_dst == GGML_TYPE_Q5_0 || type_dst == GGML_TYPE_Q5_1) {
                err_estimate /= 2.0f;
            }
            if (type_dst == GGML_TYPE_Q8_0) {
                err_estimate /= 8.0f;
            }
            err_estimate *= err_estimate;
            if (type_src == GGML_TYPE_F16) {
                err_estimate *= 16.0f;
            }
            err_estimate /= 0.25f*float(ne[0] * r * ne[2]*nr23[0] * ne[3]*nr23[1]);
            return err_estimate;
        }
        if (type_dst == GGML_TYPE_TQ3_1S || type_dst == GGML_TYPE_TQ4_1S ||
            type_dst == GGML_TYPE_TQ3_4S || type_dst == GGML_TYPE_TQ3_0 ||
            type_dst == GGML_TYPE_TQ3_4SE || type_dst == GGML_TYPE_TQ3_1S_SHIFT) {
            // Reduction order matters; both TurboQuant weight types have a
            // 32-element WHT inside the dot product which amplifies fp
            // reduction differences slightly.
            return 0.01;
        }
        return 1e-7;
    }

    // See dicussion here: https://github.com/ggml-org/llama.cpp/pull/23760#issuecomment-4566312209
    double max_nmse_err(ggml_backend_t backend) override {
        ggml_backend_reg_t reg = ggml_backend_dev_backend_reg(ggml_backend_get_device(backend));
        if (type_dst == GGML_TYPE_Q8_0) {
            if (strcmp(ggml_backend_reg_name(reg), "WebGPU") == 0) {
                return std::max(test_case::max_nmse_err(backend), 2e-7);
            }
            if (strcmp(ggml_backend_reg_name(reg), "HTP") == 0) {
                return std::max(test_case::max_nmse_err(backend), 5e-6);
            }
        }
        return test_case::max_nmse_err(backend);
    }
};

// GGML_OP_TURBO_WHT
struct test_turbo_wht : public test_case {
    const int64_t head_dim;
    const int64_t n_heads;
    const int direction; // 0=forward, 1=inverse

    std::string vars() override {
        return VARS_TO_STR3(head_dim, n_heads, direction);
    }

    double max_nmse_err() override {
        return 1e-5; // f32 SIMD reduction order varies across GPU backends
    }

    test_turbo_wht(int64_t head_dim = 128, int64_t n_heads = 4, int direction = 0)
        : head_dim(head_dim), n_heads(n_heads), direction(direction) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, head_dim, n_heads);
        ggml_set_param(a);
        ggml_set_name(a, "a");
        ggml_tensor * out = ggml_turbo_wht(ctx, a, direction, 0, nullptr);
        ggml_set_name(out, "out");
        return out;
    }
};

// GGML_OP_TURBO_WHT round-trip: forward then inverse should recover the original
struct test_turbo_wht_roundtrip : public test_case {
    const int64_t head_dim;
    const int64_t n_heads;

    std::string vars() override {
        return VARS_TO_STR2(head_dim, n_heads);
    }

    double max_nmse_err() override {
        return 1e-5; // two WHT passes compound the f32 reduction error
    }

    test_turbo_wht_roundtrip(int64_t head_dim = 128, int64_t n_heads = 4)
        : head_dim(head_dim), n_heads(n_heads) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, head_dim, n_heads);
        ggml_set_param(a);
        ggml_set_name(a, "a");
        // forward WHT (direction=0), then inverse WHT (direction=1)
        ggml_tensor * fwd = ggml_turbo_wht(ctx, a, 0, 0, nullptr);
        ggml_tensor * inv = ggml_turbo_wht(ctx, fwd, 1, 0, nullptr);
        ggml_set_name(inv, "out");
        return inv;
    }
};

// Test SET_ROWS with turbo3 destination, then dequantize and compare.
// This validates the full quantization pipeline: f32 -> WHT -> PolarQuant -> turbo3
// followed by dequantization: turbo3 -> f32. The round-trip error should be bounded.
// Unlike the generic SET_ROWS test (which compares raw quantized bytes), this test
// compares the dequantized f32 output, tolerating the lossy quantization error.
struct test_set_rows_turbo3 : public test_case {
    const ggml_type type_idx;
    const int64_t ne0; // head dim (must be multiple of 128)
    const int64_t ne1; // rows in dst
    const int r;       // rows to write

    std::string vars() override {
        return VARS_TO_STR4(type_idx, ne0, ne1, r);
    }

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "SET_ROWS_TURBO3";
    }

    test_set_rows_turbo3(ggml_type type_idx = GGML_TYPE_I32,
            int64_t ne0 = 128, int64_t ne1 = 8, int r = 4)
        : type_idx(type_idx), ne0(ne0), ne1(ne1), r(r) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        // dst: the turbo3 KV cache buffer
        ggml_tensor * dst = ggml_new_tensor_2d(ctx, GGML_TYPE_TURBO3_0, ne0, ne1);
        ggml_set_name(dst, "dst");

        // src: f32 values to quantize into the cache
        ggml_tensor * src = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, ne0, r);
        ggml_set_name(src, "src");

        // row indices
        ggml_tensor * row_idxs = ggml_new_tensor_1d(ctx, type_idx, r);
        ggml_set_name(row_idxs, "row_idxs");

        // Write f32 data into turbo3 dst via SET_ROWS (includes WHT + quantize)
        ggml_tensor * written = ggml_set_rows(ctx, dst, src, row_idxs);

        // Read it back by dequantizing the written rows to f32
        ggml_tensor * out = ggml_cpy(ctx, written, ggml_new_tensor_2d(ctx, GGML_TYPE_F32, ne0, ne1));
        ggml_set_name(out, "out");
        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (t->type == GGML_TYPE_I64 || t->type == GGML_TYPE_I32) {
                if (ggml_is_view_op(t->op)) continue;
                init_set_rows_row_ids(t, ne1);
            } else {
                init_tensor_uniform(t);
            }
        }
    }

    double max_nmse_err() override {
        // turbo3 is 3-bit quantization with WHT rotation.
        // The round-trip error (f32 -> turbo3 -> f32) is higher than q8_0
        // but bounded. Empirically ~0.02 NMSE for uniform[-1,1] data.
        return 0.05;
    }
};

// Test SET_ROWS with turbo4 destination, then dequantize and compare.
// Mirrors test_set_rows_turbo3 for the 4-bit PolarQuant type: f32 -> WHT ->
// 16-centroid quantize -> turbo4, then turbo4 -> f32, compared against the
// CPU backend. Because `out` covers all ne1 rows, the rows NOT written by
// SET_ROWS pass through from the CPU-quantized initial upload; a backend
// whose shader block stride disagrees with the 66-byte C layout dequantizes
// those rows to garbage, so this also catches cross-backend layout drift.
struct test_set_rows_turbo4 : public test_case {
    const ggml_type type_idx;
    const int64_t ne0; // head dim (must be multiple of 128)
    const int64_t ne1; // rows in dst
    const int r;       // rows to write

    std::string vars() override {
        return VARS_TO_STR4(type_idx, ne0, ne1, r);
    }

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "SET_ROWS_TURBO4";
    }

    test_set_rows_turbo4(ggml_type type_idx = GGML_TYPE_I32,
            int64_t ne0 = 128, int64_t ne1 = 8, int r = 4)
        : type_idx(type_idx), ne0(ne0), ne1(ne1), r(r) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        // dst: the turbo4 KV cache buffer
        ggml_tensor * dst = ggml_new_tensor_2d(ctx, GGML_TYPE_TURBO4_0, ne0, ne1);
        ggml_set_name(dst, "dst");

        // src: f32 values to quantize into the cache
        ggml_tensor * src = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, ne0, r);
        ggml_set_name(src, "src");

        // row indices
        ggml_tensor * row_idxs = ggml_new_tensor_1d(ctx, type_idx, r);
        ggml_set_name(row_idxs, "row_idxs");

        // Write f32 data into turbo4 dst via SET_ROWS (includes WHT + quantize)
        ggml_tensor * written = ggml_set_rows(ctx, dst, src, row_idxs);

        // Read it back by dequantizing the written rows to f32
        ggml_tensor * out = ggml_cpy(ctx, written, ggml_new_tensor_2d(ctx, GGML_TYPE_F32, ne0, ne1));
        ggml_set_name(out, "out");
        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (t->type == GGML_TYPE_I64 || t->type == GGML_TYPE_I32) {
                if (ggml_is_view_op(t->op)) continue;
                init_set_rows_row_ids(t, ne1);
            } else {
                init_tensor_uniform(t);
            }
        }
    }

    double max_nmse_err() override {
        // 4-bit PolarQuant round-trips tighter than turbo3's 3 bits; reuse
        // the same generous bound. A layout mismatch produces errors orders
        // of magnitude beyond it, so the bound is not load-bearing for the
        // layout regression.
        return 0.05;
    }
};

// Test SET_ROWS with turbo2 destination, then dequantize and compare.
// Added by lane-148: turbo2 had NO SET_ROWS coverage at all -- the generic test_set_rows
// iterates all_types, which excludes the TURBO ids -- while set_rows_f32_turbo2_0_* shaders are
// generated and pipeline_set_rows[...][GGML_TYPE_TURBO2_0] is created. A claimed, shipped write
// path with zero executed cases. Same bespoke shape as its turbo3/turbo4 neighbours above.
struct test_set_rows_turbo2 : public test_case {
    const ggml_type type_idx;
    const int64_t ne0; // head dim (must be multiple of 128)
    const int64_t ne1; // rows in dst
    const int r;       // rows to write

    std::string vars() override {
        return VARS_TO_STR4(type_idx, ne0, ne1, r);
    }

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "SET_ROWS_TURBO2";
    }

    test_set_rows_turbo2(ggml_type type_idx = GGML_TYPE_I32,
            int64_t ne0 = 128, int64_t ne1 = 8, int r = 4)
        : type_idx(type_idx), ne0(ne0), ne1(ne1), r(r) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * dst = ggml_new_tensor_2d(ctx, GGML_TYPE_TURBO2_0, ne0, ne1);
        ggml_set_name(dst, "dst");

        ggml_tensor * src = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, ne0, r);
        ggml_set_name(src, "src");

        ggml_tensor * row_idxs = ggml_new_tensor_1d(ctx, type_idx, r);
        ggml_set_name(row_idxs, "row_idxs");

        ggml_tensor * written = ggml_set_rows(ctx, dst, src, row_idxs);

        ggml_tensor * out = ggml_cpy(ctx, written, ggml_new_tensor_2d(ctx, GGML_TYPE_F32, ne0, ne1));
        ggml_set_name(out, "out");
        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (t->type == GGML_TYPE_I64 || t->type == GGML_TYPE_I32) {
                if (ggml_is_view_op(t->op)) continue;
                init_set_rows_row_ids(t, ne1);
            } else {
                init_tensor_uniform(t);
            }
        }
    }

    double max_nmse_err() override {
        // 2 bits per element: a looser round-trip bound than turbo3/turbo4's 0.05.
        // A layout or LUT mismatch lands orders of magnitude beyond it either way.
        return 0.35;
    }
};

// Test SET_ROWS with TQ4_1S destination (weight quantization), then dequantize and compare.
// Validates: f32 -> WHT forward -> 16-centroid quantize -> nibble pack -> SET_ROWS
// followed by: GET_ROWS/CPY -> WHT inverse -> f32 dequant. Round-trip error is bounded.
struct test_set_rows_tq4_1s : public test_case {
    const ggml_type type_idx;
    const int64_t ne0; // row width (must be multiple of 32)
    const int64_t ne1; // rows in dst
    const int r;       // rows to write

    std::string vars() override {
        return VARS_TO_STR4(type_idx, ne0, ne1, r);
    }

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "SET_ROWS_TQ4_1S";
    }

    test_set_rows_tq4_1s(ggml_type type_idx = GGML_TYPE_I32,
            int64_t ne0 = 32, int64_t ne1 = 8, int r = 4)
        : type_idx(type_idx), ne0(ne0), ne1(ne1), r(r) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        // dst: the TQ4_1S weight buffer
        ggml_tensor * dst = ggml_new_tensor_2d(ctx, GGML_TYPE_TQ4_1S, ne0, ne1);
        ggml_set_name(dst, "dst");

        // src: f32 values to quantize
        ggml_tensor * src = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, ne0, r);
        ggml_set_name(src, "src");

        // row indices
        ggml_tensor * row_idxs = ggml_new_tensor_1d(ctx, type_idx, r);
        ggml_set_name(row_idxs, "row_idxs");

        // Write f32 data into TQ4_1S dst via SET_ROWS (includes WHT + quantize)
        ggml_tensor * written = ggml_set_rows(ctx, dst, src, row_idxs);

        // Read it back by dequantizing to f32
        ggml_tensor * out = ggml_cpy(ctx, written, ggml_new_tensor_2d(ctx, GGML_TYPE_F32, ne0, ne1));
        ggml_set_name(out, "out");
        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (t->type == GGML_TYPE_I64 || t->type == GGML_TYPE_I32) {
                if (ggml_is_view_op(t->op)) continue;
                init_set_rows_row_ids(t, ne1);
            } else if (t->type == GGML_TYPE_TQ4_1S) {
                // Zero-fill TQ4_1S dst to avoid fp16 NaN in unwritten rows' d0/d1
                std::vector<uint8_t> zeros(ggml_nbytes(t), 0);
                ggml_backend_tensor_set(t, zeros.data(), 0, zeros.size());
            } else {
                init_tensor_uniform(t);
            }
        }
    }

    double max_nmse_err() override {
        // lane-150: this was 5.0, justified by a comment claiming observed GPU/CPU divergence from
        // subgroupAdd order compounding over the 6 refinement iterations. That divergence was never
        // observed: until lane-150 wired the pipelines, this case class ran 17 cases / 0 executed,
        // so 5.0 was a waive written blind — and 5.0 on a 4-bit format passes almost any output.
        // MEASURED once the path actually executed: ERR = 0.000000000 on all 17 cases (below the
        // harness's %.9f print resolution), the same value its known-good sibling
        // test_set_rows_turbo2 reports under the identical probe. Tightened to the turbo3/turbo4
        // family bound, which leaves ~8 orders of margin over the measurement and still catches a
        // layout, LUT, subgroup-width or dispatch-sizing regression by orders of magnitude
        // (proven: mutating the SET_ROWS dispatch arm back to the generic quant sizing fails here).
        return 0.05;
    }
};

// GGML_OP_ROPE + GGML_OP_VIEW + GGML_OP_SET_ROWS
struct test_rope_set_rows : public test_case {
    const ggml_type type;
    const ggml_type type_idx;
    const std::array<int64_t, 4> ne_a;
    int mode;
    const int n_ctx{512};
    const int n_dims{128};

    std::string vars() override {
        return VARS_TO_STR4(type, type_idx, ne_a, mode);
    }

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "ROPE_SET_ROWS";
    }

    bool run_whole_graph() override { return true; }

    test_rope_set_rows(ggml_type type,
            ggml_type type_idx,
            std::array<int64_t, 4> ne_a,
            int mode)
        : type(type), type_idx(type_idx), ne_a(ne_a), mode(mode) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, ne_a[0], ne_a[1], ne_a[2], 1);
        ggml_set_name(a, "a");

        const bool is_mrope = mode & GGML_ROPE_TYPE_MROPE;
        const bool is_vision = mode == GGML_ROPE_TYPE_VISION;

        ggml_tensor * pos;
        if (is_mrope || is_vision) {
            pos = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, ne_a[2] * 4);
        } else {
            pos = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, ne_a[2]);
        }
        ggml_set_name(pos, "pos");

        float fs = 1.4245f;
        float ef = 0.7465f;
        float af = 1.4245f;
        ggml_tensor * freq = nullptr;

        ggml_tensor * rope = nullptr;
        if (is_mrope) {
            if (is_vision) {
                GGML_ASSERT(n_dims/4 > 0);
                int rope_sections[4] = {n_dims/4, n_dims/4, 0, 0}; // Vision-RoPE only use first two dimension for image (x, y) coordinate
                rope = ggml_rope_multi(ctx, a, pos, freq, n_dims/2, rope_sections, mode, 0, 10000.0f, fs, ef, af, 1.0f, 1.0f);
            } else {
                GGML_ASSERT(n_dims/3 > 0);
                int rope_sections[4] = {n_dims/3, n_dims/3, n_dims/3, 0};
                rope = ggml_rope_multi(ctx, a, pos, freq, n_dims, rope_sections, mode, 0, 10000.0f, fs, ef, af, 1.0f, 1.0f);
            }
        } else {
            rope = ggml_rope(ctx, a, pos, ne_a[0], mode);
        }

        ggml_tensor * view = ggml_view_2d(ctx, rope, ne_a[0] * ne_a[1], ne_a[2], rope->nb[2], 0);

        ggml_tensor * dst = ggml_new_tensor_4d(ctx, type, ne_a[0] * ne_a[1], ne_a[2] * ne_a[3], 1, 1);
        ggml_set_name(dst, "dst");

        ggml_tensor * row_idxs = ggml_new_tensor_3d(ctx, type_idx, ne_a[2], 1, 1);
        ggml_set_name(row_idxs, "row_idxs");

        ggml_tensor * out = ggml_set_rows(ctx, dst, view, row_idxs);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (strcmp(t->name, "row_idxs") == 0) {
                if (ggml_is_view_op(t->op)) {
                    continue;
                }
                init_set_rows_row_ids(t, ne_a[2]);
            } else if (t->type == GGML_TYPE_I32) {
                // pos
                const int num_pos_ids = (mode & GGML_ROPE_TYPE_MROPE) ? ne_a[2] * 4 : ne_a[2];
                std::vector<int> data(num_pos_ids);
                for (int i = 0; i < num_pos_ids; i++) {
                    data[i] = rand() % n_ctx;
                }
                ggml_backend_tensor_set(t, data.data(), 0, num_pos_ids * sizeof(int));
            } else {
                if (t->ne[0] == n_dims/2) {
                    // frequency factors in the range [0.9f, 1.1f]
                    init_tensor_uniform(t, 0.9f, 1.1f);
                } else {
                    init_tensor_uniform(t);
                }
            }
        }
    }
};

// GGML_OP_RMS_NORM with optional GGML_OP_MUL, GGML_OP_ROPE, GGML_OP_VIEW and GGML_OP_SET_ROWS
struct test_rms_norm_mul_rope : public test_case {
    const std::array<int64_t, 4> ne;
    const float eps;
    const bool multi_add; // test a sequence of adds feeding into rms_norm
    const bool mul;
    const bool rope;
    const bool set_rows;
    const bool broadcast; // multiply by a 1D [ne0] weight, as model norm weights are
    const ggml_type set_rows_type;
    int mode;

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "RMS_NORM_MUL_ROPE";
    }

    bool run_whole_graph() override { return true; }

    std::string vars() override {
        return VARS_TO_STR9(ne, eps, multi_add, mul, rope, set_rows, broadcast, mode, set_rows_type);
    }

    test_rms_norm_mul_rope(std::array<int64_t, 4> ne, float eps = 1e-6f, bool multi_add = false,
                           bool set_rows = false, bool broadcast = false, int mode = GGML_ROPE_TYPE_NORMAL,
                           bool mul = true, bool rope = true, ggml_type set_rows_type = GGML_TYPE_F16)
        : ne(ne), eps(eps), multi_add(multi_add), mul(mul), rope(rope), set_rows(set_rows), broadcast(broadcast),
          set_rows_type(set_rows_type), mode(mode) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, ne[0], ne[1], ne[2], ne[3]);

        ggml_tensor * b = nullptr;
        ggml_tensor * c = nullptr;
        ggml_tensor * w = nullptr;

        if (multi_add || (mul && !broadcast)) {
            b = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, ne[0], ne[1], ne[2], 1);
        }
        if (multi_add) {
            c = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, ne[0], ne[1], ne[2], 1);
        }
        if (mul) {
            w = broadcast ? ggml_new_tensor_1d(ctx, GGML_TYPE_F32, ne[0]) : b;
        }

        if (multi_add) {
            a = ggml_add(ctx, ggml_add(ctx, a, b), c);
        }

        a = ggml_rms_norm(ctx, a, eps);

        if (mul) {
            a = ggml_mul(ctx, a, w);
        }

        if (rope) {
            const bool is_mrope = mode & GGML_ROPE_TYPE_MROPE;
            ggml_tensor * pos = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, ne[2] * (is_mrope ? 4 : 1));

            if (is_mrope) {
                const int n_dims = ne[0];
                int sections[4] = { n_dims/3, n_dims/3, n_dims/3, 0 };
                a = ggml_rope_multi(ctx, a, pos, nullptr, n_dims, sections, mode, 0, 10000.0f, 1.0f, 0.0f, 1.0f, 32.0f, 1.0f);
            } else {
                a = ggml_rope(ctx, a, pos, ne[0], mode);
            }
        }

        if (set_rows) {
            ggml_tensor * view = ggml_view_2d(ctx, a, ne[0] * ne[1], ne[2], a->nb[2], 0);

            ggml_tensor * dst = ggml_new_tensor_2d(ctx, set_rows_type, ne[0] * ne[1], ne[2] * 2);
            ggml_set_name(dst, "dst");

            ggml_tensor * row_idxs = ggml_new_tensor_1d(ctx, GGML_TYPE_I64, ne[2]);
            ggml_set_name(row_idxs, "row_idxs");

            a = ggml_set_rows(ctx, dst, view, row_idxs);
        }

        ggml_set_name(a, "out");
        return a;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (t->type == GGML_TYPE_I64) {
                init_set_rows_row_ids(t, ne[2] * 2);
            } else if (t->type == GGML_TYPE_I32) {
                std::vector<int32_t> data(ggml_nelements(t));
                for (int32_t & value : data) {
                    value = rand() % 512;
                }
                ggml_backend_tensor_set(t, data.data(), 0, ggml_nbytes(t));
            } else {
                init_tensor_uniform(t);
            }
        }
    }

    double max_nmse_err() override {
        return ne[0] == 8192 ? 5e-6 : test_case::max_nmse_err();
    }
};

// GGML_OP_ARGMAX
struct test_argmax : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;

    std::string vars() override {
        return VARS_TO_STR2(type, ne);
    }

    test_argmax(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 100, 1, 1})
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_argmax(ctx, a);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        std::random_device rd;
        std::default_random_engine rng(rd());
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (t->type == GGML_TYPE_F32) {
                // initialize with unique values to avoid ties
                for (int64_t r = 0; r < ggml_nrows(t); r++) {
                    std::vector<float> data(t->ne[0]);
                    for (int i = 0; i < t->ne[0]; i++) {
                        data[i] = i;
                    }
                    std::shuffle(data.begin(), data.end(), rng);
                    ggml_backend_tensor_set(t, data.data(), r * t->nb[1], t->ne[0] * sizeof(float));
                }
            } else {
                init_tensor_uniform(t);
            }
        }
    }

    double max_nmse_err() override {
        return 0.0;
    }
};

// GGML_OP_COUNT_EQUAL
struct test_count_equal : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;

    std::string vars() override {
        return VARS_TO_STR2(type, ne);
    }

    test_count_equal(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {4, 500, 1, 1})
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(a, "a");

        ggml_tensor * a_argmax = ggml_argmax(ctx, a);
        ggml_set_name(a_argmax, "a_argmax");

        ggml_tensor * b = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(b, "b");

        ggml_tensor * b_argmax = ggml_argmax(ctx, b);
        ggml_set_name(b_argmax, "b_argmax");

        ggml_tensor * out = ggml_count_equal(ctx, a_argmax, b_argmax);
        ggml_set_name(out, "out");

        return out;
    }

    double max_nmse_err() override {
        return 0.0;
    }

    void initialize_tensors(ggml_context * ctx) override {
        std::random_device rd;
        std::default_random_engine rng(rd());
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (t->type == GGML_TYPE_F32) {
                // initialize with unique values to avoid ties
                for (int64_t r = 0; r < ggml_nrows(t); r++) {
                    std::vector<float> data(t->ne[0]);
                    for (int i = 0; i < t->ne[0]; i++) {
                        data[i] = i;
                    }
                    std::shuffle(data.begin(), data.end(), rng);
                    ggml_backend_tensor_set(t, data.data(), r * t->nb[1], t->ne[0] * sizeof(float));
                }
            } else {
                init_tensor_uniform(t);
            }
        }
    }
};

// GGML_OP_REPEAT
struct test_repeat : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const std::array<int, 4> nr;

    std::string vars() override {
        return VARS_TO_STR3(type, ne, nr);
    }

    size_t op_size(ggml_tensor * t) override {
        return ggml_nbytes(t) * 2;
    }

    test_repeat(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 5, 4, 3},
            std::array<int, 4> nr = {2, 2, 2, 2})
        : type(type), ne(ne), nr(nr) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * target = ggml_new_tensor_4d(ctx, type, ne[0]*nr[0], ne[1]*nr[1], ne[2]*nr[2], ne[3]*nr[3]);
        ggml_set_name(target, "target");

        ggml_tensor * src = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(src);
        ggml_set_name(src, "src");

        ggml_tensor * out = ggml_repeat(ctx, src, target);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_REPEAT_BACK
struct test_repeat_back : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const std::array<int, 4> nr;
    const bool v; // whether src is a noncontiguous view

    std::string vars() override {
        return VARS_TO_STR4(type, ne, nr, v);
    }

    size_t op_size(ggml_tensor * t) override {
        return ggml_nbytes(t) * 2;
    }

    test_repeat_back(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {8, 6, 4, 2},
            std::array<int, 4> nr = {2, 2, 2, 2},
            bool v = false)
        : type(type), ne(ne), nr(nr), v(v) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * src = ggml_new_tensor_4d(ctx, type, ne[0]*nr[0], ne[1]*nr[1], ne[2]*nr[2], ne[3]*nr[3]);
        ggml_set_name(src, "src");

        if (v) {
            GGML_ASSERT(ne[0] % 2 == 0);
            GGML_ASSERT(ne[1] % 2 == 0);
            GGML_ASSERT(ne[2] % 2 == 0);
            GGML_ASSERT(ne[3] % 2 == 0);
            GGML_ASSERT(nr[0] % 2 == 0 || nr[0] == 1);
            GGML_ASSERT(nr[1] % 2 == 0 || nr[1] == 1);
            GGML_ASSERT(nr[2] % 2 == 0 || nr[2] == 1);
            GGML_ASSERT(nr[3] % 2 == 0 || nr[3] == 1);

            const int64_t ne00 = nr[0] == 1 ? src->ne[0] : src->ne[0] / 2;
            const int64_t ne01 = nr[1] == 1 ? src->ne[1] : src->ne[1] / 2;
            const int64_t ne02 = nr[2] == 1 ? src->ne[2] : src->ne[2] / 2;
            const int64_t ne03 = nr[3] == 1 ? src->ne[3] : src->ne[3] / 2;

            src = ggml_view_4d(ctx, src, ne00, ne01, ne02, ne03, src->nb[1], src->nb[2], src->nb[3], 0);
        }

        ggml_tensor * target = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(target, "target");

        ggml_tensor * out = ggml_repeat_back(ctx, src, target);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_DUP
struct test_dup : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const std::array<int64_t, 4> permute;
    bool _use_permute;

    std::string vars() override {
        std::string v = VARS_TO_STR2(type, ne);
        if (_use_permute) v += "," + VAR_TO_STR(permute);
        return v;
    }

    test_dup(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 10, 20, 1},
            std::array<int64_t, 4> permute = {0, 0, 0, 0})
        : type(type), ne(ne), permute(permute),
            _use_permute(permute[0] + permute[1] + permute[2] + permute[3] > 0) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * src = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(src);
        ggml_set_name(src, "src");

        if (_use_permute) {
            src = ggml_permute(ctx, src, permute[0], permute[1], permute[2], permute[3]);
            ggml_set_name(src, "src_permuted");
        }

        ggml_tensor * out = ggml_dup(ctx, src);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_SET
struct test_set : public test_case {
    const ggml_type type_src;
    const ggml_type type_dst;
    const std::array<int64_t, 4> ne;
    const int dim;
    const bool inplace;

    std::string vars() override {
        return VARS_TO_STR5(type_src, type_dst, ne, dim, inplace);
    }

    size_t op_size(ggml_tensor * t) override {
        return ggml_nbytes(t) + ggml_nbytes(t->src[0]);
    }

    test_set(ggml_type type_src = GGML_TYPE_F32, ggml_type type_dst = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {6, 5, 4, 3}, int dim = 1, bool inplace = false)
        : type_src(type_src), type_dst(type_dst), ne(ne), dim(dim), inplace(inplace) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * src = ggml_new_tensor(ctx, type_src, 4, ne.data());
        ggml_set_param(src);
        ggml_set_name(src, "src");

        auto ne_dst = ne;
        for (int i = 0; i < dim; ++i) {
            ne_dst[i] *= 2;
        }
        ggml_tensor * dst = ggml_new_tensor(ctx, type_dst, 4, ne_dst.data());
        ggml_set_param(dst);
        ggml_set_name(dst, "dst");

        size_t offset = 0;
        for (int i = 0; i < dim; ++i) {
            offset += ((ne_dst[i] - ne[i])/2)*dst->nb[i];
        }
        ggml_tensor * out;
        if (inplace) {
            out = ggml_set_inplace(ctx, dst, src,
                    // The backward pass requires setting a contiguous region:
                    src->nb[1], src->nb[2], src->nb[3], offset);
        } else {
            out = ggml_set(ctx, dst, src,
                    // The backward pass requires setting a contiguous region:
                    src->nb[1], src->nb[2], src->nb[3], offset);
        }
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_CPY
struct test_cpy : public test_case {
    const ggml_type type_src;
    const ggml_type type_dst;
    const std::array<int64_t, 4> ne_src;
    const std::array<int64_t, 4> ne_dst;
    const std::array<int64_t, 4> permute_src;
    const std::array<int64_t, 4> permute_dst;
    const std::array<int64_t, 4> dst_alloc; // if set, dst is a view into a larger buffer (strided)
    const int64_t dst_view_offset_rows;     // with dst_alloc: view starts this many rows into dst_buf
    bool _src_use_permute;
    bool _dst_use_permute;
    bool _src_transpose;
    bool _use_dst_shape;
    bool _use_dst_alloc;

    std::string vars() override {
        if (_use_dst_alloc) {
            return VARS_TO_STR9(type_src, type_dst, ne_src, ne_dst, permute_src, permute_dst, _src_transpose, dst_alloc, dst_view_offset_rows);
        }
        if (_use_dst_shape) {
            return VARS_TO_STR7(type_src, type_dst, ne_src, ne_dst, permute_src, permute_dst, _src_transpose);
        }
        return VARS_TO_STR6(type_src, type_dst, ne_src, permute_src, permute_dst, _src_transpose);
    }

    int64_t total_elements() const {
        return ne_src[0] * ne_src[1] * ne_src[2] * ne_src[3];
    }

    double max_nmse_err() override {
        if (type_src == type_dst) {
            return 0.0;
        }
        if (type_dst == GGML_TYPE_Q4_0 || type_dst == GGML_TYPE_Q4_1 || type_dst == GGML_TYPE_IQ4_NL ||
            type_dst == GGML_TYPE_Q5_0 || type_dst == GGML_TYPE_Q5_1 || type_dst == GGML_TYPE_Q8_0) {
            // estimate what the max nmse error would be if one quantized value is
            // off by one. The test values are distributed in [-150,150], so it'll be
            // roughly (150*2.0 / 2^bits)^2, divided by the mean square value of the reference,
            // which is roughly 0.25*150^2 times the number of elements.
            double err_estimate = 1.0f/8.0f * 150.0f;
            if (type_dst == GGML_TYPE_IQ4_NL) {
                // iq4_nl values are a bit more spread out
                err_estimate *= 2.0f;
            }
            if (type_dst == GGML_TYPE_Q5_0 || type_dst == GGML_TYPE_Q5_1) {
                err_estimate /= 2.0f;
            }
            if (type_dst == GGML_TYPE_Q8_0) {
                err_estimate /= 8.0f;
            }
            err_estimate *= err_estimate;
            err_estimate /= (150.0f*150.0f*0.25f)*float(total_elements());
            return err_estimate;
        }
        return 1e-6;
    }

    size_t op_size(ggml_tensor * t) override {
        return ggml_nbytes(t) + ggml_nbytes(t->src[0]);
    }

    test_cpy(ggml_type type_src = GGML_TYPE_F32, ggml_type type_dst = GGML_TYPE_F32,
            std::array<int64_t, 4> ne_src = {10, 10, 10, 1},
            std::array<int64_t, 4> ne_dst = {-1, -1, -1, -1},
            std::array<int64_t, 4> permute_src = {0, 0, 0, 0},
            std::array<int64_t, 4> permute_dst = {0, 0, 0, 0},
            bool transpose_src = false,
            std::array<int64_t, 4> dst_alloc = {0, 0, 0, 0},
            int64_t dst_view_offset_rows = 0)
        : type_src(type_src), type_dst(type_dst), ne_src(ne_src), ne_dst(ne_dst), permute_src(permute_src), permute_dst(permute_dst),
          dst_alloc(dst_alloc), dst_view_offset_rows(dst_view_offset_rows),
          _src_use_permute(permute_src[0] + permute_src[1] + permute_src[2] + permute_src[3] > 0),
          _dst_use_permute(permute_dst[0] + permute_dst[1] + permute_dst[2] + permute_dst[3] > 0),
          _src_transpose(transpose_src),
          _use_dst_shape(ne_dst[0] >= 0 && ne_dst[1] >= 0 && ne_dst[2] >= 0 && ne_dst[3] >= 0),
          _use_dst_alloc(dst_alloc[0] > 0){}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * src = ggml_new_tensor(ctx, type_src, 4, ne_src.data());
        ggml_set_param(src);
        ggml_set_name(src, "src");

        if (_src_use_permute) {
            src = ggml_permute(ctx, src, permute_src[0], permute_src[1], permute_src[2], permute_src[3]);
            ggml_set_name(src, "src_permuted");
        }

        if (_src_transpose) {
            src = ggml_transpose(ctx, src);
            ggml_set_name(src, "src_transposed");
        }

        std::array<int64_t, 4> dst_ne = _use_dst_shape ? ne_dst : std::array<int64_t, 4>{src->ne[0], src->ne[1], src->ne[2], src->ne[3]};
        ggml_tensor * dst;

        if (_use_dst_alloc) {
            // view a sub-block of a larger buffer -> strided dst
            ggml_tensor * dst_buf = ggml_new_tensor(ctx, type_dst, 4, dst_alloc.data());
            ggml_set_name(dst_buf, "dst_buf");
            dst = ggml_view_4d(ctx, dst_buf, dst_ne[0], dst_ne[1], dst_ne[2], dst_ne[3],
                dst_buf->nb[1], dst_buf->nb[2], dst_buf->nb[3],
                dst_view_offset_rows * dst_buf->nb[1]);
            ggml_set_name(dst, "dst_view");
        } else {
            dst = ggml_new_tensor(ctx, type_dst, 4, dst_ne.data());
            ggml_set_name(dst, "dst");

            if (_dst_use_permute) {
                dst = ggml_permute(ctx, dst, permute_dst[0], permute_dst[1], permute_dst[2], permute_dst[3]);
                ggml_set_name(dst, "dst_permuted");
            }
        }

        ggml_tensor * out = ggml_cpy(ctx, src, dst);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            // test extended range of values to check if casting between f32 and i32 is consistent
            init_tensor_uniform(t, -150.f, 150.f);
        }
    }
};

// GGML_OP_CONT
// permute = {0, 0, 0, 0} means no permutation: the source is transposed (or
// view-sliced). A non-identity permute applies ggml_permute before ggml_cont.
// ring-repair 2026-08-24: the recurrent-cache seq-start zeroing pattern - scale a row VIEW of a
// tall (plane-widened) tensor to zero in place, then gather that row with GET_ROWS. Vulkan
// leaked prior state through exactly this pattern (cross-request F-136 mechanism); CPU is clean.
struct test_scale_view_zero_gather : public test_case {
    const int64_t n_embd;
    const int64_t n_rows;
    const int64_t zero_row;

    std::string vars() override {
        return VARS_TO_STR3(n_embd, n_rows, zero_row);
    }

    test_scale_view_zero_gather(int64_t n_embd = 4096, int64_t n_rows = 6, int64_t zero_row = 0)
        : n_embd(n_embd), n_rows(n_rows), zero_row(zero_row) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * states = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, n_embd, n_rows);
        ggml_set_param(states);
        ggml_set_name(states, "states");

        ggml_tensor * zero_view = ggml_view_1d(ctx, states, n_embd, zero_row * states->nb[1]);
        ggml_tensor * zeroed = ggml_scale_inplace(ctx, zero_view, 0.0f);
        ggml_set_name(zeroed, "zeroed");

        ggml_tensor * rows = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, 1);
        ggml_set_name(rows, "rows");

        ggml_tensor * gathered = ggml_get_rows(ctx, states, rows);
        gathered = ggml_reshape_1d(ctx, gathered, n_embd);

        // concat(zeroed, gathered): forward-expand visits `zeroed` first, so the node order is
        // zero -> gather, exactly the production build_rs ordering (zero expanded before reads).
        ggml_tensor * out = ggml_concat(ctx, zeroed, gathered, 0);
        ggml_set_name(out, "out");
        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (t->type == GGML_TYPE_I32) {
                std::vector<int32_t> idx = { (int32_t) zero_row };
                ggml_backend_tensor_set(t, idx.data(), 0, sizeof(int32_t));
            } else {
                init_tensor_uniform(t, -50.f, 50.f);
            }
        }
    }
};

struct test_cont : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    bool use_view_slice;
    const std::array<int64_t, 4> permute;

    std::string vars() override {
        return VARS_TO_STR4(type, ne, use_view_slice, permute);
    }

    test_cont(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 10, 10, 1},
            bool use_view_slice = false,
            std::array<int64_t, 4> permute = {0, 0, 0, 0})
        : type(type), ne(ne), use_view_slice(use_view_slice), permute(permute) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * src = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(src);
        ggml_set_name(src, "src");

        const bool permuted = permute[0] != 0 || permute[1] != 0 || permute[2] != 0 || permute[3] != 0;

        ggml_tensor * dst;
        if (permuted) {
            dst = ggml_permute(ctx, src, permute[0], permute[1], permute[2], permute[3]);
            ggml_set_name(dst, "src_permuted");
        } else if (use_view_slice) {
            dst = ggml_view_4d(ctx, src, src->ne[0], 1, src->ne[2], src->ne[3],
                src->nb[1], src->nb[2], src->nb[3], src->nb[0] * (src->ne[1] - 1));
            ggml_set_name(dst, "src_view_slice");
        } else {
            dst = ggml_transpose(ctx, src);
            ggml_set_name(dst, "src_transposed");
        }

        ggml_tensor * out = ggml_cont(ctx, dst);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_ADD
// GGML_OP_SUB
// GGML_OP_MUL
// GGML_OP_DIV
struct test_bin_bcast : public test_case {
    using op_t = ggml_tensor * (*) (ggml_context *, ggml_tensor *, ggml_tensor *);
    op_t op;
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const std::array<int, 4> nr;
    int nf; // number of fused ops, nf == 1 -> single op (no fusion)
    bool perm1; // permute src1?
    bool src_overlap; // src0 and src1 are overlapping views of the same buffer

    bool run_whole_graph() override { return nf > 1; }

    std::string vars() override {
        return VARS_TO_STR6(type, ne, nr, nf, perm1, src_overlap);
    }

    size_t op_size(ggml_tensor * t) override {
        return ggml_nbytes(t) * 3;
    }

    test_bin_bcast(op_t op, ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 10, 1, 1},
            std::array<int, 4> nr = {1, 2, 1, 1},
            int nf = 1,
            bool perm1 = false, bool src_overlap = false)
        : op(op), type(type), ne(ne), nr(nr), nf(nf), perm1(perm1), src_overlap(src_overlap) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        GGML_ASSERT(nf <= 16);

        ggml_tensor * a = ggml_new_tensor_4d(ctx, type, ne[0]*nr[0], ne[1]*nr[1], ne[2]*nr[2], ne[3]*nr[3]);
        ggml_set_name(a, "a");

        ggml_tensor * b[16];
        for (int i = 0; i < nf; ++i) {
            if (perm1) {
                const int p[4] = { 1, 2, 0, 3 }; // hardcoded for now

                b[i] = ggml_new_tensor_4d(ctx, type, ne[p[0]], ne[p[1]], ne[p[2]], ne[p[3]]);
                b[i] = ggml_permute(ctx, b[i], p[0], p[1], p[2], p[3]);
            } else if (src_overlap) {
                b[i] = ggml_view_4d(ctx, a, ne[0], ne[1], ne[2], 2 * (ne[3] / 3), a->nb[1], a->nb[2], a->nb[3], (ne[3] / 3) * a->nb[3]);
            } else {
                b[i] = ggml_new_tensor(ctx, type, 4, ne.data());
            }
            ggml_set_name(b[i], (std::string("b") + std::to_string(i)).c_str());
        }

        // The backward pass supports broadcasting only for GGML_ADD:
        const bool grad_supported = op == ggml_add && ggml_are_same_shape(a, b[0]) && nf == 1 && !perm1;
        if (grad_supported) {
            ggml_set_param(a);
            ggml_set_param(b[0]);
        }

        ggml_tensor *out;

        if (src_overlap) {
            out = ggml_view_4d(ctx, a, ne[0], ne[1], ne[2], 2 * (ne[3] / 3), a->nb[1], a->nb[2], a->nb[3], 0);
        } else {
            out = a;
        }

        for (int i = 0; i < nf; ++i) {
            out = op(ctx, out, b[i]);
        }

        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (op == ggml_mul || op == ggml_div) {
                // MUL and DIV have numerical issues around zero:
                init_tensor_uniform(t, 0.9f, 1.1f);
            } else {
                init_tensor_uniform(t);
            }
        }
    }

    float grad_eps() override {
        return 0.1f * (op == ggml_mul ? ne[0]*ne[1]*ne[2]*ne[3] : 1);
    }

    bool grad_precise() override {
        return op == ggml_div;
    }

    double max_nmse_err() override {
        if (op == ggml_add && type == GGML_TYPE_F16 && nf > 1) {
            // Fused ADDs can keep FP32 intermediates while the CPU rounds each ADD to FP16.
            return 1e-6;
        }
        return test_case::max_nmse_err();
    }

    double max_maa_err() override {
        return op == ggml_add ? 1e-4 : 1e-3;
    }
};

// GGML_OP_ADD_ID
struct test_add_id : public test_case {
    const ggml_type type_a;
    const ggml_type type_b;
    const int64_t n_embd;
    const int64_t n_experts;
    const int64_t n_experts_used;
    const int64_t n_token;

    std::string vars() override {
        return VARS_TO_STR6(type_a, type_b, n_embd, n_experts, n_experts_used, n_token);
    }

    size_t op_size(ggml_tensor * t) override {
        return ggml_nbytes(t) + ggml_nbytes(t->src[0]) + ggml_nbytes(t->src[2]);
    }

    test_add_id(ggml_type type_a = GGML_TYPE_F32,
            ggml_type type_b = GGML_TYPE_F32,
            int64_t n_embd = 128,
            int64_t n_experts = 16,
            int64_t n_experts_used = 8,
            int64_t n_token = 10)
        : type_a(type_a), type_b(type_b), n_embd(n_embd),
          n_experts(n_experts), n_experts_used(n_experts_used), n_token(n_token) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor_3d(ctx, type_a, n_embd, n_experts_used, n_token);
        ggml_tensor * b = ggml_new_tensor_2d(ctx, type_b, n_embd, n_experts);
        ggml_tensor * ids = ggml_new_tensor_2d(ctx, GGML_TYPE_I32, n_experts, n_token);
        if (n_experts_used != n_experts) {
            ids = ggml_view_2d(ctx, ids, n_experts_used, n_token, ids->nb[1], 0);
            ggml_set_name(ids, "view_of_ids");
        }

        ggml_tensor * out = ggml_add_id(ctx, a, b, ids);
        ggml_set_name(out, "out");
        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (t->type == GGML_TYPE_I32) {
                if (ggml_is_view_op(t->op)) { continue; }
                std::random_device rd;
                std::default_random_engine rng(rd());
                // ids
                for (int64_t r = 0; r < ggml_nrows(t); r++) {
                    std::vector<int32_t> data(t->ne[0]);
                    for (int i = 0; i < t->ne[0]; i++) {
                        data[i] = i % n_experts;
                    }
                    std::shuffle(data.begin(), data.end(), rng);
                    ggml_backend_tensor_set(t, data.data(), r * t->nb[1], t->ne[0] * sizeof(int32_t));
                }
            } else {
                init_tensor_uniform(t);
            }
        }
    }
};

// GGML_OP_SCALE
struct test_scale : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    float scale;
    float bias;
    bool inplace;

    std::string vars() override {
        return VARS_TO_STR5(type, ne, scale, bias, inplace);
    }

    test_scale(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 10, 10, 10},
            float scale = 2.0f,
            float bias = 0.0f,
            bool inplace = false)
        : type(type), ne(ne), scale(scale), bias(bias), inplace(inplace) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * out;
        if (inplace) {
            out = ggml_scale_bias_inplace(ctx, a, scale, bias);
        } else {
            out = ggml_scale_bias(ctx, a, scale, bias);
        }
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_SCALE + GGML_UNARY_OP_TANH + GGML_OP_SCALE
struct test_softcap : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    float softcap;

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "SOFTCAP";
    }

    bool run_whole_graph() override { return true; }

    std::string vars() override {
        return VARS_TO_STR3(type, ne, softcap);
    }

    test_softcap(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 10, 10, 10},
            float softcap = 30.0f)
        : type(type), ne(ne), softcap(softcap) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());

        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_scale(ctx, ggml_tanh(ctx, ggml_scale(ctx, a, 1.0f / softcap)), softcap);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_SILU_BACK
struct test_silu_back : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    float eps;

    std::string vars() override {
        return VARS_TO_STR3(type, ne, eps);
    }

    test_silu_back(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {64, 5, 4, 3},
            float eps = 1e-6f)
        : type(type), ne(ne), eps(eps) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(a, "a");

        ggml_tensor * grad = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(grad, "grad");

        ggml_tensor * out = ggml_silu_back(ctx, a, grad);
        ggml_set_name(out, "out");

        return out;
    }

    bool grad_precise() override {
        return true;
    }
};

// GGML_OP_NORM
struct test_norm : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const bool v; // whether a is a non-contiguous view
    const float eps;
    const bool noncontig_rows;

    std::string vars() override {
        return VARS_TO_STR5(type, ne, v, eps, noncontig_rows);
    }

    test_norm(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {64, 5, 4, 3},
            bool v = false,
            float eps = 1e-6f,
            bool noncontig_rows = false)
        : type(type), ne(ne), v(v), eps(eps), noncontig_rows(noncontig_rows) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        const std::array<int64_t, 4> ne_a = noncontig_rows ?
            std::array<int64_t, 4>{ ne[1], ne[0], ne[2], ne[3] } : ne;
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne_a.data());
        ggml_set_name(a, "a");

        if (noncontig_rows) {
            a = ggml_permute(ctx, a, 1, 0, 2, 3);
            ggml_set_name(a, "permuted a");
        }
        if (v) {
            a = ggml_view_4d(ctx, a, a->ne[0]/2, a->ne[1]/2, a->ne[2]/2, a->ne[3]/2, a->nb[1], a->nb[2], a->nb[3], 0);
            ggml_set_name(a, "view of a");
        }

        ggml_tensor * out = ggml_norm(ctx, a, eps);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_NORM + GGML_OP_MUL + GGML_OP_ADD
struct test_norm_mul_add : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    float eps;
    const bool broadcast;

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "NORM_MUL_ADD";
    }

    bool run_whole_graph() override { return true; }

    std::string vars() override {
        return VARS_TO_STR4(type, ne, eps, broadcast);
    }

    test_norm_mul_add(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {128, 2, 1, 1},
            float eps = 1e-5f,
            bool broadcast = false)
        : type(type), ne(ne), eps(eps), broadcast(broadcast) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        std::array<int64_t, 4> broadcast_dims = {ne[0], ne[1] * 2, ne[2] * 2, ne[3] * 2};

        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, broadcast ? broadcast_dims.data() : ne.data());
        ggml_tensor * w = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_tensor * b = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(a); ggml_set_param(w); ggml_set_param(b);
        ggml_set_name(a, "a"); ggml_set_name(w, "w"); ggml_set_name(b, "b");

        // Use a, w and b early to avoid OP_NONE in graph
        a = ggml_add(ctx, ggml_add(ctx, a, w), b);

        ggml_tensor * n = ggml_norm(ctx, a, eps);
        ggml_tensor * m = ggml_mul(ctx, n, w);
        ggml_tensor * out = ggml_add(ctx, m, b);
        ggml_set_name(out, "out");
        return out;
    }
};
// GGML_OP_NORM/RMS_NORM + GGML_OP_SCALE
struct test_norm_scale : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const float eps;
    const bool rms;
    const float scale;

    std::string vars() override {
        return VARS_TO_STR5(type, ne, eps, rms, scale);
    }

    test_norm_scale(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {64, 5, 4, 3},
            float eps = 1e-6f,
            bool rms = false,
            float scale = 1.5f)
        : type(type), ne(ne), eps(eps), rms(rms), scale(scale) {}

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return rms ? "RMS_NORM_SCALE" : "NORM_SCALE";
    }

    bool run_whole_graph() override { return true; }

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(a, "a");

        ggml_tensor * n = rms ? ggml_rms_norm(ctx, a, eps) : ggml_norm(ctx, a, eps);
        ggml_tensor * out = ggml_scale(ctx, n, scale);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_RMS_NORM
struct test_rms_norm : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const bool v; // whether a is a non-contiguous view
    const float eps;
    const bool inplace; // whether to do the operation inplace

    std::string vars() override {
        return VARS_TO_STR5(type, ne, v, eps, inplace);
    }

    test_rms_norm(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {64, 5, 4, 3},
            bool v = false,
            float eps = 1e-6f,
            bool inplace = false)
        : type(type), ne(ne), v(v), eps(eps), inplace(inplace) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(a);
        ggml_set_name(a, "a");

        if (v) {
            a = ggml_view_4d(ctx, a, a->ne[0]/2, a->ne[1]/2, a->ne[2]/2, a->ne[3]/2, a->nb[1], a->nb[2], a->nb[3], 0);
            ggml_set_name(a, "view of a");
        }

        ggml_tensor * out;
        if (inplace) {
            out = ggml_rms_norm_inplace(ctx, a, eps);
        } else {
            out = ggml_rms_norm(ctx, a, eps);
        }
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, -10.f, 10.f);
        }
    }

    float grad_eps() override {
        return 1.0f;
    }

    bool grad_precise() override {
        return true;
    }
};

// GGML_OP_RMS_NORM_BACK
struct test_rms_norm_back : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const float eps;

    std::string vars() override {
        return VARS_TO_STR3(type, ne, eps);
    }

    test_rms_norm_back(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {64, 5, 4, 3},
            float eps = 1e-6f)
        : type(type), ne(ne), eps(eps) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(a, "a");

        ggml_tensor * b = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(b, "b");

        ggml_tensor * out = ggml_rms_norm_back(ctx, a, b, eps);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, -10.f, 10.f);
        }
    }
};

// GGML_OP_RMS_NORM + GGML_OP_MUL + GGML_OP_ADD (+ GGML_OP_MUL)
struct test_rms_norm_mul_add : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const float eps;
    const bool broadcast;
    const bool multi_add; // test a sequence of adds feeding into rms_norm
    const bool post_mul;
    const bool alias_rms_input;
    const bool weight_broadcast;

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "RMS_NORM_MUL_ADD";
    }

    bool run_whole_graph() override { return true; }

    std::string vars() override {
        return VARS_TO_STR8(type, ne, eps, broadcast, multi_add, post_mul, alias_rms_input, weight_broadcast);
    }

    test_rms_norm_mul_add(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {64, 5, 4, 3},
            float eps = 1e-6f, bool broadcast = false, bool multi_add = false, bool post_mul = false,
            bool alias_rms_input = false, bool weight_broadcast = false)
        : type(type), ne(ne), eps(eps), broadcast(broadcast), multi_add(multi_add), post_mul(post_mul),
          alias_rms_input(alias_rms_input), weight_broadcast(weight_broadcast) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        std::array<int64_t, 4> broadcast_dims = {ne[0]*2, ne[1]*3, ne[2]*3, ne[3]*4};

        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, broadcast ? broadcast_dims.data() : ne.data());
        ggml_tensor * b = weight_broadcast ? ggml_new_tensor_1d(ctx, type, ne[0]) : ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_tensor * c = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_tensor * d = nullptr;

        ggml_set_param(a);
        ggml_set_name(a, "a");
        ggml_set_param(b);
        ggml_set_name(b, "b");
        ggml_set_param(c);
        ggml_set_name(c, "c");

        // Use a, b and c early, so we don't end up with an OP_NONE between rms_norm and mul
        a = ggml_add(ctx, ggml_add(ctx, a, b), c);
        if (post_mul) {
            d = ggml_new_tensor_1d(ctx, type, 1);
            ggml_set_param(d);
            ggml_set_name(d, "d");
            a = ggml_add(ctx, a, d);
        }
        if (multi_add) {
            a = ggml_add(ctx, ggml_add(ctx, a, b), c);
        }
        ggml_tensor * mul = ggml_mul(ctx, ggml_rms_norm(ctx, a, eps), b);
        ggml_tensor * out = alias_rms_input ? ggml_add_inplace(ctx, a, mul) : ggml_add(ctx, mul, c);
        if (post_mul) {
            out = ggml_mul(ctx, out, d);
        }
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, -10.f, 10.f);
        }
    }

    float grad_eps() override {
        return 1.0f;
    }

    bool grad_precise() override {
        return true;
    }
};

// GGML_OP_ADD + GGML_OP_ADD (fused residual chain)
struct test_add_add : public test_case {
    const ggml_type type;
    const ggml_type type_addend;
    const std::array<int64_t, 4> ne;
    const bool broadcast;
    const bool view; // non-contiguous a via view_4d

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "ADD_ADD";
    }

    bool run_whole_graph() override { return true; }

    std::string vars() override {
        return VARS_TO_STR5(type, type_addend, ne, broadcast, view);
    }

    test_add_add(ggml_type type = GGML_TYPE_F32,
            ggml_type type_addend = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {64, 5, 4, 3},
            bool broadcast = false,
            bool view = false)
        : type(type), type_addend(type_addend), ne(ne), broadcast(broadcast), view(view) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        std::array<int64_t, 4> broadcast_dims = {ne[0], 1, 1, 1};

        ggml_tensor * a;
        if (view) {
            std::array<int64_t, 4> parent = { ne[0] * 3, ne[1] * 2, ne[2], ne[3] };
            a = ggml_new_tensor(ctx, type, 4, parent.data());
            ggml_set_name(a, "a_parent");
            a = ggml_view_4d(ctx, a, ne[0], ne[1], ne[2], ne[3], a->nb[1], a->nb[2], a->nb[3], 0);
            ggml_set_name(a, "a");
        } else {
            a = ggml_new_tensor(ctx, type, 4, ne.data());
            ggml_set_name(a, "a");
        }

        ggml_tensor * b = ggml_new_tensor(ctx, type_addend, 4, ne.data());
        ggml_tensor * c = ggml_new_tensor(ctx, type_addend, 4, broadcast ? broadcast_dims.data() : ne.data());

        ggml_set_name(b, "b");
        ggml_set_name(c, "c");

        ggml_tensor * out = ggml_add(ctx, ggml_add(ctx, a, b), c);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_ADD + GGML_OP_RMS_NORM (fused operation)
struct test_add_rms_norm : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const float eps;
    const bool broadcast;

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "ADD_RMS_NORM";
    }

    bool run_whole_graph() override { return true; }

    std::string vars() override {
        return VARS_TO_STR4(type, ne, eps, broadcast);
    }

    test_add_rms_norm(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {64, 5, 4, 3},
            float eps = 1e-6f, bool broadcast = false)
        : type(type), ne(ne), eps(eps), broadcast(broadcast) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        std::array<int64_t, 4> broadcast_dims = {ne[0]*2, ne[1]*3, ne[2]*3, ne[3]*4};

        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, broadcast ? broadcast_dims.data() : ne.data());
        ggml_tensor * b = ggml_new_tensor(ctx, type, 4, ne.data());

        ggml_set_param(a);
        ggml_set_name(a, "a");
        ggml_set_param(b);
        ggml_set_name(b, "b");

        // ADD operation followed by RMS_NORM
        ggml_tensor * add_result = ggml_add(ctx, a, b);
        ggml_set_name(add_result, "add_result");

        ggml_tensor * out = ggml_rms_norm(ctx, add_result, eps);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, -10.f, 10.f);
        }
    }

    float grad_eps() override {
        return 1.0f;
    }

    bool grad_precise() override {
        return true;
    }
};

// GGML_OP_UNARY(RELU) + GGML_OP_SQR (fused operation)
struct test_relu_sqr : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "RELU_SQR";
    }

    bool run_whole_graph() override { return true; }

    std::string vars() override {
        return VARS_TO_STR2(type, ne);
    }

    test_relu_sqr(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {128, 2, 2, 2})
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(a, "a");

        ggml_tensor * r = ggml_relu(ctx, a);
        ggml_set_name(r, "relu");

        ggml_tensor * out = ggml_sqr(ctx, r);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_UNARY(GELU|SILU|SIGMOID|SOFTPLUS) + GGML_OP_MUL (fused operation).
struct test_unary_mul : public test_case {
    const ggml_unary_op op;
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const bool swap;          // unary result is the second MUL operand
    const std::string layout; // operand layout, see build_graph()
    const std::string tail;   // extra consumer past the MUL, see build_graph()

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return std::string(ggml_unary_op_name(op)) + "_MUL";
    }

    bool run_whole_graph() override { return true; }

    double max_nmse_err() override {
        // the fused kernel elides the rounding of the unary result that the CPU chain
        // performs; relax the tolerance to match that drift
        switch (type) {
            case GGML_TYPE_F16: return 5e-5;
            // gelu shader uses exp form, CPU uses tanhf
            default:            return op == GGML_UNARY_OP_GELU ? 5e-7 : 1e-7;
        }
    }

    std::string vars() override {
        return VARS_TO_STR5(type, ne, swap, layout, tail);
    }

    test_unary_mul(ggml_unary_op op,
            ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {128, 2, 2, 2},
            bool swap = false,
            std::string layout = "packed",
            std::string tail = "")
        : op(op), type(type), ne(ne), swap(swap), layout(std::move(layout)), tail(std::move(tail)) {}

    // `ne` viewed out of a wider tensor: rows stay contiguous, but the stride exceeds the width
    ggml_tensor * padded(ggml_context * ctx, const char * name, int64_t mul0, int64_t off0) {
        std::array<int64_t, 4> ne_w = ne;
        ne_w[0] *= mul0;
        ggml_tensor * base = ggml_new_tensor(ctx, type, 4, ne_w.data());
        ggml_set_name(base, name);
        return ggml_view_4d(ctx, base, ne[0], ne[1], ne[2], ne[3],
                            base->nb[1], base->nb[2], base->nb[3], off0 * base->nb[0]);
    }

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = nullptr; // unary source
        ggml_tensor * b = nullptr; // other MUL operand

        if (layout == "packed") {
            a = ggml_new_tensor(ctx, type, 4, ne.data());
            b = ggml_new_tensor(ctx, type, 4, ne.data());
        } else if (layout == "pad_unary") {
            a = padded(ctx, "a", 3, 0);
            b = ggml_new_tensor(ctx, type, 4, ne.data());
        } else if (layout == "pad_other") {
            a = ggml_new_tensor(ctx, type, 4, ne.data());
            b = padded(ctx, "b", 3, 0);
        } else if (layout == "halves") {
            // the shape the Conformer audio encoders build: one tensor split in two
            std::array<int64_t, 4> ne_w = ne;
            ne_w[0] *= 2;
            ggml_tensor * base = ggml_new_tensor(ctx, type, 4, ne_w.data());
            ggml_set_name(base, "base");
            b = ggml_view_4d(ctx, base, ne[0], ne[1], ne[2], ne[3], base->nb[1], base->nb[2], base->nb[3], 0);
            a = ggml_view_4d(ctx, base, ne[0], ne[1], ne[2], ne[3], base->nb[1], base->nb[2], base->nb[3],
                             ne[0] * base->nb[0]);
        } else if (layout == "strided_dim1") {
            // contiguous rows but a strided dim 1: not ggml_is_contiguous_1, must not fuse
            std::array<int64_t, 4> ne_w = ne;
            ne_w[1] *= 3;
            ggml_tensor * base = ggml_new_tensor(ctx, type, 4, ne_w.data());
            ggml_set_name(base, "a");
            a = ggml_view_4d(ctx, base, ne[0], ne[1], ne[2], ne[3], base->nb[1], base->nb[2], base->nb[3], 0);
            b = ggml_new_tensor(ctx, type, 4, ne.data());
        } else if (layout == "bcast") {
            a = ggml_new_tensor(ctx, type, 4, ne.data());
            b = ggml_new_tensor_4d(ctx, type, ne[0], 1, 1, 1);
        } else if (layout == "rep_ne0") {
            // repeat on dim 0
            a = ggml_new_tensor(ctx, type, 4, ne.data());
            std::array<int64_t, 4> ne_b = ne;
            ne_b[0] /= 4;
            b = ggml_new_tensor(ctx, type, 4, ne_b.data());
        } else if (layout == "view_mid") {
            // VIEW between UNARY and MUL
            a = ggml_new_tensor(ctx, type, 4, ne.data());
            b = nullptr;
        } else if (layout == "gate") {
            // small gate on src1
            const std::array<int64_t, 4> ne_gate = { 1, ne[1], ne[2], ne[3] };
            a = ggml_new_tensor(ctx, type, 4, ne_gate.data());
            b = ggml_new_tensor(ctx, type, 4, ne.data());
        } else {
            GGML_ABORT("unknown layout %s", layout.c_str());
        }
        if (a != nullptr) {
            ggml_set_name(a, "a");
        }
        if (b != nullptr) {
            ggml_set_name(b, "b");
        }

        ggml_tensor * u = ggml_unary(ctx, a, op);
        ggml_set_name(u, "unary");

        // a broadcasting operand can only be the second one
        const bool second = layout == "gate" || (swap && layout != "bcast" && layout != "view_mid");
        if (layout == "view_mid") {
            std::array<int64_t, 4> ne_base = ne;
            ne_base[0] *= 2;
            ggml_tensor * base = ggml_new_tensor(ctx, type, 4, ne_base.data());
            ggml_set_name(base, "base");
            b = ggml_view_4d(ctx, base, ne[0], ne[1], ne[2], ne[3],
                             base->nb[1], base->nb[2], base->nb[3], 0);
            ggml_set_name(b, "b");
        }
        ggml_tensor * out = second ? ggml_mul(ctx, b, u) : ggml_mul(ctx, u, b);

        if (tail == "reuse") {
            // a second read of the unary result must block the fusion
            ggml_set_name(out, "mul");
            out = ggml_add(ctx, out, u);
        } else if (tail == "consumer") {
            // fusion still applies; catches a dispatcher that skips one node too many
            ggml_set_name(out, "mul");
            out = ggml_add(ctx, out, b);
        } else if (!tail.empty()) {
            GGML_ABORT("unknown tail %s", tail.c_str());
        }
        ggml_set_name(out, "out");

        return out;
    }
};

// SNAKE activation fusion: y = x + sin(a*x)^2 * inv_b
// CUDA backend matches the naive 5-op chain (mul, sin, sqr, mul, add)
// and dispatches a single fused kernel.
struct test_snake_fuse : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;   // [T, C, D2, D3]

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "SNAKE_FUSE";
    }

    bool run_whole_graph() override { return true; }

    double max_nmse_err() override {
        // BF16 epsilon ~ 7.8e-3, F16 epsilon ~ 9.7e-4: relax tolerance to match
        // the natural roundoff drift between the naive CPU chain and the fused
        // CUDA kernel. F32 keeps the default tight bound.
        switch (type) {
            case GGML_TYPE_BF16: return 5e-3;
            case GGML_TYPE_F16:  return 5e-5;
            default:             return 1e-7;
        }
    }

    std::string vars() override {
        return VARS_TO_STR2(type, ne);
    }

    test_snake_fuse(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {256, 192, 1, 1})
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * x = ggml_new_tensor_4d(ctx, type, ne[0], ne[1], ne[2], ne[3]);
        ggml_set_name(x, "x");

        ggml_tensor * a = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 1, ne[1]);
        ggml_set_name(a, "a");

        ggml_tensor * inv_b = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 1, ne[1]);
        ggml_set_name(inv_b, "inv_b");

        // exact 5-op chain that BigVGAN / Vocos frontends emit
        ggml_tensor * ax     = ggml_mul(ctx, x, a);
        ggml_tensor * sin_ax = ggml_sin(ctx, ax);
        ggml_tensor * sin_sq = ggml_sqr(ctx, sin_ax);
        ggml_tensor * scaled = ggml_mul(ctx, sin_sq, inv_b);
        ggml_tensor * out    = ggml_add(ctx, x, scaled);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        // x in [-pi, pi] to exercise sin periodicity, params in default [-1, 1]
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != nullptr; t = ggml_get_next_tensor(ctx, t)) {
            const std::string name = ggml_get_name(t);
            if (name == "x") {
                init_tensor_uniform(t, -3.14159f, 3.14159f);
            } else {
                init_tensor_uniform(t);
            }
        }
    }
};


struct test_dsv4_hc : public test_case {
    static constexpr int64_t hc = 4;

    ggml_tensor * out = nullptr;

    static uint32_t tensor_seed(const ggml_tensor * t) {
        uint32_t seed = 2166136261u;
        for (const char * p = ggml_get_name(t); *p; ++p) {
            seed ^= (uint8_t) *p;
            seed *= 16777619u;
        }
        for (int i = 0; i < GGML_MAX_DIMS; ++i) {
            seed ^= (uint32_t) t->ne[i];
            seed *= 16777619u;
        }
        return seed;
    }

    static bool tensor_range(const std::string & name, float & lo, float & hi) {
        if (name == "mixes") {
            lo = -2.0f; hi = 2.0f; return true;
        }
        if (name == "scale") {
            lo = -0.5f; hi = 0.5f; return true;
        }
        if (name == "base") {
            lo = -0.25f; hi = 0.25f; return true;
        }
        if (name == "weights" || name == "comb") {
            lo = 0.0f; hi = 1.0f; return true;
        }
        if (name == "post") {
            lo = 0.0f; hi = 2.0f; return true;
        }
        if (name == "gate") {
            lo = -4.0f; hi = 4.0f; return true;
        }
        if (name == "x" || name == "residual") {
            lo = -1.0f; hi = 1.0f; return true;
        }
        return false;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != nullptr; t = ggml_get_next_tensor(ctx, t)) {
            const std::string name = ggml_get_name(t);
            float lo;
            float hi;
            if (!tensor_range(name, lo, hi)) {
                init_tensor_uniform(t);
                continue;
            }

            GGML_ASSERT(t->type == GGML_TYPE_F32);
            std::mt19937 rng(tensor_seed(t));
            std::uniform_real_distribution<float> dist(lo, hi);
            std::vector<float> data(ggml_nelements(t));
            for (float & v : data) {
                v = dist(rng);
            }
            ggml_backend_tensor_set(t, data.data(), 0, data.size()*sizeof(float));
        }
    }
};

struct test_dsv4_hc_comb : public test_dsv4_hc {
    const int64_t n_tokens;
    const int32_t n_iter;
    const float eps;

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "DSV4_HC_COMB";
    }

    std::string vars() override {
        return VARS_TO_STR3(n_tokens, n_iter, eps);
    }

    test_dsv4_hc_comb(int64_t n_tokens = 17, int32_t n_iter = 4, float eps = 1e-6f)
        : n_tokens(n_tokens), n_iter(n_iter), eps(eps) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * mixes = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, (2 + hc)*hc, n_tokens);
        ggml_set_name(mixes, "mixes");

        ggml_tensor * scale = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 3);
        ggml_set_name(scale, "scale");

        ggml_tensor * base = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, (2 + hc)*hc);
        ggml_set_name(base, "base");

        out = ggml_dsv4_hc_comb(ctx, mixes, scale, base, eps, n_iter);
        ggml_set_name(out, "out");
        return out;
    }
};

struct test_dsv4_hc_pre : public test_dsv4_hc {
    const int64_t n_embd;
    const int64_t n_hc;
    const int64_t n_tokens;
    const bool    gated;

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "DSV4_HC_PRE";
    }

    std::string vars() override {
        return VARS_TO_STR4(n_embd, n_hc, n_tokens, gated);
    }

    test_dsv4_hc_pre(int64_t n_embd = 31, int64_t n_hc = 4, int64_t n_tokens = 17, bool gated = false)
        : n_embd(n_embd), n_hc(n_hc), n_tokens(n_tokens), gated(gated) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * x = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, n_embd, n_hc, n_tokens);
        ggml_set_name(x, "x");

        if (gated) {
            ggml_tensor * gate = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, n_embd, n_hc, n_tokens);
            ggml_set_name(gate, "gate");

            out = ggml_dsv4_hc_pre_gated(ctx, x, gate, 1.0f/n_hc);
        } else {
            ggml_tensor * weights = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, n_hc, n_tokens);
            ggml_set_name(weights, "weights");

            out = ggml_dsv4_hc_pre(ctx, x, weights);
        }
        ggml_set_name(out, "out");
        return out;
    }
};

struct test_dsv4_hc_post : public test_dsv4_hc {
    const int64_t n_embd;
    const int64_t n_tokens;
    const bool    identity;

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "DSV4_HC_POST";
    }

    std::string vars() override {
        return VARS_TO_STR3(n_embd, n_tokens, identity);
    }

    test_dsv4_hc_post(int64_t n_embd = 31, int64_t n_tokens = 17, bool identity = false)
        : n_embd(n_embd), n_tokens(n_tokens), identity(identity) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * x = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, n_embd, n_tokens);
        ggml_set_name(x, "x");

        ggml_tensor * residual = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, n_embd, hc, n_tokens);
        ggml_set_name(residual, "residual");

        ggml_tensor * post = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, hc, n_tokens);
        ggml_set_name(post, "post");

        ggml_tensor * comb = nullptr;
        if (!identity) {
            comb = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, hc, hc, n_tokens);
            ggml_set_name(comb, "comb");
        }

        out = ggml_dsv4_hc_post(ctx, x, residual, post, comb);
        ggml_set_name(out, "out");
        return out;
    }
};


// GGML_OP_SSM_CONV
// GGML_OP_ESCHA_MM — ArifiLabs Escha-W2 fused linear (lane-164)
struct test_escha_mm : public test_case {
    const ggml_type type;   // GGML_TYPE_ESCHA2 or GGML_TYPE_ESCHA3
    const int64_t n_in;
    const int64_t n_out;
    const int64_t ncols;

    std::string vars() override {
        return VARS_TO_STR4(type, n_in, n_out, ncols);
    }

    double max_nmse_err() override {
        return 1e-6; // decode is integer-deterministic; only accumulation order differs
    }

    test_escha_mm(ggml_type type = GGML_TYPE_ESCHA2, int64_t n_in = 512, int64_t n_out = 256, int64_t ncols = 4)
        : type(type), n_in(n_in), n_out(n_out), ncols(ncols) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a   = ggml_new_tensor_2d(ctx, type, n_in, n_out);
        ggml_tensor * b   = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, n_in, ncols);
        ggml_tensor * aux = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, n_in + 2*n_out);
        ggml_set_name(a, "codes");
        ggml_set_name(b, "x");
        ggml_set_name(aux, "aux");
        return ggml_escha_mm(ctx, a, b, aux);
    }

    void initialize_tensors(ggml_context * ctx) override {
        std::default_random_engine gen(0xE5C4A);
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != nullptr; t = ggml_get_next_tensor(ctx, t)) {
            if (t->type == GGML_TYPE_ESCHA2 || t->type == GGML_TYPE_ESCHA3) {
                // any random bytes are valid escha codes
                std::vector<uint8_t> data(ggml_nbytes(t));
                std::uniform_int_distribution<int> dist(0, 255);
                for (auto & v : data) {
                    v = (uint8_t) dist(gen);
                }
                ggml_backend_tensor_set(t, data.data(), 0, data.size());
            } else {
                init_tensor_uniform(t);
            }
        }
    }
};

struct test_ssm_conv : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne_a;
    const std::array<int64_t, 4> ne_b;

    std::string vars() override {
        return VARS_TO_STR3(type, ne_a, ne_b);
    }

    test_ssm_conv(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne_a = {10, 10, 10, 1},
            std::array<int64_t, 4> ne_b = {3, 3, 1, 1})
        : type(type), ne_a(ne_a), ne_b(ne_b) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a   = ggml_new_tensor(ctx, type, 4, ne_a.data());
        ggml_tensor * b   = ggml_new_tensor(ctx, type, 4, ne_b.data());
        ggml_tensor * out = ggml_ssm_conv(ctx, a, b);
        return out;
    }
};

// GGML_OP_SSM_CONV + GGML_OP_ADD (channel-wise bias, optional) + GGML_OP_UNARY(SILU) (fused operation)
struct test_ssm_conv_bias_silu : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne_a;
    const std::array<int64_t, 4> ne_b;
    const bool fuse_bias;

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "SSM_CONV_BIAS_SILU";
    }

    bool run_whole_graph() override { return true; }

    std::string vars() override {
        return VARS_TO_STR4(type, ne_a, ne_b, fuse_bias);
    }

    test_ssm_conv_bias_silu(ggml_type type, std::array<int64_t, 4> ne_a, std::array<int64_t, 4> ne_b,
            bool fuse_bias)
        : type(type), ne_a(ne_a), ne_b(ne_b), fuse_bias(fuse_bias) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne_a.data());
        ggml_tensor * b = ggml_new_tensor(ctx, type, 4, ne_b.data());
        ggml_set_name(a, "a");
        ggml_set_name(b, "b");

        ggml_tensor * out = ggml_ssm_conv(ctx, a, b);

        if (fuse_bias) {
            ggml_tensor * bias = ggml_new_tensor_1d(ctx, type, out->ne[0]);
            ggml_set_name(bias, "bias");
            out = ggml_add(ctx, out, bias);
        }

        out = ggml_silu(ctx, out);

        ggml_set_name(out, "out");
        return out;
    }
};

// GGML_OP_SSM_SCAN
struct test_ssm_scan : public test_case {
    const ggml_type type;

    const int64_t d_state;
    const int64_t head_dim;
    const int64_t n_head;
    const int64_t n_group;
    const int64_t n_seq_tokens;
    const int64_t n_seqs;
    const bool    xbc_overlap;
    const int64_t K;
    const bool    weak_decay;

    std::string vars() override {
        return VARS_TO_STR10(type, d_state, head_dim, n_head, n_group, n_seq_tokens, n_seqs, xbc_overlap, K, weak_decay);
    }

    test_ssm_scan(ggml_type type = GGML_TYPE_F32,
            int64_t d_state = 32,
            int64_t head_dim = 1, // 1 = Mamba-1; > 1 = Mamba-2 (scalar A per head)
            int64_t n_head  = 32,
            int64_t n_group = 1,
            int64_t n_seq_tokens = 32,
            int64_t n_seqs = 32,
            bool xbc_overlap = false,
            int64_t K = 1,
            bool weak_decay = false)
        : type(type), d_state(d_state), head_dim(head_dim), n_head(n_head), n_group(n_group), n_seq_tokens(n_seq_tokens), n_seqs(n_seqs), xbc_overlap(xbc_overlap), K(K), weak_decay(weak_decay) {}

    double max_nmse_err() override {
        // SSD path (head_dim > 1) uses FP16 intermediates (M matrix, X_dt); Mamba-1 is pure FP32.
        return (head_dim > 1) ? 2e-7 : 1e-7;
    }

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * s   = ggml_new_tensor_4d(ctx, type, d_state,  head_dim,     n_head,       n_seqs);
        ggml_tensor * dt  = ggml_new_tensor_3d(ctx, type, n_head,   n_seq_tokens, n_seqs);
        ggml_tensor * A   = ggml_new_tensor_2d(ctx, type, (head_dim > 1) ? 1 : d_state, n_head);
        ggml_tensor * x;
        ggml_tensor * B;
        ggml_tensor * C;

        if (xbc_overlap) {
            ggml_tensor * xbc = ggml_new_tensor_4d(ctx, type, d_state, n_head, n_seq_tokens, 2 * n_seqs);
            x = ggml_view_4d(ctx, xbc, head_dim, n_head, n_seq_tokens, n_seqs,
                             xbc->nb[1], xbc->nb[2], xbc->nb[3], xbc->nb[3]);
            B = ggml_view_4d(ctx, xbc, d_state, n_group, n_seq_tokens, n_seqs,
                             xbc->nb[1], xbc->nb[2], xbc->nb[3], 0);
            C = ggml_view_4d(ctx, xbc, d_state, n_group, n_seq_tokens, n_seqs,
                             xbc->nb[1], xbc->nb[2], xbc->nb[3], 2 * xbc->nb[3]);
        } else {
            x = ggml_new_tensor_4d(ctx, type, head_dim, n_head, n_seq_tokens, n_seqs);
            B = ggml_new_tensor_4d(ctx, type, d_state,  n_group, n_seq_tokens, n_seqs);
            C = ggml_new_tensor_4d(ctx, type, d_state,  n_group, n_seq_tokens, n_seqs);
        }
        ggml_tensor * ids = ggml_new_tensor_1d(ctx, GGML_TYPE_I32,  n_seqs);
        ggml_tensor * out = ggml_ssm_scan(ctx, s, x, dt, A, B, C, ids, K);
        return out;
    }


    void initialize_tensors(ggml_context * ctx) override {
        std::random_device rd;
        std::default_random_engine rng(rd());
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (t->type == GGML_TYPE_I32) {
                if (ggml_is_view_op(t->op)) { continue; }
                // ids: permutation of [0..n_seqs)
                for (int64_t r = 0; r < ggml_nrows(t); r++) {
                    std::vector<int32_t> data(t->ne[0]);
                    for (int i = 0; i < t->ne[0]; i++) {
                        data[i] = i;
                    }
                    std::shuffle(data.begin(), data.end(), rng);
                    ggml_backend_tensor_set(t, data.data(), r * t->nb[1], t->ne[0] * sizeof(int32_t));
                }
            } else if (ggml_is_view_op(t->op)) {
                continue;
            } else if (t->ne[1] == n_head && t->ne[2] == 1) {
                // A {1 or d_state, n_head}: negative decay (2-D tensor, ne[2]==1 distinguishes from 3-D/4-D tensors)
                init_tensor_uniform(t, weak_decay ? -0.02f : -1.0f, weak_decay ? -0.005f : -0.5f);
            } else {
                init_tensor_uniform(t);
            }
        }
    }
};

struct test_ssm_scan_rollback : public test_case {
    const ggml_type type;

    const int64_t d_state;
    const int64_t head_dim;
    const int64_t n_head;
    const int64_t n_group;
    const int64_t n_seq_tokens;
    const int64_t n_seqs;
    const int64_t K;

    std::string vars() override {
        return VARS_TO_STR8(type, d_state, head_dim, n_head, n_group, n_seq_tokens, n_seqs, K);
    }

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "SSM_SCAN_ROLLBACK";
    }

    bool run_whole_graph() override {
        return true;
    }

    double max_err() override {
        return 1e-6;
    }

    double err(const float * a, const float * b, size_t n) override {
        double result = 0.0;
        for (size_t i = 0; i < n; ++i) {
            result = std::max(result, (double) fabsf(a[i]));
            result = std::max(result, (double) fabsf(b[i]));
        }
        return result;
    }

    test_ssm_scan_rollback(ggml_type type = GGML_TYPE_F32,
            int64_t d_state = 32,
            int64_t head_dim = 64,
            int64_t n_head  = 16,
            int64_t n_group = 2,
            int64_t n_seq_tokens = 8,
            int64_t n_seqs = 2,
            int64_t K = 3)
        : type(type), d_state(d_state), head_dim(head_dim), n_head(n_head), n_group(n_group),
          n_seq_tokens(n_seq_tokens), n_seqs(n_seqs), K(K) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * s   = ggml_new_tensor_4d(ctx, type, d_state,  head_dim,     n_head,       n_seqs);
        ggml_tensor * x   = ggml_new_tensor_4d(ctx, type, head_dim, n_head,       n_seq_tokens, n_seqs);
        ggml_tensor * dt  = ggml_new_tensor_3d(ctx, type, n_head,   n_seq_tokens, n_seqs);
        ggml_tensor * A   = ggml_new_tensor_2d(ctx, type, 1,        n_head);
        ggml_tensor * B   = ggml_new_tensor_4d(ctx, type, d_state,  n_group,      n_seq_tokens, n_seqs);
        ggml_tensor * C   = ggml_new_tensor_4d(ctx, type, d_state,  n_group,      n_seq_tokens, n_seqs);
        ggml_tensor * ids = ggml_new_tensor_1d(ctx, GGML_TYPE_I32,  n_seqs);

        ggml_tensor * full = ggml_ssm_scan(ctx, s, x, dt, A, B, C, ids, K);

        const int64_t y_elems     = head_dim * n_head * n_seq_tokens * n_seqs;
        const int64_t state_elems = d_state  * head_dim * n_head      * n_seqs;

        ggml_tensor * out = nullptr;
        for (int64_t slot = 0; slot < K; ++slot) {
            const int64_t prefix_tokens = n_seq_tokens - slot;

            ggml_tensor * x_prefix  = ggml_cont(ctx, ggml_view_4d(ctx, x,  head_dim, n_head,  prefix_tokens, n_seqs, x->nb[1],  x->nb[2],  x->nb[3],  0));
            ggml_tensor * dt_prefix = ggml_cont(ctx, ggml_view_3d(ctx, dt, n_head,   prefix_tokens, n_seqs, dt->nb[1], dt->nb[2], 0));
            ggml_tensor * B_prefix  = ggml_cont(ctx, ggml_view_4d(ctx, B,  d_state,  n_group, prefix_tokens, n_seqs, B->nb[1],  B->nb[2],  B->nb[3],  0));
            ggml_tensor * C_prefix  = ggml_cont(ctx, ggml_view_4d(ctx, C,  d_state,  n_group, prefix_tokens, n_seqs, C->nb[1],  C->nb[2],  C->nb[3],  0));

            ggml_tensor * prefix = ggml_ssm_scan(ctx, s, x_prefix, dt_prefix, A, B_prefix, C_prefix, ids, /*K=*/1);

            ggml_tensor * full_state   = ggml_view_1d(ctx, full,   state_elems, (y_elems + slot*state_elems)*ggml_element_size(full));
            ggml_tensor * prefix_state = ggml_view_1d(ctx, prefix, state_elems, (head_dim*n_head*prefix_tokens*n_seqs)*ggml_element_size(prefix));
            ggml_tensor * diff         = ggml_sum(ctx, ggml_sqr(ctx, ggml_sub(ctx, full_state, prefix_state)));

            out = out == nullptr ? diff : ggml_add(ctx, out, diff);
        }

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        std::random_device rd;
        std::default_random_engine rng(rd());
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (t->type == GGML_TYPE_I32) {
                if (ggml_is_view_op(t->op)) { continue; }
                for (int64_t r = 0; r < ggml_nrows(t); r++) {
                    std::vector<int32_t> data(t->ne[0]);
                    for (int i = 0; i < t->ne[0]; i++) {
                        data[i] = i;
                    }
                    std::shuffle(data.begin(), data.end(), rng);
                    ggml_backend_tensor_set(t, data.data(), r * t->nb[1], t->ne[0] * sizeof(int32_t));
                }
            } else if (ggml_is_view_op(t->op)) {
                continue;
            } else if (t->ne[1] == n_head && t->ne[2] == 1) {
                init_tensor_uniform(t, -1.0f, -0.5f);
            } else {
                init_tensor_uniform(t);
            }
        }
    }
};

// GGML_OP_RWKV_WKV6
struct test_rwkv_wkv6 : public test_case {
    const ggml_type type;

    const int64_t head_count;
    const int64_t head_size;
    const int64_t n_seq_tokens;
    const int64_t n_seqs;

    std::string vars() override {
        return VARS_TO_STR5(type, head_count, head_size, n_seq_tokens, n_seqs);
    }

    test_rwkv_wkv6(ggml_type type = GGML_TYPE_F32,
            int64_t head_count = 32, int64_t head_size = 64, int64_t n_seq_tokens = 32, int64_t n_seqs = 32)
        : type(type), head_count(head_count), head_size(head_size), n_seq_tokens(n_seq_tokens), n_seqs(n_seqs) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        const int64_t n_tokens = n_seq_tokens * n_seqs;
        ggml_tensor * r   = ggml_new_tensor(ctx, type, 3, std::vector<int64_t>{ head_size, head_count, n_tokens }.data());
        ggml_tensor * k   = ggml_new_tensor(ctx, type, 3, std::vector<int64_t>{ head_size, head_count, n_tokens }.data());
        ggml_tensor * v   = ggml_new_tensor(ctx, type, 3, std::vector<int64_t>{ head_size, head_count, n_tokens }.data());
        ggml_tensor * tf  = ggml_new_tensor(ctx, type, 2, std::vector<int64_t>{ head_size, head_count }.data());
        ggml_tensor * td  = ggml_new_tensor(ctx, type, 3, std::vector<int64_t>{ head_size, head_count, n_tokens }.data());
        ggml_tensor * s   = ggml_new_tensor(ctx, type, 2, std::vector<int64_t>{ head_size * head_size * head_count, n_seqs }.data());
        ggml_tensor * out = ggml_rwkv_wkv6(ctx, k, v, r, tf, td, s);
        return out;
    }
};

// GGML_OP_GATED_DELTA_NET
struct test_gated_delta_net : public test_case {
    const ggml_type type;

    const int64_t head_count;
    const int64_t head_size;
    const int64_t n_seq_tokens;
    const int64_t n_seqs;
    const int     v_repeat;
    const bool    permuted;
    const bool    kda;
    const int64_t K; // snapshot slot count: 1 = final-only, >1 = last K states

    std::string vars() override {
        return VARS_TO_STR9(type, head_count, head_size, n_seq_tokens, n_seqs, v_repeat, permuted, kda, K);
    }

    test_gated_delta_net(ggml_type type = GGML_TYPE_F32,
            int64_t head_count = 4, int64_t head_size = 16, int64_t n_seq_tokens = 1, int64_t n_seqs = 1,
            int v_repeat = 1, bool permuted = false, bool kda = false, int64_t K = 1)
        : type(type), head_count(head_count), head_size(head_size), n_seq_tokens(n_seq_tokens), n_seqs(n_seqs),
          v_repeat(v_repeat), permuted(permuted), kda(kda), K(K) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * q;
        ggml_tensor * k;
        ggml_tensor * v;
        if (permuted) {
            // create with dims 1 and 2 swapped, then permute back to get non-contiguous layout
            q = ggml_permute(ctx, ggml_new_tensor_4d(ctx, type, head_size, n_seq_tokens, head_count, n_seqs), 0, 2, 1, 3);
            k = ggml_permute(ctx, ggml_new_tensor_4d(ctx, type, head_size, n_seq_tokens, head_count, n_seqs), 0, 2, 1, 3);
            v = ggml_permute(ctx, ggml_new_tensor_4d(ctx, type, head_size, n_seq_tokens, head_count * v_repeat, n_seqs), 0, 2, 1, 3);
        } else {
            q = ggml_new_tensor_4d(ctx, type, head_size, head_count, n_seq_tokens, n_seqs);
            k = ggml_new_tensor_4d(ctx, type, head_size, head_count, n_seq_tokens, n_seqs);
            v = ggml_new_tensor_4d(ctx, type, head_size, head_count * v_repeat, n_seq_tokens, n_seqs);
        }
        ggml_set_name(q, "q");
        ggml_set_name(k, "k");
        ggml_set_name(v, "v");
        const int64_t g_ne0 = kda ? head_size : 1;
        ggml_tensor * g     = ggml_new_tensor_4d(ctx, type, g_ne0, head_count * v_repeat, n_seq_tokens, n_seqs);
        ggml_tensor * beta  = ggml_new_tensor_4d(ctx, type, 1, head_count * v_repeat, n_seq_tokens, n_seqs);
        ggml_tensor * state = ggml_new_tensor_4d(ctx, type, head_size, head_size, head_count * v_repeat, n_seqs);
        ggml_set_name(g,     "g");
        ggml_set_name(beta,  "beta");
        ggml_set_name(state, "state");
        // q/k are L2-normalised in qwen35/kimi-linear before delta_net
        q = ggml_l2_norm(ctx, q, 1e-6f);
        k = ggml_l2_norm(ctx, k, 1e-6f);
        ggml_tensor * out   = ggml_gated_delta_net(ctx, q, k, v, g, beta, state, K);
        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != nullptr; t = ggml_get_next_tensor(ctx, t)) {
            if (ggml_is_view_op(t->op)) { continue; }
            if (strcmp(t->name, "g") == 0) {
                init_tensor_uniform(t, -20.0f, -1e-4f);
            } else if (strcmp(t->name, "beta") == 0) {
                init_tensor_uniform(t, 0.0f, 1.0f);
            } else if (strcmp(t->name, "v") == 0) {
                init_tensor_uniform(t, -0.3f, 5.0f);
            } else {
                init_tensor_uniform(t);
            }
        }
    }
};

// GGML_OP_GATED_DELTA_NET + GGML_OP_CPY (recurrent cache fusion)
struct test_gated_delta_net_cache_fusion : public test_case {
    const ggml_type type;

    const int64_t head_count;
    const int64_t head_size;
    const int64_t n_seq_tokens;
    const int64_t n_seqs;
    const int64_t K; // snapshot slot count (>1)

    ggml_tensor * cpy_node = nullptr;

    std::string vars() override {
        return VARS_TO_STR6(type, head_count, head_size, n_seq_tokens, n_seqs, K);
    }

    test_gated_delta_net_cache_fusion(ggml_type type = GGML_TYPE_F32,
            int64_t head_count = 4, int64_t head_size = 32, int64_t n_seq_tokens = 2, int64_t n_seqs = 1,
            int64_t K = 2)
        : type(type), head_count(head_count), head_size(head_size), n_seq_tokens(n_seq_tokens), n_seqs(n_seqs), K(K) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        const int64_t S_v = head_size;
        const int64_t H_v = head_count;
        const int64_t H_k = head_count;
        const int64_t D   = S_v * S_v * H_v;
        const int64_t n_written = std::min<int64_t>(n_seq_tokens, K);

        ggml_tensor * q     = ggml_new_tensor_4d(ctx, type, head_size, H_k, n_seq_tokens, n_seqs);
        ggml_tensor * k     = ggml_new_tensor_4d(ctx, type, head_size, H_k, n_seq_tokens, n_seqs);
        ggml_tensor * v     = ggml_new_tensor_4d(ctx, type, head_size, H_v, n_seq_tokens, n_seqs);
        ggml_set_name(q, "q");
        ggml_set_name(k, "k");
        ggml_set_name(v, "v");
        ggml_tensor * g     = ggml_new_tensor_4d(ctx, type, 1, H_v, n_seq_tokens, n_seqs);
        ggml_tensor * beta  = ggml_new_tensor_4d(ctx, type, 1, H_v, n_seq_tokens, n_seqs);
        ggml_tensor * state = ggml_new_tensor_4d(ctx, type, head_size, head_size, H_v, n_seqs);
        ggml_set_name(g,     "g");
        ggml_set_name(beta,  "beta");
        ggml_set_name(state, "state");

        q = ggml_l2_norm(ctx, q, 1e-6f);
        k = ggml_l2_norm(ctx, k, 1e-6f);

        ggml_tensor * gdn_out = ggml_gated_delta_net(ctx, q, k, v, g, beta, state, K);
        ggml_set_name(gdn_out, "gdn_out");

        // attn scores view (first part of the gdn output)
        ggml_tensor * attn = ggml_view_4d(ctx, gdn_out,
                S_v, H_v, n_seq_tokens, n_seqs,
                ggml_row_size(gdn_out->type, S_v),
                ggml_row_size(gdn_out->type, S_v * H_v),
                ggml_row_size(gdn_out->type, S_v * H_v * n_seq_tokens), 0);
        ggml_set_name(attn, "attn");

        // snapshot tail view [D, n_seqs, n_written]
        const int64_t attn_score_elems = S_v * H_v * n_seq_tokens * n_seqs;
        ggml_tensor * src = ggml_view_3d(ctx, gdn_out,
                D, n_seqs, n_written,
                ggml_row_size(gdn_out->type, D),
                ggml_row_size(gdn_out->type, D * n_seqs),
                ggml_row_size(gdn_out->type, attn_score_elems));

        // recurrent cache view [D, n_seqs, n_written]
        ggml_tensor * cache = ggml_new_tensor_3d(ctx, type, D, n_seqs, n_written);
        ggml_set_name(cache, "cache");
        ggml_tensor * dst = ggml_view_3d(ctx, cache,
                D, n_seqs, n_written,
                ggml_row_size(cache->type, D),
                ggml_row_size(cache->type, D * n_seqs), 0);

        ggml_tensor * cpy = ggml_cpy(ctx, src, dst);
        ggml_set_name(cpy, "gdn_cache_cpy");
        cpy_node = cpy;

        // read the cpy output (not the plain dst view, which would not pull the cpy into the graph)
        // so that neither the gdn nor the cpy is the graph output
        ggml_tensor * out = ggml_sum(ctx, cpy);
        return out;
    }

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "GATED_DELTA_NET_CACHE_FUSION";
    }

    bool run_whole_graph() override { return true; }
    std::vector<ggml_tensor *> fusion_test_nodes() override { return { cpy_node }; }

    uint64_t op_flops(ggml_tensor * t) override {
        GGML_UNUSED(t);
        const uint64_t S_v = head_size;
        const uint64_t H_v = head_count;
        const uint64_t T   = n_seq_tokens;
        const uint64_t B   = n_seqs;
        return (4ull*S_v + 2ull*S_v*S_v) * H_v * T * B;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != nullptr; t = ggml_get_next_tensor(ctx, t)) {
            if (ggml_is_view_op(t->op)) { continue; }
            if (strcmp(t->name, "g") == 0) {
                init_tensor_uniform(t, -20.0f, -1e-4f);
            } else if (strcmp(t->name, "beta") == 0) {
                init_tensor_uniform(t, 0.0f, 1.0f);
            } else if (strcmp(t->name, "v") == 0) {
                init_tensor_uniform(t, -0.3f, 5.0f);
            } else if (strcmp(t->name, "cache") == 0) {
                init_tensor_uniform(t, 0.0f, 0.0f);
            } else {
                init_tensor_uniform(t);
            }
        }
    }
};

// GGML_OP_GATED_LINEAR_ATTN
struct test_gla : public test_case {
    const ggml_type type;

    const int64_t head_count;
    const int64_t head_size;
    const int64_t n_seq_tokens;
    const int64_t n_seqs;

    std::string vars() override {
        return VARS_TO_STR5(type, head_count, head_size, n_seq_tokens, n_seqs);
    }

    test_gla(ggml_type type = GGML_TYPE_F32,
            int64_t head_count = 32, int64_t head_size = 64, int64_t n_seq_tokens = 32, int64_t n_seqs = 32)
        : type(type), head_count(head_count), head_size(head_size), n_seq_tokens(n_seq_tokens), n_seqs(n_seqs) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        const int64_t n_tokens = n_seq_tokens * n_seqs;
        ggml_tensor * q   = ggml_new_tensor(ctx, type, 3, std::vector<int64_t>{ head_size, head_count, n_tokens }.data());
        ggml_tensor * k   = ggml_new_tensor(ctx, type, 3, std::vector<int64_t>{ head_size, head_count, n_tokens }.data());
        ggml_tensor * v   = ggml_new_tensor(ctx, type, 3, std::vector<int64_t>{ head_size, head_count, n_tokens }.data());
        ggml_tensor * g   = ggml_new_tensor(ctx, type, 3, std::vector<int64_t>{ head_size, head_count, n_tokens }.data());
        ggml_tensor * s   = ggml_new_tensor(ctx, type, 2, std::vector<int64_t>{ head_size * head_size * head_count, n_seqs }.data());
        ggml_tensor * out = ggml_gated_linear_attn(ctx, k, v, q, g, s, pow(head_size, -0.5));
        return out;
    }
};

// GGML_OP_RWKV_WKV7
struct test_rwkv_wkv7 : public test_case {
    const ggml_type type;

    const int64_t head_count;
    const int64_t head_size;
    const int64_t n_seq_tokens;
    const int64_t n_seqs;

    std::string vars() override {
        return VARS_TO_STR5(type, head_count, head_size, n_seq_tokens, n_seqs);
    }

    test_rwkv_wkv7(ggml_type type = GGML_TYPE_F32,
            int64_t head_count = 32, int64_t head_size = 64, int64_t n_seq_tokens = 32, int64_t n_seqs = 32)
        : type(type), head_count(head_count), head_size(head_size), n_seq_tokens(n_seq_tokens), n_seqs(n_seqs) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        const int64_t n_tokens = n_seq_tokens * n_seqs;
        ggml_tensor * r   = ggml_new_tensor(ctx, type, 3, std::vector<int64_t>{ head_size, head_count, n_tokens }.data());
        ggml_tensor * w   = ggml_new_tensor(ctx, type, 3, std::vector<int64_t>{ head_size, head_count, n_tokens }.data());
        ggml_tensor * k   = ggml_new_tensor(ctx, type, 3, std::vector<int64_t>{ head_size, head_count, n_tokens }.data());
        ggml_tensor * v   = ggml_new_tensor(ctx, type, 3, std::vector<int64_t>{ head_size, head_count, n_tokens }.data());
        ggml_tensor * a   = ggml_new_tensor(ctx, type, 3, std::vector<int64_t>{ head_size, head_count, n_tokens }.data());
        ggml_tensor * b   = ggml_new_tensor(ctx, type, 3, std::vector<int64_t>{ head_size, head_count, n_tokens }.data());
        // Outputs may become NaN with long seqlen without these normalization
        a = ggml_l2_norm(ctx, a, 1e-7F);
        b = ggml_l2_norm(ctx, b, 1e-7F);
        ggml_tensor * s   = ggml_new_tensor(ctx, type, 2, std::vector<int64_t>{ head_size * head_size * head_count, n_seqs }.data());
        ggml_tensor * out = ggml_rwkv_wkv7(ctx, r, w, k, v, a, b, s);
        return out;
    }
};

// GGML_OP_MUL_MAT
struct test_mul_mat : public test_case {
    const ggml_type type_a;
    const ggml_type type_b;
    const int64_t m;
    const int64_t n;
    const int64_t k;
    const std::array<int64_t, 2> bs;  // dims 3 and 4
    const std::array<int64_t, 2> nr;  // repeat in dims 3 and 4
    const std::array<int64_t, 4> per; // permutation of dimensions
    const int64_t k_v; // size of k in memory, resulting in a non-contiguous view for k_v > k, no view for k_v == 0
    const uint32_t o; // number of outputs
    const bool src_overlap; // a and b are overlapping views of the same tensor

    std::string vars() override {
        return VARS_TO_STR11(type_a, type_b, m, n, k, bs, nr, per, k_v, o, src_overlap);
    }

    double max_nmse_err() override {
        return 5e-4;
    }

    double max_nmse_err(ggml_backend_t backend) override {
        // for blackwell we quantize activations to mxfp4 instead of q8_1 so we add higher tolerance
        if ((type_a == GGML_TYPE_MXFP4 || type_a == GGML_TYPE_NVFP4) && backend_has_feature(backend, "BLACKWELL_NATIVE_FP4")) {
            return 2e-2;
        }
        return max_nmse_err();
    }

    int64_t grad_nmax() override {
        return 20000;
    }

    uint64_t op_flops(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return 2 * m * n * k * bs[0] * nr[0] * bs[1] * nr[1];
    }

    test_mul_mat(ggml_type type_a = GGML_TYPE_F32, ggml_type type_b = GGML_TYPE_F32,
            int64_t m = 32, int64_t n = 32, int64_t k = 32,
            std::array<int64_t, 2> bs = {10, 10},
            std::array<int64_t, 2> nr = {2, 2},
            std::array<int64_t, 4> per = {0, 1, 2, 3},
            int64_t k_v = 0, uint32_t o = 1, bool src_overlap = false)
        : type_a(type_a), type_b(type_b), m(m), n(n), k(k), bs(bs), nr(nr), per(per), k_v(k_v), o(o), src_overlap(src_overlap) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        // C^T = A * B^T: (k, m) * (k, n) => (m, n)
        ggml_tensor * a;
        ggml_tensor * b;

        const int npermuted = (per[0] != 0) + (per[1] != 1) + (per[2] != 2) + (per[3] != 3);
        if (npermuted > 0) {
            GGML_ASSERT(npermuted == 2);
            GGML_ASSERT(k_v == 0); // not handled
            GGML_ASSERT(!ggml_is_quantized(type_a) || per[0] == 0);
            GGML_ASSERT(!ggml_is_quantized(type_b) || per[0] == 0);

            // Create tensors with the permuted dimensions, then permute them back to the dimensions given by m,n,k.
            const int64_t ne_a[4] = {k, m, bs[0],       bs[1]};
            const int64_t ne_b[4] = {k, n, bs[0]*nr[0], bs[1]*nr[1]};

            a = ggml_new_tensor_4d(ctx, type_a, ne_a[per[0]], ne_a[per[1]], ne_a[per[2]], ne_a[per[3]]);
            b = ggml_new_tensor_4d(ctx, type_b, ne_b[per[0]], ne_b[per[1]], ne_b[per[2]], ne_b[per[3]]);
            if (!ggml_is_quantized(type_a)) {
                if (bs[1] == 1 && nr[1] == 1) {
                    ggml_set_param(a);
                }
                ggml_set_param(b);
            }
            ggml_set_name(a, "a");
            ggml_set_name(b, "b");

            a = ggml_permute(ctx, a, per[0], per[1], per[2], per[3]);
            b = ggml_permute(ctx, b, per[0], per[1], per[2], per[3]);
            ggml_set_name(a, "a_permuted");
            ggml_set_name(b, "b_permuted");
        } else if (src_overlap) {
            GGML_ASSERT(type_a == type_b);
            GGML_ASSERT(k_v == 0);

            // a and b are interleaved views of the same tensor: (e.g. fused QKV in MiniMax-01)
            ggml_tensor * base = ggml_new_tensor_4d(ctx, type_a, 2*k, std::max(m, n), bs[0]*nr[0], bs[1]*nr[1]);
            ggml_set_name(base, "base");

            a = ggml_view_4d(ctx, base, k, m, bs[0],       bs[1],       base->nb[1], base->nb[2], base->nb[3], 0);
            b = ggml_view_4d(ctx, base, k, n, bs[0]*nr[0], bs[1]*nr[1], base->nb[1], base->nb[2], base->nb[3], k*ggml_type_size(type_a));
            ggml_set_name(a, "a");
            ggml_set_name(b, "b");
        } else {
            const int64_t k_physical = k_v == 0 ? k : k_v;
            a = ggml_new_tensor_4d(ctx, type_a, k_physical, m, bs[0],       bs[1]);
            b = ggml_new_tensor_4d(ctx, type_b, k_physical, n, bs[0]*nr[0], bs[1]*nr[1]);

            if (!ggml_is_quantized(type_a)) {
                if (bs[1] == 1 && nr[1] == 1) {
                    ggml_set_param(a);
                }
                ggml_set_param(b);
            }

            if (k_v != 0) {
                GGML_ASSERT(k_v > k);
                a = ggml_view_4d(ctx, a, k, m, bs[0],       bs[1],       a->nb[1], a->nb[2], a->nb[3], 0);
                b = ggml_view_4d(ctx, b, k, n, bs[0]*nr[0], bs[1]*nr[1], b->nb[1], b->nb[2], b->nb[3], 0);
            }
            ggml_set_name(a, "a");
            ggml_set_name(b, "b");
        }

        ggml_tensor * out = ggml_mul_mat(ctx, a, b);
        ggml_set_name(out, "out");
        for (uint32_t i = 1; i < o; ++i) {
            ggml_tensor * out2 = ggml_mul_mat(ctx, a, b);
            ggml_set_name(out2, "out2");
            out = ggml_add(ctx, out, out2);
        }

        return out;
    }

    bool run_whole_graph() override { return o > 1; }

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return ggml_op_name(GGML_OP_MUL_MAT);
    }
};

// ArifiLabs lane-206: adversarial data patterns for MUL_MAT.
//
// Every other MUL_MAT case fills both operands from uniform[-1,1]. That distribution cannot reach
// the regimes where a quantized-activation (q8_1) mat-vec kernel diverges from the f32 dequant
// path: it has a narrow per-row dynamic range, never a degenerate scale, and never sits on a
// quantization code boundary. These patterns do.
enum adv_pattern {
    ADV_EXTREME_RANGE,      // magnitudes spanning ~1e-15..1e15 within a single row
    ADV_DENORMAL,           // f32 denormals mixed with normals
    ADV_SIGN_ALTERNATING,   // exact +-1, so every partial sum cancels
    ADV_ZERO_ROWS,          // every other row all zeros -> a zero block scale
    ADV_FP4_CODE_SWEEP,     // cycles the E2M1 magnitudes, so every code and both signs are hit
    ADV_MIXED_BLOCK_SCALE,  // per-32-block scale swept over 2^-20..2^19
};

static const char * adv_pattern_name(adv_pattern p) {
    switch (p) {
        case ADV_EXTREME_RANGE:     return "extreme_range";
        case ADV_DENORMAL:          return "denormal";
        case ADV_SIGN_ALTERNATING:  return "sign_alternating";
        case ADV_ZERO_ROWS:         return "zero_rows";
        case ADV_FP4_CODE_SWEEP:    return "fp4_code_sweep";
        case ADV_MIXED_BLOCK_SCALE: return "mixed_block_scale";
    }
    return "?";
}

// Deterministic by (pattern, row, index): the two arms of an MMVQ-vs-f32 comparison must see
// byte-identical inputs, so this must not depend on a random engine or on thread scheduling.
static float adv_value(adv_pattern p, int64_t row, int64_t i) {
    const int64_t j    = row * 1315423911LL + i;      // a cheap, stable per-element mix
    const float   sign = (j & 1) ? -1.0f : 1.0f;
    switch (p) {
        case ADV_EXTREME_RANGE: {
            // 1e-6 .. 1e3, so a single 32-value block spans 9 decades against the 2 decades a
            // uniform[-1,1] block spans. The TOP of the range is not arbitrary: the MMVQ path
            // quantizes src1 to block_q8_1, whose `d` AND `s = d*sum(qs)` are both ggml_half
            // (CHECKED ggml-common.h). sum(qs) reaches 32*127 = 4064, so a block max above about
            // 2045 makes `s` overflow f16 and the path returns inf REGARDLESS of the kernel. That
            // is a structural ceiling of q8_1 activation quantization, shared by every MMVQ type,
            // not a ROCmFP4 defect - so probing past it would only manufacture a FAIL that says
            // nothing about the shader under test. 1e3 sits an order of magnitude inside it.
            const int e = (int)(j % 10) - 6;
            return sign * std::pow(10.0f, (float) e);
        }
        case ADV_DENORMAL: {
            // FLT_TRUE_MIN is 1.4e-45; 1e-40 is comfortably denormal. Two in three elements are
            // denormal so that a flush-to-zero difference between the two paths would show up.
            if (j % 3 == 0) return sign * 1.0f;
            return sign * 1e-40f * (float)(1 + (j % 7));
        }
        case ADV_SIGN_ALTERNATING:
            return sign;
        case ADV_ZERO_ROWS:
            return (row % 2) ? 0.0f : sign * (1.0f + (float)(j % 5));
        case ADV_FP4_CODE_SWEEP: {
            static const float e2m1[8] = {0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f};
            return sign * e2m1[j % 8];
        }
        case ADV_MIXED_BLOCK_SCALE: {
            // one scale OCTAVE per 32-value block, 2^-24 .. 2^7, so neighbouring blocks differ by
            // up to 31 octaves while no single block exceeds the q8_1 f16 `s` ceiling noted above.
            const int e = (int)((i / 32) % 32) - 24;
            return sign * std::ldexp(1.0f + (float)(j % 3), e);
        }
    }
    return 0.0f;
}

// Fills a CONTIGUOUS f32 or quantized tensor from an adversarial pattern. Deliberately narrower
// than init_tensor_uniform: it handles only what these cases build, so it cannot change the
// initialization of any existing test.
static void init_tensor_adversarial(ggml_tensor * tensor, adv_pattern p) {
    GGML_ASSERT(ggml_is_contiguous(tensor));
    const int64_t nrows = ggml_nrows(tensor);
    const int64_t ncols = tensor->ne[0];
    std::vector<float> data((size_t)(nrows * ncols));
    for (int64_t r = 0; r < nrows; r++) {
        for (int64_t i = 0; i < ncols; i++) {
            data[(size_t)(r * ncols + i)] = adv_value(p, r, i);
        }
    }

    if (tensor->type == GGML_TYPE_F32) {
        ggml_backend_tensor_set(tensor, data.data(), 0, data.size() * sizeof(float));
        return;
    }
    GGML_ASSERT(ggml_is_quantized(tensor->type));
    GGML_ASSERT(ncols % ggml_blck_size(tensor->type) == 0);
    std::vector<uint8_t> dataq(ggml_row_size(tensor->type, data.size()));
    const size_t blck   = ggml_blck_size(tensor->type);
    const size_t nblock = data.size() / blck;
    // no imatrix: the pattern IS the importance statement, and an imatrix would smooth it away
    ggml_quantize_chunk(tensor->type, data.data(), dataq.data(), 0, nblock, blck, nullptr);
    ggml_backend_tensor_set(tensor, dataq.data(), 0, dataq.size());
}

// MUL_MAT over adversarial data, reporting max abs / max rel error on EVERY case rather than only
// when the nmse bar is crossed. The numbers are the deliverable here; the pass/fail is secondary.
struct test_mul_mat_adversarial : public test_mul_mat {
    const adv_pattern pattern;

    test_mul_mat_adversarial(ggml_type type_a, adv_pattern pattern,
            int64_t m = 1024, int64_t n = 3, int64_t k = 5120)
        : test_mul_mat(type_a, GGML_TYPE_F32, m, n, k, {1, 1}, {1, 1}), pattern(pattern) {}

    std::string vars() override {
        return test_mul_mat::vars() + ",adv=" + adv_pattern_name(pattern);
    }

    // Adversarial inputs are, by construction, hard for a 4-bit format: an extreme-range row
    // quantizes to a block scale that cannot represent its own small elements, so a LARGE nmse
    // against the f32 reference is the expected, correct behaviour of the FORMAT and says nothing
    // about the kernel. What must not differ is the two Vulkan arms, which is read off the printed
    // numbers by running the set twice. The bar here therefore only catches gross breakage.
    double max_nmse_err() override {
        return 1.0;
    }

    double err(const float * a, const float * b, size_t n) override {
        double max_abs = 0.0;
        double max_rel = 0.0;
        for (size_t i = 0; i < n; i++) {
            const double d   = std::fabs((double) a[i] - (double) b[i]);
            const double den = std::max(std::fabs((double) a[i]), std::fabs((double) b[i]));
            max_abs = std::max(max_abs, d);
            if (den > 0.0) {
                max_rel = std::max(max_rel, d / den);
            }
        }
        const double e = nmse(a, b, n);
        printf("[ADV %s] nmse=%.6e max_abs=%.6e max_rel=%.6e ", adv_pattern_name(pattern), e, max_abs, max_rel);
        return e;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != nullptr; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_adversarial(t, pattern);
        }
    }
};
// GGML_HINT_SRC0_IS_HADAMARD
struct test_mul_mat_hadamard : public test_mul_mat {
    test_mul_mat_hadamard(ggml_type type_a = GGML_TYPE_F32, ggml_type type_b = GGML_TYPE_F32,
            int64_t m = 32, int64_t n = 32, int64_t k = 32,
            std::array<int64_t, 2> bs = {1, 1},
            std::array<int64_t, 2> nr = {1, 1})
        : test_mul_mat(type_a, type_b, m, n, k, bs, nr) {
            GGML_ASSERT(type_a == GGML_TYPE_F32);
        }

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * out = test_mul_mat::build_graph(ctx);
        // Find the mul_mat op in the graph and set the hint
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (t->op == GGML_OP_MUL_MAT) {
                ggml_mul_mat_set_hint(t, GGML_HINT_SRC0_IS_HADAMARD);
            }
        }
        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (strcmp(t->name, "a") == 0) {
                const int64_t n_cols = t->ne[0];
                const int64_t n_rows = ggml_nrows(t);
                std::vector<float> data(n_cols * n_rows);
                float scale = 1.0f / sqrtf((float)n_cols);
                for (int64_t r = 0; r < n_rows; r++) {
                    float * row_data = data.data() + r * n_cols;
                    for (int64_t i = 0; i < n_cols; i++) {
                        int pop = 0;
                        int64_t val = r & i;
                        while (val) {
                            pop += (val & 1);
                            val >>= 1;
                        }
                        row_data[i] = (pop % 2 == 0) ? scale : -scale;
                    }
                }
                ggml_backend_tensor_set(t, data.data(), 0, data.size() * sizeof(float));
            } else if (t->type == GGML_TYPE_F32 || t->type == GGML_TYPE_F16) {
                init_tensor_uniform(t);
            }
        }
    }

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "MUL_MAT_HADAMARD";
    }
};

static void init_mul_mat_id_ids(ggml_context * ctx, int n_mats) {
    std::random_device rd;
    std::default_random_engine rng(rd());
    for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
        if (t->type != GGML_TYPE_I32 || ggml_is_view_op(t->op)) {
            continue;
        }
        for (int64_t r = 0; r < ggml_nrows(t); r++) {
            std::vector<int32_t> data(t->ne[0]);
            for (int i = 0; i < t->ne[0]; i++) {
                data[i] = i % n_mats;
            }
            std::shuffle(data.begin(), data.end(), rng);
            ggml_backend_tensor_set(t, data.data(), r * t->nb[1], t->ne[0] * sizeof(int32_t));
        }
    }
}

static void init_mul_mat_id_tensors(ggml_context * ctx, int n_mats, float amax = 1.0f) {
    for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
        if (t->type == GGML_TYPE_I32) {
            continue;
        } else if (amax != 1.0f && t->type == GGML_TYPE_F32) {
            init_tensor_uniform(t, -amax, amax);
        } else {
            init_tensor_uniform(t);
        }
    }
    init_mul_mat_id_ids(ctx, n_mats);
}

// GGML_OP_MUL_MAT_ID
struct test_mul_mat_id : public test_case {
    const ggml_type type_a;
    const ggml_type type_b;
    const int n_mats;
    const int n_used;
    const bool b; // broadcast b matrix
    const int64_t m;
    const int64_t n;
    const int64_t k;
    const float amax; // magnitude of src1

    std::string vars() override {
        return VARS_TO_STR9(type_a, type_b, n_mats, n_used, b, m, n, k, amax);
    }

    double max_nmse_err() override {
        return 5e-4;
    }

    double max_nmse_err(ggml_backend_t backend) override {
        // for blackwell we quantize activations to mxfp4 instead of q8_1 so we add higher tolerance
        if ((type_a == GGML_TYPE_MXFP4 || type_a == GGML_TYPE_NVFP4) && backend_has_feature(backend, "BLACKWELL_NATIVE_FP4")) {
            return 2e-2;
        }
        return max_nmse_err();
    }

    uint64_t op_flops(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return 2 * m * k * n * n_used;
    }

    test_mul_mat_id(ggml_type type_a = GGML_TYPE_F32, ggml_type type_b = GGML_TYPE_F32,
            int n_mats = 8, int n_used = 2, bool b = false,
            int64_t m = 32, int64_t n = 32, int64_t k = 32,
            float amax = 1.0f)
        : type_a(type_a), type_b(type_b), n_mats(n_mats), n_used(n_used), b(b),
            m(m), n(n), k(k), amax(amax) {
            GGML_ASSERT(n_used <= n_mats);
        }

    ggml_tensor * build_graph(ggml_context * ctx) override {
        // C^T = A * B^T: (k, m) * (k, n) => (m, n)
        ggml_tensor * as = ggml_new_tensor_3d(ctx, type_a, k, m, n_mats);
        ggml_set_name(as, "as");

        ggml_tensor * ids = ggml_new_tensor_2d(ctx, GGML_TYPE_I32, n_mats, n);
        ggml_set_name(ids, "ids");
        if (n_used != n_mats) {
            ids = ggml_view_2d(ctx, ids, n_used, n, ids->nb[1], 0);
            ggml_set_name(ids, "view_of_ids");
        }

        ggml_tensor * b = ggml_new_tensor_3d(ctx, type_b, k, this->b ? 1 : n_used, n);
        ggml_set_name(b, "b");

        ggml_tensor * out = ggml_mul_mat_id(ctx, as, b, ids);
        ggml_set_name(out, "out");

        if (amax > 65504.0f) {
            // src1 exceeds F16 range
            ggml_prec_set_src(out, GGML_PREC_F32, 1);
        }

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        init_mul_mat_id_tensors(ctx, n_mats, amax);
    }

    void reinit_perf_iter(ggml_context * ctx) override {
        init_mul_mat_id_ids(ctx, n_mats);
    }
};

// GGML_OP_MUL_MAT_ID + GGML_OP_ADD or GGML_OP_MUL
struct test_mul_mat_id_fusion : public test_case {
    const ggml_type type_a;
    const ggml_type type_b;
    const int n_mats;
    const int n_used;
    const bool b; // broadcast b matrix
    const int64_t m;
    const int64_t n;
    const int64_t k;
    const uint32_t o; // number of outputs
    const bool mul;

    std::string vars() override {
        return VARS_TO_STR10(type_a, type_b, n_mats, n_used, b, m, n, k, o, mul);
    }

    double max_nmse_err() override {
        return 5e-4;
    }

    uint64_t op_flops(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return 2 * m * k * n * n_used;
    }

    test_mul_mat_id_fusion(ggml_type type_a = GGML_TYPE_F32, ggml_type type_b = GGML_TYPE_F32,
            int n_mats = 8, int n_used = 2, bool b = false,
            int64_t m = 32, int64_t n = 32, int64_t k = 32, uint32_t o = 1, bool mul = false)
        : type_a(type_a), type_b(type_b), n_mats(n_mats), n_used(n_used), b(b),
            m(m), n(n), k(k), o(o), mul(mul) {
            GGML_ASSERT(n_used <= n_mats);
        }

    ggml_tensor * build_graph(ggml_context * ctx) override {
        // C^T = A * B^T: (k, m) * (k, n) => (m, n)
        ggml_tensor * as = ggml_new_tensor_3d(ctx, type_a, k, m, n_mats);
        ggml_set_name(as, "as");

        ggml_tensor * ids = ggml_new_tensor_2d(ctx, GGML_TYPE_I32, n_mats, n);
        ggml_set_name(ids, "ids");
        if (n_used != n_mats) {
            ids = ggml_view_2d(ctx, ids, n_used, n, ids->nb[1], 0);
            ggml_set_name(ids, "view_of_ids");
        }

        ggml_tensor * b = ggml_new_tensor_3d(ctx, type_b, k, this->b ? 1 : n_used, n);
        ggml_set_name(b, "b");

        ggml_tensor * out = ggml_mul_mat_id(ctx, as, b, ids);
        ggml_set_name(out, "out");

        for (uint32_t i = 1; i < o; ++i) {
            ggml_tensor * a2 = ggml_new_tensor_3d(ctx, type_a, k, m, n_mats);
            ggml_tensor * out2 = ggml_mul_mat_id(ctx, a2, b, ids);
            ggml_set_name(out2, "out2");
            out = ggml_add(ctx, out, out2);
        }

        if (mul) {
            std::array<int64_t, 4> ne { 1, out->ne[1], out->ne[2], out->ne[3] };
            ne[0] = 1;
            ggml_tensor * m = ggml_new_tensor(ctx, out->type, 4, ne.data());
            out = ggml_mul(ctx, out, m);
        }

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        init_mul_mat_id_tensors(ctx, n_mats);
    }

    bool run_whole_graph() override { return true; }

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "MUL_MAT_ID_FUSION";
    }
};

// GGML_OP_OUT_PROD
struct test_out_prod : public test_case {
    const ggml_type type_a;
    const ggml_type type_b;
    const int64_t m;
    const int64_t n;
    const int64_t k;
    const std::array<int64_t, 2> bs; // dims 3 and 4
    const std::array<int64_t, 2> nr; // repeat in dims 3 and 4
    const bool trans_b;

    std::string vars() override {
        return VARS_TO_STR8(type_a, type_b, m, n, k, bs, nr, trans_b);
    }

    double max_nmse_err() override {
        return 5e-4;
    }

    test_out_prod(ggml_type type_a = GGML_TYPE_F32, ggml_type type_b = GGML_TYPE_F32,
            int64_t m = 32, int64_t n = 32, int64_t k = 32,
            std::array<int64_t, 2> bs = {10, 10},
            std::array<int64_t, 2> nr = {2, 2},
            bool trans_b = false)
        : type_a(type_a), type_b(type_b), m(m), n(n), k(k), bs(bs), nr(nr), trans_b(trans_b) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor_4d(ctx, type_a, m, k, bs[0], bs[1]);
        ggml_set_name(a, "a");

        ggml_tensor * b;
        if (trans_b) {
            b = ggml_new_tensor_4d(ctx, type_b, k, n, bs[0]*nr[0], bs[1]*nr[1]);
            b = ggml_transpose(ctx, b);
        } else {
            b = ggml_new_tensor_4d(ctx, type_b, n, k, bs[0]*nr[0], bs[1]*nr[1]);
        }
        ggml_set_name(b, "b");

        ggml_tensor * out = ggml_out_prod(ctx, a, b);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_SQR
struct test_sqr : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;

    std::string vars() override {
        return VARS_TO_STR2(type, ne);
    }

    test_sqr(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 5, 4, 3})
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_sqr(ctx, a);
        ggml_set_name(out, "out");

        return out;
    }

    float grad_eps() override {
        return 0.1f * 0.25f*ne[0]*ne[1]*ne[2]*ne[3]; // 10% of expected value of sum.
    }
};

// GGML_OP_SQRT
struct test_sqrt : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;

    std::string vars() override {
        return VARS_TO_STR2(type, ne);
    }

    test_sqrt(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 3, 3, 2})
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_sqrt(ctx, a);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        // fill with positive values
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, 50.0f, 100.0f);
        }
    }

    float grad_eps() override {
        return 20.0f;
    }

    bool grad_precise() override {
        return true;
    }
};

// GGML_OP_LOG
struct test_log : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;

    std::string vars() override {
        return VARS_TO_STR2(type, ne);
    }

    test_log(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 5, 4, 3})
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_log(ctx, a);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            // log(1) == 0, cluster values there to keep the sum low for better precision in the backward pass:
            init_tensor_uniform(t, 0.9f, 1.1f);
        }
    }

    bool grad_precise() override {
        return true;
    }
};

// GGML_OP_SIN
struct test_sin : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;

    std::string vars() override {
        return VARS_TO_STR2(type, ne);
    }

    test_sin(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 2, 2, 2})
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_sin(ctx, a);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, -6.5f, 6.5f); // Covers interval [-2*pi, 2*pi].
        }
    }

    double max_maa_err() override {
        return 1e-3;
    }

    float grad_eps() override {
        return 0.2f;
    }

    bool grad_precise() override {
        return true;
    }
};

// GGML_OP_COS
struct test_cos : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;

    std::string vars() override {
        return VARS_TO_STR2(type, ne);
    }

    test_cos(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 2, 2, 2})
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_cos(ctx, a);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, -6.5f, 6.5f); // Covers interval [-2*pi, 2*pi].
        }
    }

    double max_maa_err() override {
        return 1e-3;
    }

    float grad_eps() override {
        return 0.2f;
    }

    bool grad_precise() override {
        return true;
    }
};

// GGML_OP_CLAMP
struct test_clamp : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    float min;
    float max;

    std::string vars() override {
        return VARS_TO_STR4(type, ne, min, max);
    }

    test_clamp(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 5, 4, 3},
            float min = -0.5f, float max = 0.5f)
        : type(type), ne(ne), min(min), max(max) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_clamp(ctx, a, min, max);
        ggml_set_name(out, "out");

        return out;
    }

    float grad_eps() override {
        return 1e-2f;
    }

    std::vector<float> grad_expect() override {
        return {0.0f, 1.0f};
    }
};

// GGML_OP_FLOOR
struct test_floor : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;

    std::string vars() override {
        return VARS_TO_STR2(type, ne);
    }

    test_floor(ggml_type type = GGML_TYPE_F32,
               std::array<int64_t, 4> ne = {10, 2, 2, 2})
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_floor(ctx, a);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, -10.0f, 10.0f);
        }
    }
};

// GGML_OP_CEIL
struct test_ceil : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;

    std::string vars() override {
        return VARS_TO_STR2(type, ne);
    }

    test_ceil(ggml_type type = GGML_TYPE_F32,
              std::array<int64_t, 4> ne = {10, 2, 2, 2})
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_ceil(ctx, a);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, -10.0f, 10.0f);
        }
    }
};

// GGML_OP_ROUND
struct test_round : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;

    std::string vars() override {
        return VARS_TO_STR2(type, ne);
    }

    test_round(ggml_type type = GGML_TYPE_F32,
               std::array<int64_t, 4> ne = {10, 2, 2, 2})
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_round(ctx, a);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, -10.0f, 10.0f);
        }
    }
};

// GGML_OP_TRUNC
struct test_trunc : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;

    std::string vars() override {
        return VARS_TO_STR2(type, ne);
    }

    test_trunc(ggml_type type = GGML_TYPE_F32,
               std::array<int64_t, 4> ne = {10, 2, 2, 2})
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_trunc(ctx, a);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, -10.0f, 10.0f);
        }
    }
};

// GGML_OP_DIAG_MASK_INF
struct test_diag_mask_inf : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const int n_past;

    std::string vars() override {
        return VARS_TO_STR3(type, ne, n_past);
    }

    test_diag_mask_inf(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 10, 3, 2},
            int n_past = 5)
        : type(type), ne(ne), n_past(n_past) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_diag_mask_inf(ctx, a, n_past);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_SOFT_MAX
struct test_soft_max : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const bool mask;
    const bool sinks;
    const ggml_type m_prec;
    const std::array<int64_t, 2> nr23; // broadcast only dims 2 and 3
    const float scale;
    const float max_bias;
    const bool inplace;

    std::string vars() override {
        return VARS_TO_STR9(type, ne, mask, sinks, m_prec, nr23, scale, max_bias, inplace);
    }

    // the 1024 test with bias occasionally fails:
    // SOFT_MAX(type=f32,ne=[1024,16,1,1],mask=1,scale=1.000000,max_bias=8.000000): [SOFT_MAX] NMSE = 0.000000103 > 0.000000100 FAIL
    virtual double max_nmse_err() override {
        return 1e-6;
    }

    test_soft_max(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 5, 4, 3},
            bool mask = false,
            bool sinks = false,
            ggml_type m_prec = GGML_TYPE_F32,
            std::array<int64_t, 2> nr23 = {1, 1},
            float scale = 1.0f,
            float max_bias = 0.0f,
            bool inplace = false)
        : type(type), ne(ne), mask(mask), sinks(sinks), m_prec(m_prec), nr23(nr23), scale(scale), max_bias(max_bias), inplace(inplace) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor_4d(ctx, type, ne[0], ne[1], ne[2]*nr23[0], ne[3]*nr23[1]);
        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * mask = nullptr;
        if (this->mask) {
            mask = ggml_new_tensor_4d(ctx, m_prec, ne[0], ne[1], ne[2], ne[3]);
            ggml_set_name(mask, "mask");
        }

        ggml_tensor * sinks = nullptr;
        if (this->sinks) {
            sinks = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, ne[2]*nr23[0]);
            ggml_set_name(sinks, "sinks");
        }

        ggml_tensor * out;
        if (inplace) {
            out = ggml_soft_max_ext_inplace(ctx, a, mask, scale, max_bias);
        } else {
            out = ggml_soft_max_ext(ctx, a, mask, scale, max_bias);
        }
        ggml_soft_max_add_sinks(out, sinks);
        ggml_set_name(out, "out");

        return out;
    }

    bool grad_precise() override {
        return true;
    }
};

// GGML_OP_SOFT_MAX_BACK
struct test_soft_max_back : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const float scale;
    const float max_bias;

    std::string vars() override {
        return VARS_TO_STR4(type, ne, scale, max_bias);
    }

    test_soft_max_back(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 5, 4, 3},
            float scale = 1.0f,
            float max_bias = 0.0f)
        : type(type), ne(ne), scale(scale), max_bias(max_bias) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(a, "a");

        ggml_tensor * b = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_soft_max_ext_back(ctx, a, b, scale, max_bias);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_ROPE + GGML_OP_ROPE_BACK
struct test_rope : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne_a;
    int n_dims;
    int mode;
    int n_ctx; // used to generate positions
    float fs; // freq_scale
    float ef; // ext_factor
    float af; // attn_factor
    bool ff;
    int v; // view (1 : non-contiguous a)
    bool forward;
    bool inplace;
    int n_offs; // offset of the rotated dims window, set via ggml_rope_set_offset()

    std::string vars() override {
        // forward can be inferred from the op, does not need to be printed
        return VARS_TO_STR12(type, ne_a, n_dims, mode, n_ctx, fs, ef, af, ff, v, inplace, n_offs);
    }

    test_rope(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne_a = {10, 5, 3, 1},
            int n_dims = 10, int mode = GGML_ROPE_TYPE_NORMAL, int n_ctx = 512, float fs = 1.0f,
            float ef = 0.0f, float af = 0.0f, bool ff = false, int v = 0, bool forward = true, bool inplace = false,
            int n_offs = 0)
        : type(type), ne_a(ne_a), n_dims(n_dims), mode(mode), n_ctx(n_ctx), fs(fs), ef(ef), af(af), ff(ff), v(v), forward(forward), inplace(inplace), n_offs(n_offs) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a;
        if (v & 1) {
            auto ne = ne_a; ne[0] *= 2; ne[1] *= 4; ne[2] *= 3;
            a = ggml_new_tensor(ctx, type, 4, ne.data());
            if (forward && n_offs == 0) {
                // FIXME: support gradients with n_offs > 0
                ggml_set_param(a);
            }
            ggml_set_name(a, "a");

            a = ggml_view_4d(ctx, a, ne_a[0], ne_a[1], ne_a[2], ne_a[3], a->nb[1], a->nb[2], a->nb[3], 0);
            ggml_set_name(a, "view_of_a");
        } else if (v == 2) {
            // second-half slice along dim 0 (mimics build_rope_2d in clip.cpp).
            // The non-zero view offset (ne_a[0] * elem_size) often produces a
            // non-aligned buffer offset, which exercises backends' alignment paths.
            auto ne = ne_a; ne[0] *= 2;
            a = ggml_new_tensor(ctx, type, 4, ne.data());
            if (forward && n_offs == 0) {
                // FIXME: support gradients with n_offs > 0
                ggml_set_param(a);
            }
            ggml_set_name(a, "a");

            a = ggml_view_4d(ctx, a, ne_a[0], ne_a[1], ne_a[2], ne_a[3],
                             a->nb[1], a->nb[2], a->nb[3],
                             ne_a[0] * ggml_element_size(a));
            ggml_set_name(a, "view_of_a");
        } else {
            a = ggml_new_tensor(ctx, type, 4, ne_a.data());
            if (forward && n_offs == 0) {
                // FIXME: support gradients with n_offs > 0
                ggml_set_param(a);
            }
            ggml_set_name(a, "a");
        }

        const bool is_mrope = mode & GGML_ROPE_TYPE_MROPE;
        const bool is_vision = mode == GGML_ROPE_TYPE_VISION;

        ggml_tensor * pos;
        if (is_mrope || is_vision) {
            pos = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, ne_a[2] * 4);
        } else {
            pos = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, ne_a[2]);
        }
        ggml_set_name(pos, "pos");

        ggml_tensor * freq = nullptr;
        if (ff) {
            freq = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, n_dims/2);
            ggml_set_name(freq, "freq");
        }

        ggml_tensor * out;
        if (is_mrope) {
            if (is_vision) {
                GGML_ASSERT(n_dims/4 > 0);
                int rope_sections[4] = {n_dims/4, n_dims/4, 0, 0}; // Vision-RoPE only use first two dimension for image (x, y) coordinate
                if (forward) {
                    if (inplace) {
                        out = ggml_rope_multi_inplace(ctx, a, pos, freq, n_dims/2, rope_sections, mode, 0, 10000.0f, fs, ef, af, 1.0f, 1.0f);
                    } else {
                        out = ggml_rope_multi(ctx, a, pos, freq, n_dims/2, rope_sections, mode, 0, 10000.0f, fs, ef, af, 1.0f, 1.0f);
                    }
                } else {
                    out = ggml_rope_multi_back(ctx, a, pos, freq, n_dims/2, rope_sections, mode, 0, 10000.0f, fs, ef, af, 1.0f, 1.0f);
                }
            } else {
                GGML_ASSERT(n_dims/3 > 0);
                int rope_sections[4] = {n_dims/3, n_dims/3, n_dims/3, 0};
                if (forward) {
                    if (inplace) {
                        out = ggml_rope_multi_inplace(ctx, a, pos, freq, n_dims, rope_sections, mode, 0, 10000.0f, fs, ef, af, 1.0f, 1.0f);
                    } else {
                        out = ggml_rope_multi(ctx, a, pos, freq, n_dims, rope_sections, mode, 0, 10000.0f, fs, ef, af, 1.0f, 1.0f);
                    }
                } else {
                    out = ggml_rope_multi_back(ctx, a, pos, freq, n_dims, rope_sections, mode, 0, 10000.0f, fs, ef, af, 1.0f, 1.0f);
                }
            }
        } else {
            if (forward) {
                if (inplace) {
                    out = ggml_rope_ext_inplace(ctx, a, pos, freq, n_dims, mode, 0, 10000.0f, fs, ef, af, 1.0f, 1.0f);
                } else {
                    out = ggml_rope_ext(ctx, a, pos, freq, n_dims, mode, 0, 10000.0f, fs, ef, af, 1.0f, 1.0f);
                }
            } else {
                out = ggml_rope_ext_back(ctx, a, pos, freq, n_dims, mode, 0, 10000.0f, fs, ef, af, 1.0f, 1.0f);
            }
        }
        if (n_offs != 0) {
            out = ggml_rope_set_offset(out, n_offs);
        }
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (t->type == GGML_TYPE_I32) {
                // pos
                const int num_pos_ids = (mode & GGML_ROPE_TYPE_MROPE) ? ne_a[2] * 4 : ne_a[2];
                std::vector<int> data(num_pos_ids);
                for (int i = 0; i < num_pos_ids; i++) {
                    data[i] = rand() % n_ctx;
                }
                ggml_backend_tensor_set(t, data.data(), 0, num_pos_ids * sizeof(int));
            } else {
                if (t->ne[0] == n_dims/2) {
                    // frequency factors in the range [0.9f, 1.1f]
                    init_tensor_uniform(t, 0.9f, 1.1f);
                } else {
                    init_tensor_uniform(t);
                }
            }
        }
    }

    double max_maa_err() override {
        return 1e-3;
    }

    bool grad_precise() override {
        return true;
    }
};

// GGML_OP_POOL2D
struct test_pool2d : public test_case {
    enum ggml_op_pool pool_type;
    const ggml_type type_input;
    const std::array<int64_t, 4> ne_input;
    // kernel size
    const int k0;
    const int k1;
    // stride
    const int s0;
    const int s1;
    // padding
    const int p0;
    const int p1;

    std::string vars() override {
        return VARS_TO_STR9(pool_type, type_input, ne_input, k0, k1, s0, s1, p0, p1);
    }

    test_pool2d(ggml_op_pool pool_type = GGML_OP_POOL_AVG,
            ggml_type type_input = GGML_TYPE_F32,
            std::array<int64_t, 4> ne_input = {10, 10, 3, 1}, // [input_width, input_height, input_channels, 1]
            int k0 = 3, int k1 = 3,
            int s0 = 1, int s1 = 1,
            int p0 = 1, int p1 = 1)
        : pool_type(pool_type), type_input(type_input), ne_input(ne_input), k0(k0), k1(k1), s0(s0), s1(s1), p0(p0), p1(p1) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * input = ggml_new_tensor(ctx, type_input, 4, ne_input.data());
        ggml_set_param(input);
        ggml_set_name(input, "input");

        ggml_tensor * out = ggml_pool_2d(ctx, input, pool_type, k0, k1, s0, s1, p0, p1);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_POOL1D
struct test_pool1d : public test_case {
    enum ggml_op_pool pool_type;
    const ggml_type type_input;
    const std::array<int64_t, 4> ne_input;
    const int k0;
    const int s0;
    const int p0;

    std::string vars() override {
        return VARS_TO_STR6(pool_type, type_input, ne_input, k0, s0, p0);
    }

    test_pool1d(ggml_op_pool pool_type = GGML_OP_POOL_AVG,
                ggml_type type_input = GGML_TYPE_F32,
                std::array<int64_t,4> ne_input = {10, 1, 1, 1},
                int k0 = 3, int s0 = 3, int p0 = 0)
        : pool_type(pool_type), type_input(type_input), ne_input(ne_input), k0(k0), s0(s0), p0(p0) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * input = ggml_new_tensor(ctx, type_input, 4, ne_input.data());
        ggml_set_param(input);
        ggml_set_name(input, "input");

        ggml_tensor * out = ggml_pool_1d(ctx, input, pool_type, k0, s0, p0);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_CONV_TRANSPOSE_1D
struct test_conv_transpose_1d : public test_case {
    const std::array<int64_t, 4> ne_input;
    const std::array<int64_t, 4> ne_kernel;

    const int s0; // stride
    const int p0; // padding
    const int d0; // dilation

    std::string vars() override {
        return VARS_TO_STR5(ne_input, ne_kernel, s0, p0, d0);
    }

    test_conv_transpose_1d(std::array<int64_t, 4> ne_input = {197, 32, 1, 1}, // [input_width, input_channels, 1 /* assert in cpu kernel*/, 1 (should be batch)]
                           std::array<int64_t, 4> ne_kernel = {16, 32, 32, 1}, // [kernel_width, output_channels, input_channels, 1 (should be batch)]
                           int s0 = 1, int p0 = 0, int d0 = 1)
        : ne_input(ne_input), ne_kernel(ne_kernel), s0(s0), p0(p0), d0(d0) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * input = ggml_new_tensor(ctx, GGML_TYPE_F32, 4, ne_input.data());
        ggml_set_name(input, "input");

        ggml_tensor * kernel = ggml_new_tensor(ctx, GGML_TYPE_F32, 4, ne_kernel.data());
        ggml_set_name(kernel, "kernel");

        ggml_tensor * out = ggml_conv_transpose_1d(ctx, kernel, input, s0, p0, d0);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_COL2IM_1D
struct test_col2im_1d : public test_case {
    const ggml_type type;
    const int64_t K;    // kernel size
    const int64_t OC;   // output channels
    const int64_t T_in; // input length (number of columns)
    const int s0;       // stride
    const int p0;       // padding cropped from both sides

    std::string vars() override {
        return VARS_TO_STR6(type, K, OC, T_in, s0, p0);
    }

    double max_nmse_err() override {
        return type == GGML_TYPE_F32 ? 1e-7 : 5e-4;
    }

    test_col2im_1d(ggml_type type = GGML_TYPE_F32,
            int64_t K = 4, int64_t OC = 3, int64_t T_in = 7,
            int s0 = 2, int p0 = 0)
        : type(type), K(K), OC(OC), T_in(T_in), s0(s0), p0(p0) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * cols = ggml_new_tensor_2d(ctx, type, K*OC, T_in);
        ggml_set_name(cols, "cols");

        ggml_tensor * out = ggml_col2im_1d(ctx, cols, s0, (int) OC, p0);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_CONV_TRANSPOSE_2D
struct test_conv_transpose_2d : public test_case {
    // Dimensions
    const std::array<int64_t, 4> ne_input;
    const std::array<int64_t, 4> ne_kernel;
    const int stride;
    // Types
    const ggml_type kernel_type;

    std::string vars() override {
        return VARS_TO_STR4(kernel_type, ne_input, ne_kernel, stride);
    }

    double max_nmse_err() override {
        return 5e-4; // The default 1e-7 is too small for Vulkan.
    }

    test_conv_transpose_2d(
        std::array<int64_t, 4> ne_input = {10, 10, 3, 1}, // [input_width, input_height, input_channels, 1]
        std::array<int64_t, 4> ne_kernel = {3, 3, 3, 1}, // [kernel_width, kernel_height, input_channels, 1]
        int stride = 1,
        ggml_type kernel_type = GGML_TYPE_F16
    ) : ne_input(ne_input), ne_kernel(ne_kernel), stride(stride), kernel_type(kernel_type) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * input = ggml_new_tensor(ctx, GGML_TYPE_F32, 4, ne_input.data());
        ggml_set_name(input, "input");

        ggml_tensor * kernel = ggml_new_tensor(ctx, kernel_type, 4, ne_kernel.data());
        ggml_set_name(kernel, "kernel");

        ggml_tensor * out = ggml_conv_transpose_2d_p0(ctx, kernel, input, stride);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_IM2COL
struct test_im2col : public test_case {
    const ggml_type type_input;
    const ggml_type type_kernel;
    const ggml_type dst_type;
    const std::array<int64_t, 4> ne_input;
    const std::array<int64_t, 4> ne_kernel;
    // stride
    const int s0;
    const int s1;
    // padding
    const int p0;
    const int p1;
    // dilation
    const int d0;
    const int d1;
    // mode
    const bool is_2D;

    std::string vars() override {
        return VARS_TO_STR12(type_input, type_kernel, dst_type, ne_input, ne_kernel, s0, s1, p0, p1, d0, d1, is_2D);
    }

    test_im2col(ggml_type type_input = GGML_TYPE_F32, ggml_type type_kernel = GGML_TYPE_F16, ggml_type dst_type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne_input = {10, 10, 3, 1}, // [input_width, input_height, input_channels, 1]
            std::array<int64_t, 4> ne_kernel = {3, 3, 3, 1}, // [kernel_width, kernel_height, input_channels, 1]
            int s0 = 1, int s1 = 1,
            int p0 = 1, int p1 = 1,
            int d0 = 1, int d1 = 1,
            bool is_2D = true)
        : type_input(type_input), type_kernel(type_kernel), dst_type(dst_type), ne_input(ne_input), ne_kernel(ne_kernel), s0(s0), s1(s1), p0(p0), p1(p1), d0(d0), d1(d1), is_2D(is_2D) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * input = ggml_new_tensor(ctx, type_input, 4, ne_input.data());
        ggml_set_param(input);
        ggml_set_name(input, "input");

        ggml_tensor * kernel = ggml_new_tensor(ctx, type_kernel, 4, ne_kernel.data());
        ggml_set_name(kernel, "kernel");

        ggml_tensor * out = ggml_im2col(ctx, kernel, input, s0, s1, p0, p1, d0, d1, is_2D, dst_type);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_IM2COL_3D
struct test_im2col_3d : public test_case {
    const ggml_type type_input;
    const ggml_type type_kernel;
    const ggml_type dst_type;
    const std::array<int64_t, 4> ne_input;
    const std::array<int64_t, 4> ne_kernel;
    // stride
    const int s0;
    const int s1;
    const int s2;
    // padding
    const int p0;
    const int p1;
    const int p2;
    // dilation
    const int d0;
    const int d1;
    const int d2;

    const int64_t IC;
    const bool v;

    std::string vars() override {
        return VARS_TO_STR16(type_input, type_kernel, dst_type, ne_input, ne_kernel, IC, s0, s1, s2, p0, p1, p2, d0, d1, d2, v);
    }

    test_im2col_3d(ggml_type type_input = GGML_TYPE_F32, ggml_type type_kernel = GGML_TYPE_F16, ggml_type dst_type = GGML_TYPE_F32,
                std::array<int64_t, 4> ne_input = {10, 10, 10, 9}, // [OC*IC, KD, KH, KW]
                std::array<int64_t, 4> ne_kernel = {3, 3, 3, 1}, // [N*IC, ID, IH, IW]
                int64_t IC = 3,
                int s0 = 1, int s1 = 1, int s2 = 1,
                int p0 = 1, int p1 = 1, int p2 = 1,
                int d0 = 1, int d1 = 1, int d2 = 1,
                bool v = false)
        : type_input(type_input), type_kernel(type_kernel), dst_type(dst_type), ne_input(ne_input), ne_kernel(ne_kernel), s0(s0), s1(s1), s2(s2), p0(p0), p1(p1), p2(p2), d0(d0), d1(d1), d2(d2), IC(IC), v(v) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * input = ggml_new_tensor(ctx, type_input, 4, ne_input.data());
        ggml_set_param(input);
        ggml_set_name(input, "input");

        if (v) {
            input = ggml_view_4d(ctx, input, ne_input[0] - 2, ne_input[1] - 2, ne_input[2] - 2, ne_input[3] - 2, input->nb[1], input->nb[2], input->nb[3], 0);
            ggml_set_name(input, "view_of_input");
        }

        ggml_tensor * kernel = ggml_new_tensor(ctx, type_kernel, 4, ne_kernel.data());
        ggml_set_name(kernel, "kernel");

        ggml_tensor * out = ggml_im2col_3d(ctx, kernel, input, IC, s0, s1, s2, p0, p1, p2, d0, d1, d2, dst_type);
        ggml_set_name(out, "out");

        return out;
    }
};

// CONV_2D
struct test_conv_2d : public test_case {
    const std::array<int64_t, 4> ne_input;
    const std::array<int64_t, 4> ne_kernel;
    const ggml_type              type_kernel;
    const int                    stride0;
    const int                    stride1;
    const int                    padding0;
    const int                    padding1;
    const int                    dilation0;
    const int                    dilation1;
    // Whether the inputs are contiguous in the channel dim or the width dim
    const bool                   cwhn;
    const int                    kernel_offset;

    std::string vars() override {
        return VARS_TO_STR11(ne_input, ne_kernel, type_kernel, stride0, stride1, padding0, padding1, dilation0, dilation1, cwhn, kernel_offset);
    }

    double max_nmse_err() override {
        return 5e-4;
    }

    uint64_t op_flops(ggml_tensor * t) override {
        GGML_UNUSED(t);
        // Just counting matmul costs:
        // KxCRS @ CRSxNPQ = KxNPQ --> KxNPQx(CRS+CRS-1) flops

        // Copied from ggml.c: int64_t ggml_calc_conv_output_size(int64_t ins, int64_t ks, int s, int p, int d)
        auto calc_conv_output_size = [](int64_t ins, int64_t ks, int s, int p, int d) -> int64_t {
            return (ins + 2 * p - d * (ks - 1) - 1) / s + 1;
        };

        int64_t W    = ne_input[0];
        int64_t H    = ne_input[1];
        int64_t KW   = ne_kernel[0];
        int64_t KH   = ne_kernel[1];
        int64_t Cin  = ne_kernel[2];
        int64_t Cout = ne_kernel[3];
        int64_t N    = ne_input[3];
        int64_t OH   = calc_conv_output_size(H, KH, stride0, padding0, dilation0);
        int64_t OW   = calc_conv_output_size(W, KW, stride0, padding0, dilation0);

        int64_t K   = Cout;
        int64_t CRS = Cin * KH * KW;
        int64_t NPQ = N * OH * OW;

        return K * NPQ * (2 * CRS - 1);
    }

    test_conv_2d(std::array<int64_t, 4> ne_input  = { 64, 64, 16, 1 },
                 std::array<int64_t, 4> ne_kernel = { 3, 3, 1, 16 }, ggml_type type_kernel = GGML_TYPE_F32, int stride0 = 1,
                 int stride1 = 1, int padding0 = 0, int padding1 = 0, int dilation0 = 1, int dilation1 = 1, bool cwhn = false,
                 int kernel_offset = 0) :
        ne_input(ne_input),
        ne_kernel(ne_kernel),
        type_kernel(type_kernel),
        stride0(stride0),
        stride1(stride1),
        padding0(padding0),
        padding1(padding1),
        dilation0(dilation0),
        dilation1(dilation1),
        cwhn(cwhn),
        kernel_offset(kernel_offset) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * input = ggml_new_tensor(ctx, GGML_TYPE_F32, 4, ne_input.data());
        ggml_set_name(input, "input");

        ggml_tensor * kernel;
        if (kernel_offset == 0) {
            kernel = ggml_new_tensor(ctx, type_kernel, 4, ne_kernel.data());
        } else {
            const int64_t nelem = ne_kernel[0] * ne_kernel[1] * ne_kernel[2] * ne_kernel[3];
            ggml_tensor * storage = ggml_new_tensor_1d(ctx, type_kernel, nelem + kernel_offset);
            const size_t element_size = ggml_type_size(type_kernel);
            kernel = ggml_view_4d(ctx, storage, ne_kernel[0], ne_kernel[1], ne_kernel[2], ne_kernel[3],
                                 ne_kernel[0] * element_size, ne_kernel[0] * ne_kernel[1] * element_size,
                                 ne_kernel[0] * ne_kernel[1] * ne_kernel[2] * element_size,
                                 kernel_offset * element_size);
        }
        ggml_set_name(kernel, "kernel");

        if (cwhn) {
            // change memory layout to channel-most-contiguous (CWHN),
            // then permute it back so NE matches the original input
            input  = ggml_cont(ctx, ggml_permute(ctx, input, 1, 2, 0, 3));
            input  = ggml_permute(ctx, input, 2, 0, 1, 3);
            kernel = ggml_cont(ctx, ggml_permute(ctx, kernel, 2, 3, 1, 0));
            kernel = ggml_permute(ctx, kernel, 3, 2, 0, 1);
        }

        ggml_tensor * out =
            ggml_conv_2d_direct(ctx, kernel, input, stride0, stride1, padding0, padding1, dilation0, dilation1);
        ggml_set_name(out, "out");
        return out;
    }
};

// GGML_OP_CONV_2D_DW
struct test_conv_2d_dw : public test_case {
    const std::array<int64_t, 4> ne_input;
    const std::array<int64_t, 4> ne_kernel;
    const ggml_type type_kernel;
    const int stride;
    const int padding;
    const int dilation;
    const bool cwhn;

    std::string vars() override {
        return VARS_TO_STR7(ne_input, ne_kernel, type_kernel, stride, padding, dilation, cwhn);
    }

    test_conv_2d_dw(
            std::array<int64_t, 4> ne_input = {64, 64, 16, 1},
            std::array<int64_t, 4> ne_kernel = {3, 3, 1, 16},
            ggml_type type_kernel = GGML_TYPE_F32,
            int stride = 1, int padding = 0, int dilation = 1, bool cwhn = false)
        : ne_input(ne_input), ne_kernel(ne_kernel), type_kernel(type_kernel), stride(stride), padding(padding), dilation(dilation), cwhn(cwhn) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * input = ggml_new_tensor(ctx, GGML_TYPE_F32, 4, ne_input.data());
        ggml_set_name(input, "input");

        ggml_tensor * kernel = ggml_new_tensor(ctx, type_kernel, 4, ne_kernel.data());
        ggml_set_name(kernel, "kernel");

        if (cwhn) {
            // change memory layout to channel-most-contiguous (CWHN),
            // then permute it back so NE matches the original input
            input = ggml_cont(ctx, ggml_permute(ctx, input, 1, 2, 0, 3));
            input = ggml_permute(ctx, input, 2, 0, 1, 3);
            kernel = ggml_cont(ctx, ggml_permute(ctx, kernel, 2, 3, 1, 0));
            kernel = ggml_permute(ctx, kernel, 3, 2, 0, 1);
        }

        ggml_tensor * out = ggml_conv_2d_dw_direct(
            ctx, kernel, input,
            stride, stride, padding, padding, dilation, dilation);
        ggml_set_name(out, "out");
        return out;
    }
};

// GGML_OP_CONV_3D
struct test_conv_3d : public test_case {
    // Logical 5D dimensions
    const int64_t N, IC, ID, IH, IW;
    const int64_t OC, KD, KH, KW;
    // Conv params
    const int s0, s1, s2;
    const int p0, p1, p2;
    const int d0, d1, d2;
    // Types
    const ggml_type type_kernel;
    const int kernel_offset;

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "CONV_3D";
    }

    std::string vars() override {
        return VARS_TO_STR11(N, IC, ID, IH, IW, OC, KD, KH, KW, s0, s1) + "," +
               VARS_TO_STR9(s2, p0, p1, p2, d0, d1, d2, type_kernel, kernel_offset);
    }

    double max_nmse_err() override {
        return 5e-4;
    }

    uint64_t op_flops(ggml_tensor * t) override {
        GGML_UNUSED(t);
        auto calc_conv_output_size = [](int64_t ins, int64_t ks, int s, int p, int d) -> int64_t {
            return (ins + 2 * p - d * (ks - 1) - 1) / s + 1;
        };
        const int64_t OD = calc_conv_output_size(ID, KD, s2, p2, d2);
        const int64_t OH = calc_conv_output_size(IH, KH, s1, p1, d1);
        const int64_t OW = calc_conv_output_size(IW, KW, s0, p0, d0);

        return (uint64_t)N * OC * OD * OH * OW * std::max<int64_t>(0, 2 * IC * KD * KH * KW - 1);
    }

    test_conv_3d(
        int64_t N, int64_t IC, int64_t ID, int64_t IH, int64_t IW,
        int64_t OC, int64_t KD, int64_t KH, int64_t KW,
        int s0, int s1, int s2,
        int p0, int p1, int p2,
        int d0, int d1, int d2,
        ggml_type type_kernel, int kernel_offset = 0
    ) : N(N), IC(IC), ID(ID), IH(IH), IW(IW),
        OC(OC), KD(KD), KH(KH), KW(KW),
        s0(s0), s1(s1), s2(s2),
        p0(p0), p1(p1), p2(p2),
        d0(d0), d1(d1), d2(d2),
        type_kernel(type_kernel), kernel_offset(kernel_offset) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        // GGML input tensor is packed as [W, H, D, C*N]
        const int64_t ne_input[] = {IW, IH, ID, IC * N};
        ggml_tensor * input = ggml_new_tensor(ctx, GGML_TYPE_F32, 4, ne_input);
        ggml_set_name(input, "input");

        // GGML kernel tensor is packed as [KW, KH, KD, IC*OC]
        const int64_t ne_kernel[] = {KW, KH, KD, IC * OC};
        ggml_tensor * kernel;
        if (kernel_offset == 0) {
            kernel = ggml_new_tensor(ctx, type_kernel, 4, ne_kernel);
        } else {
            ggml_tensor * storage = ggml_new_tensor_1d(ctx, type_kernel, KW * KH * KD * IC * OC + kernel_offset);
            const size_t element_size = ggml_type_size(type_kernel);
            kernel = ggml_view_4d(ctx, storage, KW, KH, KD, IC * OC,
                                 KW * element_size, KW * KH * element_size, KW * KH * KD * element_size,
                                 kernel_offset * element_size);
        }
        ggml_set_name(kernel, "kernel");

        ggml_tensor * out = ggml_conv_3d_direct(ctx, kernel, input, s0, s1, s2, p0, p1, p2, d0, d1, d2, (int)IC, (int)N, (int)OC);
        ggml_set_name(out, "out");
        return out;
    }
};

// GGML_OP_CONCAT
struct test_concat : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne_a;
    const int64_t ne_b_d;
    const int dim;
    const int v; // view (1 << 0: non-cont a (first 3 dim), 1 << 1: non-cont b (first 3 dim), 1 << 2: non-cont a (last 2 dim), 1 << 3: non-cont b (last 2 dim))

    std::string vars() override {
        return VARS_TO_STR5(type, ne_a, ne_b_d, dim, v);
    }

    test_concat(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne_a = {10, 5, 5, 5},
            int64_t ne_b_d = 5,
            int dim = 2, int v = 0)
        : type(type), ne_a(ne_a), ne_b_d(ne_b_d), dim(dim), v(v) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        auto ne_b = ne_a;
        ne_b[dim] = ne_b_d;
        ggml_tensor * a;
        if (v & 1) {
            auto ne = ne_a; ne[0] *= 2; ne[1] *= 4; ne[2] *= 3;
            a = ggml_new_tensor(ctx, type, 4, ne.data());
            ggml_set_name(a, "a");

            a = ggml_view_4d(ctx, a, ne_a[0], ne_a[1], ne_a[2], ne_a[3], a->nb[1], a->nb[2], a->nb[3], 0);
            ggml_set_name(a, "view_of_a");
        } else if (v & 4) {
            auto ne = ne_a; ne[2] *= 2; ne[3] *= 4;
            a = ggml_new_tensor(ctx, type, 4, ne.data());
            ggml_set_name(a, "a");

            a = ggml_view_4d(ctx, a, ne_a[0], ne_a[1], ne_a[2], ne_a[3], a->nb[1], a->nb[2], a->nb[3], 0);
            ggml_set_name(a, "view_of_a");
        } else {
            a = ggml_new_tensor(ctx, type, 4, ne_a.data());
            ggml_set_name(a, "a");
        }
        ggml_tensor * b;
        if (v & 2) {
            auto ne = ne_b; ne[0] *= 3; ne[1] *= 2; ne[2] *= 4;
            b = ggml_new_tensor(ctx, type, 4, ne.data());
            ggml_set_name(b, "b");

            b = ggml_view_4d(ctx, b, ne_b[0], ne_b[1], ne_b[2], ne_b[3], b->nb[1], b->nb[2], b->nb[3], 0);
            ggml_set_name(b, "view_of_b");
        } else if (v & 8) {
            auto ne = ne_b; ne[2] *= 3; ne[3] *= 2;
            b = ggml_new_tensor(ctx, type, 4, ne.data());
            ggml_set_name(b, "b");

            b = ggml_view_4d(ctx, b, ne_b[0], ne_b[1], ne_b[2], ne_b[3], b->nb[1], b->nb[2], b->nb[3], 0);
            ggml_set_name(b, "view_of_b");
        } else {
            b = ggml_new_tensor(ctx, type, 4, ne_b.data());
            ggml_set_name(b, "b");
        }

        ggml_tensor * out = ggml_concat(ctx, a, b, dim);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_ARGSORT
struct test_argsort : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    ggml_sort_order order;

    std::string vars() override {
        return VARS_TO_STR3(type, ne, order);
    }

    test_argsort(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {16, 10, 10, 10},
            ggml_sort_order order = GGML_SORT_ORDER_ASC)
        : type(type), ne(ne), order(order) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_argsort(ctx, a, order);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        std::random_device rd;
        std::default_random_engine rng(rd());
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (t->type == GGML_TYPE_I32) {
                // indices
                std::vector<int> data(ggml_nelements(t));
                for (int i = 0; i < ggml_nelements(t); i++) {
                    data[i] = rand();
                }
                std::shuffle(data.begin(), data.end(), rng);
                ggml_backend_tensor_set(t, data.data(), 0, ne[0]*ne[1]*ne[2]*ne[3] * sizeof(int));
            } else if (t->type == GGML_TYPE_F32) {
                // initialize with unique values to avoid ties
                for (int64_t r = 0; r < ggml_nrows(t); r++) {
                    std::vector<float> data(t->ne[0]);
                    for (int i = 0; i < t->ne[0]; i++) {
                        data[i] = i;
                    }
                    std::shuffle(data.begin(), data.end(), rng);
                    ggml_backend_tensor_set(t, data.data(), r * t->nb[1], t->ne[0] * sizeof(float));
                }
            } else {
                GGML_ABORT("fatal error");
            }
        }
    }
};

// GGML_OP_TOP_K
struct test_top_k : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const int k;
    const bool ties;
    ggml_tensor * input {};

    std::string vars() override {
        return VARS_TO_STR4(type, ne, k, ties);
    }

    test_top_k(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {16, 10, 10, 10},
            int k = 4, bool ties = false)
        : type(type), ne(ne), k(k), ties(ties) {}

    double max_err() override {
        return 0.0;
    }

    // When there are ties, only validate the final result.
    // The logic in err can't handle the sentinel tensors.
    bool run_whole_graph() override { return ties; }

    double err(const float * a, const float * b, size_t n) override {
        // When there are no ties, we expect the exact same set of indices,
        // but possibly in a different order. When there are ties, the indices
        // can be different but the input values they correspond to should be
        // the same. The logic for ties could work for non-ties, but only for
        // the output tensor, not for the sentinel tensors.
        if (ties) {
            std::vector<float> src(ggml_nelements(input));

            ggml_backend_tensor_get(input, src.data(), 0, ggml_nelements(input) * ggml_type_size(type));

            double diff = 0.0f;

            GGML_ASSERT(n == (size_t)(ggml_nrows(input) * k));
            int64_t cols = input->ne[0];
            std::vector<int32_t> ia(k);
            std::vector<int32_t> ib(k);
            std::vector<float> asrc(k);
            std::vector<float> bsrc(k);
            for (int64_t r = 0; r < ggml_nrows(input); r++) {
                // Convert indices for the row back to integer
                for (int64_t c = 0; c < k; c++) {
                    ia[c] = (int32_t)a[r * k + c];
                    ib[c] = (int32_t)b[r * k + c];
                }
                // The src values for each row should match.
                for (int64_t c = 0; c < k; c++) {
                    asrc[c] = src[r * cols + ia[c]];
                    bsrc[c] = src[r * cols + ib[c]];
                }
                diff += jdst(asrc.data(), bsrc.data(), k);
                // There should be no duplicate indices
                std::sort(ia.begin(), ia.end());
                std::sort(ib.begin(), ib.end());
                if (std::adjacent_find(ia.begin(), ia.end()) != ia.end()) {
                    diff += 1;
                }
                if (std::adjacent_find(ib.begin(), ib.end()) != ib.end()) {
                    diff += 1;
                }
            }
            return diff;
        } else {
            std::vector<int32_t> ia(n);
            std::vector<int32_t> ib(n);

            double diff = 0.0f;

            for (size_t i = 0; i < n; i++) {
                ia[i] = (int32_t) a[i];
                ib[i] = (int32_t) b[i];

                // penalize the result if the data is not integer valued
                diff += std::fabs(a[i] - ia[i]);
                diff += std::fabs(b[i] - ib[i]);
            }

            return diff + jdst(ia.data(), ib.data(), n);
        }
    }

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(a, "a");

        // Save 'a' for err()
        input = a;

        ggml_tensor * out = ggml_top_k(ctx, a, k);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        std::random_device rd;
        std::default_random_engine rng(rd());
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            int tie_denom = std::max(1, std::min(10, k / 2));
            for (int64_t r = 0; r < ggml_nrows(t); r++) {
                std::vector<float> data(t->ne[0]);
                for (int i = 0; i < t->ne[0]; i++) {
                    if (ties) {
                        // integer division to introduce duplicates
                        data[i] = i / tie_denom;
                    } else {
                        data[i] = i;
                    }
                }
                std::shuffle(data.begin(), data.end(), rng);
                ggml_backend_tensor_set(t, data.data(), r * t->nb[1], t->ne[0] * sizeof(float));
            }
        }
    }
};

// qwen4exp QSA indexer top-k fusion: expand per-block scores to cells, add the f16 mask, top-k.
struct test_topk_qsa : public test_case {
    const int64_t n_blocks;
    const int64_t n_kv;
    const int64_t n_tps;
    const int64_t n_stream;
    const int     width;
    ggml_tensor * out {};

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "TOPK_QSA";
    }

    std::string vars() override {
        return VARS_TO_STR5(n_blocks, n_kv, n_tps, n_stream, width);
    }

    test_topk_qsa(int64_t n_blocks = 512, int64_t n_kv = 2048, int64_t n_tps = 2, int64_t n_stream = 1, int width = 1500)
        : n_blocks(n_blocks), n_kv(n_kv), n_tps(n_tps), n_stream(n_stream), width(width) {}

    double max_err() override { return 0.0; }
    bool run_whole_graph() override { return true; }

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * score = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, n_blocks, n_tps, n_stream);
        ggml_set_name(score, "score");
        ggml_tensor * cell_blk = ggml_new_tensor_2d(ctx, GGML_TYPE_I32, n_kv, n_stream);
        ggml_set_name(cell_blk, "cell_blk");
        ggml_tensor * kq_mask = ggml_new_tensor_3d(ctx, GGML_TYPE_F16, n_kv, n_tps, n_stream);
        ggml_set_name(kq_mask, "kq_mask");

        ggml_tensor * a = ggml_cont(ctx, ggml_permute(ctx, score, 1, 0, 2, 3));
        ggml_tensor * e = ggml_get_rows(ctx, a, cell_blk);
        e = ggml_cont(ctx, ggml_permute(ctx, e, 1, 0, 2, 3));
        ggml_tensor * m = ggml_cast(ctx, kq_mask, GGML_TYPE_F32);
        e = ggml_add(ctx, e, ggml_reshape_3d(ctx, m, n_kv, n_tps, n_stream));
        out = ggml_top_k(ctx, e, width);
        ggml_set_name(out, "out");
        return out;
    }

    std::vector<ggml_tensor *> fusion_test_nodes() override { return { out }; }

    // distinct mask ramp + small scores keep every cell value unique, so no top-k ties
    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (t->op != GGML_OP_NONE) {
                continue;
            }
            if (t->type == GGML_TYPE_I32) {
                std::vector<int32_t> data(ggml_nelements(t));
                for (auto & v : data) { v = rand() % n_blocks; }
                ggml_backend_tensor_set(t, data.data(), 0, data.size() * sizeof(int32_t));
            } else if (t->type == GGML_TYPE_F16) {
                std::vector<ggml_fp16_t> data(ggml_nelements(t));
                for (int64_t r = 0; r < ggml_nrows(t); r++) {
                    for (int64_t i = 0; i < n_kv; i++) {
                        data[r * n_kv + i] = ggml_fp32_to_fp16((float) i);
                    }
                }
                ggml_backend_tensor_set(t, data.data(), 0, data.size() * sizeof(ggml_fp16_t));
            } else {
                init_tensor_uniform(t, 0.0f, 0.5f);
            }
        }
    }

    // top-k output order is unspecified; compare as a set of indices
    double err(const float * a, const float * b, size_t n) override {
        std::vector<int32_t> ia(n), ib(n);
        double diff = 0.0;
        for (size_t i = 0; i < n; i++) {
            ia[i] = (int32_t) a[i];
            ib[i] = (int32_t) b[i];
            diff += std::fabs(a[i] - ia[i]) + std::fabs(b[i] - ib[i]);
        }
        return diff + jdst(ia.data(), ib.data(), n);
    }
};

enum MoeGatingFunc {
    GATING_FUNC_SOFTMAX,
    GATING_FUNC_SIGMOID,
    GATING_FUNC_SOFTMAX_WEIGHT,
    GATING_FUNC_SQRT_SOFTPLUS,
};

struct test_topk_moe : public test_case {
    const std::array<int64_t, 4> ne;
    const int n_expert_used;
    const bool with_norm;
    const bool bias_probs;
    const MoeGatingFunc gating_func;
    const float scale_w;
    ggml_tensor * weights {};
    ggml_tensor * selected_experts {};

    test_topk_moe(std::array<int64_t, 4> ne              = { 10, 5, 1, 1 },
                  int                    n_expert_used   = 1,
                  bool                   with_norm       = false,
                  bool                   bias_probs      = false,
                  MoeGatingFunc          gating_func     = GATING_FUNC_SOFTMAX,
                  float                  scale_w         = 0.0f) :
        ne(ne),
        n_expert_used(n_expert_used),
        with_norm(with_norm),
        bias_probs(bias_probs),
        gating_func(gating_func),
        scale_w(scale_w) {
        GGML_ASSERT(n_expert_used <= ne[0]);
    }

    std::string vars() override { return VARS_TO_STR6(ne, n_expert_used, with_norm, bias_probs, gating_func, scale_w); }

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "TOPK_MOE";
    }

    bool run_whole_graph() override { return true; }

    ggml_tensor * build_graph(ggml_context * ctx) override {
        const int n_expert = ne[0];
        const int n_tokens = ne[1];

        ggml_tensor * logits = ggml_new_tensor(ctx, GGML_TYPE_F32, 4, ne.data());
        ggml_tensor * probs            =
            (gating_func == GATING_FUNC_SOFTMAX) ? ggml_soft_max(ctx, logits) :
            (gating_func == GATING_FUNC_SIGMOID) ? ggml_sigmoid(ctx, logits) :
            (gating_func == GATING_FUNC_SQRT_SOFTPLUS) ? ggml_sqrt(ctx, ggml_softplus(ctx, logits)) : logits;
        ggml_set_name(probs, "probs");

        ggml_tensor * selection_probs = probs;
        if (bias_probs) {
            ggml_tensor * exp_probs_b = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, ne[0]);
            ggml_set_name(exp_probs_b, "exp_probs_b");
            selection_probs = ggml_add(ctx, probs, exp_probs_b);
            ggml_set_name(selection_probs, "selection_probs");
        }

        selected_experts = ggml_argsort_top_k(ctx, selection_probs, n_expert_used); // [n_expert_used, n_tokens]
        ggml_set_name(selected_experts, "selected_experts");

        weights = ggml_get_rows(ctx, ggml_reshape_3d(ctx, probs, 1, n_expert, n_tokens), selected_experts); // [1, n_expert_used, n_tokens]
        ggml_set_name(weights, "weights");

        if (gating_func == GATING_FUNC_SOFTMAX_WEIGHT) {
            weights = ggml_reshape_2d(ctx, weights, n_expert_used, n_tokens);
            weights = ggml_soft_max(ctx, weights);  // [n_expert_used, n_tokens]
            weights = ggml_reshape_3d(ctx, weights, 1, n_expert_used, n_tokens);
        }

        if (with_norm) {
            weights = ggml_reshape_2d(ctx, weights, n_expert_used, n_tokens);
            ggml_tensor * weights_sum = ggml_sum_rows(ctx, weights); // [1, n_tokens]
            ggml_set_name(weights_sum, "weights_sum");

            weights_sum = ggml_clamp(ctx, weights_sum, 6.103515625e-5, INFINITY);
            weights = ggml_div(ctx, weights, weights_sum); // [n_expert_used, n_tokens]
            weights = ggml_reshape_3d(ctx, weights, 1, n_expert_used, n_tokens);
        }

        if (scale_w) {
            weights = ggml_scale(ctx, weights, scale_w);
        }

        ggml_set_name(weights, "weights");
        return weights;
    }
    // Verify two outputs
    std::vector<ggml_tensor *> fusion_test_nodes() override { return { selected_experts, weights }; }

    // allow output in arbitrary order
    double err(const float * a, const float * b, size_t n) override {
        std::vector<float> a2(n);
        std::vector<float> b2(n);
        for (size_t i = 0; i < n; ++i) {
            a2[i] = a[i];
            b2[i] = b[i];
        }
        std::sort(a2.begin(), a2.end());
        std::sort(b2.begin(), b2.end());
        return nmse(a2.data(), b2.data(), n);
    }
};

struct test_moe_reduce : public test_case {
    const int64_t n_embd;
    const int64_t n_expert_used;
    const int64_t n_tokens;
    const bool unaligned_experts;
    const bool with_expert_scale;
    const bool interleaved_views_adds;

    test_moe_reduce(
            int64_t n_embd, int64_t n_expert_used, int64_t n_tokens,
            bool unaligned_experts = false, bool with_expert_scale = false, bool interleaved_views_adds = false) :
        n_embd(n_embd), n_expert_used(n_expert_used), n_tokens(n_tokens),
        unaligned_experts(unaligned_experts), with_expert_scale(with_expert_scale),
        interleaved_views_adds(interleaved_views_adds) {}

    std::string vars() override {
        return VARS_TO_STR6(n_embd, n_expert_used, n_tokens, unaligned_experts, with_expert_scale, interleaved_views_adds);
    }

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "MOE_REDUCE";
    }

    bool run_whole_graph() override { return true; }

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * experts;
        if (unaligned_experts) {
            ggml_tensor * storage = ggml_new_tensor_1d(
                ctx, GGML_TYPE_F32, n_embd * n_expert_used * n_tokens + 1);
            ggml_set_name(storage, "experts_storage");
            experts = ggml_view_3d(ctx, storage, n_embd, n_expert_used, n_tokens,
                n_embd * sizeof(float), n_embd * n_expert_used * sizeof(float), sizeof(float));
        } else {
            experts = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, n_embd, n_expert_used, n_tokens);
        }
        ggml_set_name(experts, "experts");
        ggml_tensor * weights = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, 1, n_expert_used, n_tokens);
        ggml_set_name(weights, "weights");

        ggml_tensor * scaled = experts;
        if (with_expert_scale) {
            ggml_tensor * expert_scale = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, 1, n_expert_used, n_tokens);
            ggml_set_name(expert_scale, "expert_scale");
            scaled = ggml_mul(ctx, experts, expert_scale);
            ggml_set_name(scaled, "scaled_experts");
        }

        ggml_tensor * weighted = ggml_mul(ctx, scaled, weights);
        ggml_set_name(weighted, "weighted_experts");

        std::vector<ggml_tensor *> views(n_expert_used);
        for (int64_t expert = 0; expert < n_expert_used; ++expert) {
            views[expert] = ggml_view_2d(
                ctx, weighted, n_embd, n_tokens, weighted->nb[2], expert * weighted->nb[1]);
            if (!interleaved_views_adds && mode == MODE_TEST) {
                ggml_build_forward_expand(gf, views[expert]);
            }
        }

        ggml_tensor * out = views[0];
        for (int64_t expert = 1; expert < n_expert_used; ++expert) {
            out = ggml_add(ctx, out, views[expert]);
            if (!interleaved_views_adds && mode == MODE_TEST) {
                ggml_build_forward_expand(gf, out);
            }
        }
        ggml_set_name(out, "moe_reduce");
        return out;
    }
};

struct test_mul_mat_vec_fusion : public test_case {
    const ggml_type type;
    const ggml_glu_op glu_op;
    const int64_t m;
    const int64_t n;
    const int64_t k;
    const bool use_id;
    const int n_mats;
    const int n_used;
    const bool b;        // broadcast b matrix (only for use_id)
    const bool with_bias;
    const bool with_gate;
    const bool with_lane_scale;
    std::array<int64_t, 2> batch_dims;

    test_mul_mat_vec_fusion(ggml_type type, ggml_glu_op op, int64_t m, int64_t n, int64_t k,
                        bool use_id = false, int n_mats = 1, int n_used = 1, bool b = false, bool with_bias = false, bool with_gate = true,
                        bool with_lane_scale = false, std::array<int64_t, 2> batch_dims = {4, 2})
    : type(type), glu_op(op), m(m), n(n), k(k), use_id(use_id), n_mats(n_mats), n_used(n_used), b(b), with_bias(with_bias),
        with_gate(with_gate), with_lane_scale(with_lane_scale), batch_dims(batch_dims) {
        if (use_id) {
            GGML_ASSERT(n_used <= n_mats);
        }
    }

    std::string vars() override {
        return VARS_TO_STR13(type, glu_op, m, n, k, use_id, n_mats, n_used, b, with_bias, with_gate, with_lane_scale, batch_dims);
    }

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "MUL_MAT_VEC_FUSION";
    }

    bool run_whole_graph() override { return true; }
    bool use_weight_context() override { return use_id && with_lane_scale; }

    ggml_tensor * build_gate(ggml_context * ctx, ggml_tensor * ffn_gate, ggml_tensor * ffn_up) {
        ggml_tensor * out = nullptr;
        if (with_gate) {
            if (glu_op == GGML_GLU_OP_SWIGLU_OAI) {
                constexpr float alpha = 1.702f;
                constexpr float limit = 7.0f;
                out = ggml_swiglu_oai(ctx, ffn_gate, ffn_up, alpha, limit);
            } else if (glu_op == GGML_GLU_OP_SWIGLU_CLAMP) {
                constexpr float limit = 10.0f;
                out                   = ggml_swiglu_clamp(ctx, ffn_gate, ffn_up, limit);
            } else {
                out = ggml_glu_split(ctx, ffn_gate, ffn_up, glu_op);
            }
        }
        return out;
    }

    ggml_tensor * build_lane_scale_dense(ggml_context * ctx, ggml_tensor * out) {
        ggml_tensor * scale = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 1);
        return ggml_mul(ctx, out, scale);
    }

    ggml_tensor * build_lane_scale_id(ggml_context * ctx, ggml_context * ctx_weights, ggml_tensor * out, ggml_tensor * ids) {
        GGML_ASSERT(ctx_weights);
        ggml_tensor * scale = ggml_new_tensor_1d(ctx_weights, GGML_TYPE_F32, n_mats);
        ggml_tensor * s = ggml_reshape_3d(ctx, scale, 1, n_mats, 1);
        s = ggml_repeat_4d(ctx, s, 1, n_mats, m, 1);
        s = ggml_get_rows(ctx, s, ids);
        return ggml_mul(ctx, out, s);
    }

    ggml_tensor * build_graph(ggml_context * ctx) override {
        GGML_ASSERT(!use_weight_context());
        return build_graph(ctx, nullptr);
    }

    ggml_tensor * build_graph(ggml_context * ctx, ggml_context * ctx_weights) override {
        if (!use_id) {
            const int              channels = batch_dims[0];
            const int              samples  = batch_dims[1];
            std::array<int64_t, 4> ne       = { k, m, channels, samples };
            std::array<int64_t, 4> ne0      = { k, n, channels, samples };

            ggml_tensor * cur  = ggml_new_tensor(ctx, GGML_TYPE_F32, 4, ne.data());
            ggml_tensor * gate = with_gate ? ggml_new_tensor(ctx, type, 4, ne0.data()) : nullptr;
            ggml_tensor * up   = ggml_new_tensor(ctx, type, 4, ne0.data());

            auto build_lane_up = [&]() {
                ggml_tensor * ffn_up = ggml_mul_mat(ctx, up, cur);
                if (with_lane_scale) {
                    ffn_up = build_lane_scale_dense(ctx, ffn_up);
                }
                if (with_bias) {
                    std::array<int64_t, 4> bias_ne = { ffn_up->ne[0], 1, channels, samples };
                    ggml_tensor * up_bias = ggml_new_tensor(ctx, GGML_TYPE_F32, 4, bias_ne.data());
                    ffn_up = ggml_add(ctx, ffn_up, up_bias);
                }
                return ffn_up;
            };

            auto build_lane_gate = [&]() {
                ggml_tensor * ffn_gate = ggml_mul_mat(ctx, gate, cur);
                if (with_lane_scale) {
                    ffn_gate = build_lane_scale_dense(ctx, ffn_gate);
                }
                if (with_bias) {
                    std::array<int64_t, 4> bias_ne   = { ffn_gate->ne[0], 1, channels, samples };
                    ggml_tensor * gate_bias = ggml_new_tensor(ctx, GGML_TYPE_F32, 4, bias_ne.data());
                    ffn_gate = ggml_add(ctx, ffn_gate, gate_bias);
                }
                return ffn_gate;
            };

            ggml_tensor * ffn_up = build_lane_up();
            ggml_tensor * ffn_gate = with_gate ? build_lane_gate() : nullptr;

            ggml_tensor * out = with_gate ? build_gate(ctx, ffn_gate, ffn_up) : ffn_up;

            std::array<int64_t, 4> bias2_ne   = { out->ne[0], 1, channels, samples };
            ggml_tensor * bias2 = ggml_new_tensor(ctx, GGML_TYPE_F32, 4, bias2_ne.data());
            out = ggml_add(ctx, out, bias2);

            ggml_set_name(out, "out");
            return out;
        } else {
            ggml_tensor * gates = ggml_new_tensor_3d(ctx, type, k, n, n_mats);
            ggml_tensor * ups   = ggml_new_tensor_3d(ctx, type, k, n, n_mats);
            ggml_tensor * ids   = ggml_new_tensor_2d(ctx, GGML_TYPE_I32, n_mats, m);

            if (n_used != n_mats) {
                ids = ggml_view_2d(ctx, ids, n_used, m, ids->nb[1], 0);
            }

            ggml_tensor * cur = ggml_new_tensor_3d(ctx, GGML_TYPE_F32, k, this->b ? 1 : n_used, m);
            ggml_set_name(cur, "cur");

            auto build_lane_up = [&]() {
                ggml_tensor * ffn_up = ggml_mul_mat_id(ctx, ups, cur, ids);
                if (with_lane_scale) {
                    ffn_up = build_lane_scale_id(ctx, ctx_weights, ffn_up, ids);
                }
                if (with_bias) {
                    ggml_tensor * up_bias_param = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, ffn_up->ne[0], n_mats);
                    ffn_up = ggml_add_id(ctx, ffn_up, up_bias_param, ids);
                }
                return ffn_up;
            };

            auto build_lane_gate = [&]() {
                ggml_tensor * ffn_gate = ggml_mul_mat_id(ctx, gates, cur, ids);
                if (with_lane_scale) {
                    ffn_gate = build_lane_scale_id(ctx, ctx_weights, ffn_gate, ids);
                }
                if (with_bias) {
                    ggml_tensor * gate_bias_param = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, ffn_gate->ne[0], n_mats);
                    ffn_gate = ggml_add_id(ctx, ffn_gate, gate_bias_param, ids);
                }
                return ffn_gate;
            };

            ggml_tensor * ffn_up = build_lane_up();
            ggml_tensor * ffn_gate = with_gate ? build_lane_gate() : nullptr;

            ggml_tensor * out = with_gate ? build_gate(ctx, ffn_gate, ffn_up) : ffn_up;

            std::array<int64_t, 4> scale_ne { 1, out->ne[1], out->ne[2], out->ne[3] };
            ggml_tensor * scale = ggml_new_tensor(ctx, out->type, 4, scale_ne.data());
            out = ggml_mul(ctx, out, scale);

            ggml_set_name(out, "out");
            return out;
        }
    }

    void initialize_tensors(ggml_context * ctx) override {
        if (!use_id) {
            for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
                init_tensor_uniform(t);
            }
        } else {
            init_mul_mat_id_tensors(ctx, n_mats);
        }
    }

    double max_nmse_err() override {
        return 5e-3;
    }
};

// GGML_OP_SUM
struct test_sum : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const std::array<int64_t, 4> permute;
    bool _use_permute;

    std::string vars() override {
        std::string v = VARS_TO_STR2(type, ne);
        if (_use_permute) v += "," + VAR_TO_STR(permute);
        return v;
    }

    test_sum(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 5, 4, 3},
            std::array<int64_t, 4> permute = {0, 0, 0, 0})
        : type(type), ne(ne), permute(permute),
            _use_permute(permute[0] + permute[1] + permute[2] + permute[3] > 0) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(a);
        ggml_set_name(a, "a");

        if (_use_permute) {
            a = ggml_permute(ctx, a, permute[0], permute[1], permute[2], permute[3]);
            ggml_set_name(a, "a_permuted");
        }

        ggml_tensor * out = ggml_sum(ctx, a);
        ggml_set_name(out, "out");

        return out;
    }

    float grad_eps() override {
        return 0.1f * sqrtf(ne[0]*ne[1]*ne[2]*ne[3]);
    }

    // Don't center the distribution around zero. Helps to avoid catastrophic cancellation.
    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != nullptr; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, -0.9f, 1.1f);
        }
    }
};

// GGML_OP_SUM_ROWS
struct test_sum_rows : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const bool permute;
    const bool slice;

    std::string vars() override {
        return VARS_TO_STR4(type, ne, permute, slice);
    }

    test_sum_rows(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 5, 4, 3},
            bool permute = false, bool slice = false)
        : type(type), ne(ne), permute(permute), slice(slice) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(a);
        ggml_set_name(a, "a");

        if (slice) {
            a = ggml_view_4d(ctx, a,
                             ne[0], ne[1], ne[2] / 2, ne[3] - 1,
                             a->nb[1], a->nb[2] * 2, a->nb[3], /*offset=*/a->nb[3]);
        }
        if (permute) {
            a = ggml_permute(ctx, a, 0, 2, 3, 1);
        }

        ggml_tensor * out = ggml_sum_rows(ctx, a);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_MEAN
struct test_mean : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const bool permute;
    const bool slice;

    std::string vars() override {
        return VARS_TO_STR4(type, ne, permute, slice);
    }

    test_mean(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 5, 4, 3},
            bool permute = false, bool slice = false)
        : type(type), ne(ne), permute(permute), slice(slice) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(a);
        ggml_set_name(a, "a");

        if (slice) {
            a = ggml_view_4d(ctx, a,
                             ne[0], ne[1], ne[2] / 2, ne[3] - 1,
                             a->nb[1], a->nb[2] * 2, a->nb[3], /*offset=*/a->nb[3]);
        }
        if (permute) {
            a = ggml_permute(ctx, a, 0, 2, 3, 1);
        }

        ggml_tensor * out = ggml_mean(ctx, a);
        ggml_set_name(out, "out");

        return out;
    }

    float grad_eps() override {
        return 0.1f * ne[0]*ne[1]*ne[2]*ne[3];
    }

    // Don't center the distribution around zero. Helps to avoid catastrophic cancellation.
    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != nullptr; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, -0.9f, 1.1f);
        }
    }
};

// GGML_OP_UPSCALE
struct test_upscale : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const int32_t scale_factor;
    const bool transpose;
    const ggml_scale_mode mode;

    std::string vars() override {
        return VARS_TO_STR5(type, ne, scale_factor, mode, transpose);
    }

    test_upscale(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {512, 512, 3, 1},
            int32_t scale_factor = 2, ggml_scale_mode mode = GGML_SCALE_MODE_NEAREST, bool transpose = false)
        : type(type), ne(ne), scale_factor(scale_factor), transpose(transpose), mode(mode) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(a, "a");

        if (transpose) {
            a = ggml_transpose(ctx, a);
            ggml_set_name(a, "a_transposed");
        }

        ggml_tensor * out = ggml_upscale(ctx, a, scale_factor, mode);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_UPSCALE (via ggml_interpolate)
struct test_interpolate : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const std::array<int64_t, 4> ne_tgt;
    const ggml_scale_mode mode = GGML_SCALE_MODE_NEAREST;

    std::string vars() override {
        return VARS_TO_STR4(type, ne, ne_tgt, mode);
    }

    test_interpolate(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne     = {2, 5,  7, 11},
            std::array<int64_t, 4> ne_tgt = {5, 7, 11, 13},
            ggml_scale_mode mode = GGML_SCALE_MODE_NEAREST)
        : type(type), ne(ne), ne_tgt(ne_tgt), mode(mode) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_interpolate(ctx, a, ne_tgt[0], ne_tgt[1],ne_tgt[2], ne_tgt[3], mode);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_GROUP_NORM
struct test_group_norm : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const int32_t num_groups;
    const float eps;

    std::string vars() override {
        return VARS_TO_STR4(type, ne, num_groups, eps);
    }

    test_group_norm(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {64, 64, 320, 1},
            int32_t num_groups = 32,
            float eps = 1e-6f)
        : type(type), ne(ne), num_groups(num_groups), eps(eps) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_group_norm(ctx, a, num_groups, eps);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_GROUP_NORM + GGML_OP_MUL + GGML_OP_ADD
struct test_group_norm_mul_add : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    int num_groups;
    float eps;

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "GROUP_NORM_MUL_ADD";
    }

    bool run_whole_graph() override { return true; }

    std::string vars() override {
        return VARS_TO_STR4(type, ne, num_groups, eps);
    }

    test_group_norm_mul_add(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {128, 1, 1, 1},
            int num_groups = 4,
            float eps = 1e-5f)
        : type(type), ne(ne), num_groups(num_groups), eps(eps) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_tensor * w = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_tensor * b = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(a); ggml_set_param(w); ggml_set_param(b);
        ggml_set_name(a, "a"); ggml_set_name(w, "w"); ggml_set_name(b, "b");
        ggml_tensor * n = ggml_group_norm(ctx, a, num_groups, eps);
        ggml_tensor * m = ggml_mul(ctx, n, w);
        ggml_tensor * out = ggml_add(ctx, m, b);
        ggml_set_name(out, "out");
        return out;
    }
};

// GGML_OP_L2_NORM x N: independent same-shape norms in one graph (strided qkv views or
// contiguous), consuming adds nested so the norms stay adjacent in the graph.
struct test_l2_norm_batch : public test_case {
    const ggml_type              type;
    const std::array<int64_t, 4> ne;
    const int                    n_norms;
    const float                  eps;
    const bool                   strided;

    std::string vars() override { return VARS_TO_STR5(type, ne, n_norms, eps, strided); }
    std::string op_desc(ggml_tensor * t) override { GGML_UNUSED(t); return "L2_NORM_BATCH"; }
    bool run_whole_graph() override { return true; }

    test_l2_norm_batch(ggml_type type = GGML_TYPE_F32, std::array<int64_t, 4> ne = { 128, 16, 16, 1 },
                       int n_norms = 4, float eps = 1e-12f, bool strided = true)
        : type(type), ne(ne), n_norms(n_norms), eps(eps), strided(strided) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        GGML_ASSERT(n_norms >= 2 && n_norms <= 8);
        ggml_tensor * parent = nullptr;
        if (strided) {
            parent = ggml_new_tensor_4d(ctx, type, ne[0], ne[1] * n_norms, ne[2], ne[3]);  // qkv buffer
        }
        ggml_tensor * norms[8] = {};
        for (int t = 0; t < n_norms; ++t) {
            ggml_tensor * src;
            if (strided) {
                src = ggml_view_4d(ctx, parent, ne[0], ne[1], ne[2], ne[3], parent->nb[1], parent->nb[2],
                                   parent->nb[3], t * ne[1] * parent->nb[1]);
            } else {
                src = ggml_new_tensor(ctx, type, 4, ne.data());
            }
            norms[t] = ggml_l2_norm(ctx, src, eps);
        }
        ggml_tensor * out = norms[n_norms - 1];
        for (int t = n_norms - 2; t >= 0; --t) {
            out = ggml_add(ctx, norms[t], out);
        }
        ggml_set_name(out, "out");
        return out;
    }
};

// GGML_OP_L2_NORM
struct test_l2_norm : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const float eps;
    bool v;
    bool noncontig_rows;

    std::string vars() override {
        return VARS_TO_STR5(type, ne, eps, v, noncontig_rows);
    }

    test_l2_norm(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {64, 64, 320, 1},
            float eps = 1e-12f,
            bool v = false,
            bool noncontig_rows = false)
        : type(type), ne(ne), eps(eps), v(v), noncontig_rows(noncontig_rows) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        const std::array<int64_t, 4> ne_a = noncontig_rows ?
            std::array<int64_t, 4>{ ne[1], ne[0], ne[2], ne[3] } : ne;
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne_a.data());
        ggml_set_name(a, "a");

        if (noncontig_rows) {
            a = ggml_permute(ctx, a, 1, 0, 2, 3);
            ggml_set_name(a, "permuted a");
        }
        if (v) {
            a = ggml_view_4d(ctx, a, a->ne[0]/2, a->ne[1]/2, a->ne[2]/2, a->ne[3]/2, a->nb[1], a->nb[2], a->nb[3], 0);
            ggml_set_name(a, "view of a");
        }

        ggml_tensor * out = ggml_l2_norm(ctx, a, eps);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_ACC
struct test_acc : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne_a;
    const std::array<int64_t, 4> ne_b;
    const int64_t stride_dim;

    std::string vars() override {
        return VARS_TO_STR4(type, ne_a, ne_b, stride_dim);
    }

    test_acc(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne_a = {256, 17, 2, 3},
            std::array<int64_t, 4> ne_b = {256, 16, 2, 3},
            uint64_t stride_dim = -1)
        : type(type), ne_a(ne_a), ne_b(ne_b), stride_dim(stride_dim) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne_a.data());
        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * b;
        if (stride_dim == 1 || stride_dim == 2 || stride_dim == 3) {
            // Create a larger tensor and take a view at a non-zero offset.
            // This tests that the backend correctly handles b's data offset
            std::array<int64_t, 4> ne_b_pad = {ne_b[0], ne_b[1], ne_b[2], ne_b[3]};
            ne_b_pad[stride_dim] += 1;
            ggml_tensor * b_pad = ggml_new_tensor(ctx, type, 4, ne_b_pad.data());
            ggml_set_param(b_pad);
            ggml_set_name(b_pad, "b_pad");
            // View that skips the first row, so b has a non-zero byte offset
            b = ggml_view_4d(ctx, b_pad,
                ne_b[0], ne_b[1], ne_b[2], ne_b[3],
                b_pad->nb[1], b_pad->nb[2], b_pad->nb[3],
                b_pad->nb[1]);
        } else {
            b = ggml_new_tensor(ctx, type, 4, ne_b.data());
            ggml_set_param(b);
        }
        ggml_set_name(b, "b");

        // When ne_b[0] < ne_a[0], a->nb[1] != b->nb[1], so the stride
        // parameters to ggml_acc don't match b's natural stride.
        ggml_tensor * out = ggml_acc(ctx, a, b, a->nb[1], a->nb[2], a->nb[3], 0);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_PAD
struct test_pad : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne_a;
    const int pad_0;
    const int pad_1;
    const bool circular;

    std::string vars() override {
        return VARS_TO_STR5(type, ne_a, pad_0, pad_1, circular);
    }

    test_pad(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne_a = {512, 512, 1, 1},
            int pad_0 = 1, int pad_1 = 1, bool circular = false)
        : type(type), ne_a(ne_a), pad_0(pad_0), pad_1(pad_1), circular(circular) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne_a.data());
        ggml_set_name(a, "a");

        ggml_tensor * out = circular
            ? ggml_pad_circular(ctx, a, pad_0, pad_1, 0, 0)
            : ggml_pad(ctx, a, pad_0, pad_1, 0, 0);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_PAD (with extension)
struct test_pad_ext : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne_a;
    const int lp0;
    const int rp0;
    const int lp1;
    const int rp1;
    const int lp2;
    const int rp2;
    const int lp3;
    const int rp3;
    const int tfrm; // 0 - none, 1 - non-cont, 2 - perm
    const bool circular;

    std::string vars() override {
        return VARS_TO_STR12(type, ne_a, lp0, rp0, lp1, rp1, lp2, rp2, lp3, rp3, tfrm, circular);
    }

    test_pad_ext(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne_a = {512, 512, 3, 1},
            int lp0 = 1, int rp0 = 1, int lp1 = 1, int rp1 = 1,
            int lp2 = 1, int rp2 = 1, int lp3 = 1, int rp3 = 1,
            int tfrm = 0, bool circular = false)
        : type(type), ne_a(ne_a), lp0(lp0), rp0(rp0), lp1(lp1), rp1(rp1), lp2(lp2), rp2(rp2), lp3(lp3), rp3(rp3),
          tfrm(tfrm), circular(circular) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne_a.data());
        ggml_set_name(a, "a");

        if (tfrm == 1) {
            a = ggml_view_4d(ctx, a, (a->ne[0] + 1) / 2, (a->ne[1] + 1) / 2, (a->ne[2] + 1) / 2, (a->ne[3] + 1) / 2, a->nb[1], a->nb[2], a->nb[3], 0);
            ggml_set_name(a, "view of a");
        } else if (tfrm == 2) {
            a = ggml_permute(ctx, a, 2, 1, 0, 3);
            ggml_set_name(a, "permuted a");
        }

        ggml_tensor * out = circular
            ? ggml_pad_ext_circular(ctx, a, lp0, rp0, lp1, rp1, lp2, rp2, lp3, rp3)
            : ggml_pad_ext         (ctx, a, lp0, rp0, lp1, rp1, lp2, rp2, lp3, rp3);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_PAD_REFLECT_1D
struct test_pad_reflect_1d : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne_a;
    const int pad_0;
    const int pad_1;

    std::string vars() override {
        return VARS_TO_STR4(type, ne_a, pad_0, pad_1);
    }

    test_pad_reflect_1d(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne_a = {512, 34, 2, 1},
            int pad_0 = 10, int pad_1 = 9)
        : type(type), ne_a(ne_a), pad_0(pad_0), pad_1(pad_1)  {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 2, ne_a.data());
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_pad_reflect_1d(ctx, a, pad_0, pad_1);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_ROLL
struct test_roll : public test_case {
    const int shift0;
    const int shift1;
    const int shift3;
    const int shift4;
    const bool permute;

    std::string vars() override {
        return VARS_TO_STR5(shift0, shift1, shift3, shift4, permute);
    }

    test_roll(int shift0 = 3, int shift1 = -2, int shift3 = 1, int shift4 = -1, bool permute = false)
        : shift0(shift0), shift1(shift1), shift3(shift3), shift4(shift4), permute(permute) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        int64_t ne[4] = {10, 5, 4, 3};
        ggml_tensor * a = ggml_new_tensor(ctx, GGML_TYPE_F32, 4, ne);
        ggml_set_name(a, "a");

        if (permute) {
            // ggml_roll only requires nb[0] == type size, so a permuted src is valid
            a = ggml_permute(ctx, a, 0, 2, 1, 3);
            ggml_set_name(a, "a_permuted");
        }

        ggml_tensor * out = ggml_roll(ctx, a, shift0, shift1, shift3, shift4);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_ARANGE
struct test_arange : public test_case {
    const ggml_type type;
    const float start;
    const float stop;
    const float step;

    std::string vars() override {
        return VARS_TO_STR4(type, start, stop, step);
    }

    test_arange(ggml_type type = GGML_TYPE_F32,
            float start = 0.f, float stop = 10.f, float step = 1.f)
        : type(type), start(start), stop(stop), step(step)  {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * out = ggml_arange(ctx, start, stop, step);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_TIMESTEP_EMBEDDING
struct test_timestep_embedding : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne_a;
    const int dim;
    const int max_period;

    std::string vars() override {
        return VARS_TO_STR4(type, ne_a, dim, max_period);
    }

    test_timestep_embedding(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne_a = {2, 1, 1, 1},
            int dim = 320, int max_period=10000)
        : type(type), ne_a(ne_a), dim(dim), max_period(max_period)  {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne_a.data());
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_timestep_embedding(ctx, a, dim, max_period);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_LEAKY_RELU
struct test_leaky_relu : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne_a;
    const float negative_slope;

    std::string vars() override {
        return VARS_TO_STR3(type, ne_a, negative_slope);
    }

    test_leaky_relu(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne_a = {10, 5, 4, 3},
            float negative_slope = 0.1f)
        : type(type), ne_a(ne_a), negative_slope(negative_slope)  {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor(ctx, type, 4, ne_a.data());
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_leaky_relu(ctx, a, negative_slope, true);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_FLASH_ATTN_EXT
struct test_flash_attn_ext : public test_case {
    const int64_t hsk; // K head size
    const int64_t hsv; // V head size
    const int64_t nh; // num heads
    const std::array<int64_t, 2> nr23; // repeat in dim 2 and 3, tests for grouped-query attention
    const int64_t kv; // kv size
    const int64_t nb; // batch size

    const bool mask; // use mask
    const bool sinks; // use sinks

    const float max_bias; // ALiBi
    const float logit_softcap; // Gemma 2

    const ggml_prec prec;
    const ggml_type type_K;
    const ggml_type type_V;
    std::array<int32_t, 4> permute;
    const bool kv_view; // create K/V as views of a larger buffer (like a KV cache)
    const bool v_is_view_of_k;
    const int64_t n_kv_max;

    std::string vars() override {
        return VARS_TO_STR17(hsk, hsv, nh, nr23, kv, nb, mask, sinks, max_bias, logit_softcap, prec, type_K, type_V, permute, kv_view, v_is_view_of_k, n_kv_max);
    }

    double max_nmse_err() override {
        return 5e-4;
    }

    uint64_t op_flops(ggml_tensor * t) override {
        GGML_UNUSED(t);
        // Just counting matmul costs:
        // Q*K^T is nb x hsk x kv, P*V is nb x kv x hsv, per head
        return (2 * nh*nr23[0] * nb * (hsk + hsv) * kv)*nr23[1];
    }

    test_flash_attn_ext(int64_t hsk = 128, int64_t hsv = 128, int64_t nh = 32, std::array<int64_t, 2> nr23 = {1, 1}, int64_t kv = 96, int64_t nb = 8,
                        bool mask = true, bool sinks = false, float max_bias = 0.0f, float logit_softcap = 0.0f, ggml_prec prec = GGML_PREC_F32,
                        ggml_type type_K = GGML_TYPE_F16, ggml_type type_V = GGML_TYPE_F16, std::array<int32_t, 4> permute = {0, 1, 2, 3},
                        bool kv_view = true, bool v_is_view_of_k = false, int64_t n_kv_max = 0)
        : hsk(hsk), hsv(hsv), nh(nh), nr23(nr23), kv(kv), nb(nb), mask(mask), sinks(sinks), max_bias(max_bias), logit_softcap(logit_softcap), prec(prec),
          type_K(type_K), type_V(type_V), permute(permute), kv_view(kv_view), v_is_view_of_k(v_is_view_of_k), n_kv_max(n_kv_max) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        const int64_t hsk_padded = GGML_PAD(hsk, ggml_blck_size(type_K));
        const int64_t hsv_padded = GGML_PAD(hsv, ggml_blck_size(type_V));

        auto const &create_permuted = [&](ggml_type type, int64_t ne0, int64_t ne1, int64_t ne2, int64_t ne3, bool is_view) -> ggml_tensor * {
            int64_t ne[4] = {ne0, ne1, ne2, ne3};
            int64_t ne_perm[4];
            for (int i = 0; i < 4; ++i) {
                ne_perm[permute[i]] = ne[i];
            }
            ggml_tensor * t;
            if (is_view) {
                ggml_tensor * t0 = ggml_new_tensor_4d(ctx, type, ne_perm[0], 2*ne_perm[1], ne_perm[2], ne_perm[3]);
                t = ggml_view_4d(ctx, t0, ne_perm[0], ne_perm[1], ne_perm[2], ne_perm[3], t0->nb[1], t0->nb[2], t0->nb[3], 0);
            } else {
                t = ggml_new_tensor_4d(ctx, type, ne_perm[0], ne_perm[1], ne_perm[2], ne_perm[3]);
            }
            if (permute != std::array<int32_t, 4>{0, 1, 2, 3}) {
                t = ggml_permute(ctx, t, permute[0], permute[1], permute[2], permute[3]);
            }
            return t;
        };

        ggml_tensor * q = create_permuted(GGML_TYPE_F32, hsk_padded, nb, nh*nr23[0], nr23[1], false);
        ggml_set_name(q, "q");

        ggml_tensor * k = create_permuted(type_K,        hsk_padded, kv, nh,         nr23[1], kv_view); // the K tensor is usually a view of the K cache
        ggml_set_name(k, "k");

        ggml_tensor * v = nullptr;
        if (v_is_view_of_k) {
            // the V cache is a sub-view of the K cache. this is used by some MLA-based models
            // for more info:
            //   - https://github.com/ggml-org/llama.cpp/pull/13435
            //   - https://github.com/ggml-org/llama.cpp/pull/18953#issuecomment-3774948392
            //   - https://github.com/ggml-org/llama.cpp/pull/18986
            GGML_ASSERT(type_K == type_V && hsv_padded <= hsk_padded);

            v = ggml_view_4d(ctx, k, hsv_padded, kv, nh, nr23[1], k->nb[1], k->nb[2], k->nb[3], 0);
        } else {
            v = create_permuted(type_V,        hsv_padded, kv, nh,         nr23[1], kv_view); // the V tensor is usually a view of the V cache
        }
        ggml_set_name(v, "v");

        ggml_tensor * m = nullptr;
        if (mask) {
            m = ggml_new_tensor_4d(ctx, GGML_TYPE_F16, kv, nb, 1, nr23[1]);
            ggml_set_name(m, "m");
        }

        ggml_tensor * s = nullptr;
        if (sinks) {
            s = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, q->ne[2]);
            ggml_set_name(s, "s");
        }

        ggml_tensor * out = ggml_flash_attn_ext(ctx, q, k, v, m, 1.0f/sqrtf(hsk), max_bias, logit_softcap);
        ggml_flash_attn_ext_add_sinks(out, s);
        ggml_flash_attn_ext_set_n_kv_max(out, n_kv_max);
        ggml_prec_set_acc(out, prec);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (strcmp(t->name, "s") == 0) {
                // make the sink values more noticeable in order to trigger a test failure when the implementation is wrong
                init_tensor_uniform(t, -10.0f, 10.0f);
            } else if (strcmp(t->name, "m") == 0) {
                if (n_kv_max > 0) {
                    init_tensor_kq_mask_sparse(t, n_kv_max);
                } else {
                    init_tensor_kq_mask(t);
                }
            } else {
                init_tensor_uniform(t);
            }
        }
    }

    bool grad_precise() override {
        return true;
    }
};

// GGML_OP_CROSS_ENTROPY_LOSS
struct test_cross_entropy_loss : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;

    std::string vars() override {
        return VARS_TO_STR2(type, ne);
    }

    test_cross_entropy_loss(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 5, 4, 3})
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * logits = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_param(logits);
        ggml_set_name(logits, "logits");

        ggml_tensor * labels = ggml_new_tensor(ctx, type, 4, ne.data());
        // The labels are assumed to be constant -> no gradients.
        ggml_set_name(labels, "labels");

        // Ensure labels add up to 1:
        labels = ggml_soft_max(ctx, labels);
        ggml_set_name(labels, "labels_normalized");

        ggml_tensor * out = ggml_cross_entropy_loss(ctx, logits, labels);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        // For larger abs. diffs between logits softmax is more linear, therefore more precise num. gradients.
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, -100.0f, 100.0f);
        }
    }

    float grad_eps() override {
        return 1.0f;
    }

    bool grad_precise() override {
        return true;
    }
};

// GGML_OP_CROSS_ENTROPY_LOSS_BACK
struct test_cross_entropy_loss_back : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;

    std::string vars() override {
        return VARS_TO_STR2(type, ne);
    }

    test_cross_entropy_loss_back(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 5, 4, 3})
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * grad = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 1);
        ggml_set_name(grad, "grad");

        ggml_tensor * logits = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(logits, "logits");

        ggml_tensor * labels = ggml_new_tensor(ctx, type, 4, ne.data());
        ggml_set_name(labels, "labels");

        // Ensure labels add up to 1:
        labels = ggml_soft_max(ctx, labels);
        ggml_set_name(labels, "labels_normalized");

        ggml_tensor * out = ggml_cross_entropy_loss_back(ctx, grad, logits, labels);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_OPT_STEP_ADAMW
struct test_opt_step_adamw : public test_case {
    const ggml_type type;
    const std::array<int64_t, 4> ne;

    std::string vars() override {
        return VARS_TO_STR2(type, ne);
    }

    test_opt_step_adamw(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = {10, 5, 4, 3})
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor_4d(ctx, type, ne[0], ne[1], ne[2], ne[3]);
        ggml_set_param(a); // Despite tensor a having gradients the output tensor will not.
        ggml_set_name(a, "a");

        ggml_tensor * grad = ggml_new_tensor_4d(ctx, type, ne[0], ne[1], ne[2], ne[3]);
        ggml_set_name(grad, "grad");

        ggml_tensor * grad_m = ggml_new_tensor_4d(ctx, type, ne[0], ne[1], ne[2], ne[3]);
        ggml_set_name(grad_m, "grad_m");

        ggml_tensor * grad_v = ggml_new_tensor_4d(ctx, type, ne[0], ne[1], ne[2], ne[3]);
        ggml_set_name(grad_v, "grad_v");

        ggml_tensor * adamw_params = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 7);
        ggml_set_name(adamw_params, "adamw_params");

        ggml_tensor * out = ggml_opt_step_adamw(ctx, a, grad, grad_m, grad_v, adamw_params);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, 0.0f, 1.0f); // grad_v and adamw_params need non-negative values.
        }
    }

    bool grad_precise() override {
        return true;
    }
};

// GGML_OP_OPT_STEP_SGD
struct test_opt_step_sgd : public test_case {
    const ggml_type              type;
    const std::array<int64_t, 4> ne;

    std::string vars() override { return VARS_TO_STR2(type, ne); }

    test_opt_step_sgd(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = { 10, 5, 4, 3 })
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor_4d(ctx, type, ne[0], ne[1], ne[2], ne[3]);
        ggml_set_param(a);  // Despite tensor a having gradients the output tensor will not.
        ggml_set_name(a, "a");

        ggml_tensor * grad = ggml_new_tensor_4d(ctx, type, ne[0], ne[1], ne[2], ne[3]);
        ggml_set_name(grad, "grad");

        ggml_tensor * sgd_params = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, 2);
        ggml_set_name(sgd_params, "sgd_params");

        ggml_tensor * out = ggml_opt_step_sgd(ctx, a, grad, sgd_params);

        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, 0.0f, 1.0f);  // sgd_params need non-negative values.
        }
    }

    bool grad_precise() override {
        return true;
    }
};

// GGML_OP_CUMSUM
struct test_cumsum : public test_case {
    const ggml_type              type;
    const std::array<int64_t, 4> ne;

    std::string vars() override { return VARS_TO_STR2(type, ne); }

    test_cumsum(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = { 10, 5, 4, 3 })
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor_4d(ctx, type, ne[0], ne[1], ne[2], ne[3]);
        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_cumsum(ctx, a);

        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, -1.0f, 1.0f);
        }
    }
};

// GGML_OP_XIELU
struct test_xielu : public test_case {
    const ggml_type              type;
    const std::array<int64_t, 4> ne;

    std::string vars() override { return VARS_TO_STR2(type, ne); }

    test_xielu(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = { 10, 5, 4, 3 })
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor_4d(ctx, type, ne[0], ne[1], ne[2], ne[3]);
        ggml_set_param(a);
        ggml_set_name(a, "a");

        float alpha_n = 4.0f;
        float alpha_p = 20.0f;
        float beta = 0.5f;
        float eps = 0.0000001f;

        ggml_tensor * out = ggml_xielu(ctx, a, alpha_n, alpha_p, beta, eps);

        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, -1.0f, 1.0f);
        }
    }
};

// GGML_OP_TRI
struct test_tri : public test_case {
    const ggml_type              type;
    const std::array<int64_t, 4> ne;
    const ggml_tri_type          tri_type;

    std::string vars() override { return VARS_TO_STR3(type, ne, tri_type); }

    test_tri(ggml_tri_type tri_type, ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = { 10, 10, 4, 3 })
        : type(type), ne(ne), tri_type(tri_type) {
            GGML_ASSERT(ne[0] == ne[1]);
        }

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor_4d(ctx, type, ne[0], ne[1], ne[2], ne[3]);
        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_tri(ctx, a, tri_type);

        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            init_tensor_uniform(t, -1.0f, 1.0f);
        }
    }
};

// GGML_OP_FILL
struct test_fill : public test_case {
    const ggml_type              type;
    const std::array<int64_t, 4> ne;
    float                        c;

    std::string vars() override { return VARS_TO_STR3(type, ne, c); }

    test_fill(float c, ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = { 10, 10, 4, 3 })
        : type(type), ne(ne), c(c) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor_4d(ctx, type, ne[0], ne[1], ne[2], ne[3]);
        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_fill(ctx, a, c);

        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_SOLVE_TRI
struct test_solve_tri : public test_case {
    const ggml_type              type;
    const std::array<int64_t, 4> ne_lhs;
    const std::array<int64_t, 4> ne_rhs;

    std::string vars() override { return VARS_TO_STR3(type, ne_lhs, ne_rhs); }

    uint64_t op_flops(ggml_tensor * t) override {
        GGML_UNUSED(t);
        int64_t n = ne_lhs[0];
        int64_t k = ne_rhs[0];
        int64_t batch = ne_lhs[2] * ne_lhs[3];
        // n * (n + 1) / 2 non-zero elements of lhs, 2 flops each, for each col of rhs
        return n * (n + 1) * k * batch;
    }

    test_solve_tri(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne_lhs = { 10, 10, 4, 3 },
            std::array<int64_t, 4> ne_rhs = { 3, 10, 4, 3 }
        )
        : type(type), ne_lhs(ne_lhs), ne_rhs(ne_rhs) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * a = ggml_new_tensor_4d(ctx, type, ne_lhs[0], ne_lhs[1], ne_lhs[2], ne_lhs[3]);
        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * b = ggml_new_tensor_4d(ctx, type, ne_rhs[0], ne_rhs[1], ne_rhs[2], ne_rhs[3]);
        ggml_set_param(b);
        ggml_set_name(b, "b");

        ggml_tensor * out = ggml_solve_tri(ctx, a, b, true, true, false);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (strcmp(t->name, "a") == 0) {
                // note: avoid zeros in the diagonal
                init_tensor_tril(t, 0.1, 1.0f);
            } else {
                init_tensor_uniform(t, -1.0f, 1.0f);
            }
        }
    }
};

// GGML_OP_DIAG
struct test_diag : public test_case {
    const ggml_type              type;
    const std::array<int64_t, 4> ne;

    std::string vars() override { return VARS_TO_STR2(type, ne); }

    test_diag(ggml_type type = GGML_TYPE_F32,
            std::array<int64_t, 4> ne = { 10, 1, 4, 3 })
        : type(type), ne(ne) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        GGML_ASSERT(ne[1] == 1);
        ggml_tensor * a = ggml_new_tensor_4d(ctx, type, ne[0], ne[1], ne[2], ne[3]);
        ggml_set_param(a);
        ggml_set_name(a, "a");

        ggml_tensor * out = ggml_diag(ctx, a);
        ggml_set_name(out, "out");

        return out;
    }
};

// GGML_OP_LIGHTNING_INDEXER
struct test_lightning_indexer : public test_case {
    const int64_t hsk; // indexer K head size
    const int64_t nh; // num indexer heads
    const int64_t kv; // kv size
    const int64_t nb; // batch size
    const int64_t ns; // num streams
    const int64_t nm; // ne[3] of mask

    const ggml_type type_K;

    std::string vars() override {
        return VARS_TO_STR7(hsk, nh, kv, nb, ns, nm, type_K);
    }

    double max_nmse_err() override {
        return 1e-6;
    }

    uint64_t op_flops(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return ((2 * hsk + 2) * nh + 1) * kv * nb * ns;
    }

    test_lightning_indexer(int64_t hsk = 128, int64_t nh = 64, int64_t kv = 256, int64_t nb = 128, int64_t ns = 1, int64_t nm = 1, ggml_type type_K = GGML_TYPE_F16)
        : hsk(hsk), nh(nh), kv(kv), nb(nb), ns(ns), nm(nm), type_K(type_K) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        ggml_tensor * q = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, hsk, nh, nb, ns);
        ggml_set_param(q);
        ggml_set_name(q, "q");

        ggml_tensor * k = ggml_new_tensor_4d(ctx, type_K, hsk, 1, kv, ns);
        ggml_set_param(k);
        ggml_set_name(k, "k");

        ggml_tensor * w = ggml_new_tensor_4d(ctx, GGML_TYPE_F32, nh, nb, 1, ns);
        ggml_set_param(w);
        ggml_set_name(w, "w");

        ggml_tensor * m = ggml_new_tensor_4d(ctx, GGML_TYPE_F16, kv, nb, 1, nm);
        ggml_set_param(m);
        ggml_set_name(m, "m");

        ggml_tensor * out = ggml_lightning_indexer(ctx, q, k, w, m);
        ggml_set_name(out, "out");

        return out;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (strcmp(t->name, "m") == 0) {
                init_tensor_kq_mask(t);
            } else {
                init_tensor_uniform(t);
            }
        }
    }
};

// Deserializable generic test case
struct input_tensor {
    ggml_type type;
    std::array<int64_t, 4> ne;
    std::array<size_t, 4> nb; // strides (0 = use default contiguous strides)
};

static bool is_non_contiguous(const input_tensor & src) {
    if (src.nb[0] == 0) {
        return false;
    }
    const size_t default_nb0 = ggml_type_size(src.type);
    const size_t default_nb1 = default_nb0 * (src.ne[0] / ggml_blck_size(src.type));
    const size_t default_nb2 = default_nb1 * src.ne[1];
    const size_t default_nb3 = default_nb2 * src.ne[2];
    return src.nb[0] != default_nb0 ||
           src.nb[1] != default_nb1 ||
           src.nb[2] != default_nb2 ||
           src.nb[3] != default_nb3;
}

static std::string var_to_str(const std::vector<input_tensor>& sources) {
    std::ostringstream oss;
    bool first = true;
    for (const auto& src : sources) {
        if (!first) oss << ",";
        oss << ggml_type_name(src.type) << "[" << src.ne[0] << "," << src.ne[1] << "," << src.ne[2] << "," << src.ne[3] << "]";
        if (is_non_contiguous(src)) {
            oss << "nb[" << src.nb[0] << "," << src.nb[1] << "," << src.nb[2] << "," << src.nb[3] << "]";
        }
        first = false;
    }
    return oss.str();
}

static std::string var_to_str(const std::array<int32_t, GGML_MAX_OP_PARAMS / sizeof(int32_t)>& params) {
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (size_t i = 0; i < params.size(); ++i) {
        if (params[i] != 0) {
            if (!first) oss << ",";
            oss << i << ":" << params[i];
            first = false;
        }
    }
    oss << "]";
    return oss.str();
}


struct test_generic_op : public test_case {
    const ggml_op op;
    const ggml_type type;
    const std::array<int64_t, 4> ne;
    const std::array<int32_t, GGML_MAX_OP_PARAMS / sizeof(int32_t)> op_params;

    const std::vector<input_tensor> sources;
    const std::string name;

    std::string vars() override {
        if (name.empty()) {
            return VARS_TO_STR4(type, ne, op_params, sources);
        }

        return VARS_TO_STR5(name, type, ne, op_params, sources);
    }

    test_generic_op(ggml_op op, ggml_type type, std::array<int64_t, 4> ne,
                    std::array<int32_t, GGML_MAX_OP_PARAMS / sizeof(int32_t)> op_params,
                    std::vector<input_tensor> sources, std::string name = "")
        : op(op), type(type), ne(ne), op_params(op_params), sources(sources), name(std::move(name)) {}

    ggml_tensor * build_graph(ggml_context * ctx) override {
        const size_t source_count = std::min(sources.size(), (size_t)GGML_MAX_SRC);

        std::array<ggml_tensor *, GGML_MAX_SRC> source_tensors;
        for (size_t i = 0; i < source_count; ++i) {
            const input_tensor& src = sources[i];

            if (is_non_contiguous(src)) {
                size_t total_size;
                const size_t blck_size = ggml_blck_size(src.type);
                if (blck_size == 1) {
                    total_size = ggml_type_size(src.type);
                    for (int d = 0; d < 4; d++) {
                        total_size += (src.ne[d] - 1) * src.nb[d];
                    }
                } else {
                    total_size = src.ne[0] * src.nb[0] / blck_size;
                    for (int d = 1; d < 4; d++) {
                        total_size += (src.ne[d] - 1) * src.nb[d];
                    }
                }

                // Convert bytes to elements, padded to block size for quantized types
                const size_t type_size = ggml_type_size(src.type);
                size_t backing_elements = (total_size * blck_size + type_size - 1) / type_size;
                backing_elements = ((backing_elements + blck_size - 1) / blck_size) * blck_size;
                ggml_tensor * backing = ggml_new_tensor_1d(ctx, src.type, backing_elements);
                source_tensors[i] = ggml_view_4d(ctx, backing,
                    src.ne[0], src.ne[1], src.ne[2], src.ne[3],
                    src.nb[1], src.nb[2], src.nb[3], 0);
                // nb[0] does not get set by view_4d, so set it manually
                source_tensors[i]->nb[0] = src.nb[0];
            } else {
                source_tensors[i] = ggml_new_tensor_4d(ctx, src.type, src.ne[0], src.ne[1], src.ne[2], src.ne[3]);
            }
        }

        // Ops with an inplace flag create a view of src[0] as their output.
        bool inplace = false;
        if (op == GGML_OP_SET || op == GGML_OP_ACC) {
            inplace = op_params[4] != 0;
        } else if (op == GGML_OP_ADD_REL_POS) {
            inplace = op_params[0] != 0;
        }

        ggml_tensor * out;
        if (inplace && source_count > 0) {
            out = ggml_view_tensor(ctx, source_tensors[0]);
        } else {
            out = ggml_new_tensor_4d(ctx, type, ne[0], ne[1], ne[2], ne[3]);
        }
        out->op = op;
        for (size_t i = 0; i < source_count; ++i) {
            out->src[i] = source_tensors[i];
        }

        memcpy(out->op_params, op_params.data(), GGML_MAX_OP_PARAMS);
        ggml_set_name(out, "out");

        return out;
    }

    double max_nmse_err() override {
        switch (op) {
        case GGML_OP_MUL_MAT:
        case GGML_OP_MUL_MAT_ID:
        case GGML_OP_OUT_PROD:
        case GGML_OP_CONV_TRANSPOSE_2D:
        case GGML_OP_IM2COL:
        case GGML_OP_CONV_2D:
        case GGML_OP_CONV_3D:
        case GGML_OP_SET_ROWS:
        case GGML_OP_CPY:
            return 5e-4;
        case GGML_OP_SOFT_MAX:
            return 1e-6;
        case GGML_OP_RWKV_WKV7:
            return 5e-3;
        case GGML_OP_FLASH_ATTN_EXT:
        {
            // Scale error with kv length to account for accumulating floating point error
            const int64_t kv = sources[1].ne[1];
            return 5e-4 * std::max(1.0, kv / 20000.0);
        }
        default:
            return 1e-7;
        }
    }

    void initialize_tensors(ggml_context * ctx) override {
        ggml_tensor * out = ggml_get_tensor(ctx, "out");

        std::random_device rd;
        std::default_random_engine rng(rd());

        for (size_t i = 0; i < sources.size() && i < GGML_MAX_SRC; i++) {
            ggml_tensor * t = out->src[i];
            if (!t) {
                break;
            }

            // FLASH_ATTN_EXT: src[3] is the KQ mask
            if (op == GGML_OP_FLASH_ATTN_EXT && i == 3) {
                init_tensor_kq_mask(t);
                continue;
            }

            if (t->type == GGML_TYPE_I32 || t->type == GGML_TYPE_I64) {
                if (op == GGML_OP_GET_ROWS || op == GGML_OP_GET_ROWS_BACK) {
                    const int64_t num_rows = sources[0].ne[1];
                    const int64_t nels = ggml_nelements(t);
                    std::vector<int32_t> data(nels);
                    std::uniform_int_distribution<int32_t> dist(0, num_rows - 1);
                    for (int64_t i = 0; i < nels; i++) {
                        data[i] = dist(rng);
                    }
                    ggml_backend_tensor_set(t, data.data(), 0, nels * sizeof(int32_t));
                } else if (op == GGML_OP_SET_ROWS) {
                    init_set_rows_row_ids(t, ne[1]);
                } else if (op == GGML_OP_ROPE) {
                    const int mode = op_params[2];
                    const int64_t nels = (mode & GGML_ROPE_TYPE_MROPE) ? ne[2] * 4 : ne[2];
                    std::vector<int32_t> data(nels);
                    std::uniform_int_distribution<int32_t> dist(0, ne[2] - 1);
                    for (int64_t i = 0; i < nels; i++) {
                        data[i] = dist(rng);
                    }
                    ggml_backend_tensor_set(t, data.data(), 0, nels * sizeof(int32_t));
                } else if (op == GGML_OP_MUL_MAT_ID || op == GGML_OP_ADD_ID) {
                    const int64_t n_expert = (op == GGML_OP_MUL_MAT_ID) ? sources[0].ne[2] : sources[1].ne[1];
                    for (int64_t r = 0; r < ggml_nrows(t); r++) {
                        std::vector<int32_t> data(t->ne[0]);
                        for (int32_t i = 0; i < t->ne[0]; i++) {
                            data[i] = i % n_expert;
                        }
                        std::shuffle(data.begin(), data.end(), rng);
                        ggml_backend_tensor_set(t, data.data(), r * t->nb[1], t->ne[0] * sizeof(int32_t));
                    }
                } else if (op == GGML_OP_SSM_SCAN) {
                    for (int64_t r = 0; r < ggml_nrows(t); r++) {
                        std::vector<int32_t> data(t->ne[0]);
                        for (int32_t i = 0; i < t->ne[0]; i++) {
                            data[i] = i;
                        }
                        std::shuffle(data.begin(), data.end(), rng);
                        ggml_backend_tensor_set(t, data.data(), r * t->nb[1], t->ne[0] * sizeof(int32_t));
                    }
                } else {
                    init_tensor_uniform(t);
                }
            } else {
                init_tensor_uniform(t);
            }
        }
    }
};


enum llm_norm_type {
    LLM_NORM,
    LLM_NORM_RMS,
};

struct llama_hparams {
    uint32_t n_vocab;
    uint32_t n_embd;
    uint32_t n_head;
    uint32_t n_head_kv;
    static constexpr uint32_t n_layer = 1;
    uint32_t n_rot;
    uint32_t n_embd_head; // dimension of values (d_v)
    uint32_t n_ff;

    float f_norm_eps;
    float f_norm_rms_eps;

    // cparams
    static constexpr uint32_t n_ctx = 512; // user-specified context size
    static constexpr uint32_t n_ctx_orig = n_ctx;

    // batch
    int32_t n_tokens;

    // llm_build_context
    static constexpr int32_t n_kv    = 32; // size of KV cache to consider (n_kv <= n_ctx
    static constexpr int32_t kv_head = 1;  // index of where we store new KV data in the cache

    uint32_t n_embd_gqa() const { // dimension of key embeddings across all k-v heads
        return n_embd_head * n_head_kv;
    }
};

// LLM base class
struct test_llm : public test_case {
    llama_hparams hp;

protected:
    test_llm(llama_hparams hp)
        : hp(std::move(hp)) {
    }

public:
    struct ggml_tensor * llm_build_norm(
            struct ggml_context * ctx,
             struct ggml_tensor * cur,
             struct ggml_tensor * mw,
             struct ggml_tensor * mb,
                  llm_norm_type   type) {
        switch (type) {
            case LLM_NORM:     cur = ggml_norm    (ctx, cur, hp.f_norm_eps); break;
            case LLM_NORM_RMS: cur = ggml_rms_norm(ctx, cur, hp.f_norm_rms_eps); break;
        }
        cur = ggml_mul(ctx, cur, mw);
        if (mb) {
            cur = ggml_add(ctx, cur, mb);
        }
        return cur;
    }

    void llm_build_kv_store(
            struct ggml_context * ctx,
             struct ggml_tensor * k_l,
             struct ggml_tensor * v_l,
             struct ggml_tensor * k_cur,
             struct ggml_tensor * v_cur) {
        // compute the transposed [n_tokens, n_embd] V matrix
        struct ggml_tensor * v_cur_t = ggml_transpose(ctx, ggml_reshape_2d(ctx, v_cur, hp.n_embd_gqa(), hp.n_tokens));

        struct ggml_tensor * k_cache_view = ggml_view_1d(ctx, k_l, hp.n_tokens*hp.n_embd_gqa(),
                (ggml_row_size(k_l->type, hp.n_embd_gqa()))*hp.kv_head);

        struct ggml_tensor * v_cache_view = ggml_view_2d(ctx, v_l, hp.n_tokens, hp.n_embd_gqa(),
                (  hp.n_ctx)*ggml_element_size(v_l),
                (hp.kv_head)*ggml_element_size(v_l));

        // important: storing RoPE-ed version of K in the KV cache!
        ggml_cpy(ctx, k_cur,   k_cache_view);
        ggml_cpy(ctx, v_cur_t, v_cache_view);
    }

    struct ggml_tensor * llm_build_kqv(
            struct ggml_context * ctx,
             struct ggml_tensor * k_l,
             struct ggml_tensor * v_l,
             struct ggml_tensor * q_cur,
             struct ggml_tensor * kq_mask,
                        float     kq_scale) {
        struct ggml_tensor * q = ggml_permute(ctx, q_cur, 0, 2, 1, 3);

        struct ggml_tensor * k =
            ggml_view_3d(ctx, k_l,
                    hp.n_embd_head, hp.n_kv, hp.n_head_kv,
                    ggml_row_size(k_l->type, hp.n_embd_gqa()),
                    ggml_row_size(k_l->type, hp.n_embd_head),
                    0);

        struct ggml_tensor * kq = ggml_mul_mat(ctx, k, q);

        kq = ggml_soft_max_ext(ctx, kq, kq_mask, kq_scale, 0.0f);

        // split cached v into n_head heads
        struct ggml_tensor * v =
            ggml_view_3d(ctx, v_l,
                    hp.n_kv, hp.n_embd_head, hp.n_head_kv,
                    ggml_element_size(v_l)*hp.n_ctx,
                    ggml_element_size(v_l)*hp.n_ctx*hp.n_embd_head,
                    0);

        struct ggml_tensor * kqv = ggml_mul_mat(ctx, v, kq);

        struct ggml_tensor * kqv_merged = ggml_permute(ctx, kqv, 0, 2, 1, 3);

        struct ggml_tensor * cur = ggml_cont_2d(ctx, kqv_merged, hp.n_embd_head*hp.n_head, hp.n_tokens);

        struct ggml_tensor * wo = ggml_new_tensor_2d(ctx, GGML_TYPE_Q4_0, hp.n_embd, hp.n_embd);
        cur = ggml_mul_mat(ctx, wo, cur);

        return cur;
    }

    void initialize_tensors(ggml_context * ctx) override {
        for (ggml_tensor * t = ggml_get_first_tensor(ctx); t != NULL; t = ggml_get_next_tensor(ctx, t)) {
            if (t->type == GGML_TYPE_I32) {
                // pos
                std::vector<int> data(hp.n_tokens);
                for (int i = 0; i < hp.n_tokens; i++) {
                    data[i] = rand() % hp.n_ctx;
                }
                ggml_backend_tensor_set(t, data.data(), 0, hp.n_tokens * sizeof(int));
            } else {
                init_tensor_uniform(t);
            }
        }
    }
};

// Llama
struct test_llama : public test_llm {
    static constexpr float freq_base = 10000.0f;
    static constexpr float freq_scale = 1.0f;
    static constexpr float ext_factor = 0.0f;
    static constexpr float attn_factor = 1.0f;
    static constexpr float beta_fast = 32.0f;
    static constexpr float beta_slow = 1.0f;
    bool fused;

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "LLAMA";
    }

    std::string vars() override {
        auto n_tokens = hp.n_tokens;
        return VARS_TO_STR1(n_tokens);
    }

    double max_nmse_err() override {
        return 2e-3;
    }

    bool run_whole_graph() override { return fused; }

    test_llama(int n_tokens = 1, bool fused = false)
        : test_llm({
            /*n_vocab        =*/ 32000,
            /*n_embd         =*/ 3200,
            /*n_head         =*/ 32,
            /*n_head_kv      =*/ 32,
            /*n_rot          =*/ 100,
            /*n_embd_head    =*/ 100,
            /*n_ff           =*/ 8640,
            /*f_norm_eps     =*/ 0.f,
            /*f_norm_rms_eps =*/ 1e-5f,
            /*n_tokens       =*/ n_tokens,
        })
        , fused(fused)
    {
    }

    ggml_tensor * build_graph(ggml_context * ctx) override {
        struct ggml_tensor * cur;
        struct ggml_tensor * inpL;

        inpL = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, hp.n_embd, hp.n_tokens);

        // inp_pos - contains the positions
        struct ggml_tensor * inp_pos = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, hp.n_tokens);

        // KQ_mask (mask for 1 head, it will be broadcasted to all heads)
        struct ggml_tensor * KQ_mask = ggml_new_tensor_3d(ctx, GGML_TYPE_F16, hp.n_kv, hp.n_tokens, 1);

        ggml_tensor * k_l = ggml_new_tensor_1d(ctx, GGML_TYPE_F16, 1638400);
        ggml_tensor * v_l = ggml_new_tensor_1d(ctx, GGML_TYPE_F16, 1638400);

        for (uint32_t il = 0; il < hp.n_layer; ++il) {
            struct ggml_tensor * inpSA = inpL;

            // norm
            ggml_tensor * attn_norm = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, hp.n_embd);
            cur = llm_build_norm(ctx, inpL, attn_norm, nullptr, LLM_NORM_RMS);

            // self-attention
            {
                ggml_tensor * wq = ggml_new_tensor_2d(ctx, GGML_TYPE_Q4_0, hp.n_embd, hp.n_embd);
                ggml_tensor * wk = ggml_new_tensor_2d(ctx, GGML_TYPE_Q4_0, hp.n_embd, hp.n_embd_gqa());
                ggml_tensor * wv = ggml_new_tensor_2d(ctx, GGML_TYPE_Q4_0, hp.n_embd, hp.n_embd_gqa());

                // compute Q and K and RoPE them
                struct ggml_tensor * Qcur = ggml_mul_mat(ctx, wq, cur);
                struct ggml_tensor * Kcur = ggml_mul_mat(ctx, wk, cur);
                struct ggml_tensor * Vcur = ggml_mul_mat(ctx, wv, cur);

                Qcur = ggml_rope_ext(
                    ctx, ggml_reshape_3d(ctx, Qcur, hp.n_embd_head, hp.n_head,    hp.n_tokens), inp_pos, nullptr,
                    hp.n_rot, 0, hp.n_ctx_orig, freq_base, freq_scale,
                    ext_factor, attn_factor, beta_fast, beta_slow
                );

                Kcur = ggml_rope_ext(
                    ctx, ggml_reshape_3d(ctx, Kcur, hp.n_embd_head, hp.n_head_kv, hp.n_tokens), inp_pos, nullptr,
                    hp.n_rot, 0, hp.n_ctx_orig, freq_base, freq_scale,
                    ext_factor, attn_factor, beta_fast, beta_slow
                );

                llm_build_kv_store(ctx, k_l, v_l, Kcur, Vcur);

                cur = llm_build_kqv(ctx, k_l, v_l, Qcur, KQ_mask, 1.0f/sqrtf(float(hp.n_embd_head)));
            }

            struct ggml_tensor * ffn_inp = ggml_add(ctx, cur, inpSA);

            // feed-forward network
            ggml_tensor * ffn_norm = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, hp.n_embd);
            cur = llm_build_norm(ctx, ffn_inp, ffn_norm, nullptr, LLM_NORM_RMS);

            ggml_tensor * ffn_gate = ggml_new_tensor_2d(ctx, GGML_TYPE_Q4_0, hp.n_embd, hp.n_ff);
            ggml_tensor * ffn_down = ggml_new_tensor_2d(ctx, GGML_TYPE_Q4_0, hp.n_ff,   hp.n_embd);
            ggml_tensor * ffn_up   = ggml_new_tensor_2d(ctx, GGML_TYPE_Q4_0, hp.n_embd, hp.n_ff);
            struct ggml_tensor * tmp = ggml_mul_mat(ctx, ffn_up, cur);
            cur = ggml_mul_mat(ctx, ffn_gate, cur);
            cur = ggml_silu(ctx, cur);
            cur = ggml_mul(ctx, cur, tmp);
            cur = ggml_mul_mat(ctx, ffn_down, cur);

            cur = ggml_add(ctx, cur, ffn_inp);

            // input for next layer
            inpL = cur;
        }

        cur = inpL;

        ggml_tensor * output_norm = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, hp.n_embd);
        cur = llm_build_norm(ctx, cur, output_norm, nullptr, LLM_NORM_RMS);

        // lm_head
        ggml_tensor * output = ggml_new_tensor_2d(ctx, GGML_TYPE_Q4_0, hp.n_embd, hp.n_vocab);
        cur = ggml_mul_mat(ctx, output, cur);

        return cur;
    }
};

// Falcon
struct test_falcon : public test_llm {
    static constexpr float freq_base = 10000.0f;
    static constexpr float freq_scale = 1.0f;
    static constexpr float ext_factor = 0.0f;
    static constexpr float attn_factor = 1.0f;
    static constexpr float beta_fast = 32.0f;
    static constexpr float beta_slow = 1.0f;

    std::string op_desc(ggml_tensor * t) override {
        GGML_UNUSED(t);
        return "FALCON";
    }

    std::string vars() override {
        auto n_tokens = hp.n_tokens;
        return VARS_TO_STR1(n_tokens);
    }

    double max_nmse_err() override {
        return 2e-3;
    }

    test_falcon(int n_tokens = 1)
        : test_llm({
            /*n_vocab        =*/ 32000,
            /*n_embd         =*/ 3200,
            /*n_head         =*/ 50,
            /*n_head_kv      =*/ 1,
            /*n_rot          =*/ 64,
            /*n_embd_head    =*/ 64,
            /*n_ff           =*/ 8640,
            /*f_norm_eps     =*/ 1e-5f,
            /*f_norm_rms_eps =*/ 0.f,
            /*n_tokens       =*/ n_tokens,
        }) {
    }

    ggml_tensor * build_graph(ggml_context * ctx) override {
        struct ggml_tensor * cur;
        struct ggml_tensor * inpL;

        inpL = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, hp.n_embd, hp.n_tokens);

        // inp_pos - contains the positions
        struct ggml_tensor * inp_pos = ggml_new_tensor_1d(ctx, GGML_TYPE_I32, hp.n_tokens);

        // KQ_mask (mask for 1 head, it will be broadcasted to all heads)
        struct ggml_tensor * KQ_mask = ggml_new_tensor_3d(ctx, GGML_TYPE_F16, hp.n_kv, hp.n_tokens, 1);

        ggml_tensor * k_l = ggml_new_tensor_1d(ctx, GGML_TYPE_F16, 1638400);
        ggml_tensor * v_l = ggml_new_tensor_1d(ctx, GGML_TYPE_F16, 1638400);

        for (uint32_t il = 0; il < hp.n_layer; ++il) {
            // norm
            ggml_tensor * attn_norm_w = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, hp.n_embd);
            ggml_tensor * attn_norm_b = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, hp.n_embd);
            ggml_tensor * attn_norm = llm_build_norm(ctx, inpL, attn_norm_w, attn_norm_b, LLM_NORM);

            // self-attention
            {
                cur = attn_norm;

                ggml_tensor * wqkv = ggml_new_tensor_2d(ctx, GGML_TYPE_Q4_0, hp.n_embd, hp.n_embd + 2*hp.n_embd_gqa());

                cur = ggml_mul_mat(ctx, wqkv, cur);

                struct ggml_tensor * Qcur = ggml_cont(ctx, ggml_view_2d(ctx, cur, hp.n_embd,     hp.n_tokens, cur->nb[1], 0*sizeof(float)*(hp.n_embd)));
                struct ggml_tensor * Kcur = ggml_cont(ctx, ggml_view_2d(ctx, cur, hp.n_embd_gqa(), hp.n_tokens, cur->nb[1], 1*sizeof(float)*(hp.n_embd)));
                struct ggml_tensor * Vcur = ggml_cont(ctx, ggml_view_2d(ctx, cur, hp.n_embd_gqa(), hp.n_tokens, cur->nb[1], 1*sizeof(float)*(hp.n_embd + hp.n_embd_gqa())));

                Qcur = ggml_reshape_3d(ctx, Qcur, hp.n_embd_head, hp.n_head,    hp.n_tokens);
                Kcur = ggml_reshape_3d(ctx, Kcur, hp.n_embd_head, hp.n_head_kv, hp.n_tokens);

                // using mode = 2 for neox mode
                Qcur = ggml_rope_ext(
                    ctx, Qcur, inp_pos, nullptr, hp.n_rot, 2, hp.n_ctx_orig,
                    freq_base, freq_scale, ext_factor, attn_factor, beta_fast, beta_slow
                );

                Kcur = ggml_rope_ext(
                    ctx, Kcur, inp_pos, nullptr, hp.n_rot, 2, hp.n_ctx_orig,
                    freq_base, freq_scale, ext_factor, attn_factor, beta_fast, beta_slow
                );

                llm_build_kv_store(ctx, k_l, v_l, Kcur, Vcur);

                cur = llm_build_kqv(ctx, k_l, v_l, Qcur, KQ_mask, 1.0f/sqrtf(float(hp.n_embd_head)));
            }

            struct ggml_tensor * ffn_inp = cur;

            // feed forward
            {
                ggml_tensor * ffn_up   = ggml_new_tensor_2d(ctx, GGML_TYPE_Q4_0, hp.n_embd, hp.n_ff);
                ggml_tensor * ffn_down = ggml_new_tensor_2d(ctx, GGML_TYPE_Q4_0, hp.n_ff, hp.n_embd);
                cur = attn_norm;
                cur = ggml_mul_mat(ctx, ffn_up, cur);
                cur = ggml_gelu(ctx, cur);
                cur = ggml_mul_mat(ctx, ffn_down, cur);
            }

            cur = ggml_add(ctx, cur, ffn_inp);

            cur = ggml_add(ctx, cur, inpL);

            // input for next layer
            inpL = cur;
        }

        cur = inpL;

        ggml_tensor * output_norm   = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, hp.n_embd);
        ggml_tensor * output_norm_b = ggml_new_tensor_1d(ctx, GGML_TYPE_F32, hp.n_embd);
        cur = llm_build_norm(ctx, cur, output_norm, output_norm_b, LLM_NORM);

        // lm_head
        ggml_tensor * output = ggml_new_tensor_2d(ctx, GGML_TYPE_Q8_0, hp.n_embd, hp.n_vocab);
        cur = ggml_mul_mat(ctx, output, cur);

        return cur;
    }
};


// ###########################################
// ## Section 3: GGML Op Test Instantiation ##
// ###########################################
static const ggml_type all_types[] = {
    GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_BF16,
    GGML_TYPE_Q4_0, GGML_TYPE_Q4_1,
    GGML_TYPE_Q5_0, GGML_TYPE_Q5_1,
    GGML_TYPE_Q8_0,
    GGML_TYPE_Q1_0,
    GGML_TYPE_Q2_0,
    // lane-149 (F-109): Q2_0_G128 had two op cells in the whole sweep because it was never listed
    // here — the 128-value sibling of Q2_0, with its own block struct and its own shaders.
    GGML_TYPE_Q2_0_G128,
    GGML_TYPE_MXFP4, GGML_TYPE_NVFP4,
    GGML_TYPE_Q2_K, GGML_TYPE_Q3_K,
    GGML_TYPE_Q4_K, GGML_TYPE_Q5_K,
    GGML_TYPE_Q6_K,
    GGML_TYPE_TQ2_0,
    GGML_TYPE_TQ1_0,
    GGML_TYPE_IQ2_XXS, GGML_TYPE_IQ2_XS, GGML_TYPE_IQ2_S,
    GGML_TYPE_IQ3_XXS, GGML_TYPE_IQ1_S, GGML_TYPE_IQ1_M,
    GGML_TYPE_IQ4_NL, GGML_TYPE_IQ3_S, GGML_TYPE_IQ4_XS,
    GGML_TYPE_TQ3_1S, GGML_TYPE_TQ4_1S,
    // S-X8 v4.3 (id 57). CPU-only kernels today; backends without it report not supported.
    GGML_TYPE_SX8,
    // jtrefon TBQ family (ids 58..59, TYPE-ID-ALLOCATION 3.1.3). CPU reference only in this
    // lane: every backend without a kernel must report these as not supported, never a wrong value.
    GGML_TYPE_TBQ3_0, GGML_TYPE_TBQ4_0,
    // tq3 family (ids 48..51, TYPE-ID-ALLOCATION 3.1.1). Listing them here is the
    // whole backend sweep: get_rows, mul_mat and cpy on every built backend.
    GGML_TYPE_TQ3_4S, GGML_TYPE_TQ3_0, GGML_TYPE_TQ3_4SE, GGML_TYPE_TQ3_1S_SHIFT,
    // ROCmFP4 / ROCmFPX (ids 100-104, 107). Absent until lane-144: these types had
    // Vulkan dequant, get_rows and mat-vec pipelines but were never swept here, so
    // nothing checked them against the CPU reference. Listing them is what proves
    // the fused mul_mm rows this lane wired.
    GGML_TYPE_Q4_0_ROCMFP4, GGML_TYPE_Q4_0_ROCMFP4_FAST,
    GGML_TYPE_Q2_0_ROCMFPX, GGML_TYPE_Q3_0_ROCMFPX,
    GGML_TYPE_Q6_0_ROCMFPX, GGML_TYPE_Q8_0_ROCMFPX,
};

// ---------------------------------------------------------------------------------------------
// lane-149 / failure-ledger F-109: all_types is HAND-CURATED, and whole format families sat outside
// it for weeks (ROCmFP4/ROCmFPX, the tq3 family, Q2_0_G128). A green harness proves nothing about a
// family its corpus never entered. This assert makes the omission mechanical: every quantized type
// in enum ggml_type that this build actually implements must appear in all_types, or carry a
// waiver WITH A REASON here. Adding a type without its harness cases now FAILS the run.
struct type_coverage_waiver { ggml_type type; const char * reason; };

static const type_coverage_waiver all_types_waivers[] = {
    { GGML_TYPE_Q8_1,     "quantiser staging block for the int-dot path, never a tested tensor type" },
    { GGML_TYPE_Q8_K,     "CPU-side intermediate for the K-quant dot products, no backend op takes it" },
    { GGML_TYPE_TQ1_0,    "upstream: not implemented on all backends (see the TODO in all_types)" },
    { GGML_TYPE_TURBO2_0, "runtime-only KV codec: covered by the bespoke test_set_rows_turbo2 class, "
                          "not by the generic all_types sweep (it is never a weight tensor)" },
    { GGML_TYPE_TURBO3_0, "runtime-only KV codec: covered by test_set_rows_turbo3 + the FA K/V cases" },
    { GGML_TYPE_TURBO4_0, "runtime-only KV codec: covered by test_set_rows_turbo4" },
};

static bool check_all_types_coverage() {
    bool ok = true;
    for (int i = 0; i < GGML_TYPE_COUNT; i++) {
        const ggml_type t = (ggml_type) i;
        const ggml_type_traits * tr = ggml_get_type_traits(t);
        // a type this build did not compile in (e.g. the ROCmFPX block with the flag OFF) has a
        // zero-filled traits row: nothing to test, and gguf.cpp already fails closed on it.
        if (tr->type_name == nullptr || tr->blck_size == 0 || !tr->is_quantized) {
            continue;
        }
        if (tr->to_float == nullptr && tr->from_float_ref == nullptr) {
            continue;   // declared but unimplemented on the CPU reference: nothing to compare against
        }
        bool listed = false;
        for (ggml_type a : all_types) {
            listed = listed || a == t;
        }
        for (const auto & w : all_types_waivers) {
            listed = listed || w.type == t;
        }
        if (!listed) {
            printf("  \033[1;31mCOVERAGE GAP\033[0m: %s (id %d) is implemented but is not in "
                   "all_types and has no waiver — add its cases, or add a waiver with a reason "
                   "(test-backend-ops.cpp all_types_waivers). failure-ledger F-109.\n",
                   tr->type_name, i);
            ok = false;
        }
    }
    for (const auto & w : all_types_waivers) {
        if (w.reason == nullptr || w.reason[0] == '\0') {
            printf("  \033[1;31mCOVERAGE WAIVER\033[0m for type id %d carries no reason\n", (int) w.type);
            ok = false;
        }
    }
    return ok;
}

static const ggml_type base_types[] = {
    GGML_TYPE_F32, GGML_TYPE_F16,
    GGML_TYPE_Q8_0, // for I8MM tests
    GGML_TYPE_Q1_0,
    GGML_TYPE_Q2_0,
    GGML_TYPE_Q4_0,
    GGML_TYPE_Q4_1, // for I8MM tests
    GGML_TYPE_Q4_K,
    GGML_TYPE_MXFP4, GGML_TYPE_NVFP4, // TODO: or "other"
    GGML_TYPE_IQ2_XXS
};

static const ggml_type other_types[] = {
    GGML_TYPE_Q4_1,
    GGML_TYPE_Q5_0, GGML_TYPE_Q5_1,
    GGML_TYPE_Q8_0,
    GGML_TYPE_Q1_0,
    GGML_TYPE_Q2_0,
    GGML_TYPE_Q2_K, GGML_TYPE_Q3_K,
    GGML_TYPE_Q5_K,
    GGML_TYPE_Q6_K,
    GGML_TYPE_TQ2_0,
    GGML_TYPE_TQ1_0,
    GGML_TYPE_IQ2_XS, GGML_TYPE_IQ2_S,
    GGML_TYPE_IQ3_XXS, GGML_TYPE_IQ1_S, GGML_TYPE_IQ1_M,
    GGML_TYPE_IQ4_NL, GGML_TYPE_IQ3_S, GGML_TYPE_IQ4_XS,
    GGML_TYPE_BF16,
};

#ifdef _MSC_VER
// Workaround long compile time with msvc
#pragma optimize("", off)
#endif

// Test cases for evaluation: should try to cover edge cases while using small input sizes to keep the runtime low
static std::vector<std::unique_ptr<test_case>> make_test_cases_eval() {
    std::vector<std::unique_ptr<test_case>> test_cases;
    std::default_random_engine rng(0);

    // unary ops
    for (ggml_type type : {GGML_TYPE_F16, GGML_TYPE_F32}) {
        for (int v : {0, 1}) {
            for (int op = 0; op < GGML_UNARY_OP_COUNT; op++) {
                if (op == GGML_UNARY_OP_XIELU) {
                    continue; // need extra params, separate test
                }
                test_cases.emplace_back(new test_unary((ggml_unary_op) op, type, { 128, 2, 2, 2 }, v));
                test_cases.emplace_back(new test_unary((ggml_unary_op) op, type, { 5, 7, 11, 13 }, v));
            }
        }
    }

    // fused relu + sqr (squared ReLU)
    for (ggml_type type : {GGML_TYPE_F16, GGML_TYPE_F32}) {
        test_cases.emplace_back(new test_relu_sqr(type, { 128, 2, 2, 2 }));
        test_cases.emplace_back(new test_relu_sqr(type, { 5, 7, 11, 13 }));
    }

    // fused unary + mul (gated activations that are not expressed as GGML_OP_GLU)
    for (ggml_unary_op op : { GGML_UNARY_OP_GELU, GGML_UNARY_OP_SILU, GGML_UNARY_OP_SIGMOID, GGML_UNARY_OP_SOFTPLUS }) {
        for (ggml_type type : { GGML_TYPE_F16, GGML_TYPE_F32 }) {
            for (bool swap : { false, true }) {
                test_cases.emplace_back(new test_unary_mul(op, type, { 128, 2, 2, 2 }, swap));
            }
            test_cases.emplace_back(new test_unary_mul(op, type, { 5, 7, 11, 13 }));
            test_cases.emplace_back(new test_unary_mul(op, type, { 128, 2, 2, 2 }, false, "pad_unary"));
            // a view only stays out from between the two ops when the unary result is second
            test_cases.emplace_back(new test_unary_mul(op, type, { 128, 2, 2, 2 }, true, "pad_other"));
            test_cases.emplace_back(new test_unary_mul(op, type, { 128, 2, 2, 2 }, true, "halves"));
            test_cases.emplace_back(new test_unary_mul(op, type, { 128, 2, 2, 2 }, false, "packed", "consumer"));
            test_cases.emplace_back(new test_unary_mul(op, type, { 128, 2, 2, 2 }, false, "bcast"));
            test_cases.emplace_back(new test_unary_mul(op, type, { 128, 2, 2, 2 }, false, "rep_ne0"));
            test_cases.emplace_back(new test_unary_mul(op, type, { 128, 2, 2, 2 }, false, "view_mid"));
            test_cases.emplace_back(new test_unary_mul(op, type, { 128, 2, 2, 2 }, false, "gate"));
            // must not fuse
            test_cases.emplace_back(new test_unary_mul(op, type, { 128, 2, 2, 2 }, false, "strided_dim1"));
            test_cases.emplace_back(new test_unary_mul(op, type, { 128, 2, 2, 2 }, false, "packed", "reuse"));
        }
    }

    // SNAKE activation fusion: x + sin(a*x)^2 * inv_b
    for (ggml_type type : { GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_BF16 }) {
        test_cases.emplace_back(new test_snake_fuse(type, {   5,   7, 1, 1}));   // primes sub-block
        test_cases.emplace_back(new test_snake_fuse(type, {  33,  32, 1, 1}));   // boundary
        test_cases.emplace_back(new test_snake_fuse(type, {1025,  13, 1, 1}));   // large prime, grid-stride
        test_cases.emplace_back(new test_snake_fuse(type, { 128,  16, 1, 1}));   // power-of-two
        test_cases.emplace_back(new test_snake_fuse(type, { 256, 192, 1, 1}));   // BigVGAN-ish
        // higher-rank shapes: matcher must reject fusion, fallback to naive chain
        test_cases.emplace_back(new test_snake_fuse(type, {  64,  32, 2, 1}));   // ne[2] > 1
        test_cases.emplace_back(new test_snake_fuse(type, {  64,  32, 1, 2}));   // ne[3] > 1
        test_cases.emplace_back(new test_snake_fuse(type, {  64,  32, 2, 3}));   // ne[2] > 1 and ne[3] > 1
    }

    test_cases.emplace_back(new test_dsv4_hc_comb(1, 1));
    test_cases.emplace_back(new test_dsv4_hc_comb(17, 4));
    test_cases.emplace_back(new test_dsv4_hc_comb(257, 8));
    test_cases.emplace_back(new test_dsv4_hc_comb(17, 20));
    // production n_iter (DeepSeek-V4 uses 20) across batch sizes that cross
    // subgroup and workgroup boundaries; 1 = single-token decode
    for (int64_t n_tokens : {1, 256, 336, 512, 513, 1024, 2048}) {
        test_cases.emplace_back(new test_dsv4_hc_comb(n_tokens, 20));
    }

    // DSV4_HC_COMB eps sweep (restored; lost in the #244 rebase) — validates
    // CPU/CUDA Sinkhorn eps semantics match across orders of magnitude
    for (float eps : {1.0e-6f, 1.0e-3f, 1.0e-1f}) {
        for (int64_t nt : {1, 2, 4, 8, 64}) {
            for (int ni : {1, 3, 5}) {
                test_cases.emplace_back(new test_dsv4_hc_comb(nt, ni, eps));
            }
        }
    }

    test_cases.emplace_back(new test_dsv4_hc_pre(1, 4, 1));
    test_cases.emplace_back(new test_dsv4_hc_pre(31, 4, 17));
    test_cases.emplace_back(new test_dsv4_hc_pre(128, 4, 257));
    test_cases.emplace_back(new test_dsv4_hc_pre(4096, 4, 21));
    test_cases.emplace_back(new test_dsv4_hc_pre(31, 4, 17, true));
    test_cases.emplace_back(new test_dsv4_hc_pre(4096, 4, 21, true));
    for (int64_t n_hc : {1, 2, 3, 5, 8, 65}) {
        test_cases.emplace_back(new test_dsv4_hc_pre(128, n_hc, 17));
        test_cases.emplace_back(new test_dsv4_hc_pre(128, n_hc, 17, true));
    }

    test_cases.emplace_back(new test_dsv4_hc_post(1, 1));
    test_cases.emplace_back(new test_dsv4_hc_post(31, 17));
    test_cases.emplace_back(new test_dsv4_hc_post(128, 257));
    test_cases.emplace_back(new test_dsv4_hc_post(4096, 21));
    test_cases.emplace_back(new test_dsv4_hc_post(31, 17, true));
    test_cases.emplace_back(new test_dsv4_hc_post(4096, 21, true));

    // glu ops
    for (ggml_type type : {GGML_TYPE_F16, GGML_TYPE_F32}) {
        for (int v : {0, 1}) {
            for (int op = 0; op < GGML_GLU_OP_COUNT; op++) {
                if (op == GGML_GLU_OP_SWIGLU_OAI || op == GGML_GLU_OP_SWIGLU_CLAMP) {
                    continue;
                }

                for (bool swapped : {false, true}) {
                    test_cases.emplace_back(new test_glu((ggml_glu_op) op, type, { 128, 2, 2, 2 }, v, swapped));
                    test_cases.emplace_back(new test_glu((ggml_glu_op) op, type, { 5, 7, 11, 13 }, v, swapped));
                }

                test_cases.emplace_back(new test_glu_split((ggml_glu_op) op, type, { 128, 2, 2, 2 }, v));
                test_cases.emplace_back(new test_glu_split((ggml_glu_op) op, type, { 5, 7, 11, 13 }, v));
            }
        }
    }

    for (int v : {0, 1}) {
        for (float alpha : {.5f, 1.702f}) {
            for (float limit : {2.0f, 7.0f}) {
                test_cases.emplace_back(new test_swiglu_oai(GGML_TYPE_F32, { 128, 2, 2, 2 }, v, alpha, limit));
            }
        }
    }

    for (ggml_type type : {GGML_TYPE_F16, GGML_TYPE_F32}) {
        for (int v : {0, 1}) {
            for (float limit : {2.0f, 10.0f}) {
                test_cases.emplace_back(new test_swiglu_clamp(type, { 128, 2, 2, 2 }, v, limit));
            }
        }
    }

    for (ggml_type type : {GGML_TYPE_F32, GGML_TYPE_Q4_0}) {
        test_cases.emplace_back(new test_get_rows(type, 300*256,   5,         4,   1,   2, false));
        test_cases.emplace_back(new test_get_rows(type,     256,   80000, 70000,   2,   1, false));
        test_cases.emplace_back(new test_get_rows(type,     256,   5,         4, 700, 100, false));
        // ring-repair 2026-08-24: recurrent-bank restore gathers - HUGE rows (n_embd_s-scale),
        // few rows, high row index. Vulkan restored garbage state on exactly this shape class.
        test_cases.emplace_back(new test_get_rows(type,  163840,   6,         1,   1,   1, false));
        test_cases.emplace_back(new test_get_rows(type,   36864,  12,         2,   1,   1, false));
    }

    test_cases.emplace_back(new test_get_rows(GGML_TYPE_F32, 1, 8, 2, 1, 1, false));
    for (ggml_type type : all_types) {
        for (int b : {1, 7}) {
            for (bool v : {false, true}) {
                for (bool vs0 : {false, true}) {
                    test_cases.emplace_back(new test_get_rows(type, 256, 5, 4, b, 1, v, vs0));
                }
            }
        }
    }
    for (int b : {1, 7}) {
        for (bool v : {false, true}) {
            for (bool vs0 : {false, true}) {
                test_cases.emplace_back(new test_get_rows(GGML_TYPE_I32, 256, 5, 4, b, 1, v, vs0));
            }
        }
    }
    test_cases.emplace_back(new test_get_rows(GGML_TYPE_F32, 256, 8, 2, 1, 1, false, true, 3));

    test_cases.emplace_back(new test_get_rows_back(GGML_TYPE_F32, 1, 8, 2, 1, false));
    test_cases.emplace_back(new test_get_rows_back(GGML_TYPE_F32, 1, 70000, 4, 1, false)); // row count > CUDA grid-y limit (65535)
    for (ggml_type type : all_types) {
        for (bool v : {false, true}) {
            test_cases.emplace_back(new test_get_rows_back(type, 256, 5, 4, 1, v));
        }
    }
    for (bool v : {false, true}) {
        test_cases.emplace_back(new test_get_rows_back(GGML_TYPE_I32, 256, 5, 4, 1, v));
    }

    test_cases.emplace_back(new test_set_rows(GGML_TYPE_F32, GGML_TYPE_F32, GGML_TYPE_I64, { 1, 8, 1, 3 }, { 1, 1 }, 2, false));
    test_cases.emplace_back(new test_set_rows(GGML_TYPE_F32, GGML_TYPE_F32, GGML_TYPE_I32, { 1, 8, 1, 3 }, { 1, 1 }, 2, false));
    test_cases.emplace_back(new test_set_rows(GGML_TYPE_F32, GGML_TYPE_Q8_0, GGML_TYPE_I32, { 256, 5, 1, 3 }, { 1, 1, }, 1, false));
    for (ggml_type src_type : {GGML_TYPE_F16, GGML_TYPE_F32}) {
        for (ggml_type type : all_types) {
            for (int b : {1, 7}) {
                for (bool v : {false, true}) {
                    test_cases.emplace_back(new test_set_rows(src_type, type, GGML_TYPE_I64, { 256, 5,  b, 3 }, { 1, 1, }, 1, v));
                    test_cases.emplace_back(new test_set_rows(src_type, type, GGML_TYPE_I64, { 256, 11, 1, b }, { 2, 3, }, 7, v));

                    test_cases.emplace_back(new test_set_rows(src_type, type, GGML_TYPE_I64, { 3*ggml_blck_size(type), 3, b, 1 }, { 2, 3, }, 2, v));

                    if (ggml_blck_size(type) == 1) {
                        test_cases.emplace_back(new test_set_rows(src_type, type, GGML_TYPE_I64, { 31, 3, b, 1 }, { 2, 3, }, 2, v));
                        test_cases.emplace_back(new test_set_rows(src_type, type, GGML_TYPE_I64, { 33, 5, 1, b }, { 2, 3, }, 1, v));
                    }
                }
            }
        }
    }
    test_cases.emplace_back(new test_set_rows(GGML_TYPE_F16, GGML_TYPE_F16, GGML_TYPE_I64, { 1, 8, 1, 3 }, { 1, 1 }, 2, false));
    test_cases.emplace_back(new test_set_rows(GGML_TYPE_F16, GGML_TYPE_F16, GGML_TYPE_I32, { 1, 8, 1, 3 }, { 1, 1 }, 2, false));
    test_cases.emplace_back(new test_set_rows(GGML_TYPE_F16, GGML_TYPE_F16, GGML_TYPE_I64, { 1, 8, 1, 3 }, { 1, 1 }, 2, true));
    test_cases.emplace_back(new test_set_rows(GGML_TYPE_F16, GGML_TYPE_F16, GGML_TYPE_I32, { 1, 8, 1, 3 }, { 1, 1 }, 2, true));

    // TURBO_WHT tests
    for (int dir : {0, 1}) {
        for (int64_t hd : {128, 256, 512}) {
            for (int64_t nh : {1, 4, 8}) {
                test_cases.emplace_back(new test_turbo_wht(hd, nh, dir));
            }
        }
    }

    // TURBO_WHT round-trip tests (forward then inverse = identity)
    for (int64_t hd : {128, 256, 512}) {
        for (int64_t nh : {1, 4, 8}) {
            test_cases.emplace_back(new test_turbo_wht_roundtrip(hd, nh));
        }
    }

    // SET_ROWS with turbo3 destination: quantize then dequant round-trip
    // Small tensors (single-dim dispatch)
    for (ggml_type idx_type : {GGML_TYPE_I32, GGML_TYPE_I64}) {
        for (int64_t ne0 : {128, 256, 512}) {
            for (int r : {1, 4, 7}) {
                test_cases.emplace_back(new test_set_rows_turbo3(idx_type, ne0, 16, r));
            }
        }
    }
    // Large tensors -- exercises 2D dispatch grid (>512 workgroups),
    // matching actual inference dimensions (4 kv_heads, batch=1024+)
    test_cases.emplace_back(new test_set_rows_turbo3(GGML_TYPE_I32, 128, 4096, 1024));
    test_cases.emplace_back(new test_set_rows_turbo3(GGML_TYPE_I32, 256, 2048, 512));
    test_cases.emplace_back(new test_set_rows_turbo3(GGML_TYPE_I32, 512, 1024, 256));

    // SET_ROWS with turbo4 destination: quantize then dequant round-trip
    for (ggml_type idx_type : {GGML_TYPE_I32, GGML_TYPE_I64}) {
        for (int64_t ne0 : {128, 256, 512}) {
            for (int r : {1, 4, 7}) {
                test_cases.emplace_back(new test_set_rows_turbo4(idx_type, ne0, 16, r));
            }
        }
    }
    // Large tensors -- exercises 2D dispatch grid and pushes 68-byte-stride
    // addressing past the 66-byte-sized allocation (the gfx1151 bug)
    test_cases.emplace_back(new test_set_rows_turbo4(GGML_TYPE_I32, 128, 4096, 1024));
    test_cases.emplace_back(new test_set_rows_turbo4(GGML_TYPE_I32, 256, 2048, 512));
    test_cases.emplace_back(new test_set_rows_turbo4(GGML_TYPE_I32, 512, 1024, 256));

    // SET_ROWS with turbo2 destination: quantize then dequant round-trip (lane-148)
    for (ggml_type idx_type : {GGML_TYPE_I32, GGML_TYPE_I64}) {
        for (int64_t ne0 : {128, 256, 512}) {
            for (int r : {1, 4, 7}) {
                test_cases.emplace_back(new test_set_rows_turbo2(idx_type, ne0, 16, r));
            }
        }
    }
    test_cases.emplace_back(new test_set_rows_turbo2(GGML_TYPE_I32, 128, 4096, 1024));
    test_cases.emplace_back(new test_set_rows_turbo2(GGML_TYPE_I32, 256, 2048, 512));
    test_cases.emplace_back(new test_set_rows_turbo2(GGML_TYPE_I32, 512, 1024, 256));

    // SET_ROWS with TQ4_1S destination: quantize then dequant round-trip
    for (ggml_type idx_type : {GGML_TYPE_I32, GGML_TYPE_I64}) {
        for (int64_t ne0 : {32, 64, 128, 256}) {
            for (int r : {1, 4}) {
                test_cases.emplace_back(new test_set_rows_tq4_1s(idx_type, ne0, 16, r));
            }
        }
    }
    // Large tensor
    test_cases.emplace_back(new test_set_rows_tq4_1s(GGML_TYPE_I32, 128, 256, 64));

    for (int mode : { GGML_ROPE_TYPE_NORMAL, GGML_ROPE_TYPE_NEOX, GGML_ROPE_TYPE_MROPE, GGML_ROPE_TYPE_VISION, GGML_ROPE_TYPE_IMROPE }) {
        for (ggml_type type : {GGML_TYPE_F16, GGML_TYPE_F32}) {
            for (int ne2 : {1, 8, 512}) {
                test_cases.emplace_back(new test_rope_set_rows(type, GGML_TYPE_I64, { 128, 32, ne2, 1 }, mode));
                test_cases.emplace_back(new test_rope_set_rows(type, GGML_TYPE_I64, { 128, 32, ne2, 3 }, mode));
            }
        }
    }
    test_cases.emplace_back(new test_rope_set_rows(GGML_TYPE_F32, GGML_TYPE_I32, { 128, 32, 8, 1 }, GGML_ROPE_TYPE_IMROPE));

    for (ggml_type type_input : {GGML_TYPE_F32}) {
        for (ggml_op_pool pool_type : {GGML_OP_POOL_AVG, GGML_OP_POOL_MAX}) {
            for (int k0 : {1, 3}) {
                for (int k1 : {1, 3}) {
                    for (int s0 : {1, 2}) {
                        for (int s1 : {1, 2}) {
                            for (int p0 : {0, 1}) {
                                for (int p1 : {0, 1}) {
                                    test_cases.emplace_back(new test_pool2d(pool_type, type_input, {10, 10, 3, 1}, k0, k1, s0, s1, p0, p1));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    for (ggml_type type_input : {GGML_TYPE_F32}) {
        for (ggml_op_pool pool_type : {GGML_OP_POOL_AVG, GGML_OP_POOL_MAX}) {
            for (int k0 : {1, 2, 3}) {
                for (int s0 : {1, 2, 3}) {
                    for (int p0 : {0, 1, 2, 3}) {
                        test_cases.emplace_back(new test_pool1d(pool_type, type_input, { 10,  3, 2, 1 }, k0, s0, p0));
                        test_cases.emplace_back(new test_pool1d(pool_type, type_input, { 11,  1, 3, 2 }, k0, s0, p0));
                        test_cases.emplace_back(new test_pool1d(pool_type, type_input, { 128, 2, 1, 3 }, k0, s0, p0));
                    }
                }
            }
        }
    }

#if 0
    // >4GB im2col destination. Too slow to run by default.
    // Test cases taken from Wan2.1 T2V 1.3B.
    test_cases.emplace_back(new test_im2col   (GGML_TYPE_F32, GGML_TYPE_F32, GGML_TYPE_F32, {832, 480, 192, 4}, {3, 3, 192, 96}, 1, 1, 1, 1, 1, 1, true));
    test_cases.emplace_back(new test_im2col_3d(GGML_TYPE_F32, GGML_TYPE_F32, GGML_TYPE_F32, {834, 482, 6, 96},  {3, 3,3, 9216}, 96, 1, 1, 1, 0, 0, 0, 1, 1, 1, false));
#endif

    // im2col 1D
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F32, GGML_TYPE_F32, {3000, 128, 1, 1}, {3, 128, 1280, 1}, 1, 0, 1, 0, 1, 0, false));
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F32, {3000, 128, 1, 1}, {3, 128, 1280, 1}, 1, 0, 1, 0, 1, 0, false));
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F16, {3000, 128, 1, 1}, {3, 128, 1280, 1}, 1, 0, 1, 0, 1, 0, false));
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F16, {3000, 384, 1, 1}, {3, 384,  384, 1}, 1, 0, 1, 0, 1, 0, false));
    for (int s0 : {1, 3}) {
        for (int p0 : {0, 3}) {
            for (int d0 : {1, 3}) {
                test_cases.emplace_back(new test_im2col(
                    GGML_TYPE_F32, GGML_TYPE_F32, GGML_TYPE_F32, {20, 2, 2, 1}, {3, 2, 2, 1},
                    s0, 0, p0, 0, d0, 0, false));
            }
        }
    }

    // im2col 2D
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F32, GGML_TYPE_F32));
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F32, GGML_TYPE_F16));
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F32));
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    for (int s0 : {1, 3}) {
        for (int s1 : {1, 3}) {
            for (int p0 : {0, 3}) {
                for (int p1 : {0, 3}) {
                    for (int d0 : {1, 3}) {
                        for (int d1 : {1, 3}) {
                            test_cases.emplace_back(new test_im2col(
                                GGML_TYPE_F32, GGML_TYPE_F32, GGML_TYPE_F32, {20, 20, 2, 2}, {3, 3, 2, 2},
                                s0, s1, p0, p1, d0, d1, true));
                        }
                    }
                }
            }
        }
    }

    // extra tests for im2col 2D
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F16, {12, 12, 1, 32}, {3, 3, 1, 32}, 1, 1, 1, 1, 1, 1, true));
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F16, {12, 12, 2, 32}, {3, 3, 2, 32}, 1, 1, 1, 1, 1, 1, true));
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F16, {12, 12, 1, 1024}, {3, 3, 1, 1024}, 1, 1, 1, 1, 1, 1, true));
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F16, {12, 12, 2, 1024}, {3, 3, 2, 1024}, 1, 1, 1, 1, 1, 1, true));
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F16, {12, 12, 1, 2048}, {3, 3, 1, 2048}, 1, 1, 1, 1, 1, 1, true));
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F16, {12, 12, 2, 2048}, {3, 3, 2, 2048}, 1, 1, 1, 1, 1, 1, true));
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F16, {12, 12, 1, 2560}, {3, 3, 1, 2560}, 1, 1, 1, 1, 1, 1, true));
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F16, {12, 12, 2, 2560}, {3, 3, 2, 2560}, 1, 1, 1, 1, 1, 1, true));
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F16, {5, 5, 1, 32}, {3, 4, 1, 32}, 1, 1, 0, 0, 1, 1, true));
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F32, GGML_TYPE_F32, {2, 2, 1536, 729}, {2, 2, 1536, 4096}, 1, 1, 0, 0, 1, 1, true));
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F16, {128, 128, 1, 2}, {32, 33, 1, 2}, 1, 1, 1, 1, 1, 1, true));
    test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F16, {128, 128, 2, 1}, {33, 34, 2, 1}, 1, 1, 1, 1, 1, 1, true));

    // im2col 3D
    test_cases.emplace_back(new test_im2col_3d(GGML_TYPE_F32, GGML_TYPE_F32, GGML_TYPE_F32));
    test_cases.emplace_back(new test_im2col_3d(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F32));
    test_cases.emplace_back(new test_im2col_3d(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    for (int s0 : {1, 3}) {
        for (int s1 : {1, 3}) {
            for (int s2 : {1, 3}) {
                for (int p0 : {0, 3}) {
                    for (int p1 : {0, 3}) {
                        for (int p2 : {0, 3}) {
                            for (int d0 : {1, 3}) {
                                for (int d1 : {1, 3}) {
                                    for (int d2 : {1, 3}) {
                                        for (int IC : {1, 3}) {
                                            for (bool v : {false, true}) {
                                                test_cases.emplace_back(new test_im2col_3d(
                                                    GGML_TYPE_F32, GGML_TYPE_F32, GGML_TYPE_F32, {20, 20, 10, 3}, {3, 3, 3, 3},
                                                    IC, s0, s1, s2, p0, p1, p2, d0, d1, d2, v));
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

// Conv_2D test cases
#ifdef DETAILED_TESTS
    // Probably we do not have enough time to execute these in the pipeline.
    uint32_t iwh_idx  = 0;
    uint32_t kwh_idx  = 1;
    uint32_t Cout_idx = 2;
    uint32_t Cin_idx  = 3;
    uint32_t B_idx    = 4;

    std::vector<std::array<int, 5>> cases = {
  //{IWH, KWH, Cout, Cin, B}
  // K=CRS=NPQ=4096 conv_2d matmul performance
        {19,   4, 4096, 256, 16},
 // K=128, CRS=128, NPQ=4096
        { 19,  4, 128,  8,   16},
 // K=130, CRS=128, NPQ=4096
        { 19,  4, 130,  8,   16},
 // Edge case: K x CRS is small
        { 19,  2, 4,    4,   16},
 // A ConvNet's first layer
        { 224, 3, 8,    3,   1 },
 // A ConvNet's first layer with 2x2 convolution, and 1 channel
        { 224, 2, 8,    1,   1 },
 // A ConvNet's first layer with 2x2 convolution, and 1 channel, several images in the batch
        { 224, 2, 8,    1,   8 },
 // A middle layer of a ConvNet
        { 58,  3, 64,   32,  1 },
 // A middle layer of a ConvNet, several images in the batch
        { 58,  3, 64,   32,  8 },
 // A deep layer of a ConvNet, several images in the batch
        { 16,  3, 256,  128, 8 }
    };

    for (auto kernel_type : {GGML_TYPE_F32, GGML_TYPE_F16}) {
        for (auto act_case : cases) {
            test_cases.emplace_back(new test_conv_2d(
                { act_case[iwh_idx], act_case[iwh_idx], act_case[Cin_idx], act_case[B_idx] },
                { act_case[kwh_idx], act_case[kwh_idx], act_case[Cin_idx], act_case[Cout_idx] },
                kernel_type, 1, 1, 0, 0, 1, 1, false));  // bool cwhn = false
            test_cases.emplace_back(new test_conv_2d(
                { act_case[iwh_idx], act_case[iwh_idx], act_case[Cin_idx], act_case[B_idx] },
                { act_case[kwh_idx], act_case[kwh_idx], act_case[Cin_idx], act_case[Cout_idx] },
                kernel_type, 1, 1, 0, 0, 1, 1, true));  // bool cwhn = true
        }
    }
#endif

    // CONV_2D:
    auto calc_conv_output_size = [](int64_t ins, int64_t ks, int s, int p, int d) -> int64_t {
        return (ins + 2 * p - d * (ks - 1) - 1) / s + 1;
    };

    //uint32_t s0 = 3;
    uint32_t s1 = 5;
    uint32_t p0 = 5;
    //uint32_t p1 = 2;
    uint32_t d0 = 2;
    uint32_t d1 = 4;

    for (uint32_t s0 : { 1, 3 }) {
        for (uint32_t p1 : { 2, 5 }) {
            for (uint32_t Cin : { 1, 25 }) {
                for (uint32_t Cout : { 1, 12 }) {
                    for (uint32_t KH : { 1, 2, 3, 11 }) {
                        for (uint32_t KW : { 1, 2, 3, 11 }) {
                            for (uint32_t H : { 1, 133 }) {
                                for (uint32_t W : { 1, 141 }) {
                                    if (calc_conv_output_size(W, KW, s0, p0, d0) > 0 &&
                                        calc_conv_output_size(H, KH, s1, p1, d1) > 0) {
                                        for (auto kernel_type : {GGML_TYPE_F32, GGML_TYPE_F16}) {
                                            test_cases.emplace_back(new test_conv_2d(
                                                { W, H, Cin, 2 }, { KW, KH, Cin, Cout }, kernel_type, s0, s1, p0, p1, d0, d1, false)); // bool cwhn = false
                                            test_cases.emplace_back(new test_conv_2d(
                                                { W, H, Cin, 2 }, { KW, KH, Cin, Cout }, kernel_type, s0, s1, p0, p1, d0, d1, true));  // bool cwhn = true
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    for (auto kernel_type : {GGML_TYPE_F32, GGML_TYPE_F16}) {
        test_cases.emplace_back(new test_conv_2d({ 256, 256, 192, 1 }, { 3, 3, 192, 96 }, kernel_type, 1, 1, 1, 1, 1, 1, false)); // bool cwhn = false
        test_cases.emplace_back(new test_conv_2d({ 256, 256, 192, 1 }, { 3, 3, 192, 96 }, kernel_type, 1, 1, 1, 1, 1, 1, true));  // bool cwhn = true
    }
    test_cases.emplace_back(new test_conv_2d({ 19, 17, 8, 2 }, { 3, 3, 8, 65 }, GGML_TYPE_F16, 1, 1, 1, 1, 1, 1));
    test_cases.emplace_back(new test_conv_2d({ 19, 17, 16, 3 }, { 3, 3, 16, 33 }, GGML_TYPE_F16, 2, 3, 4, 2, 2, 1));
    test_cases.emplace_back(new test_conv_2d({ 13, 11, 16, 3 }, { 1, 1, 16, 33 }, GGML_TYPE_F16, 1, 1, 0, 0, 1, 1));
    test_cases.emplace_back(new test_conv_2d({ 19, 17, 8, 2 }, { 3, 3, 8, 17 }, GGML_TYPE_F16, 1, 1, 1, 1, 1, 1, false, 1));

    // sycl backend will limit task global_range < MAX_INT
    // test cases for 2D im2col with large input W and H (occurs in stable-diffusion)
    // however these cases need to alloc more memory which may fail in some devices (Intel Arc770, etc.)
    // these cases are verified (pass) in Intel(R) Data Center GPU Max 1100 (sycl backend) and NV A30 (cuda backend)
    // test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F16, {1024, 1024, 256, 1}, {3, 3, 256, 1}, 1, 1, 1, 1, 1, 1, true));
    // test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F32, {1024, 1024, 256, 1}, {3, 3, 256, 1}, 1, 1, 1, 1, 1, 1, true));

    test_cases.emplace_back(new test_conv_2d_dw({17, 34, 9, 1}, {3, 3, 1, 9},  GGML_TYPE_F32, 1, 0, 1, false));
    test_cases.emplace_back(new test_conv_2d_dw({17, 34, 9, 1}, {3, 3, 1, 9},  GGML_TYPE_F32, 1, 0, 1, true));
    test_cases.emplace_back(new test_conv_2d_dw({32, 8, 64, 1}, {3, 3, 1, 64}, GGML_TYPE_F32, 2, 1, 1, false));
    test_cases.emplace_back(new test_conv_2d_dw({32, 8, 64, 1}, {3, 3, 1, 64}, GGML_TYPE_F32, 2, 1, 1, true));

    test_cases.emplace_back(new test_conv_2d_dw({17, 34, 9, 1}, {3, 3, 1, 9},  GGML_TYPE_F16, 1, 0, 1, false));
    test_cases.emplace_back(new test_conv_2d_dw({17, 34, 9, 1}, {3, 3, 1, 9},  GGML_TYPE_F16, 1, 0, 1, true));
    test_cases.emplace_back(new test_conv_2d_dw({32, 8, 64, 1}, {3, 3, 1, 64}, GGML_TYPE_F16, 2, 1, 1, false));
    test_cases.emplace_back(new test_conv_2d_dw({32, 8, 64, 1}, {3, 3, 1, 64}, GGML_TYPE_F16, 2, 1, 1, true));

    // CONV_3D
    auto calc_conv_output_size_3d = [](int64_t ins, int64_t ks, int s, int p, int d) -> int64_t {
        return (ins + 2 * p - d * (ks - 1) - 1) / s + 1;
    };

    for (ggml_type kernel_type : {GGML_TYPE_F32, GGML_TYPE_F16}) {
        for (int N : {1, 2}) {
            for (int IC : {1, 3}) {
                for (int OC : {1, 4}) {
                    for (int s0 : {1, 2}) {
                        for (int p1 : {0, 1}) {
                            for (int d2 : {1, 2}) {
                                int64_t IW = 20, IH = 22, ID = 18;
                                int64_t KW = 3,  KH = 3,  KD = 3;
                                int s1 = s0, s2 = s0;
                                int p0 = p1, p2 = p1;
                                int d0 = d2, d1 = d2;

                                if (calc_conv_output_size_3d(IW, KW, s0, p0, d0) <= 0 ||
                                    calc_conv_output_size_3d(IH, KH, s1, p1, d1) <= 0 ||
                                    calc_conv_output_size_3d(ID, KD, s2, p2, d2) <= 0) {
                                    continue;
                                }
                                test_cases.emplace_back(new test_conv_3d(
                                    N, IC, ID, IH, IW,
                                    OC, KD, KH, KW,
                                    s0, s1, s2, p0, p1, p2, d0, d1, d2,
                                    kernel_type));

                                // Asymmetric kernel and params
                                int64_t asym_KW = 5, asym_KH = 1, asym_KD = 3;
                                int asym_s0 = 2, asym_s1 = 1, asym_s2 = 1;
                                int asym_p0 = 2, asym_p1 = 0, asym_p2 = 1;
                                int asym_d0 = 1, asym_d1 = 1, asym_d2 = 2;

                                if (calc_conv_output_size_3d(IW, asym_KW, asym_s0, asym_p0, asym_d0) <= 0 ||
                                    calc_conv_output_size_3d(IH, asym_KH, asym_s1, asym_p1, asym_d1) <= 0 ||
                                    calc_conv_output_size_3d(ID, asym_KD, asym_s2, asym_p2, asym_d2) <= 0) {
                                    continue;
                                }
                                test_cases.emplace_back(new test_conv_3d(
                                    N, IC, ID, IH, IW,
                                    OC, asym_KD, asym_KH, asym_KW,
                                    asym_s0, asym_s1, asym_s2, asym_p0, asym_p1, asym_p2, asym_d0, asym_d1, asym_d2,
                                    kernel_type));
                            }
                        }
                    }
                }
            }
        }
        // Case with kernel size 1
        test_cases.emplace_back(new test_conv_3d(1, 4, 8, 8, 8, 8, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, kernel_type));
        test_cases.emplace_back(new test_conv_3d(2, 8, 5, 11, 9, 65, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, kernel_type));
        test_cases.emplace_back(new test_conv_3d(2, 5, 7, 9, 13, 17, 2, 3, 4, 2, 1, 3, 3, 2, 2, 2, 1, 2, kernel_type));
        test_cases.emplace_back(new test_conv_3d(3, 16, 3, 7, 9, 33, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, kernel_type));
        test_cases.emplace_back(new test_conv_3d(2, 8, 7, 5, 9, 33, 3, 1, 1, 1, 1, 2, 0, 0, 2, 1, 1, 2, kernel_type));
        test_cases.emplace_back(new test_conv_3d(2, 3, 1, 2, 1, 7, 1, 1, 1, 1, 1, 1, 3, 4, 2, 1, 1, 1, kernel_type));
        test_cases.emplace_back(new test_conv_3d(2, 8, 5, 7, 9, 17, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, kernel_type, 1));
        test_cases.emplace_back(new test_conv_3d(1, 1, 2, 2, 2, 1, 1, 1, 0, 1, 1, 1, 0, 0, 0, 1, 1, 1, kernel_type));
        test_cases.emplace_back(new test_conv_3d(1, 1, 2, 2, 2, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, kernel_type));
        test_cases.emplace_back(new test_conv_3d(1, 1, 2, 2, 2, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, kernel_type));
    }

    for(uint32_t Cout : {1, 9}){
        for(uint32_t Cin : {1, 7}){
            for(uint32_t K : {1, 3, 1337}){
                for(uint32_t L : {1, 2, 13}){
                    for(uint32_t s0: {1, 2, 3}){
                        test_cases.emplace_back(new test_conv_transpose_1d({L,Cin,1,1}, {K,Cout,Cin,1}, s0, 0, 1));
                    }
                }
            }
        }
    }

    test_cases.emplace_back(new test_conv_transpose_1d());
    test_cases.emplace_back(new test_conv_transpose_1d({3,2,1,1}, {2,3,2,1}, 3, 0, 1));
    test_cases.emplace_back(new test_conv_transpose_1d({3,2,1,1}, {2,3,2,1}, 2, 0, 1));
    test_cases.emplace_back(new test_conv_transpose_1d({3,2,1,1}, {2,3,2,1}, 1, 0, 1));
    test_cases.emplace_back(new test_conv_transpose_1d({3,2,1,1}, {3,2,2,1}, 2, 0, 1));
    test_cases.emplace_back(new test_conv_transpose_1d({3,2,1,1}, {3,2,2,1}, 1, 0, 1));
    test_cases.emplace_back(new test_conv_transpose_1d({3,2,1,1}, {3,1,2,1}, 1, 0, 1));
    test_cases.emplace_back(new test_conv_transpose_1d({2,1,1,1}, {3,1,1,1}, 1, 0, 1));

    for (ggml_type type : {GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_BF16}) {
        // ConvTranspose1d expressed as mul_mat + col2im (DAC decoder upsampling)
        test_cases.emplace_back(new test_col2im_1d(type, 16, 32, 197, 8, 0)); // kernel = 2*stride
        test_cases.emplace_back(new test_col2im_1d(type, 4, 3, 7, 2, 0));
        test_cases.emplace_back(new test_col2im_1d(type, 1, 5, 13, 1, 0));    // stride 1, no overlap
        test_cases.emplace_back(new test_col2im_1d(type, 6, 4, 11, 3, 1));    // with cropping
        test_cases.emplace_back(new test_col2im_1d(type, 2, 3, 9, 3, 0));     // kernel < stride, gap positions are zeroed
        test_cases.emplace_back(new test_col2im_1d(type, 5, 4, 11, 2, 0));    // kernel not a multiple of stride, alternating overlap
        test_cases.emplace_back(new test_col2im_1d(type, 8, 4, 13, 4, 2));    // padding = stride/2 (DAC causal cropping)
        test_cases.emplace_back(new test_col2im_1d(type, 4, 3, 1, 2, 0));     // single column, pure kernel unfold
        test_cases.emplace_back(new test_col2im_1d(type, 16, 1, 197, 8, 0));   // OC = 1, mono output stage
        test_cases.emplace_back(new test_col2im_1d(type, 1, 5, 13, 3, 0));     // K = 1 with stride > 1, sparse scatter
        test_cases.emplace_back(new test_col2im_1d(type, 8, 2, 3, 2, 5));      // cropping eats most of the signal, T_out = 2
    }

    for (ggml_type kernel_type : {GGML_TYPE_F32, GGML_TYPE_F16}) {
        test_cases.emplace_back(new test_conv_transpose_2d({3, 2, 3, 1}, {2, 2, 1, 3}, 1, kernel_type));
        test_cases.emplace_back(new test_conv_transpose_2d({10, 10, 9, 1}, {3, 3, 1, 9}, 2, kernel_type));
        test_cases.emplace_back(new test_conv_transpose_2d({129, 63, 35, 1}, {3, 3, 48, 35}, 1, kernel_type));
        test_cases.emplace_back(new test_conv_transpose_2d({10, 10, 9, 2}, {3, 3, 1, 9}, 2, kernel_type)); // for multiple batches
    }

    test_cases.emplace_back(new test_count_equal(GGML_TYPE_F32, {4,  500, 1, 1}));
    test_cases.emplace_back(new test_count_equal(GGML_TYPE_F32, {4, 5000, 1, 1}));

    test_cases.emplace_back(new test_argmax(GGML_TYPE_F32, {32,    1, 1, 1}));
    test_cases.emplace_back(new test_argmax(GGML_TYPE_F32, {32,  513, 1, 1}));
    test_cases.emplace_back(new test_argmax(GGML_TYPE_F32, {100,  10, 1, 1}));
    test_cases.emplace_back(new test_argmax(GGML_TYPE_F32, {1024, 10, 1, 1}));
    test_cases.emplace_back(new test_argmax(GGML_TYPE_F32, {1024, 12, 1, 1}));
    test_cases.emplace_back(new test_argmax(GGML_TYPE_F32, {2000, 10, 1, 1}));
    test_cases.emplace_back(new test_argmax(GGML_TYPE_F32, {5438,  3, 1, 1}));

    for (int ne3 : {1, 3}) { // CUDA backward pass only supports ne3 == 1
        test_cases.emplace_back(new test_repeat(GGML_TYPE_F32, {10, 5, 4, ne3}, {1, 1, 1, 1}));
        test_cases.emplace_back(new test_repeat(GGML_TYPE_F32, {10, 5, 4, ne3}, {2, 1, 1, 1}));
        test_cases.emplace_back(new test_repeat(GGML_TYPE_F32, {10, 5, 4, ne3}, {1, 2, 1, 1}));
        test_cases.emplace_back(new test_repeat(GGML_TYPE_F32, {10, 5, 4, ne3}, {1, 1, 2, 1}));
        test_cases.emplace_back(new test_repeat(GGML_TYPE_F32, {10, 5, 4, ne3}, {1, 1, 1, 2}));
        test_cases.emplace_back(new test_repeat(GGML_TYPE_F16, {10, 5, 4, ne3}, {2, 1, 1, 1}));
        test_cases.emplace_back(new test_repeat(GGML_TYPE_I32, {10, 5, 4, ne3}, {2, 1, 1, 1}));
        test_cases.emplace_back(new test_repeat(GGML_TYPE_I16, {10, 5, 4, ne3}, {1, 1, 1, 2}));
        test_cases.emplace_back(new test_repeat(GGML_TYPE_BF16, {10, 5, 4, ne3}, {2, 1, 1, 1}));
    }

    for (bool view : {false, true}) {
        test_cases.emplace_back(new test_repeat_back(GGML_TYPE_F32, {8, 6, 4, 2}, {1, 1, 1, 1}, view));
        test_cases.emplace_back(new test_repeat_back(GGML_TYPE_F32, {8, 6, 4, 2}, {2, 1, 1, 1}, view));
        test_cases.emplace_back(new test_repeat_back(GGML_TYPE_F32, {8, 6, 4, 2}, {1, 2, 1, 1}, view));
        test_cases.emplace_back(new test_repeat_back(GGML_TYPE_F32, {8, 6, 4, 2}, {1, 1, 2, 1}, view));
        test_cases.emplace_back(new test_repeat_back(GGML_TYPE_F32, {8, 6, 4, 2}, {1, 1, 1, 2}, view));
    }

    test_cases.emplace_back(new test_dup(GGML_TYPE_F32));
    test_cases.emplace_back(new test_dup(GGML_TYPE_F16));
    test_cases.emplace_back(new test_dup(GGML_TYPE_I32));
    test_cases.emplace_back(new test_dup(GGML_TYPE_I16));
    test_cases.emplace_back(new test_dup(GGML_TYPE_F32, {10, 10, 5, 1}, {0, 2, 1, 3}));
    test_cases.emplace_back(new test_dup(GGML_TYPE_F16, {10, 10, 5, 1}, {0, 2, 1, 3})); // dup by rows
    test_cases.emplace_back(new test_dup(GGML_TYPE_F32, {10, 10, 5, 1}, {1, 0, 2, 3}));
    test_cases.emplace_back(new test_dup(GGML_TYPE_F16, {10, 10, 5, 1}, {1, 0, 2, 3})); // dup dst not-contiguous
    test_cases.emplace_back(new test_dup(GGML_TYPE_I16, {10,  8, 3, 1}, {0, 2, 1, 3}));
    test_cases.emplace_back(new test_dup(GGML_TYPE_I16, {10,  8, 3, 1}, {1, 2, 0, 3}));

    for (int dim = 1; dim < GGML_MAX_DIMS; ++dim) {
        test_cases.emplace_back(new test_set(GGML_TYPE_F32, GGML_TYPE_F32, {6, 5, 4, 3}, dim, false));
        test_cases.emplace_back(new test_set(GGML_TYPE_F32, GGML_TYPE_F32, {6, 5, 4, 3}, dim, true));
    }

    for (int dim = 1; dim < GGML_MAX_DIMS; ++dim) {
        test_cases.emplace_back(new test_set(GGML_TYPE_I32, GGML_TYPE_I32, {6, 5, 4, 3}, dim, false));
        test_cases.emplace_back(new test_set(GGML_TYPE_I32, GGML_TYPE_I32, {6, 5, 4, 3}, dim, true));
    }

    // same-type copy
    for (ggml_type type : all_types) {
        const auto nk = ggml_blck_size(type);

        for (int k = 1; k < 4; ++k) {
            test_cases.emplace_back(new test_cpy(type, type, {k*nk, 2, 3, 4}));
            test_cases.emplace_back(new test_cpy(type, type, {k*nk, 2, 3, 4}, {-1,-1,-1,-1}, {0, 2, 1, 3}));
            test_cases.emplace_back(new test_cpy(type, type, {k*nk, 2, 3, 4}, {-1,-1,-1,-1}, {0, 3, 1, 2}, {0, 2, 1, 3}));
        }
    }

    for (ggml_type type_src : {GGML_TYPE_F16, GGML_TYPE_BF16, GGML_TYPE_F32}) {
        for (ggml_type type_dst : all_types) {
            test_cases.emplace_back(new test_cpy(type_src, type_dst, {256, 4, 4, 4}));
            test_cases.emplace_back(new test_cpy(type_src, type_dst, {256, 2, 3, 4}, {-1,-1,-1,-1}, {0, 2, 1, 3})); // cpy by rows
        }
    }
    for (ggml_type type_src : all_types) {
        for (ggml_type type_dst : {GGML_TYPE_F32}) {
            test_cases.emplace_back(new test_cpy(type_src, type_dst, {256, 4, 4, 4}));
            test_cases.emplace_back(new test_cpy(type_src, type_dst, {256, 2, 3, 4}, {-1,-1,-1,-1}, {0, 2, 1, 3})); // cpy by rows
        }
    }
    for (ggml_type type_src : {GGML_TYPE_F16, GGML_TYPE_F32}) {
        for (ggml_type type_dst : {GGML_TYPE_F16, GGML_TYPE_F32}) {
            test_cases.emplace_back(new test_cpy(type_src, type_dst, {256, 2, 3, 4}, {-1,-1,-1,-1}, {1, 0, 2, 3})); // cpy not-contiguous
        }
    }
    // quant block count not a multiple of the kernel block size
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_Q4_0, {96, 1, 1, 1}));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_Q4_0, GGML_TYPE_F32, {96, 1, 1, 1}));

    // lane-194: q4_0 -> F16 read-back (ARIFI-SYNC-SET cpy.quant_to_f16). This is the path the
    // KV precision tail's F16 compose dispatches, so it is covered at the shapes that path
    // actually produces: a small block-aligned row, a multi-dim contiguous block, and a
    // NON-CONTIGUOUS permuted source - the ring/body segments reach ggml_cast as strided
    // views, and the contiguous case alone would not exercise src0_idx_quant's stride math.
    //
    // F-112 / F-114: these MUST be asserted EXECUTED, not merely green. A missing shader
    // variant makes supports_op decline them and the harness prints a pass banner over zero
    // runs - the exact failure that hid lane-148's SET_ROWS_TURBO4 and lane-150's TQ4_1S.
    // Verify with: test-backend-ops test -o CPY, then read the "NOT SUPPORTED" line.
    test_cases.emplace_back(new test_cpy(GGML_TYPE_Q4_0, GGML_TYPE_F16, {96, 1, 1, 1}));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_Q4_0, GGML_TYPE_F16, {256, 4, 4, 4}));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_Q4_0, GGML_TYPE_F16, {256, 2, 3, 4}, {-1,-1,-1,-1}, {0, 2, 1, 3}));

    // Types the Vulkan supports_op names for CPY that all_types does not carry. Without a case
    // here, a backend that claims one of them with no pipeline behind it stays invisible until a
    // real graph aborts: that is how the ROCmFP SET_ROWS/CPY abort hid. TURBO3_0 is the live one
    // (its copy-to-quant shader is deliberately not generated, only copy-from-quant).
    for (ggml_type type : {GGML_TYPE_Q2_0_G128, GGML_TYPE_TURBO2_0, GGML_TYPE_TURBO3_0, GGML_TYPE_TURBO4_0}) {
        test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, type, {256, 4, 4, 4}));
        test_cases.emplace_back(new test_cpy(type, GGML_TYPE_F32, {256, 4, 4, 4}));
    }
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_I32, {256, 2, 3, 4}));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_I32, {256, 2, 3, 4}, {-1,-1,-1,-1}, {1, 0, 2, 3}));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_I32, GGML_TYPE_F32, {256, 2, 3, 4}));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_I32, GGML_TYPE_F32, {256, 2, 3, 4}, {-1,-1,-1,-1}, {1, 0, 2, 3}));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F16, GGML_TYPE_F16, {256, 4, 3, 1}, {-1,-1,-1,-1}, {0, 0, 0, 0}, {0, 0, 0, 0}, true));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_F32, {256, 4, 3, 1}, {-1,-1,-1,-1}, {0, 0, 0, 0}, {0, 0, 0, 0}, true));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_F32, {256, 4, 3, 3}, {-1,-1,-1,-1}, {0, 0, 0, 0}, {0, 0, 0, 0}, true));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_BF16, GGML_TYPE_BF16, {256, 4, 3, 1}, {-1,-1,-1,-1}, {0, 0, 0, 0}, {0, 0, 0, 0}, true));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F16, GGML_TYPE_F16, {256, 4, 1, 1}, {-1,-1,-1,-1}, {0, 0, 0, 0}, {0, 0, 0, 0}, true));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_F32, {256, 4, 1, 1}, {-1,-1,-1,-1}, {0, 0, 0, 0}, {0, 0, 0, 0}, true));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_BF16, GGML_TYPE_BF16, {256, 4, 1, 1}, {-1,-1,-1,-1}, {0, 0, 0, 0}, {0, 0, 0, 0}, true));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_I32, GGML_TYPE_I32, {256, 4, 1, 1}, {-1,-1,-1,-1}, {0, 0, 0, 0}, {0, 0, 0, 0}, true));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_I32, GGML_TYPE_I32, {256, 1, 4, 1}, {-1,-1,-1,-1}, {1, 2, 0, 3}, {0, 0, 0, 0}));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_F32, {256, 1, 4, 1}, {-1,-1,-1,-1}, {1, 2, 0, 3}, {0, 0, 0, 0}));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_F32, {2, 2097121, 1, 1}, {-1,-1,-1,-1}, {1, 0, 2, 3}));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_F32, {2, 2, 524281, 1}, {-1,-1,-1,-1}, {1, 0, 2, 3}));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_F32, {128, 2, 3, 1}, {128, 2, 3, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}, false, {128, 4, 3, 1})); // strided dst
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F16, GGML_TYPE_F16, {128, 2, 3, 1}, {128, 2, 3, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}, false, {128, 4, 3, 1})); // strided dst

    // CPY - different src/dst shapes (reshaping via CPY)
    // Use permutations of {3, 5, 7, 32}. Total elements: 3*5*7*32 = 3360.
    // Each src permutation is tested against canonical sorted and reverse dst (skip self).
    {
        std::array<int64_t, 4> dims = {3, 5, 7, 32};
        std::sort(dims.begin(), dims.end());
        std::array<int64_t, 4> canonical = dims;
        std::array<int64_t, 4> reversed  = {32, 7, 5, 3};
        for (ggml_type type : {GGML_TYPE_F32, GGML_TYPE_F16}) {
            std::array<int64_t, 4> cur = dims;
            do {
                if (cur != canonical) {
                    test_cases.emplace_back(new test_cpy(type, type, cur, canonical));
                }
                if (cur != reversed) {
                    test_cases.emplace_back(new test_cpy(type, type, cur, reversed));
                }
                if (cur[0] == 32 && type == GGML_TYPE_F32) {
                    if (canonical[0] == 32) {
                        test_cases.emplace_back(new test_cpy(GGML_TYPE_Q4_0, GGML_TYPE_Q4_0, cur, canonical));
                    }
                    if (reversed[0] == 32) {
                        test_cases.emplace_back(new test_cpy(GGML_TYPE_Q4_0, GGML_TYPE_Q4_0, cur, reversed));
                    }
                }
                std::next_permutation(cur.begin(), cur.end());
            } while (cur != canonical);
        }
    }

    // ring-repair 2026-08-24: recurrent-bank plane writes - cpy into an OFFSET view of a
    // larger buffer (conv/ssm snapshot-bank shape). KFLIP showed Vulkan corrupting these
    // while offset-0 writes are clean; these pin it at op level.
    test_cases.emplace_back(new test_scale_view_zero_gather(4096, 6, 0));
    test_cases.emplace_back(new test_scale_view_zero_gather(163840, 6, 0));
    test_cases.emplace_back(new test_scale_view_zero_gather(36864, 12, 3));

    for (int64_t off : {1, 3, 7}) {
        test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_F32, {12288, 1, 1, 1}, {-1,-1,-1,-1},
            {0,0,0,0}, {0,0,0,0}, false, {12288, 8, 1, 1}, off));
        test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_F32, {5760, 2, 1, 1}, {-1,-1,-1,-1},
            {0,0,0,0}, {0,0,0,0}, false, {5760, 16, 1, 1}, off));
        test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_F32, {12, 3, 1, 1}, {-1,-1,-1,-1},
            {0,0,0,0}, {0,0,0,0}, false, {12, 24, 1, 1}, off));
    }

    for (ggml_type type_dst : { GGML_TYPE_F32, GGML_TYPE_I32, GGML_TYPE_F16, GGML_TYPE_BF16 }) {
        for (bool use_view_slice : { true, false }) {
            for (std::array<int64_t, 4> ne : std::initializer_list<std::array<int64_t, 4>>{ {2, 1, 1, 1}, {2, 1, 3, 5},
                {2, 3, 5, 7}, {1, 4, 4, 1}, {1, 8, 17, 1}, {10, 10, 10, 1} }) {
                if (use_view_slice && (type_dst == GGML_TYPE_F16 || type_dst == GGML_TYPE_BF16)) {
                    continue; // TODO: add after WebGPU is fixed
                }
                test_cases.emplace_back(new test_cont(type_dst, ne, use_view_slice));
            }
        }
    }

    for (ggml_type type_dst : { GGML_TYPE_F32, GGML_TYPE_F16 }) {
        for (std::array<int64_t, 4> ne : std::initializer_list<std::array<int64_t, 4>>{
                {10, 10, 10, 1}, {33, 5, 7, 1}, {64, 3, 65, 1}, {2, 3, 5, 7},
                // large, tile-aligned and tile-unaligned, matching the perf cases
                {1024, 64, 64, 1}, {2304, 64, 64, 1}, {1000, 33, 65, 1} }) {
            for (std::array<int64_t, 4> perm : std::initializer_list<std::array<int64_t, 4>>{
                    {2, 1, 0, 3},   // 0<->2 swap
                    {1, 2, 0, 3},   // 3-cycle
                    {0, 2, 1, 3} }) {
                test_cases.emplace_back(new test_cont(type_dst, ne, false, perm));
            }
        }
    }

    auto add_test_bin_bcast = [&](ggml_type type, std::array<int64_t, 4> ne, std::array<int, 4> nr, bool perm1 = false, bool src_overlap = false) {
        for (auto op : {ggml_add, ggml_sub, ggml_mul, ggml_div}) {
            test_cases.emplace_back(new test_bin_bcast(op, type, ne, nr, 1, perm1, src_overlap));
        }
    };
    for (ggml_type type : {GGML_TYPE_F16, GGML_TYPE_F32}) {
        for (bool perm1 : {false, true}) {
            add_test_bin_bcast(type, {1,  1,   8,   1}, {1,  1, 1, 1}, perm1);
            add_test_bin_bcast(type, {1,  1,   1,   1}, {32, 1, 1, 1}, perm1);
            add_test_bin_bcast(type, {1,  1, 320, 320}, {1,  1, 1, 1}, perm1);
            add_test_bin_bcast(type, {10, 5,   1,   1}, {1,  1, 1, 1}, perm1);
            add_test_bin_bcast(type, {10, 5,   4,   1}, {1,  1, 1, 1}, perm1);
            add_test_bin_bcast(type, {10, 5,   4,   3}, {1,  1, 1, 1}, perm1);
            add_test_bin_bcast(type, {10, 5,   4,   3}, {2,  1, 1, 1}, perm1);
            add_test_bin_bcast(type, {10, 5,   4,   3}, {1,  2, 1, 1}, perm1);
            add_test_bin_bcast(type, {10, 5,   4,   3}, {1,  1, 2, 1}, perm1);
            add_test_bin_bcast(type, {10, 5,   4,   3}, {1,  1, 1, 2}, perm1);
            add_test_bin_bcast(type, {10, 5,   4,   3}, {1,  1, 2, 2}, perm1);
            add_test_bin_bcast(type, {10, 5,   4,   3}, {1,  2, 2, 2}, perm1);
            add_test_bin_bcast(type, {10, 5,   4,   3}, {2,  2, 2, 2}, perm1);
        }

        // src_overlap
        add_test_bin_bcast(type, {10, 5, 4, 6}, {1, 1, 1, 1}, false, true);
        add_test_bin_bcast(type, {10, 5, 4, 5}, {1, 1, 1, 1}, false, true);
        add_test_bin_bcast(type, {1, 1, 120, 120}, {1, 1, 1, 1}, false, true);
        add_test_bin_bcast(type, {1, 1, 4, 320}, {1, 1, 1, 1}, false, true);

        // test case for k_bin_bcast_unravel in CUDA backend
        add_test_bin_bcast(type, {1, 1, 65536, 1}, {256, 1, 1, 1});

        // stable diffusion
        add_test_bin_bcast(type, {1280, 1, 1, 1}, {1, 1, 1, 1});
        add_test_bin_bcast(type, {1280, 1, 1, 1}, {1, 16, 16, 1});
        add_test_bin_bcast(type, {1280, 16, 16, 1}, {1, 1, 1, 1});
        add_test_bin_bcast(type, {1280, 1, 1, 1}, {1, 256, 1, 1});
        add_test_bin_bcast(type, {1, 1, 1280, 1}, {16, 16, 1, 1});
        add_test_bin_bcast(type, {16, 16, 1280, 1}, {1, 1, 1, 1});
        add_test_bin_bcast(type, {1, 1, 1920, 1}, {16, 16, 1, 1});
        add_test_bin_bcast(type, {1, 1, 2560, 1}, {16, 16, 1, 1});
        add_test_bin_bcast(type, {1, 1, 1280, 1}, {32, 32, 1, 1});
        add_test_bin_bcast(type, {1, 1, 1920, 1}, {32, 32, 1, 1});
        add_test_bin_bcast(type, {1, 1, 640, 1}, {32, 32, 1, 1});
        add_test_bin_bcast(type, {5120, 1, 1, 1}, {1, 256, 1, 1});
        add_test_bin_bcast(type, {640, 1, 1, 1}, {1, 1, 1, 1});
        add_test_bin_bcast(type, {64, 262144, 1, 1}, {1, 1, 1, 1});
        //add_test_bin_bcast(type, {3, 3, 2560, 1280}, {1, 1, 1, 1});
        //add_test_bin_bcast(type, {3, 3, 2560, 1280}, {2, 1, 1, 1});
    }

    // single inplace tests, especially important for WebGPU backend since kernels for inplace vs. not are different
    test_cases.emplace_back(new test_bin_bcast(ggml_add_inplace, GGML_TYPE_F32, {16, 5, 4, 3}, {1, 1, 1, 1}, 16));
    test_cases.emplace_back(new test_bin_bcast(ggml_mul_inplace, GGML_TYPE_F32, {16, 5, 4, 3}, {1, 1, 1, 1}, 16));
    test_cases.emplace_back(new test_bin_bcast(ggml_sub_inplace, GGML_TYPE_F32, {16, 5, 4, 3}, {1, 1, 1, 1}, 16));
    test_cases.emplace_back(new test_bin_bcast(ggml_div_inplace, GGML_TYPE_F32, {16, 5, 4, 3}, {1, 1, 1, 1}, 16));

    // fusion
    test_cases.emplace_back(new test_bin_bcast(ggml_add, GGML_TYPE_F32, {10, 5, 4, 3}, {2, 1, 1, 1}, 2));
    test_cases.emplace_back(new test_bin_bcast(ggml_add, GGML_TYPE_F16, {10, 5, 4, 3}, {2, 1, 1, 1}, 2));
    test_cases.emplace_back(new test_bin_bcast(ggml_add, GGML_TYPE_F32, {16, 5, 4, 3}, {1, 1, 1, 1}, 2, true));
    test_cases.emplace_back(new test_bin_bcast(ggml_add, GGML_TYPE_F32, {16, 5, 4, 3}, {1, 2, 1, 1}, 3));
    test_cases.emplace_back(new test_bin_bcast(ggml_add, GGML_TYPE_F32, {10, 5, 4, 3}, {1, 1, 2, 1}, 4));
    test_cases.emplace_back(new test_bin_bcast(ggml_add, GGML_TYPE_F32, {16, 5, 4, 3}, {1, 1, 1, 2}, 5));
    test_cases.emplace_back(new test_bin_bcast(ggml_add, GGML_TYPE_F32, {10, 5, 4, 3}, {1, 1, 2, 2}, 6));
    test_cases.emplace_back(new test_bin_bcast(ggml_add, GGML_TYPE_F32, {10, 5, 4, 3}, {1, 2, 2, 2}, 7));
    test_cases.emplace_back(new test_bin_bcast(ggml_add, GGML_TYPE_F32, {16, 5, 4, 3}, {2, 2, 2, 2}, 8));
    test_cases.emplace_back(new test_bin_bcast(ggml_add, GGML_TYPE_F32, {16, 5, 4, 3}, {1, 1, 1, 1}, 16));

    test_cases.emplace_back(new test_scale());
    test_cases.emplace_back(new test_scale(GGML_TYPE_F32, {10, 10, 10, 10}, 2.0f, 1.0f));
    test_cases.emplace_back(new test_scale(GGML_TYPE_F32, {10, 10, 10, 10}, 2.0f, 1.0f, true)); // inplace test
    test_cases.emplace_back(new test_scale(GGML_TYPE_F32, {100, 10, 10, 10}, 2.0f, 1.0f));
    test_cases.emplace_back(new test_softcap(GGML_TYPE_F32, {10, 10, 10, 10}, 50.0f));
    test_cases.emplace_back(new test_silu_back());

    for (float eps : { 0.0f, 1e-6f, 1e-4f, 1e-1f, 10.f }) {
        for (uint32_t n : { 64, 1025 }) {
            for (bool v : { false, true }) {
                test_cases.emplace_back(new test_norm(GGML_TYPE_F32, { n, 5, 4, 3 }, v, eps));
                test_cases.emplace_back(new test_rms_norm(GGML_TYPE_F32, { n, 5, 4, 3 }, v, eps));
            }
            test_cases.emplace_back(new test_norm(GGML_TYPE_F32, { n, 5, 4, 3 }, false, eps, true));
            test_cases.emplace_back(new test_norm_scale(GGML_TYPE_F32, { n, 5, 4, 3 }, eps, false, 1.5f));
            test_cases.emplace_back(new test_norm_scale(GGML_TYPE_F32, { n, 5, 4, 3 }, eps, true, 1.5f));
            test_cases.emplace_back(new test_rms_norm_back(GGML_TYPE_F32, { n, 5, 4, 3 }, eps));
            test_cases.emplace_back(new test_l2_norm(GGML_TYPE_F32, { n, 5, 4, 3 }, eps, false));
            test_cases.emplace_back(new test_l2_norm(GGML_TYPE_F32, { n, 5, 4, 3 }, eps, true));
            test_cases.emplace_back(new test_l2_norm(GGML_TYPE_F32, { n, 5, 4, 3 }, eps, false, true));
            // sibling batching: strided (production shape) and contiguous, 2 and 4 wide
            test_cases.emplace_back(new test_l2_norm_batch(GGML_TYPE_F32, { n, 5, 4, 3 }, 2, eps, true));
            test_cases.emplace_back(new test_l2_norm_batch(GGML_TYPE_F32, { n, 5, 4, 3 }, 4, eps, true));
            test_cases.emplace_back(new test_l2_norm_batch(GGML_TYPE_F32, { n, 5, 4, 3 }, 4, eps, false));
        }
        // row lengths that are not a multiple of 32, for the scalar (33) and float4 (132, 260) paths
        for (uint32_t n : { 33, 132, 260 }) {
            for (bool v : { false, true }) {
                test_cases.emplace_back(new test_norm(GGML_TYPE_F32, { n, 5, 4, 3 }, v, eps));
                test_cases.emplace_back(new test_rms_norm(GGML_TYPE_F32, { n, 5, 4, 3 }, v, eps));
            }
        }
    }

    // in-place tests
    test_cases.emplace_back(new test_rms_norm(GGML_TYPE_F32, {64, 5, 4, 3}, false, 1e-6f, true));

    for (ggml_type set_rows_type : { GGML_TYPE_F32, GGML_TYPE_F16 }) {
        test_cases.emplace_back(new test_rms_norm_mul_rope({ 256, 1, 1, 1 }, 1e-6f, false, true, false, GGML_ROPE_TYPE_NORMAL, false, false, set_rows_type));
        test_cases.emplace_back(new test_rms_norm_mul_rope({ 128, 4, 3, 1 }, 1e-6f, false, true, false, GGML_ROPE_TYPE_NORMAL, false, false, set_rows_type));
    }

    for (float eps : { 0.0f, 1e-6f, 1e-4f, 1e-1f, 1.0f }) {
        for (uint32_t n : { 64, 1025 }) {
            test_cases.emplace_back(new test_rms_norm_mul_add(GGML_TYPE_F32, { n, 5, 4, 3 }, eps, false));
            test_cases.emplace_back(new test_rms_norm_mul_add(GGML_TYPE_F32, { n, 5, 4, 3 }, eps, true));
            test_cases.emplace_back(new test_norm_mul_add(GGML_TYPE_F32, { n, 5, 4, 3 }, eps, false));
            test_cases.emplace_back(new test_norm_mul_add(GGML_TYPE_F32, { n, 5, 4, 3 }, eps, true));
            test_cases.emplace_back(new test_add_rms_norm(GGML_TYPE_F32, { n, 5, 4, 3 }, eps, false));
            test_cases.emplace_back(new test_add_rms_norm(GGML_TYPE_F32, { n, 5, 4, 3 }, eps, true));
        }
    }
    for (uint32_t n : {1, 511, 1025, 8192, 33*512}) {
        for (bool multi_add : {false, true}) {
            test_cases.emplace_back(new test_rms_norm_mul_add(GGML_TYPE_F32, {n, 1, 1, 1}, 1e-6f, false, multi_add));
        }
        test_cases.emplace_back(new test_add_rms_norm(GGML_TYPE_F32, {n, 1, 1, 1}, 1e-6f, false));
    }
    for (uint32_t n : {64, 1025}) {
        test_cases.emplace_back(new test_add_add(GGML_TYPE_F32, GGML_TYPE_F32, { n, 5, 4, 3 }, false, false));
        test_cases.emplace_back(new test_add_add(GGML_TYPE_F32, GGML_TYPE_F32, { n, 5, 4, 3 }, true, false));
        test_cases.emplace_back(new test_add_add(GGML_TYPE_F32, GGML_TYPE_F32, { n, 5, 4, 3 }, false, true));
        test_cases.emplace_back(new test_add_add(GGML_TYPE_F16, GGML_TYPE_F16, { n, 5, 4, 3 }, false, false));
        test_cases.emplace_back(new test_add_add(GGML_TYPE_F16, GGML_TYPE_F32, { n, 5, 4, 3 }, false, false));
        test_cases.emplace_back(new test_add_add(GGML_TYPE_F16, GGML_TYPE_F32, { n, 5, 4, 3 }, true, false));
    }

    test_cases.emplace_back(new test_rms_norm_mul_add(GGML_TYPE_F32, { 1536, 1, 1, 1 }, 1e-6f, false, false, true));
    test_cases.emplace_back(new test_rms_norm_mul_add(GGML_TYPE_F32, { 256, 4, 1, 1 }, 1e-6f, false, false, true));
    test_cases.emplace_back(new test_rms_norm_mul_add(GGML_TYPE_F32, { 256, 4, 3, 2 }, 1e-6f, false, false, true));
    test_cases.emplace_back(new test_rms_norm_mul_add(GGML_TYPE_F32, { 256, 4, 3, 2 }, 1e-6f, false, false, true, false, true));
    test_cases.emplace_back(new test_rms_norm_mul_add(GGML_TYPE_F32, { 1536, 1, 1, 1 }, 1e-6f, false, false, false, true));
    test_cases.emplace_back(new test_rms_norm_mul_add(GGML_TYPE_F32, { 256, 4, 1, 1 }, 1e-6f, false, false, false, true));

    test_cases.emplace_back(new test_rms_norm_mul_rope({128, 4, 7, 2}));
    test_cases.emplace_back(new test_rms_norm_mul_rope({128, 4, 7, 2}, 1e-6f, false, true));

    for (auto multi_add : {false, true}) {
        for (auto set_rows : {false, true}) {
            for (auto broadcast : {false, true}) {
                for (auto rope : {GGML_ROPE_TYPE_NORMAL, GGML_ROPE_TYPE_NEOX, GGML_ROPE_TYPE_IMROPE}) {
                    test_cases.emplace_back(new test_rms_norm_mul_rope({768, 1, 1, 1}, 1e-6f, multi_add, set_rows, broadcast, rope));
                    test_cases.emplace_back(new test_rms_norm_mul_rope({768, 3, 1, 1}, 1e-6f, multi_add, set_rows, broadcast, rope));
                    test_cases.emplace_back(new test_rms_norm_mul_rope({768, 3, 5, 1}, 1e-6f, multi_add, set_rows, broadcast, rope));
                    test_cases.emplace_back(new test_rms_norm_mul_rope({128, 32, 2, 1}, 1e-6f, multi_add, set_rows, broadcast, rope));
                    test_cases.emplace_back(new test_rms_norm_mul_rope({128, 4, 2, 1}, 1e-6f, multi_add, set_rows, broadcast, rope));
                    test_cases.emplace_back(new test_rms_norm_mul_rope({128, 32, 50, 1}, 1e-6f, multi_add, set_rows, broadcast, rope));
                    test_cases.emplace_back(new test_rms_norm_mul_rope({128, 4, 50, 1}, 1e-6f, multi_add, set_rows, broadcast, rope));
                    test_cases.emplace_back(new test_rms_norm_mul_rope({8192, 2, 2, 1}, 1e-6f, multi_add, set_rows, broadcast, rope));
                    test_cases.emplace_back(new test_rms_norm_mul_rope({8192, 2, 2, 1}, 1e-6f, multi_add, set_rows, broadcast, rope));
                }
            }
        }
    }
    // ArifiLabs Escha-W2 fused linear (lane-164): mat-vec + batched, both K rates,
    // plus the real 27B projection shapes.
    // (these were previously nested in the ssm_conv loops below and registered 9x each)
    test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA2, 512, 256, 1));
    test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA2, 512, 256, 4));
    test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA3, 512, 256, 1));
    test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA3, 512, 256, 4));
    test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA2, 5120, 12288, 2));
    test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA3, 17408, 5120, 2));
    // ncols that cross several Vulkan column-chunks (ESCHA_MM_COLS_PER_DISPATCH=32) AND leave a
    // ragged tail, so col_offset and the tail guards are actually exercised. Without these the
    // chunk loop runs a single iteration with col_offset==0 and the guard is never tested.
    // 205 = 6*32 + 13, 133 = 4*32 + 5 — both tails are also partial column-blocks for C>1.
    test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA2, 512, 256, 205));
    test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA3, 512, 256, 133));
    // ncols=9 at the real 27B shapes: column-blocked kernel AT MODEL DEPTH.
    // 9 = one full 8-wide block plus a 1-wide tail.
    test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA2, 5120, 12288, 9));
    test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA3, 17408, 5120, 9));

    // COLUMN LADDER COVERAGE (seat-40). The Vulkan backend now selects among C = 1/2/4/8/16 by
    // column count, so a case only proves the rung its ncols happens to select. Every rung needs a
    // FULL block and a RAGGED tail, or the tail clamp and the store guard go unchecked on that rung.
    //
    // This is written against the SELECTION RULE, not against a remembered threshold: the previous
    // comment here claimed the ncols=2 cases "exercise the one-column kernel", which was true when
    // the floor was 8 and became false the moment it dropped to 2. A test comment that names a
    // constant rots as soon as the constant moves; naming the rung it lands on does not.
    //   ncols 2 -> C2 full     3 -> C4 ragged     4 -> C4 full
    //   ncols 5 -> C8 ragged   8 -> C8 full      16 -> C16 full     17 -> C16 + 1-wide tail
    for (int64_t ncols : {3, 5, 16, 17}) {
        test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA2, 512, 256, ncols));
        test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA3, 512, 256, ncols));
    }
    // and the two widest rungs at model depth, where register pressure is real
    test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA2, 5120, 12288, 16));
    test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA3, 17408, 5120, 17));
    // PREFILL RUNGS. C=32 and C=64 exist for prefill, where one decode is shared across far more
    // columns. Every case above tops out at 17 columns and therefore selects C=16, so without these
    // the two widest rungs ship untested - a sweep that passes while never entering the code it is
    // supposed to clear. Full block and ragged tail for each.
    for (int64_t ncols : {32, 33, 64, 65}) {
        test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA2, 512, 256, ncols));
        test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA3, 512, 256, ncols));
    }
    test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA2, 5120, 12288, 32));
    test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA3, 17408, 5120, 64));

    for (int64_t d_conv : {3, 4, 9}) {
        for (int64_t d_inner: {1024, 1536, 2048}) {
            test_cases.emplace_back(new test_ssm_conv(GGML_TYPE_F32, {d_conv, d_inner, 1, 1}, {d_conv, d_inner, 1, 1}));
            test_cases.emplace_back(new test_ssm_conv(GGML_TYPE_F32, {2 * d_conv, d_inner, 1, 1}, {d_conv, d_inner, 1, 1}));
            test_cases.emplace_back(new test_ssm_conv(GGML_TYPE_F32, {d_conv, d_inner, 4, 1}, {d_conv, d_inner, 1, 1}));
            // long token (n_t > 32, exercises the long_token kernel path)
            test_cases.emplace_back(new test_ssm_conv(GGML_TYPE_F32, {d_conv - 1 + 64, d_inner, 1, 1}, {d_conv, d_inner, 1, 1}));
            test_cases.emplace_back(new test_ssm_conv(GGML_TYPE_F32, {d_conv - 1 + 64, d_inner, 4, 1}, {d_conv, d_inner, 1, 1}));
        }
    }

    // fused ssm_conv + (optional) bias_add + silu. The bias-only graph (no silu) is intentionally
    // not tested since there's no fusion for that pattern in ggml_cuda_can_fuse.
    for (int64_t d_conv : {3, 4, 9}) {
        for (int64_t d_inner : {1024, 1536, 2048}) {
            for (bool fuse_bias : {false, true}) {
                // short token path (n_t <= 32)
                test_cases.emplace_back(new test_ssm_conv_bias_silu(
                    GGML_TYPE_F32, {d_conv, d_inner, 1, 1}, {d_conv, d_inner, 1, 1}, fuse_bias));
                test_cases.emplace_back(new test_ssm_conv_bias_silu(
                    GGML_TYPE_F32, {2 * d_conv, d_inner, 1, 1}, {d_conv, d_inner, 1, 1}, fuse_bias));
                test_cases.emplace_back(new test_ssm_conv_bias_silu(
                    GGML_TYPE_F32, {d_conv, d_inner, 4, 1}, {d_conv, d_inner, 1, 1}, fuse_bias));
                // long token path (n_t > 32)
                test_cases.emplace_back(new test_ssm_conv_bias_silu(
                    GGML_TYPE_F32, {d_conv - 1 + 64, d_inner, 1, 1}, {d_conv, d_inner, 1, 1}, fuse_bias));
                test_cases.emplace_back(new test_ssm_conv_bias_silu(
                    GGML_TYPE_F32, {d_conv - 1 + 64, d_inner, 4, 1}, {d_conv, d_inner, 1, 1}, fuse_bias));
            }
        }
    }

    test_cases.emplace_back(new test_ssm_scan(GGML_TYPE_F32, 16, 1, 1024, 1, 32, 4)); // Mamba-1
    test_cases.emplace_back(new test_ssm_scan(GGML_TYPE_F32, 128, 64, 16, 2, 32, 4)); // Mamba-2
    test_cases.emplace_back(new test_ssm_scan(GGML_TYPE_F32, 256, 64,  8, 2, 32, 4)); // Falcon-H1
    test_cases.emplace_back(new test_ssm_scan(GGML_TYPE_F32, 128, 128, 4, 4, 16, 2, true)); // x/B/C overlap
    test_cases.emplace_back(new test_ssm_scan(GGML_TYPE_F32, 128, 80, 128, 1, 256, 1)); // Nemotron-9B SSD path
    test_cases.emplace_back(new test_ssm_scan(GGML_TYPE_F32, 128, 80, 128, 1, 512, 1)); // Nemotron-9B SSD multi-chunk (2 aligned chunks)
    test_cases.emplace_back(new test_ssm_scan(GGML_TYPE_F32, 128, 64, 80, 8, 300, 2)); // Mamba-2 SSD multi-chunk (partial 2nd chunk, 2 seqs)
    test_cases.emplace_back(new test_ssm_scan(GGML_TYPE_F32, 128, 64, 16, 2, 4, 2, false, /*K=*/4)); // Mamba-2 rollback snapshots
    test_cases.emplace_back(new test_ssm_scan(GGML_TYPE_F32, 128, 64, 16, 2, 8, 2, false, /*K=*/3)); // Mamba-2 rollback overflow
    test_cases.emplace_back(new test_ssm_scan_rollback(GGML_TYPE_F32, 128, 64, 16, 2, 8, 2, /*K=*/3)); // rollback snapshots match prefix states
    test_cases.emplace_back(new test_ssm_scan(GGML_TYPE_F32, 128, 64, 16, 2, 64, 4)); // Metal SSD one chunk MMA only, no seq tail
    test_cases.emplace_back(new test_ssm_scan(GGML_TYPE_F32, 128, 64, 16, 2, 65, 2)); // SSD one chunk + 1-token sequential tail
    test_cases.emplace_back(new test_ssm_scan(GGML_TYPE_F32, 128, 64, 16, 2, 128, 2)); // SSD multi-chunk, no tail (exercises the chunk-to-chunk state handoff)
    test_cases.emplace_back(new test_ssm_scan(GGML_TYPE_F32, 128, 64, 16, 2, 128, 2, false, /*K=*/1, /*weak_decay=*/true)); // SSD multi-chunk, carried state not numerically negligible

    test_cases.emplace_back(new test_rwkv_wkv6(GGML_TYPE_F32, 32, 64, 1, 1));
    test_cases.emplace_back(new test_rwkv_wkv6(GGML_TYPE_F32, 32, 64, 32, 1));
    test_cases.emplace_back(new test_rwkv_wkv6(GGML_TYPE_F32, 32, 64, 32, 4));
    test_cases.emplace_back(new test_rwkv_wkv6(GGML_TYPE_F32, 32, 64, 128, 4));

    test_cases.emplace_back(new test_rwkv_wkv7(GGML_TYPE_F32, 32, 64, 1, 1));
    test_cases.emplace_back(new test_rwkv_wkv7(GGML_TYPE_F32, 32, 64, 1, 4));
    test_cases.emplace_back(new test_rwkv_wkv7(GGML_TYPE_F32, 32, 64, 32, 1));
    test_cases.emplace_back(new test_rwkv_wkv7(GGML_TYPE_F32, 32, 64, 32, 4));
    test_cases.emplace_back(new test_rwkv_wkv7(GGML_TYPE_F32, 32, 64, 128, 4));

    test_cases.emplace_back(new test_gla(GGML_TYPE_F32, 32, 64, 1, 1));
    test_cases.emplace_back(new test_gla(GGML_TYPE_F32, 32, 64, 32, 1));
    test_cases.emplace_back(new test_gla(GGML_TYPE_F32, 32, 64, 32, 4));
    test_cases.emplace_back(new test_gla(GGML_TYPE_F32, 32, 64, 128, 4));

    // FWHT tests
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F32, 128, 1, 128));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F32, 64, 1, 64));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F32, 256, 1, 256));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F32, 512, 1, 512));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F32, 128, 32, 128));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F32, 128, 4, 128, {2, 3}));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F32, 256, 512, 256)); // many rows
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F32, 32, 1, 32)); // too small (N<64)
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F32, 1024, 1, 1024)); // too big (N>512)
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F16, 64, 1, 64));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F16, 128, 1, 128));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F16, 256, 1, 256));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F16, 512, 1, 512));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F16, 128, 32, 128));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F16, 128, 4, 128, {2, 3}));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F16, 256, 512, 256)); // many rows

#if 0
    // > 4GB A matrix. Too slow to be enabled by default.
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F16,  900000,  3, 2592, {1, 1}, {1, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F16, 1700000, 96, 2592, {1, 1}, {1, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F16, 1700000,  3, 2592, {1, 1}, {1, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F16, 1700000,  1, 2592, {1, 1}, {1, 1}));

    test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_Q8_0, GGML_TYPE_F32, 128, 128, false, 8192, 2, 5120)); // Llama-4-Maverick-17B-128E-PAB-Q8_0
    test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_Q8_0, GGML_TYPE_F32, 128, 128, false, 8192, 1, 5120)); // Llama-4-Maverick-17B-128E-PAB-Q8_0
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q8_0, GGML_TYPE_F32, 8192, 1, 5120, {128, 1}, {1, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q8_0, GGML_TYPE_F32, 8192, 512, 5120, {128, 1}, {1, 1}));
#endif

    for (ggml_type type_a : all_types) {
        for (int i = 1; i < 10; ++i) {
            test_cases.emplace_back(new test_mul_mat(type_a,    GGML_TYPE_F32, 16,  i, 1*256, { 1,  1}, {1, 1}));
            //test_cases.emplace_back(new test_mul_mat(type_a,    GGML_TYPE_F32, 12,  i, 2*256, { 2,  1}, {1, 1}));
            //test_cases.emplace_back(new test_mul_mat(type_a,    GGML_TYPE_F32, 11,  i, 3*256, { 1,  3}, {5, 1}));
            //test_cases.emplace_back(new test_mul_mat(type_a,    GGML_TYPE_F32, 13,  i, 4*256, { 2,  3}, {1, 1}));
            //test_cases.emplace_back(new test_mul_mat(type_a,    GGML_TYPE_F32, 17,  i, 31*256, { 4,  1}, {1, 1}));
            //test_cases.emplace_back(new test_mul_mat(type_a,    GGML_TYPE_F32, 18,  i, 32*256, { 1,  1}, {8, 1}));
            //test_cases.emplace_back(new test_mul_mat(type_a,    GGML_TYPE_F32, 19,  i, 33*256, { 1,  1}, {1, 1}));
        }
        // mat-vec shaders split k across lanes and loop over the blocks in strides. k must be
        // long enough that the loop wraps, else the stride is never exercised
        test_cases.emplace_back(new test_mul_mat(type_a,    GGML_TYPE_F32, 16,  1, 16*256, { 1,  1}, {1, 1}));
        test_cases.emplace_back(new test_mul_mat(type_a,    GGML_TYPE_F32, 16,  8, 16*256, { 1,  1}, {1, 1}));
    }

    // Multi-column MMVQ coverage for the Q4_K weight-reuse path and a Q5_K control.
    for (ggml_type type_a : { GGML_TYPE_Q4_K, GGML_TYPE_Q5_K }) {
        for (int n = 1; n <= 8; ++n) {
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32, 4096, n, 1024, { 1, 1 }, { 1, 1 }));
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32, 1023, n, 4096, { 1, 1 }, { 1, 1 }));
        }
    }

    // ArifiLabs lane-235 / R48b: the A-side hoist restructures the Vulkan MMVQ mat-vec nest for
    // EVERY type that has a q8_1 integer-dot pipeline, so every one of them needs the real 27B
    // mat-vec shapes at every served width, not just the two types the block above covers.
    //
    // The type list is exactly the set with a mul_mat_vec_<t>_q8_1_f32 pipeline
    // (ggml-vulkan.cpp, the GGML_VULKAN_INTEGER_DOT_GLSLC_SUPPORT block). n = 1..8 covers the
    // NUM_COLS spec-constant range (mul_mat_vec_max_cols = 8) including the NUM_COLS == 7 tail.
    //
    // These rows only REACH the changed shader with GGML_VK_FORCE_MMVQ=1: ggml_vk_should_use_mmvq()
    // refuses Q6_K on non-Intel, gates ROCMFP4_FAST to n = 3 and 5, and drops Q5_K at every n > 1
    // on AMD. Without the force most of this sweep silently measures the f32 dequant shader
    // instead. Two of these went by default in lane-235 / R48b phase 2, where the A-side hoist is
    // live: S-X8 takes MMVQ at every width (GGML_ARIFI_SX8_MMVQ=0 hands it back) and Q4_K takes it
    // at n >= 5 when k <= 8192, so the 17408x5120 and 248320x5120 q4_K rows are on the real route
    // without the force and the 5120x17408 rows (k = 17408) still need it.
    for (ggml_type type_a : {
            GGML_TYPE_Q2_0, GGML_TYPE_Q2_0_G128,
            GGML_TYPE_Q4_0, GGML_TYPE_Q4_1, GGML_TYPE_Q5_0, GGML_TYPE_Q5_1, GGML_TYPE_Q8_0,
            GGML_TYPE_MXFP4, GGML_TYPE_Q4_0_ROCMFP4_FAST,
            GGML_TYPE_Q2_K, GGML_TYPE_Q3_K, GGML_TYPE_Q4_K, GGML_TYPE_Q5_K, GGML_TYPE_Q6_K,
            GGML_TYPE_IQ1_S, GGML_TYPE_IQ1_M,
            GGML_TYPE_SX8 }) {
        for (int n = 1; n <= 8; ++n) {
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32,  17408, n,  5120, { 1, 1 }, { 1, 1 }));
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32,   5120, n, 17408, { 1, 1 }, { 1, 1 }));
            // The embed/output shape. 17 types x 8 widths at 248320x5120 is the expensive third of
            // this sweep on a one-pool box; narrow it with -p "m=248320" when that matters.
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32, 248320, n,  5120, { 1, 1 }, { 1, 1 }));
        }
    }

    // ArifiLabs lane-249 / R61: the iquant types at the EMBED/OUTPUT shape (248320x5120), which had
    // no eval row at all. The loop above is the q8_1-MMVQ type set, and iq3_s / iq3_xxs / iq4_xs
    // have NO q8_1 mat-vec pipeline (the b_type == GGML_TYPE_Q8_1 switch in
    // ggml_vk_get_dequantize_mul_mat_vec falls to `default: return nullptr`), so they were never
    // covered there. These three carry 318 tensors of the 27B GSQ-RCO file between them
    // (IQ3_S 144, IQ4_XS 96, IQ3_XXS 78), so the CORRECTNESS coverage is owed.
    //
    // SYNTHETIC SHAPE, NOT A SERVED ONE (Sol MEDIUM review 2026-09-16 §A): the GSQ-RCO file's
    // output.weight is Q4_K, so an iq3/iq4 row at 248320x5120 does not represent that file's
    // lm_head and carries NO performance verdict. It is coverage of a shape the shader must still
    // be correct on, nothing more.
    //
    // The matching PERF rows are added in the same commit (green-but-blind: an eval row with no
    // perf row measures nothing, a perf row with no eval row proves nothing).
    for (ggml_type type_a : {GGML_TYPE_IQ3_S, GGML_TYPE_IQ3_XXS, GGML_TYPE_IQ4_XS}) {
        for (int n = 1; n <= 8; ++n) {
            // The two FFN orientations the GSQ-RCO file really carries these types at, and the
            // synthetic lm_head shape. Every perf row added for these types in the perf list has
            // its correctness twin here.
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32,  17408, n,  5120, { 1, 1 }, { 1, 1 }));
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32,   5120, n, 17408, { 1, 1 }, { 1, 1 }));
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32, 248320, n,  5120, { 1, 1 }, { 1, 1 }));
        }
    }

    // The SYCL backend picks between one and two output rows per subgroup by row count when there
    // are two destination columns (Q4_K_MMVQ_ROW_PAIR_MIN_NROWS in ggml-sycl/mmvq.cpp). Cover both
    // sides of that boundary, including an odd row count above it for the row-pair tail.
    for (int64_t m : {6271, 6272, 6273}) {
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q4_K, GGML_TYPE_F32, m, 2, 1024, { 1, 1 }, { 1, 1 }));
    }

    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q4_0, GGML_TYPE_F32, 2880, 32, 2880, {1, 1}, {1, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q8_0, GGML_TYPE_F32, 2880, 32, 2880, {1, 1}, {1, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_MXFP4, GGML_TYPE_F32, 2880, 32, 2880, {1, 1}, {1, 1}));

    // TQ4_1S: Gemma-4 E2B dimensions. The fused mul_mat_vec kernel has a
    // shared-memory WHT on the activation and dequantizes centroid*scale per
    // thread; bugs in the butterfly or reduction only surface at production sizes.
    for (int k : { 1536, 2048, 2304, 3072, 4096 }) {
        for (int m : { 256, 1152, 1536, 2048, 5120, 6144 }) {
            for (int n : { 1, 2, 4, 8 }) {
                test_cases.emplace_back(new test_mul_mat(GGML_TYPE_TQ4_1S, GGML_TYPE_F32, m, n, k, {1, 1}, {1, 1}));
                test_cases.emplace_back(new test_mul_mat(GGML_TYPE_TQ4_1S, GGML_TYPE_F16, m, n, k, {1, 1}, {1, 1}));
            }
        }
    }

    // TQ4_1S: large-batch MUL_MAT exercises the dequant + f16 matmul path used
    // during prompt processing (n > mul_mat_vec_max_cols = 8 forces this path).
    // The fused mul_mat_vec kernel is NOT used for these cases; instead the weights
    // are dequantized via pipeline_dequant[TQ4_1S] into a temporary f16 buffer and
    // then the generic f16 matmul runs on them.
    for (int k : { 1536, 2048 }) {
        for (int m : { 256, 1536, 2048 }) {
            for (int n : { 16, 64, 256 }) {
                test_cases.emplace_back(new test_mul_mat(GGML_TYPE_TQ4_1S, GGML_TYPE_F32, m, n, k, {1, 1}, {1, 1}));
            }
        }
    }

    // TQ3_1S: same two sweeps as TQ4_1S above, for the 3-bit sibling type.
    // TQ3_1S packs 8 indices per 3 bytes rather than 2 per byte, so the
    // per-thread unpack differs; the shared-memory WHT on the activation and
    // the 32-thread workgroup are identical. Bugs in the packing offset only
    // surface once k is large enough to cross many block boundaries.
    for (int k : { 1536, 2048, 2304, 3072, 4096 }) {
        for (int m : { 256, 1152, 1536, 2048, 5120, 6144 }) {
            for (int n : { 1, 2, 4, 8 }) {
                test_cases.emplace_back(new test_mul_mat(GGML_TYPE_TQ3_1S, GGML_TYPE_F32, m, n, k, {1, 1}, {1, 1}));
                test_cases.emplace_back(new test_mul_mat(GGML_TYPE_TQ3_1S, GGML_TYPE_F16, m, n, k, {1, 1}, {1, 1}));
            }
        }
    }

    // TQ3_1S: large-batch MUL_MAT, i.e. the pipeline_dequant[TQ3_1S] +
    // generic f16 matmul path taken when n > mul_mat_vec_max_cols = 8.
    // This is the only coverage the dequant shader's inverse WHT gets.
    for (int k : { 1536, 2048 }) {
        for (int m : { 256, 1536, 2048 }) {
            for (int n : { 16, 64, 256 }) {
                test_cases.emplace_back(new test_mul_mat(GGML_TYPE_TQ3_1S, GGML_TYPE_F32, m, n, k, {1, 1}, {1, 1}));
            }
        }
    }

    // TQ3_1S: DeepSeek-V4 MLA head_dim=512 batched-prefill shape.
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_TQ3_1S, GGML_TYPE_F32, 128, 512, 512, {1, 1}, {1, 1}));

    // TQ3_1S / TQ4_1S: non-contiguous src1 (k_v > k view) with batched n, to exercise
    // the rotate-act contiguity fallback. rotate-act walks src1 as a flat array so it
    // requires contiguous src1; non-contiguous must fall back to the standard mul_mm
    // path (inverse-RHT dequant), which handles strides via nb1x.
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_TQ3_1S, GGML_TYPE_F32, 256, 256, 1536, {1, 1}, {1, 1}, {0, 1, 2, 3}, 1600));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_TQ4_1S, GGML_TYPE_F32, 256, 256, 1536, {1, 1}, {1, 1}, {0, 1, 2, 3}, 1600));

    // TQ3_1S: small-batch (n<=8) FUSED multi-token kernel path. This is DeepSeek-V4-Flash's
    // real attn_q_a shape (n_embd=4096 -> q_lora_rank=1024) at an 8-token prefill batch,
    // contiguous src1 -- the case found to diverge (~2% on real weights) via eval-callback
    // node diffing against the CPU reference. The k=16384/m=24 pair mirrors hc_attn_fn's
    // shape as a control that was observed numerically fine.
    for (int nb : { 1, 2, 4, 8 }) {
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_TQ3_1S, GGML_TYPE_F32, 1024, nb, 4096, {1, 1}, {1, 1}));
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_TQ3_1S, GGML_TYPE_F32, 24, nb, 16384, {1, 1}, {1, 1}));
    }

    // m == 1, with n on both sides of MMVF_MAX_BATCH_SIZE (8): mmvf below, operand swap above
    for (int64_t n : {1, 7, 8, 9, 16, 127, 128, 511, 512}) {
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F32, GGML_TYPE_F32, 1, n, 2048, {1, 1}, {1, 1}));
    }
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F32, 1, 512, 2048, {1, 1}, {1, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F16, 1, 512, 2048, {1, 1}, {1, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F32, 1, 509, 2051, {1, 1}, {1, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F16, 1, 509, 2051, {1, 1}, {1, 1}));

    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F32, GGML_TYPE_F32, 31, 509, 2051, {1, 1}, {1, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F32, GGML_TYPE_F32, 32, 509, 2112, {1, 1}, {1, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q8_0, GGML_TYPE_F32, 32, 509, 2112, {1, 1}, {1, 1}));

    // ArifiLabs lane-206: ADVERSARIAL numerical coverage for the ROCmFP4-FAST q8_1 MMVQ mat-vec.
    //
    // The Vulkan selector routes GGML_TYPE_Q4_0_ROCMFP4_FAST through the integer-dot q8_1 MMVQ
    // shader at exactly n=3 and n=5 and through the f32 dequant shader everywhere else. n=2 was
    // removed from MMVQ because it produced a deterministic first-token EOG in a served
    // cache-history replay while the f32 arm on the same prompt and seed did not (lane-209 r10).
    // The same shader math is still live at n=3 and n=5, and the uniform[-1,1] inputs every other
    // MUL_MAT case uses cannot reach the regime that breaks: the divergence is in the q8_1
    // quantization of the ACTIVATIONS, which only bites when a row's dynamic range is wide, when
    // its scale is degenerate, or when the values sit on the FP4 code boundaries.
    //
    // So these cases hold the shapes fixed at the real 27B ffn row and vary the DATA instead.
    // Run the same set twice, once with GGML_VK_DISABLE_MMVQ=1, to separate "this input is hard
    // for FP4" (both arms move together) from "the MMVQ path diverges" (only one arm moves).
    // Every case prints its own nmse / max_abs / max_rel, so a PASS is auditable and not just a
    // silent OK.
    for (adv_pattern p : {ADV_EXTREME_RANGE, ADV_DENORMAL, ADV_SIGN_ALTERNATING,
                          ADV_ZERO_ROWS, ADV_FP4_CODE_SWEEP, ADV_MIXED_BLOCK_SCALE}) {
        // n=1 is the f32-shader control, n=3 is the DFlash2 n-max-2 verify width, n=5 the other
        // retained MMVQ width, n=2 and n=4 are the widths r10 moved back to f32 and must stay clean.
        for (int64_t n : {1, 2, 3, 4, 5}) {
            // k=5120 is the seat row; k=288 is 9 blocks of 32, an odd block count that leaves a
            // ragged tail under the shader's K_PER_ITER=8 grouping.
            test_cases.emplace_back(new test_mul_mat_adversarial(GGML_TYPE_Q4_0_ROCMFP4_FAST, p, 1024, n, 5120));
            test_cases.emplace_back(new test_mul_mat_adversarial(GGML_TYPE_Q4_0_ROCMFP4_FAST, p,  512, n,  288));
        }
    }
    // the exact seat shape, at the two retained MMVQ widths and their f32 control
    for (int64_t n : {1, 3, 5}) {
        test_cases.emplace_back(new test_mul_mat_adversarial(GGML_TYPE_Q4_0_ROCMFP4_FAST, ADV_EXTREME_RANGE, 17408, n, 5120));
    }

#if 0
    {
        // Test paths in OpenCL
        std::vector<int> ns = {32, 64, 128, 256, 512, 1024, 4096};
        std::vector<int> ks = {896, 1536, 4096};
        for (auto n : ns) {
            for (auto k : ks) {
                test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q8_0, GGML_TYPE_F32, 1024, n, k, {1, 1}, {1, 1}));
            }
        }
    }
#endif

#if 1
    for (ggml_type type_a : base_types) {
        for (ggml_type type_b : {GGML_TYPE_F32, GGML_TYPE_F16}) {
            std::vector<int> ks = { 256 };
            if (ggml_blck_size(type_a) == 1) {
                ks.push_back(4);
            }
            for (auto k : ks) {
                // test cases without permutation
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  1, k, {1, 1}, {1, 1}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  1, k, {1, 1}, {2, 1}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  1, k, {1, 1}, {1, 2}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  1, k, {3, 1}, {1, 1}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  1, k, {3, 1}, {2, 1}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  1, k, {3, 2}, {1, 1}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  1, k, {3, 2}, {2, 1}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  1, k, {3, 2}, {1, 2}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  1, k, {3, 2}, {2, 2}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  4, k, {3, 2}, {2, 2}));

                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16, 16, k, {1, 1}, {1, 1}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16, 16, k, {1, 1}, {2, 1}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16, 16, k, {1, 1}, {1, 2}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16, 16, k, {3, 1}, {1, 1}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16, 16, k, {3, 1}, {2, 1}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16, 16, k, {3, 2}, {1, 1}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16, 16, k, {3, 2}, {2, 1}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16, 16, k, {3, 2}, {1, 2}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16, 16, k, {3, 2}, {2, 2}));

                // test cases with permutation
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  1, k, {2, 3}, {1, 1}, {0, 2, 1, 3}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  1, k, {2, 3}, {1, 1}, {0, 1, 3, 2}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  1, k, {2, 3}, {1, 1}, {0, 3, 2, 1}));

                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  4, k, {2, 3}, {1, 1}, {0, 3, 2, 1}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  8, k, {2, 3}, {1, 1}, {0, 2, 1, 3}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  8, k, {2, 3}, {1, 1}, {0, 1, 3, 2}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  8, k, {2, 3}, {1, 1}, {0, 3, 2, 1}));

                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16, 16, k, {2, 3}, {1, 1}, {0, 2, 1, 3}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16, 16, k, {2, 3}, {1, 1}, {0, 1, 3, 2}));
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16, 16, k, {2, 3}, {1, 1}, {0, 3, 2, 1}));
            }

            // test cases with large ne00/ne10 to cover stream-k fixup
            test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  1, 1024, {3, 2}, {1, 1}));
            test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16,  8, 1024, {3, 2}, {1, 1}));
            test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16, 16, 1024, {3, 2}, {1, 1}));

            // test cases with large batch size
            test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16, 8, 256, {1536, 1}, {1, 1}));
        }
    }

    // BF16 is absent from base_types: add the 3 standard non-contig permutations explicitly
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_BF16, GGML_TYPE_F32, 16,  1, 256, {2, 3}, {1, 1}, {0, 2, 1, 3}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_BF16, GGML_TYPE_F32, 16,  1, 256, {2, 3}, {1, 1}, {0, 1, 3, 2}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_BF16, GGML_TYPE_F32, 16,  1, 256, {2, 3}, {1, 1}, {0, 3, 2, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_BF16, GGML_TYPE_F32, 16,  8, 256, {2, 3}, {1, 1}, {0, 2, 1, 3}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_BF16, GGML_TYPE_F32, 16,  8, 256, {2, 3}, {1, 1}, {0, 1, 3, 2}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_BF16, GGML_TYPE_F32, 16,  8, 256, {2, 3}, {1, 1}, {0, 3, 2, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_BF16, GGML_TYPE_F32, 16, 16, 256, {2, 3}, {1, 1}, {0, 2, 1, 3}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_BF16, GGML_TYPE_F32, 16, 16, 256, {2, 3}, {1, 1}, {0, 1, 3, 2}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_BF16, GGML_TYPE_F32, 16, 16, 256, {2, 3}, {1, 1}, {0, 3, 2, 1}));

    // token-tile boundary coverage. With n_used == n_mats every token routes to every expert, so
    // each expert receives exactly n rows, with no dependence on the random draw. mul_mm_id is used
    // from 32 tokens up: n = 32, 33, 47, 48, 49 reach it, leaving a last tile of 32, 1, 15, 16 and
    // 17 rows - 16 and 17 straddle the point where the upper half stops being skipped. The smaller
    // n cover the same row counts on the mat-vec path.
    for (ggml_type type_a : {GGML_TYPE_Q4_K, GGML_TYPE_IQ2_XS, GGML_TYPE_F16}) {
        for (int n : {1, 15, 16, 17, 31, 32, 33, 47, 48, 49}) {
            test_cases.emplace_back(new test_mul_mat_id(type_a, GGML_TYPE_F32, 4, 4, false, 512, n, 256));
        }
        // experts that receive no rows at all
        test_cases.emplace_back(new test_mul_mat_id(type_a, GGML_TYPE_F32, 8, 1, false, 512, 1, 256));
    }

    for (ggml_type type_a : other_types) {
        for (ggml_type type_b : {GGML_TYPE_F32}) {
            if (ggml_blck_size(type_a) != 256) {
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16, 1, ggml_blck_size(type_a), {1,  1}, {1, 1}));
            }
            test_cases.emplace_back(new test_mul_mat(type_a, type_b, 16, 1, 256, {1,  1}, {1, 1}));
        }
    }

    // Test IQP panel path for all grid IQ types
    for (ggml_type type_a : {GGML_TYPE_IQ2_XXS, GGML_TYPE_IQ2_XS, GGML_TYPE_IQ2_S, GGML_TYPE_IQ3_XXS,
                             GGML_TYPE_IQ3_S, GGML_TYPE_IQ1_S, GGML_TYPE_IQ1_M, GGML_TYPE_IQ4_XS}) {
        test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32, 16, 10, 256, {1, 1}, {1, 1}));
    }
#else
    // m = a rows
    // n = b rows
    // k = cols
    std::uniform_int_distribution<> dist_m(1, 128);
    std::uniform_int_distribution<> dist_n(16, 128);
    std::uniform_int_distribution<> dist_k(1, 16);
    for (int i = 0; i < 1000; i++) {
        for (ggml_type type_a : all_types) {
            for (ggml_type type_b : {GGML_TYPE_F32}) {
                int m = dist_m(rng);
                int n = dist_n(rng);
                int k = dist_k(rng) * ggml_blck_size(type_a);
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, m, n, k, { 1,  1}, {1, 1}));
            }
        }
    }
#endif

    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F32,  64, 2,  128, { 8,  1}, {1, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F32,  83, 2,  128, { 8,  1}, {4, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F32,  64, 2,   64, { 8,  1}, {4, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F32,  83, 2,   64, { 8,  1}, {4, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F32,  64, 45, 128, { 8,  1}, {4, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F32, 128, 45,  64, { 8,  1}, {4, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F32, 1056, 1, 193, {1,  1}, {4, 1}, {0, 2, 1, 3}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F32, 1056, 1, 67,  {1,  1}, {4, 1}, {0, 2, 1, 3}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F32, GGML_TYPE_F32, 16, 32, 32, { 1,  1}, {1, 1}, {0, 1, 2, 3}, 64, 3));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F32, GGML_TYPE_F32, 64, 77, 77, {12,1}, {1,1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F32, GGML_TYPE_F32, 32, 4, 96, {3, 2}, {1, 1}, {0, 1, 2, 3}, 0, 1, true));

    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q4_0, GGML_TYPE_F32, 576, 512, 576, {1,1}, {1,1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q4_0, GGML_TYPE_F32, 1, 2048, 8192, {1,  1}, {1, 1}));
    for (ggml_type type_a : all_types) {
        test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32, 1, 64, 256, {1,  1}, {1, 1}));
    }

    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q8_0, GGML_TYPE_F32, 6, 4096, 5120, {1, 1}, {1, 1}));

    // ArifiLabs lane-230 / R46b (checker F2): S-X8 mat-vec EDGE coverage. The R46 sweep ran f32
    // activations at m=16 with k=256 only, which exercises neither a ragged row count nor the f16
    // activation pipeline - and R46's own n <= 3 whole-block decode, plus the R46b shape-tuned
    // rows/workgroup variants, both change exactly how rows and K tails are walked.
    //   - ragged m: 7, 63, 4095, 17407 are not multiples of the 1/2/4/8 rows a workgroup can
    //     own, so every first_row/num_rows tail is hit;
    //   - k: 5120 is the real 27B row length, 1056 is a 33-block tail (QKSX8 = 32) that is not a
    //     multiple of 256 or of any workgroup width;
    //   - n runs 1..8 across the n <= 3 gate boundary, and n=1..3 also on f16 activations, which
    //     is the pipeline the shape lookup deliberately does NOT tune.
    //
    // m = 1 was in this list and is REMOVED (R46b commit G). It is not comparable under this
    // harness: NMSE at m=1,n=1 is computed over a SINGLE output element, so nothing averages the
    // per-element quantisation error down, and init_tensor_uniform (line ~62) reseeds from
    // std::random_device every run, so each sweep draws different data. Measured on this build:
    // the m=1,n=1 cases fail intermittently at ERR 8.1e-4 - 9.3e-4 against the 5e-4 bound, moving
    // between k=1056 and k=5120 from run to run. m=7 hits the same first_row/num_rows tail with
    // seven elements to average over, so no tail coverage is lost by dropping m=1.
    for (int64_t m : {7, 63, 4095, 17407}) {
        for (int64_t k : {1056, 5120}) {
            for (int n : {1, 2, 3, 4, 8}) {
                test_cases.emplace_back(new test_mul_mat(GGML_TYPE_SX8, GGML_TYPE_F32, m, n, k, {1, 1}, {1, 1}));
            }
        }
    }
    for (int64_t m : {7, 4095, 17408}) {
        for (int n : {1, 2, 3, 8}) {
            test_cases.emplace_back(new test_mul_mat(GGML_TYPE_SX8, GGML_TYPE_F16, m, n, 5120, {1, 1}, {1, 1}));
        }
    }
    // The four real 27B decode shapes at the widths the R46b lookup selects a tuned pipeline for,
    // as CORRECTNESS cases: a tuned variant that is never executed by the eval sweep is a variant
    // no receipt covers.
    for (int n : {1, 2, 3, 4, 5, 6, 7, 8}) {
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_SX8, GGML_TYPE_F32,  17408, n,  5120, {1, 1}, {1, 1}));
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_SX8, GGML_TYPE_F32,   5120, n, 17408, {1, 1}, {1, 1}));
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_SX8, GGML_TYPE_F32,   4096, n, 14336, {1, 1}, {1, 1}));
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_SX8, GGML_TYPE_F32, 248320, n,  5120, {1, 1}, {1, 1}));
    }

    // ArifiLabs lane-243 / R52b: the PREFILL width. Every S-X8 row in THIS block is n <= 8, which is
    // mat-vec territory -- ggml_vk_mul_mat_q_f16 only reaches the mul_mm (matrix-matrix) path at
    // wide n (n > mul_mat_vec_max_cols = 8, ggml-vulkan.cpp:479).
    // NOT a claim that nothing reached mul_mm before: the eval list already had SX8 at
    // m=1,n=64,k=256 (the all_types loop above) and m=16,n=9,k=256, and perf mode already timed SX8
    // at 4096x14336 n=512. What was missing is a REALISTIC-SHAPE row -- large m with k = 5120 --
    // so the S-X8 mul_mm tile decode had never been checked at a shape prompt processing actually
    // uses. n = 512 with m = 17408, k = 5120 is the 27B FFN row at one prefill chunk. q8_0 is the
    // paired control: it is the type S-X8 is being compared against on this shape, and it had no
    // row at this shape in this block either.
    for (ggml_type type_a : {GGML_TYPE_SX8, GGML_TYPE_Q8_0}) {
        test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32, 17408, 512, 5120, {1, 1}, {1, 1}));
    }

    // ArifiLabs lane-246 / R60: the 4B prefill FFN shapes across the SMALL-n band, as CORRECTNESS
    // cases. R52b's row is n = 512 on the 27B shape only, so every n between the mat-vec cutoff
    // (n > mul_mat_vec_max_cols = 8) and 512 was uncovered for S-X8 on a shape any model actually
    // runs. m = 9216, k = 2560 is Qwen3.5-4B ffn_gate/ffn_up; m = 2560, k = 9216 is ffn_down --
    // the two rows that dominate the 4B pp128 profile. q8_0 is the paired control at the same
    // shapes because it is the type S-X8 is compared against. The n sweep is the point: a decode
    // cost that fails to amortise would show as an n-dependent gap, and without these rows no
    // receipt could tell an n-dependent kernel effect from a run-level one.
    for (int n : {32, 64, 128, 256, 512}) {
        for (ggml_type type_a : {GGML_TYPE_SX8, GGML_TYPE_Q8_0}) {
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32, 9216, n, 2560, {1, 1}, {1, 1}));
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32, 2560, n, 9216, {1, 1}, {1, 1}));
        }
    }

    // ArifiLabs lane-236 / R48c: Q6_K mat-vec at the three real 27B decode shapes, n = 1..8, as
    // CORRECTNESS cases. Q6_K is 19.7% of the interactive Q4_K_XL 27B bytes (3.19 GiB, 56 tensors)
    // and mul_mat_vec_q6_k.comp gained a second specialization constant (activation hoist +
    // (-32) fold, GGML_ARIFI_Q6K_XFOLD). That arm is NOT bit-identical to the shipped one - the
    // -32 moves out of the fma chain into one fma per sub-block - so it must be held to the
    // MUL_MAT NMSE gate on the shapes it actually runs on, at every width, in both arms of
    // GGML_ARIFI_Q6K_XFOLD and both arms of GGML_VK_Q6K_DIRECT_SCALES.
    //
    // F16 src1 rows are present but DO NOT RUN, so mul_mat_vec_q6_k_f16_f32 (:7166, which takes
    // the same constant) is UNCOVERED. The CPU reference backend declines type_a=q6_K with
    // type_b=f16 - every such case reports "not supported [CPU]", 0 OK (r48c-evidence/
    // 10-mulmat-xfold-on.txt, 12-mulmat-q6k-mmvq-route.txt) - so the comparison never happens on
    // either backend. The rows are kept, not deleted: they cost one declined line each and begin
    // working the day the reference supports the combination. Do not read them as coverage.
    for (int n : {1, 2, 3, 4, 5, 6, 7, 8}) {
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q6_K, GGML_TYPE_F32,  17408, n,  5120, {1, 1}, {1, 1}));
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q6_K, GGML_TYPE_F32,   5120, n, 17408, {1, 1}, {1, 1}));
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q6_K, GGML_TYPE_F32, 248320, n,  5120, {1, 1}, {1, 1}));
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q6_K, GGML_TYPE_F16,  17408, n,  5120, {1, 1}, {1, 1}));
    }
    // Row tail: stride_d not a multiple of NUM_ROWS (rm_kq = 2 on the 780M) exercises the
    // `num_rows = p.stride_d - first_row` path in main(), where the hoisted activation tile is
    // reused across FEWER rows than the pipeline was specialized for.
    for (int n : {1, 2, 8}) {
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q6_K, GGML_TYPE_F32, 17407, n, 5120, {1, 1}, {1, 1}));
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q6_K, GGML_TYPE_F32,     7, n, 5120, {1, 1}, {1, 1}));
    }

    // ArifiLabs lane-234 / R48: the real 27B Q5_K decode shapes as CORRECTNESS cases.
    //
    // Q5_K is 33.5% of the interactive Q4_K_XL 27B file by bytes - 5.41 GiB over 141 tensors, the
    // largest single quant share - and lane-198 measured it at 31% of the decode graph. It had no
    // shape-accurate correctness coverage at these widths before this block.
    //
    // n=1..8 is not padding: the Q5_K route SPLITS at n=1 on AMD. n=1 takes the q8_1 MMVQ shader
    // (mul_mat_vecq.comp + the Q5_K arm of mul_mat_vecq_funcs.glsl) and n=2..8 take the f32 dequant
    // shader (mul_mat_vec_q5_k.comp), so a sweep that stops short of both sides of that boundary
    // leaves one of the two kernels untested. Widths 2..8 also cross the NUM_COLS values where the
    // R48 activation hoist changes how many times data_b is read, and 5120x17408 gives the second
    // k (17408) so the hoist is exercised at both block counts.
    for (int n : {1, 2, 3, 4, 5, 6, 7, 8}) {
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q5_K, GGML_TYPE_F32,  17408, n,  5120, {1, 1}, {1, 1}));
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q5_K, GGML_TYPE_F32,   5120, n, 17408, {1, 1}, {1, 1}));
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q5_K, GGML_TYPE_F32, 248320, n,  5120, {1, 1}, {1, 1}));
    }
    // Row-tail coverage for the same kernel: first_row + NUM_ROWS > stride_d takes the second arm
    // of main(), where num_rows is dynamic and smaller than the NUM_ROWS the local weight arrays
    // are sized by. That arm is what a spec-constant-sized array indexed by a dynamic bound can get
    // wrong, and no 27B shape above reaches it (all three m values are even).
    for (int n : {1, 2, 5, 8}) {
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q5_K, GGML_TYPE_F32, 17407, n, 5120, {1, 1}, {1, 1}));
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q5_K, GGML_TYPE_F32,     7, n, 5120, {1, 1}, {1, 1}));
    }

    // K not a multiple of 32
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F16, 64, 32,  65, {1, 1}, {1, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F16, 64, 32,  80, {1, 1}, {1, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F32, 64, 32,  80, {1, 1}, {1, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F32, GGML_TYPE_F32, 64, 32,  80, {1, 1}, {1, 1}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F16, 64, 32, 588, {1, 1}, {1, 1})); // 14*14*3, e.g. conv_2d im2col
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F16, 64, 32,  80, {4, 1}, {1, 1}));

#if 0
    // test the mat-mat path for Metal
    for (int k = 1; k < 512; ++k) {
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F32, 64, 127, k, {12,1}, {1,1}));
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F32, GGML_TYPE_F32, 64, 127, k, {12,1}, {1,1}));
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F32, 64, 77, k, {12,1}, {1,1}));
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F32, GGML_TYPE_F32, 64, 77, k, {12,1}, {1,1}));
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F32, 64, 128, k, {12,1}, {1,1}));
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F32, GGML_TYPE_F32, 64, 128, k, {12,1}, {1,1}));
        test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_F16, GGML_TYPE_F32, 16, 16, false, 50, 200, k));
        test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_F16, GGML_TYPE_F32, 16, 16, true, 50, 200, k));
        test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_BF16, GGML_TYPE_F32, 16, 16, false, 50, 200, k));
        test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_BF16, GGML_TYPE_F32, 16, 16, true, 50, 200, k));
        test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_F32, GGML_TYPE_F32, 16, 16, false, 50, 200, k));
        test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_F32, GGML_TYPE_F32, 16, 16, true, 50, 200, k));
    }
#endif

    for (auto bs2 : {1,3}) {
        for (auto bs : {1,2,4,8}) {
            for (auto nr : {1,4}) {
                for (uint32_t m = 0; m < 2; ++m) {
                    for (uint32_t k = 0; k < 2; ++k) {
                        for (ggml_type type: {GGML_TYPE_F16, GGML_TYPE_BF16, GGML_TYPE_F32}) {
                            test_cases.emplace_back(new test_mul_mat(type, GGML_TYPE_F32, 1056 + m, 1, 128 + k,  {bs,  bs2}, {nr, 1}, {0, 2, 1, 3}));
                            test_cases.emplace_back(new test_mul_mat(type, GGML_TYPE_F32, 128 + m,  1, 1056 + k, {bs,  bs2}, {nr, 1}, {0, 1, 2, 3}, 2*1056 + k));
                        }
                    }
                }
            }
        }
    }

    // sycl backend will limit task global_range < MAX_INT
    // test case for f16-type-convert-to-fp32 kernel with large k under fp32 compute dtype (occurs in stable-diffusion)
    // however this case needs to alloc more memory which may fail in some devices (Intel Arc770, etc.)
    // this case is verified (pass) in Intel(R) Data Center GPU Max 1100 (sycl backend) and NV A30 (cuda backend)
    // test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F16, 512, 262144, 9216, {1, 1}, {1, 1}));

    // test large experts*tokens
    for (bool b : {false, true}) {
        test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_F16, GGML_TYPE_F32, 16, 16, b, 32, 1024, 16));
        test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_F16, GGML_TYPE_F32, 2, 2, b, 32, 8192, 64));
        test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_F16, GGML_TYPE_F32, 16, 16, b, 50, 200, 64));
        test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_BF16, GGML_TYPE_F32, 16, 16, b, 32, 1024, 16));
        test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_BF16, GGML_TYPE_F32, 16, 16, b, 50, 200, 64));
    }

    // For issue 27873
    test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_IQ2_XXS, GGML_TYPE_F32, 1, 1, false, 1, 8192, 4096));

    for (int k : {1, 63, 65}) {
        test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_F16, GGML_TYPE_F32, 1, 1, false, 8, 16, k));
    }
    test_cases.emplace_back(new test_mul_mat_id_fusion(GGML_TYPE_F16, GGML_TYPE_F32, 16, 16, false, 32, 32, 32, 3));

    // gpt-oss issue with Vulkan mmq_id
    test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_MXFP4, GGML_TYPE_F32, 32, 2, false, 2880, 32, 2880));
    // more than 256 experts (hoisted row-id path): 512 as in Qwen3.8-Flash-Next,
    // and 1024 at the LLAMA_MAX_EXPERTS limit
    for (int n : {1, 5, 64, 300}) {
        test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_IQ3_S,  GGML_TYPE_F32,  512, 10, false, 128, n, 512));
        test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_Q4_0,   GGML_TYPE_F32,  512, 10, false, 256, n, 128));
        test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_IQ3_S,  GGML_TYPE_F32, 1024, 10, false, 128, n, 512));
        test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_Q4_0,   GGML_TYPE_F32, 1024, 10, false, 256, n, 128));
    }
    test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_Q4_0, GGML_TYPE_F32, 32, 2, false, 2880, 32, 2880));

    // multiple blocks per row: exercises the block-stride loop and the
    // per-expert base offset, which k == 256 alone leaves untested
    test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_TQ1_0, GGML_TYPE_F32, 28, 10, false, 1024, 1, 4096));
    test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_TQ1_0, GGML_TYPE_F32, 128, 8, false, 1024, 1, 2048));

    for (ggml_type type_a : all_types) {
        test_cases.emplace_back(new test_mul_mat_id(type_a, GGML_TYPE_F32, 4, 2, false, 64, 16, 3*ggml_blck_size(type_a)));
        // n=1 as well as n=16: ggml_vk_use_mul_mat_vec_id() selects the mat-vec path only
        // when src2->ne[1] <= 8, so the n=16 case above exercises mul_mm_id and never touches
        // the decode path. A backend that claims MUL_MAT_ID support without a mul_mat_vec_id
        // pipeline for the type passes at n=16 and asserts on a null pipeline at n=1.
        test_cases.emplace_back(new test_mul_mat_id(type_a, GGML_TYPE_F32, 4, 2, false, 64, 1, 3*ggml_blck_size(type_a)));
    }

    // Test IQP panel path for all grid IQ types
    for (ggml_type type_a : {GGML_TYPE_IQ2_XXS, GGML_TYPE_IQ2_XS, GGML_TYPE_IQ2_S, GGML_TYPE_IQ3_XXS,
                             GGML_TYPE_IQ3_S, GGML_TYPE_IQ1_S, GGML_TYPE_IQ1_M, GGML_TYPE_IQ4_XS}) {
        test_cases.emplace_back(new test_mul_mat_id(type_a, GGML_TYPE_F32, 4, 4, false, 16, 10, 256));
    }

    // test src1 f16 overflow
    for (int n : {16, 32, 64}) {
        test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_Q4_K, GGML_TYPE_F32, 128, 4, false, 4096, n, 2048, 1e5f));
        test_cases.emplace_back(new test_mul_mat_id(GGML_TYPE_Q8_0, GGML_TYPE_F32, 8,   2, false, 512,  n, 256,  1e5f));
    }

    // TurboQuant MUL_MAT_ID. TQ3_1S/TQ4_1S are in all_types but not base_types, so the
    // two cases above are their only MUL_MAT_ID coverage and the rich sweep below never
    // reaches them. Cover both sides of the n <= 8 threshold that
    // ggml_vk_use_mul_mat_vec_id() splits on (mul_mat_vec_id vs mul_mm_id), several
    // n_used counts, and broadcast -- which exercises the expert-index wrap that a
    // single non-broadcast case never touches.
    for (ggml_type type_a : {GGML_TYPE_TQ3_1S, GGML_TYPE_TQ4_1S}) {
        for (int n_used : {1, 2, 4}) {
            for (bool b : {false, true}) {
                for (int n : {1, 4, 8, 9, 17, 32}) {
                    test_cases.emplace_back(new test_mul_mat_id(type_a, GGML_TYPE_F32, 4, n_used, b, 512, n, 256));
                }
            }
        }
    }

    for (ggml_type type_a : base_types) {
        for (ggml_type type_b : {GGML_TYPE_F32 /*, GGML_TYPE_F16 */}) {
            for (int n_mats : {4, 8}) {
                for (int n_used : {1, 2, 4}) {
                    for (bool b : {false, true}) {
                        for (int n : {1, 4, 5, 17, 32, 129}) {
                            int m = 512;
                            int k = 256;
                            test_cases.emplace_back(new test_mul_mat_id(type_a, type_b, n_mats, n_used, b, m, n, k));
                        }
                    }
                }
            }
        }
    }

    for (ggml_type type_a : other_types) {
        for (ggml_type type_b : {GGML_TYPE_F32 /*, GGML_TYPE_F16 */}) {
            for (int n_mats : {4}) {
                for (int n_used : {2}) {
                    for (bool b : {false}) {
                        for (int n : {1, 32}) {
                            int m = 512;
                            int k = 256;
                            test_cases.emplace_back(new test_mul_mat_id(type_a, type_b, n_mats, n_used, b, m, n, k));
                        }
                    }
                }
            }
        }
    }

    for (int bs : {1, 4, 512}) {
        for (ggml_type type_a : {GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_Q4_0, GGML_TYPE_Q4_K}) {
            for (ggml_type type_b : {GGML_TYPE_F32}) {
                // test with mul after (ffn_moe_weighted)
                test_cases.emplace_back(new test_mul_mat_id_fusion(type_a, type_b, 128, 8, false, 768, bs, 2048, 1, true));
            }
        }
    }

    for (ggml_type type_a : base_types) {
        for (ggml_type type_b : {GGML_TYPE_F32, GGML_TYPE_F16}) {
            for (int n : {1, 16}) {
                for (int k : {1, 16}) {
                    for (int bs2 : {1, 3}) {
                        for (int bs3 : {1, 3}) {
                            for (int nr2 : {1, 2}) {
                                for (int nr3 : {1, 2}) {
                                    test_cases.emplace_back(new test_out_prod(type_a, type_b, 256, n, k, {bs2, bs3}, {nr2, nr3}));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // ne2 sweep to cover the cublasSgemmStridedBatched path (dps2 == 1, ne2 > 1)
    for (int64_t ne2 : {1, 8, 16, 32}) {
        test_cases.emplace_back(new test_out_prod(GGML_TYPE_F32, GGML_TYPE_F32,
                                                  256, 16, 16, {ne2, 1}, {1, 1}));
    }

    // nr2 sweep to cover the cublasSgemmBatched pointer-array path (dps2 > 1)
    for (int64_t nr2 : {8, 16, 32}) {
        test_cases.emplace_back(new test_out_prod(GGML_TYPE_F32, GGML_TYPE_F32,
                                                  256, 16, 16, {1, 1}, {nr2, 1}));
    }

    // add_id
    for (ggml_type type_a : {GGML_TYPE_F32}) {
        for (ggml_type type_b : {GGML_TYPE_F32}) {
            for (int n_mats : {4, 8}) {
                for (int n_used : {1, 2, 4}) {
                    for (int n_embd : {32, 129}) {
                        for (int n_token : {1, 32, 129}) {
                            test_cases.emplace_back(new test_add_id(type_a, type_b, n_embd, n_mats, n_used, n_token));
                        }
                    }
                }
            }
        }
    }

    for (ggml_type type : {GGML_TYPE_F16, GGML_TYPE_F32}) {
        test_cases.emplace_back(new test_sqr       (type));
        test_cases.emplace_back(new test_sqrt      (type));
        test_cases.emplace_back(new test_log       (type));
        test_cases.emplace_back(new test_sin       (type));
        test_cases.emplace_back(new test_cos       (type));
        test_cases.emplace_back(new test_clamp     (type));
        test_cases.emplace_back(new test_leaky_relu(type));
        test_cases.emplace_back(new test_floor     (type));
        test_cases.emplace_back(new test_ceil      (type));
        test_cases.emplace_back(new test_round     (type));
        test_cases.emplace_back(new test_trunc     (type));
        test_cases.emplace_back(new test_sqr       (type, {7, 1, 5, 3}));
        test_cases.emplace_back(new test_sqr       (type, {1024, 1024, 1, 1}));
        test_cases.emplace_back(new test_sqrt      (type, {7, 1, 5, 3}));
        test_cases.emplace_back(new test_sqrt      (type, {1024, 1024, 1, 1}));
        test_cases.emplace_back(new test_log       (type, {7, 1, 5, 3}));
        test_cases.emplace_back(new test_log       (type, {1024, 1024, 1, 1}));
        test_cases.emplace_back(new test_sin       (type, {7, 1, 5, 3}));
        test_cases.emplace_back(new test_sin       (type, {1024, 1024, 1, 1}));
        test_cases.emplace_back(new test_cos       (type, {7, 1, 5, 3}));
        test_cases.emplace_back(new test_cos       (type, {1024, 1024, 1, 1}));
        test_cases.emplace_back(new test_clamp     (type, {7, 1, 5, 3}));
        test_cases.emplace_back(new test_clamp     (type, {1024, 1024, 1, 1}));
        test_cases.emplace_back(new test_leaky_relu(type, {7, 1, 5, 3}));
        test_cases.emplace_back(new test_leaky_relu(type, {1024, 1024, 1, 1}));
        test_cases.emplace_back(new test_floor     (type, {7, 1, 5, 3}));
        test_cases.emplace_back(new test_floor     (type, {1024, 1024, 1, 1}));
        test_cases.emplace_back(new test_ceil      (type, {7, 1, 5, 3}));
        test_cases.emplace_back(new test_ceil      (type, {1024, 1024, 1, 1}));
        test_cases.emplace_back(new test_round     (type, {7, 1, 5, 3}));
        test_cases.emplace_back(new test_round     (type, {1024, 1024, 1, 1}));
        test_cases.emplace_back(new test_trunc     (type, {7, 1, 5, 3}));
        test_cases.emplace_back(new test_trunc     (type, {1024, 1024, 1, 1}));
    }

    test_cases.emplace_back(new test_diag_mask_inf(GGML_TYPE_F32, {10, 10, 1, 1}, 5));
    test_cases.emplace_back(new test_diag_mask_inf(GGML_TYPE_F32, {10, 10, 3, 1}, 5));
    test_cases.emplace_back(new test_diag_mask_inf(GGML_TYPE_F32, {10, 10, 3, 2}, 5));

#if 0
    std::uniform_int_distribution<> dist_ne1(1, 50);
    int exponent = 1;
    while (exponent < (1 << 17)) {
        std::uniform_int_distribution<> dist_ne0(exponent, 2*exponent);

        for (int n = 0; n < 10; ++n) {
            int64_t ne0 = dist_ne0(rng);
            int64_t ne1 = dist_ne1(rng);
            test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, GGML_TYPE_F32, {ne0, ne1, 1, 1}, n/2 == 0, 0.1f, ne0 < 1000 ? 4.0f : 0.0f));
        }

        exponent <<= 1;
    }
#endif
    for (bool mask : {false, true}) {
        for (bool sinks : {false, true}) {
            for (float max_bias : {0.0f, 8.0f}) {
                if (!mask && max_bias > 0.0f) continue;
                for (float scale : {1.0f, 0.1f}) {
                    for (int64_t ne0 : {16, 1024}) {
                        for (int64_t ne1 : {16, 1024}) {
                            if (mask) {
                                for (ggml_type m_prec : {GGML_TYPE_F32, GGML_TYPE_F16}) {
                                    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {ne0,   ne1,   1, 1}, mask, sinks, m_prec, {1, 1}, scale, max_bias));
                                    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {ne0-1, ne1-1, 1, 1}, mask, sinks, m_prec, {1, 1}, scale, max_bias));

                                    if (ne0 <= 32 && ne1 <= 32) {
                                        test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {ne0,   ne1,   1, 3}, mask, sinks, m_prec, {3, 1}, scale, max_bias));
                                        test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {ne0-1, ne1-1, 1, 1}, mask, sinks, m_prec, {2, 3}, scale, max_bias));
                                    }
                                }
                            } else {
                                /* The precision of mask here doesn't matter as boolean mask is false */
                                test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {ne0,   ne1,   1, 1}, mask, sinks, GGML_TYPE_F32, {1, 1}, scale, max_bias));
                                test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {ne0-1, ne1-1, 1, 1}, mask, sinks, GGML_TYPE_F32, {1, 1}, scale, max_bias));
                            }
                        }
                    }
                }
            }
            // inplace tests
            test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {16, 2, 32, 1}, mask, sinks, GGML_TYPE_F32, {1, 1}, 0.1f, 0.0f, true));
            test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {16, 2, 32, 1}, mask, sinks, GGML_TYPE_F16, {1, 1}, 0.1f, 0.0f, true));
        }
    }
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {16, 2, 32, 1}, true,  true,  GGML_TYPE_F32, {1, 1}, 0.1f, 0.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {16, 2, 32, 1}, true,  false, GGML_TYPE_F16, {1, 1}, 0.1f, 0.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {16, 2, 32, 1}, false, true,  GGML_TYPE_F32, {1, 1}, 0.1f, 0.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {32, 2, 32, 1}, true,  true,  GGML_TYPE_F32, {1, 1}, 0.1f, 0.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {32, 2, 32, 1}, true,  false, GGML_TYPE_F16, {1, 1}, 0.1f, 0.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {32, 2, 32, 1}, true,  true,  GGML_TYPE_F32, {1, 1}, 0.1f, 8.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {32, 2, 32, 1}, true,  true,  GGML_TYPE_F16, {1, 1}, 0.1f, 8.0f));

    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {200001, 2, 3, 1}, true,   true,  GGML_TYPE_F32, {1, 1}, 0.1f, 8.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {200001, 2, 3, 1}, true,   true,  GGML_TYPE_F16, {1, 1}, 0.1f, 8.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {200000, 1, 1, 1}, false,  false, GGML_TYPE_F32, {1, 1}, 1.0f, 0.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {200000, 4, 1, 1}, false,  false, GGML_TYPE_F32, {1, 1}, 1.0f, 0.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {4,      1, 1, 1}, false,  false, GGML_TYPE_F32, {1, 1}, 1.0f, 0.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {4,   1023, 1, 1}, false,  false, GGML_TYPE_F32, {1, 1}, 1.0f, 0.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {643251, 3, 1, 1}, false,  false, GGML_TYPE_F32, {1, 1}, 1.0f, 0.0f));

    for (float max_bias : {0.0f, 8.0f}) {
        for (float scale : {1.0f, 0.1f}) {
            for (int64_t ne0 : {16, 1024}) {
                for (int64_t ne1 : {16, 1024}) {
                    test_cases.emplace_back(new test_soft_max_back(GGML_TYPE_F32, {ne0,   ne1,   1, 1}, scale, max_bias));
                    test_cases.emplace_back(new test_soft_max_back(GGML_TYPE_F32, {ne0-1, ne1-1, 1, 1}, scale, max_bias));
                    test_cases.emplace_back(new test_soft_max_back(GGML_TYPE_F32, {ne0,   ne1,   2, 3}, scale, max_bias));
                }
            }
        }
    }

    for (bool fw : {true, false}) { // fw == forward
        bool all = true;

        for (float fs : { 1.0f, 1.4245f }) {
            for (float ef : { 0.0f, 0.7465f }) {
                for (float af : { 1.0f, 1.4245f }) {
                    for (ggml_type type : {GGML_TYPE_F32, GGML_TYPE_F16}) {
                        for (bool ff : {false, true}) { // freq_factors
                            for (float v : { 0, 1 }) {
                                test_cases.emplace_back(new test_rope(type, {128,  32, 2, 1}, 128, GGML_ROPE_TYPE_NORMAL, 512, fs, ef, af, ff, v, fw)); // llama 7B

                                if (all) {
                                    test_cases.emplace_back(new test_rope(type, {128,  40, 2, 1}, 128, GGML_ROPE_TYPE_NORMAL, 512, fs, ef, af, ff, v, fw)); // llama 13B
                                    test_cases.emplace_back(new test_rope(type, {128,  52, 2, 1}, 128, GGML_ROPE_TYPE_NORMAL, 512, fs, ef, af, ff, v, fw)); // llama 30B
                                    test_cases.emplace_back(new test_rope(type, {128,  64, 2, 1}, 128, GGML_ROPE_TYPE_NORMAL, 512, fs, ef, af, ff, v, fw)); // llama 65B
                                    test_cases.emplace_back(new test_rope(type, {16, 16, 8192, 1}, 16, GGML_ROPE_TYPE_NORMAL, 512, fs, ef, af, ff, v, fw));
                                }

                                if (all) {
                                    test_cases.emplace_back(new test_rope(type, { 64,   1, 2, 1},  64, GGML_ROPE_TYPE_NEOX, 512, fs, ef, af, ff, v, fw)); // neox (falcon 7B)
                                    test_cases.emplace_back(new test_rope(type, { 64,  71, 2, 1},  64, GGML_ROPE_TYPE_NEOX, 512, fs, ef, af, ff, v, fw)); // neox (falcon 7B)
                                    test_cases.emplace_back(new test_rope(type, { 64,   8, 2, 1},  64, GGML_ROPE_TYPE_NEOX, 512, fs, ef, af, ff, v, fw)); // neox (falcon 40B)

                                    test_cases.emplace_back(new test_rope(type, { 80,  32, 2, 1},  20, GGML_ROPE_TYPE_NORMAL, 512, fs, ef, af, ff, v, fw));
                                    test_cases.emplace_back(new test_rope(type, { 80,  32, 2, 1},  32, GGML_ROPE_TYPE_NORMAL, 512, fs, ef, af, ff, v, fw));
                                    test_cases.emplace_back(new test_rope(type, { 80,  32, 4, 1},  32, GGML_ROPE_TYPE_NORMAL, 512, fs, ef, af, ff, v, fw));

                                    test_cases.emplace_back(new test_rope(type, { 80,  32, 2, 1},  20, GGML_ROPE_TYPE_NEOX, 512, fs, ef, af, ff, v, fw)); // neox (stablelm)
                                    test_cases.emplace_back(new test_rope(type, { 80,  32, 2, 1},  32, GGML_ROPE_TYPE_NEOX, 512, fs, ef, af, ff, v, fw)); // neox (phi-2)
                                    test_cases.emplace_back(new test_rope(type, { 80,  32, 4, 1},  32, GGML_ROPE_TYPE_NEOX, 512, fs, ef, af, ff, v, fw)); // neox (phi-2)
                                    test_cases.emplace_back(new test_rope(type, { 16, 16, 8192, 1},  16, GGML_ROPE_TYPE_NEOX, 512, fs, ef, af, ff, v, fw));
                                }

                                if (all) {
                                    test_cases.emplace_back(new test_rope(type, {128,  12, 2, 1}, 128, GGML_ROPE_TYPE_MROPE,  512, fs, ef, af, ff, v, fw)); // rope_multi,m-rope (qwen2vl 2B)
                                    test_cases.emplace_back(new test_rope(type, {128,  28, 2, 1}, 128, GGML_ROPE_TYPE_MROPE,  512, fs, ef, af, ff, v, fw)); // rope_multi,m-rope (qwen2vl 7B)
                                    test_cases.emplace_back(new test_rope(type, {128,  12, 2, 1},  20, GGML_ROPE_TYPE_MROPE,  512, fs, ef, af, ff, v, fw));
                                    test_cases.emplace_back(new test_rope(type, {128,  28, 2, 1},  32, GGML_ROPE_TYPE_MROPE,  512, fs, ef, af, ff, v, fw));
                                    test_cases.emplace_back(new test_rope(type, {128,  12, 2, 1}, 128, GGML_ROPE_TYPE_IMROPE,  512, fs, ef, af, ff, v, fw)); // rope_multi,imrope (qwen3vl 2B)
                                    test_cases.emplace_back(new test_rope(type, {128,  28, 2, 1}, 128, GGML_ROPE_TYPE_IMROPE,  512, fs, ef, af, ff, v, fw)); // rope_multi,imrope (qwen3vl 7B)
                                    test_cases.emplace_back(new test_rope(type, {128,  12, 2, 1},  20, GGML_ROPE_TYPE_IMROPE,  512, fs, ef, af, ff, v, fw));
                                    test_cases.emplace_back(new test_rope(type, {128,  28, 2, 1},  32, GGML_ROPE_TYPE_IMROPE,  512, fs, ef, af, ff, v, fw));
                                    test_cases.emplace_back(new test_rope(type, { 80,  16, 2, 1},  80, GGML_ROPE_TYPE_VISION, 512, fs, ef, af, ff, v, fw)); // rope_multi,m-rope (qwen2vl ViT)
                                    test_cases.emplace_back(new test_rope(type, {128,  16, 2, 1}, 128, GGML_ROPE_TYPE_IMROPE, 512, fs, ef, af, ff, v, fw)); // rope_multi,m-rope (qwen3vl)
                                    test_cases.emplace_back(new test_rope(type, {16, 16, 8192, 1}, 16, GGML_ROPE_TYPE_IMROPE, 512, fs, ef, af, ff, v, fw));
                                }

                                test_cases.emplace_back(new test_rope(type, { 64, 128, 2, 1},  64, GGML_ROPE_TYPE_NEOX, 512, fs, ef, af, ff, v, fw)); // neox (falcon 40B)
                            }

                            // build_rope_2d-style: ROPE on a non-contiguous view
                            // that starts at a non-zero offset along dim 0
                            // (e.g. gemma4v vision second-half view).
                            for (int rmode : { GGML_ROPE_TYPE_NORMAL, GGML_ROPE_TYPE_NEOX, GGML_ROPE_TYPE_MROPE, GGML_ROPE_TYPE_IMROPE, GGML_ROPE_TYPE_VISION }) {
                                test_cases.emplace_back(new test_rope(type, { 36, 16, 2457, 1}, 36, rmode, 512, fs, ef, af, ff, 2, fw));
                            }
                        }

                        all = false;
                    }
                }
            }
        }
    }

    // single inplace test per type/mode/ff
    for (ggml_type type : {GGML_TYPE_F32, GGML_TYPE_F16}) {
        for (int mode : {GGML_ROPE_TYPE_NORMAL, GGML_ROPE_TYPE_NEOX, GGML_ROPE_TYPE_MROPE, GGML_ROPE_TYPE_IMROPE, GGML_ROPE_TYPE_VISION}) {
            for (bool ff : {false, true}) {
                test_cases.emplace_back(new test_rope(type, {128,  32, 2, 1}, 128, mode, 512, 1.4245f, 0.7465f, 1.4245f, ff, 0, true, true));
                test_cases.emplace_back(new test_rope(type, {128,  32, 2, 1}, 128, mode, 512, 1.4245f, 0.7465f, 1.4245f, ff, 1, true, true));
                test_cases.emplace_back(new test_rope(type, {128,  32, 2, 3}, 128, mode, 512, 1.4245f, 0.7465f, 1.4245f, ff, 1, true, true));
            }
        }
    }

    // rotated dims window at an offset (ggml_rope_set_offset), not supported for vision mode
    for (ggml_type type : {GGML_TYPE_F32, GGML_TYPE_F16}) {
        for (bool fw : {true, false}) { // fw == forward
            for (bool ff : {false, true}) {
                test_cases.emplace_back(new test_rope(type, {128, 32, 2, 1}, 32, GGML_ROPE_TYPE_NORMAL, 512, 1.4245f, 0.7465f, 1.4245f, ff, 0, fw, false, 32));
                test_cases.emplace_back(new test_rope(type, {128, 32, 2, 1}, 32, GGML_ROPE_TYPE_NEOX,   512, 1.4245f, 0.7465f, 1.4245f, ff, 0, fw, false, 32));
                test_cases.emplace_back(new test_rope(type, {128, 12, 2, 1}, 24, GGML_ROPE_TYPE_MROPE,  512, 1.4245f, 0.7465f, 1.4245f, ff, 0, fw, false, 32));
                test_cases.emplace_back(new test_rope(type, {128, 12, 2, 1}, 24, GGML_ROPE_TYPE_IMROPE, 512, 1.4245f, 0.7465f, 1.4245f, ff, 0, fw, false, 32));
            }
        }
        // inplace with an offset
        test_cases.emplace_back(new test_rope(type, {128, 32, 2, 1}, 32, GGML_ROPE_TYPE_NEOX, 512, 1.4245f, 0.7465f, 1.4245f, false, 0, true, true, 32));
    }

    // Real-model RoPE: F32 forward, packed Q, 512-token prefill.
    test_cases.emplace_back(new test_rope(GGML_TYPE_F32, {256,  8, 512, 1},  64, GGML_ROPE_TYPE_IMROPE, 512, 1.0f, 0.0f, 1.0f, false, 0, true)); // qwen3.5 0.8B
    test_cases.emplace_back(new test_rope(GGML_TYPE_F32, {256, 16, 512, 1},  64, GGML_ROPE_TYPE_IMROPE, 512, 1.0f, 0.0f, 1.0f, false, 0, true)); // qwen3.5 4B
    test_cases.emplace_back(new test_rope(GGML_TYPE_F32, {256,  8, 512, 1}, 256, GGML_ROPE_TYPE_NEOX,   512, 1.0f, 0.0f, 1.0f, false, 0, true)); // gemma4 E2B sliding
    test_cases.emplace_back(new test_rope(GGML_TYPE_F32, {512,  8, 512, 1}, 128, GGML_ROPE_TYPE_NEOX,   512, 1.0f, 0.0f, 1.0f, true,  0, true)); // gemma4 E4B global

    for (int v : { 0, 1, 2, 3 }) {
        for (int dim : { 0, 1, 2, 3, }) {
            test_cases.emplace_back(new test_concat(GGML_TYPE_F32, {11, 12, 13, 14}, 7, dim, v));
            test_cases.emplace_back(new test_concat(GGML_TYPE_F16, {11, 12, 13, 14}, 7, dim, v));
            test_cases.emplace_back(new test_concat(GGML_TYPE_BF16, {11, 12, 13, 14}, 7, dim, v));
            test_cases.emplace_back(new test_concat(GGML_TYPE_I8, {11, 12, 13, 14}, 7, dim, v));
            test_cases.emplace_back(new test_concat(GGML_TYPE_I16, {11, 12, 13, 14}, 7, dim, v));
            test_cases.emplace_back(new test_concat(GGML_TYPE_I32, {11, 12, 13, 14}, 7, dim, v));
            test_cases.emplace_back(new test_concat(GGML_TYPE_I64, {11, 12, 13, 14}, 7, dim, v));
        }
    }

    for (ggml_type type_a : { GGML_TYPE_Q4_0, GGML_TYPE_Q4_1, GGML_TYPE_Q5_0, GGML_TYPE_Q5_1, GGML_TYPE_Q8_0 }) {
        for (int v : { 0, 4, 8, 12 }) {
            for (int dim : { 0, 1, 2, 3, }) {
                test_cases.emplace_back(new test_concat(type_a, {128, 12, 13, 14}, dim == 0 ? 256 : 7, dim, v));
            }
        }
    }

    for (ggml_sort_order order : {GGML_SORT_ORDER_ASC, GGML_SORT_ORDER_DESC}) {
        for (uint32_t i = 4; i <= 1024*1024; i *= 2) {
            test_cases.emplace_back(new test_argsort(GGML_TYPE_F32, {i-1, 1, 1, 1}));
            test_cases.emplace_back(new test_argsort(GGML_TYPE_F32, {i, 1, 1, 1}));
        }
        test_cases.emplace_back(new test_argsort(GGML_TYPE_F32, {16, 10, 10, 10}, order));
        test_cases.emplace_back(new test_argsort(GGML_TYPE_F32, {60, 10, 10, 10}, order)); // qwen
        test_cases.emplace_back(new test_argsort(GGML_TYPE_F32, {1023, 2, 1, 3}, order));
        test_cases.emplace_back(new test_argsort(GGML_TYPE_F32, {1024, 2, 1, 3}, order));
        test_cases.emplace_back(new test_argsort(GGML_TYPE_F32, {1025, 2, 1, 3}, order));
        test_cases.emplace_back(new test_argsort(GGML_TYPE_F32, {1025, 256, 1, 1}, order)); // test ceildiv in CUDA's CUB's DeviceSegmentedSort
        test_cases.emplace_back(new test_argsort(GGML_TYPE_F32, {2047, 2, 1, 3}, order));
        test_cases.emplace_back(new test_argsort(GGML_TYPE_F32, {2048, 2, 1, 3}, order));
        test_cases.emplace_back(new test_argsort(GGML_TYPE_F32, {2049, 2, 1, 3}, order));
        test_cases.emplace_back(new test_argsort(GGML_TYPE_F32, {2, 8, 8192, 1}, order)); // bailingmoe2 (group selection)
        test_cases.emplace_back(new test_argsort(GGML_TYPE_F32, {2048, 512, 1, 1}, order)); // test CUDA dispatching to radix sort for nrows > = 1 in graph mode
    }

    for (int n = 1; n < 5; ++n) {
        for (int k = 1; k <= n; ++k) {
            test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {n, 2, 1, 3}, k, true));
        }
    }
    for (int i = 0; i < 20; ++i) {
        for (int k : {1, 2, 3, 7, 15, 100, 500, 1023, 9999}) {
            if (k <= 1<<i) {
                test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {(1<<i), 1, 1, 1}, k));
                test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {(1<<i) + 11, 1, 2, 1}, k));
                test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {(1<<i) + 11, 1, 2, 1}, k, true));
            }
        }
    }
    for (int k : {4, 8, 16, 32}) {
        for (int nrows : {1, 8, 16}) {
            test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {202048, nrows, 1, 1}, k));
            test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {151936, nrows, 1, 1}, k));
            test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {8192,   nrows, 1, 1}, k));
            test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {8193,   nrows, 1, 1}, k));
            test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {8192,   nrows, 1, 1}, k, true));
            test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {202048, nrows, 1, 1}, k, true));
        }
    }

    for (int k : {1, 2, 3, 7, 15}) {
        test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {16, 10, 10, 10}, k));
        test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {60, 10, 10, 10}, k));
        test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {1023, 2, 1, 3}, k));
        test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {1024, 2, 1, 3}, k));
        test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {1025, 2, 1, 3}, k));
        test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {16384, 1, 1, 1}, k));
        test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {2047, 2, 1, 3}, k));
        test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {2048, 2, 1, 3}, k));
        test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {2049, 2, 1, 3}, k));
    }

    // Large-k, including multi-row and ties (qwen4exp)
    test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, { 1024,  1, 1, 1 }, 1024));
    test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, { 2048,  2, 1, 1 }, 1024));
    test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, { 4096,  1, 1, 1 }, 2048));
    test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, { 8192,  2, 1, 1 }, 2051));
    test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, { 33024, 1, 1, 1 }, 2051));
    test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, { 33024, 4, 1, 1 }, 2051));
    test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, { 8192,  2, 1, 1 }, 2051, true));
    test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, { 33024, 4, 1, 1 }, 2051, true));

    // qwen4exp QSA indexer top-k fusion (get_rows + f16 mask + top_k)
    test_cases.emplace_back(new test_topk_qsa(512,  2048,  1, 1, 1500));
    test_cases.emplace_back(new test_topk_qsa(512,  2048,  2, 1, 1500));
    test_cases.emplace_back(new test_topk_qsa(256,  2048,  4, 2, 2000));
    test_cases.emplace_back(new test_topk_qsa(64,   256,   2, 1, 200));  // small k: unfused fallback

    // exhaustive top_k tests
    //for (int i = 1; i < 9999; ++i) {
    //    test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {i, 2, 1, 3}, rand() % i + 1));
    //}

    for (ggml_scale_mode mode : {GGML_SCALE_MODE_NEAREST, GGML_SCALE_MODE_BILINEAR, GGML_SCALE_MODE_BICUBIC, ggml_scale_mode(GGML_SCALE_MODE_BILINEAR | GGML_SCALE_FLAG_ANTIALIAS)}) {
        test_cases.emplace_back(new test_upscale(GGML_TYPE_F32, {512, 512, 3, 2}, 2, mode));
        test_cases.emplace_back(new test_upscale(GGML_TYPE_F32, {512, 512, 3, 2}, 2, mode, true));
        test_cases.emplace_back(new test_interpolate(GGML_TYPE_F32, {2, 5,  7, 11}, {5, 7, 11, 13}, mode));
        test_cases.emplace_back(new test_interpolate(GGML_TYPE_F32, {5, 7, 11, 13}, {2, 5,  7, 11}, mode));
    }
    for (ggml_scale_mode mode : {GGML_SCALE_MODE_BILINEAR, GGML_SCALE_MODE_BICUBIC}) {
        test_cases.emplace_back(new test_interpolate(GGML_TYPE_F32, {2, 5, 7, 11}, {5, 7, 11, 13}, (ggml_scale_mode)(mode | GGML_SCALE_FLAG_ALIGN_CORNERS)));
        test_cases.emplace_back(new test_interpolate(GGML_TYPE_F32, {1, 4, 3, 2}, {2, 8, 3, 2}, (ggml_scale_mode)(mode | GGML_SCALE_FLAG_ALIGN_CORNERS)));
        test_cases.emplace_back(new test_interpolate(GGML_TYPE_F32, {4, 1, 3, 2}, {1, 1, 3, 2}, (ggml_scale_mode)(mode | GGML_SCALE_FLAG_ALIGN_CORNERS)));
    }

    test_cases.emplace_back(new test_sum());
    test_cases.emplace_back(new test_sum(GGML_TYPE_F32, {11, 5, 6, 3}, {0, 2, 1, 3}));  // row-contiguous but non-contiguous
    test_cases.emplace_back(new test_sum(GGML_TYPE_F32, {11, 5, 6, 3}, {0, 3, 2, 1}));
    test_cases.emplace_back(new test_sum(GGML_TYPE_F32, {11, 5, 6, 3}, {0, 1, 3, 2}));
    test_cases.emplace_back(new test_mean());
    test_cases.emplace_back(new test_mean(GGML_TYPE_F32, { 33, 1, 1, 1 }));
    test_cases.emplace_back(new test_mean(GGML_TYPE_F32, { 33, 256, 1, 1 }));
    test_cases.emplace_back(new test_mean(GGML_TYPE_F32, { 32769, 1, 1, 1 }));
    test_cases.emplace_back(new test_mean(GGML_TYPE_F32, { 32, 1, 1, 1 }));
    test_cases.emplace_back(new test_mean(GGML_TYPE_F32, { 32, 256, 1, 1 }));
    test_cases.emplace_back(new test_mean(GGML_TYPE_F32, { 32768, 1, 1, 1 }));
    test_cases.emplace_back(new test_mean(GGML_TYPE_F32, { 11, 5, 6, 3 }, true, false));
    test_cases.emplace_back(new test_mean(GGML_TYPE_F32, { 11, 5, 6, 3 }, false, true));
    test_cases.emplace_back(new test_mean(GGML_TYPE_F32, { 11, 5, 6, 3 }, true, true));
    test_cases.emplace_back(new test_sum(GGML_TYPE_F32, { 33, 1, 1, 1 }));
    test_cases.emplace_back(new test_sum(GGML_TYPE_F32, { 33, 1024, 1, 1 }));
    test_cases.emplace_back(new test_sum(GGML_TYPE_F32, { 33, 256, 1, 1 }));
    test_cases.emplace_back(new test_sum(GGML_TYPE_F32, { 33, 256, 1, 1 }, { 1, 0, 2, 3 })); // sum dst not-contiguous
    test_cases.emplace_back(new test_sum_rows());
    test_cases.emplace_back(new test_sum_rows(GGML_TYPE_F32, { 11, 5, 6, 3 }, true, false));
    test_cases.emplace_back(new test_sum_rows(GGML_TYPE_F32, { 11, 5, 6, 3 }, false, true));
    test_cases.emplace_back(new test_sum_rows(GGML_TYPE_F32, { 11, 5, 6, 3 }, true, true));
    test_cases.emplace_back(new test_sum_rows(GGML_TYPE_F32, { 16, 5, 6, 3 }, true, false));
    test_cases.emplace_back(new test_sum_rows(GGML_TYPE_F32, { 16, 5, 6, 3 }, false, true));
    test_cases.emplace_back(new test_sum_rows(GGML_TYPE_F32, { 16, 5, 6, 3 }, true, true));
    test_cases.emplace_back(new test_sum_rows(GGML_TYPE_F32, { 33, 1, 1, 1 }));
    test_cases.emplace_back(new test_sum_rows(GGML_TYPE_F32, { 33, 1024, 1, 1 }));
    test_cases.emplace_back(new test_sum_rows(GGML_TYPE_F32, { 33, 256, 1, 1 }));
    test_cases.emplace_back(new test_group_norm(GGML_TYPE_F32, {64, 64, 320, 1}));
    test_cases.emplace_back(new test_group_norm(GGML_TYPE_F32, {9, 9, 1280, 1}));
    test_cases.emplace_back(new test_group_norm_mul_add(GGML_TYPE_F32, {64, 64, 320, 1}));
    test_cases.emplace_back(new test_group_norm_mul_add(GGML_TYPE_F32, {9, 9, 1280, 1}));
    test_cases.emplace_back(new test_acc(GGML_TYPE_F32, {256, 17, 1, 1}, {256, 16, 1, 1}, -1));
    test_cases.emplace_back(new test_acc(GGML_TYPE_F32, {256, 17, 2, 3}, {256, 16, 2, 3}, -1));
    test_cases.emplace_back(new test_acc(GGML_TYPE_F32, {256, 17, 2, 3}, {128, 16, 2, 3}, -1));
    test_cases.emplace_back(new test_acc(GGML_TYPE_F32, {256, 17, 2, 3}, {256, 16, 2, 3}, 1));
    test_cases.emplace_back(new test_acc(GGML_TYPE_F32, {256, 17, 2, 3}, {128, 16, 2, 3}, 2));
    test_cases.emplace_back(new test_acc(GGML_TYPE_F32, {256, 17, 2, 3}, {64, 16, 2, 3}, 3));

    test_cases.emplace_back(new test_pad());
    test_cases.emplace_back(new test_pad(GGML_TYPE_F32, {33, 17, 2, 1}, 4, 3, true)); // circular
    test_cases.emplace_back(new test_pad_ext());
    test_cases.emplace_back(new test_pad(GGML_TYPE_F32, {1024, 1, 1, 1}, 1, 0, false));
    test_cases.emplace_back(new test_pad(GGML_TYPE_F32, {1024, 2, 1, 1}, 1, 0, false));
    test_cases.emplace_back(new test_pad(GGML_TYPE_F32, {1024, 16, 1, 1}, 0, 1, false));
    test_cases.emplace_back(new test_pad(GGML_TYPE_F32, {1023, 1, 1, 1}, 1, 0, false));
    test_cases.emplace_back(new test_pad(GGML_TYPE_F32, {1023, 8, 1, 1}, 1, 0, false));
    test_cases.emplace_back(new test_pad(GGML_TYPE_F32, {1025, 1, 1, 1}, 1, 0, false));
    test_cases.emplace_back(new test_pad(GGML_TYPE_F32, {1025, 8, 1, 1}, 1, 0, false));
    test_cases.emplace_back(new test_pad(GGML_TYPE_F32, {2048, 1, 1, 1}, 1, 0, false));
    test_cases.emplace_back(new test_pad(GGML_TYPE_F32, {2048, 4, 1, 1}, 1, 0, false));
    test_cases.emplace_back(new test_pad(GGML_TYPE_F32, {2049, 1, 1, 1}, 1, 0, false));
    test_cases.emplace_back(new test_pad(GGML_TYPE_F32, {100, 1, 1, 1}, 100, 0, false));
    test_cases.emplace_back(new test_pad(GGML_TYPE_F32, {100, 1, 1, 1}, 0, 100, false));
    test_cases.emplace_back(new test_pad(GGML_TYPE_F32, {100, 100, 1, 1}, 50, 50, false));

    test_cases.emplace_back(new test_pad_reflect_1d());
    test_cases.emplace_back(new test_pad_reflect_1d(GGML_TYPE_F32, {3000, 384, 4, 1}));
    test_cases.emplace_back(new test_roll());
    test_cases.emplace_back(new test_roll(3, -2, 1, -1, true));
    test_cases.emplace_back(new test_arange());
    test_cases.emplace_back(new test_arange(GGML_TYPE_F32, 0.0f, 1048576.0f, 1.0f));
    test_cases.emplace_back(new test_timestep_embedding());
    test_cases.emplace_back(new test_leaky_relu());

    test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, { 10, 5, 4, 3 }));
    test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, { 127, 5, 4, 3 }));
    test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, { 128, 5, 4, 3 }));
    test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, { 128, 128, 4, 4 }));
    test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, { 255, 5, 4, 3 }));
    test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, { 256, 5, 4, 3 }));
    test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, { 511, 5, 4, 3 }));
    test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, { 512, 5, 4, 3 }));
    test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, { 1023, 5, 4, 3 }));
    test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, { 1024, 5, 4, 3 }));
    test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, { 2047, 5, 4, 3 }));
    test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, { 2048, 5, 4, 3 }));
    test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, { 201*1204, 1, 1, 1 }));
    test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, { 312*1205, 1, 1, 1 }));
    test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, { 20481, 4, 1, 1 }));

    test_cases.emplace_back(new test_xielu());
    test_cases.emplace_back(new test_xielu(GGML_TYPE_F16));
    test_cases.emplace_back(new test_xielu(GGML_TYPE_F32, { 512, 16, 1, 1 }));
    test_cases.emplace_back(new test_xielu(GGML_TYPE_F16, { 512, 16, 1, 1 }));

    test_cases.emplace_back(new test_tri(GGML_TRI_TYPE_LOWER));
    test_cases.emplace_back(new test_tri(GGML_TRI_TYPE_LOWER_DIAG));
    test_cases.emplace_back(new test_tri(GGML_TRI_TYPE_UPPER));
    test_cases.emplace_back(new test_tri(GGML_TRI_TYPE_UPPER_DIAG));

    test_cases.emplace_back(new test_fill(0.0f));
    test_cases.emplace_back(new test_fill(2.0f, GGML_TYPE_F32, { 303, 207, 11, 3 }));
    test_cases.emplace_back(new test_fill(-152.0f, GGML_TYPE_F32, { 800, 600, 4, 4 }));
    test_cases.emplace_back(new test_fill(3.5f, GGML_TYPE_F32, { 2048, 512, 2, 2 }));

    test_cases.emplace_back(new test_diag());
    test_cases.emplace_back(new test_diag(GGML_TYPE_F32, { 79, 1, 19, 13 }));
    test_cases.emplace_back(new test_diag(GGML_TYPE_F32, { 256, 1, 8, 16 }));

    test_cases.emplace_back(new test_solve_tri());
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 11, 11, 1, 1 }, { 5, 11, 1, 1 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 17, 17, 2, 4 }, { 9, 17, 2, 4 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 30, 30, 7, 1 }, { 8, 30, 7, 1 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 42, 42, 5, 2 }, { 10, 42, 5, 2 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 64, 64, 2, 2 }, { 10, 64, 2, 2 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 64, 64, 2, 2 }, { 64, 64, 2, 2 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 79, 79, 5, 3 }, { 417, 79, 5, 3 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 128, 128, 4, 2 }, { 32, 128, 4, 2 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 80, 80, 2, 8 }, { 80, 80, 2, 8 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 80, 80, 2, 8 }, { 79, 80, 2, 8 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 80, 80, 2, 8 }, { 81, 80, 2, 8 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 80, 80, 8, 8 }, { 80, 80, 8, 8 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 80, 80, 8, 8 }, { 79, 80, 8, 8 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 80, 80, 8, 8 }, { 81, 80, 8, 8 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 84, 84, 4, 4 }, { 32, 84, 4, 4 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 95, 95, 8, 8 }, { 40, 95, 8, 8 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 100, 100, 4, 4 }, { 41, 100, 4, 4 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 128, 128, 4, 4 }, { 31, 128, 4, 4 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 128, 128, 4, 4 }, { 32, 128, 4, 4 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 128, 128, 3, 4 }, { 32, 128, 3, 4 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 128, 128, 4, 1 }, { 32, 128, 4, 1 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 64, 64, 4, 4 }, { 200, 64, 4, 4 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 64, 64, 4, 4 }, { 384, 64, 4, 4 }));

    for (int tfrm : {0, 1, 2}) {
        for (bool circular : {false, true}) {
            test_cases.emplace_back(new test_pad_ext(GGML_TYPE_F32, {512, 512, 1, 1}, 0, 1, 0, 1, 0, 0, 0, 0, tfrm, circular));
            test_cases.emplace_back(new test_pad_ext(GGML_TYPE_F32, {11, 22, 33, 44}, 1, 2, 3, 4, 5, 6, 7, 8, tfrm, circular));
        }
    }

    // prefill-shaped cases with long KV (nb >= 32, kv >= 1024): covers the
    // XMX/GEMM-accelerated SYCL FA path which only activates for these shapes.
    for (int kv : { 1024, 2048, }) {
        for (int hs : { 64, 128, 256, }) {
            for (int nb : { 32, 64, }) {
                for (ggml_type type_KV : { GGML_TYPE_F16, GGML_TYPE_Q8_0, GGML_TYPE_Q4_0, }) {
                    test_cases.emplace_back(new test_flash_attn_ext(hs, hs, 8, {4, 1}, kv, nb, true, false, 0, 0, GGML_PREC_F32, type_KV, type_KV));
                }
            }
        }
    }

    for (int hsk : { 40, 64, 72, 80, 96, 128, 192, 256, 320, 512, 576 }) {
        for (int hsv : { 40, 64, 72, 80, 96, 128, 192, 256, 512 }) {
            if (hsk != 96 && hsk != 192 && hsk != 320 && hsk != 576 && hsk != hsv) continue;
            if (hsk ==  96 && (hsv !=  64 && hsv !=  96)) continue; // MiniCPM3
            if (hsk == 192 && (hsv != 128 && hsv != 192)) continue;
            if (hsk == 576 && hsv != 512) continue; // DeepSeek MLA
            if (hsk == 320 && hsv != 256) continue; // Mistral4 MLA

            for (bool mask : { true, false } ) {
                for (bool sinks : { true, false } ) {
                    for (float max_bias : { 0.0f, 8.0f }) {
                        if (!mask && max_bias > 0.0f) continue;
                        for (float logit_softcap : {0.0f, 10.0f}) {
                            if (hsk != 128 && logit_softcap != 0.0f) continue;
                            for (int nh : { 1, 4 }) {
                                if (nh == 1 && hsk != 320 && hsk != 576) continue;
                                for (int nr3 : { 1, 3, }) {
                                    if (hsk > 64 && nr3 > 1) continue; // skip broadcast for large head sizes
                                    for (int nr2 : { 1, 4, 6, 8, 9, 12, 16, 20, 32 }) {
                                        if (nr2 ==  6 && hsk != 128) continue; // non-power-of-2 GQA ratio
                                        if (nr2 ==  8 && hsk != 192) continue;
                                        if (nr2 ==  9 && hsk != 128) continue; // non-power-of-2 GQA ratio
                                        if (nr2 == 12 && hsk != 128) continue;
                                        if (nr2 == 16 && hsk != 192) continue;
                                        if (nr2 == 20 && (nh != 1 || hsk != 576)) continue;
                                        if (nr2 == 32 && (nh != 1 || hsk != 320)) continue;
                                        //for (int kv : { 1, 17, 31, 33, 61, 113, 65, 127, 129, 130, 255, 260, 371, 380, 407, 512, 1024, }) {
                                        for (int kv : { 113, 512, 1024, }) {
                                            if (nr2 != 1 && kv != 512) continue;
                                            for (int nb : { 1, 3, 32, 75, }) {
                                                for (ggml_prec prec : {GGML_PREC_F32, GGML_PREC_DEFAULT}) {
                                                    if (hsk != 128 && prec == GGML_PREC_DEFAULT) continue;
                                                    // TURBO2_0 is in this list because Vulkan supports_op claims it for FA
                                                    // (ggml-vulkan.cpp fa_kv_ok). It was absent until lane-148, which is why
                                                    // the stale FA_TYPE_TURBO2_0 spec-constant id went unnoticed alongside
                                                    // the turbo3/turbo4 ones: a claimed type with no test case is invisible.
                                                    for (ggml_type type_KV : {GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_BF16, GGML_TYPE_Q8_0, GGML_TYPE_Q5_1, GGML_TYPE_Q5_0, GGML_TYPE_Q4_1, GGML_TYPE_Q4_0, GGML_TYPE_IQ4_NL, GGML_TYPE_TURBO2_0, GGML_TYPE_TURBO3_0, GGML_TYPE_TURBO4_0}) {
                                                        if ((type_KV == GGML_TYPE_TURBO2_0 || type_KV == GGML_TYPE_TURBO3_0 || type_KV == GGML_TYPE_TURBO4_0) && hsk < 128) continue;
                                                        // turbo KV types are also used at head dim 256 (e.g. qwen35moe
                                                        // key_length=value_length=256); allow 256 for them so that shape
                                                        // is exercised rather than silently skipped.
                                                        const bool turbo_kv = (type_KV == GGML_TYPE_TURBO3_0 || type_KV == GGML_TYPE_TURBO4_0 || type_KV == GGML_TYPE_TURBO2_0);
                                                        if (type_KV != GGML_TYPE_F16 && hsk != 64 && hsk != 72 && hsk != 128 && !(turbo_kv && hsk == 256)) continue;
                                                        // DeepSeek MLA: the V cache is a sub-view of the K cache
                                                        const bool v_is_view_of_k = hsk == 576;
                                                        test_cases.emplace_back(new test_flash_attn_ext(
                                                                    hsk, hsv, nh, {nr2, nr3}, kv, nb, mask, sinks, max_bias, logit_softcap, prec, type_KV, type_KV, {0, 1, 2, 3}, true, v_is_view_of_k));
                                                        // run fewer test cases permuted
                                                        if (mask == true && max_bias == 0.0f && logit_softcap == 0 && kv == 512) {
                                                            test_cases.emplace_back(new test_flash_attn_ext(
                                                                        hsk, hsv, nh, {nr2, nr3}, kv, nb, mask, sinks, max_bias, logit_softcap, prec, type_KV, type_KV, {0, 2, 1, 3}, true, v_is_view_of_k));
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // asymmetric head_dim (hsk != hsv) with one or both sides not 64-aligned
    test_cases.emplace_back(new test_flash_attn_ext(72, 64, 4, {1, 1}, 256, 2, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(64, 72, 4, {1, 1}, 256, 2, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));

    // mixed quant and Q1_0 test cases
    test_cases.emplace_back(new test_flash_attn_ext(64, 64, 4, {1, 1}, 128, 2, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q4_0));
    test_cases.emplace_back(new test_flash_attn_ext(64, 64, 4, {1, 1}, 128, 2, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q4_0, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(72, 72, 4, {1, 1}, 96, 2, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q4_0, GGML_TYPE_Q8_0));
    test_cases.emplace_back(new test_flash_attn_ext(64, 64, 4, {1, 1}, 96, 2, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F32));
    test_cases.emplace_back(new test_flash_attn_ext(128, 128, 4, {1, 1}, 256, 1, false, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_Q4_0));
    test_cases.emplace_back(new test_flash_attn_ext(128, 128, 4, {1, 1}, 96, 2, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q1_0, GGML_TYPE_Q1_0));
    test_cases.emplace_back(new test_flash_attn_ext(128, 64, 4, {1, 1}, 128, 2, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q1_0, GGML_TYPE_Q4_0));
    test_cases.emplace_back(new test_flash_attn_ext(64, 128, 4, {1, 1}, 128, 2, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q4_0, GGML_TYPE_Q1_0));
    test_cases.emplace_back(new test_flash_attn_ext(128, 64, 4, {1, 1}, 64, 2, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q1_0, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(128, 128, 4, {1, 1}, 96, 2, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q2_0, GGML_TYPE_Q2_0));
    test_cases.emplace_back(new test_flash_attn_ext(128, 64, 4, {1, 1}, 128, 2, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q2_0, GGML_TYPE_Q4_0));
    test_cases.emplace_back(new test_flash_attn_ext(64, 128, 4, {1, 1}, 128, 2, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q4_0, GGML_TYPE_Q2_0));
    test_cases.emplace_back(new test_flash_attn_ext(128, 64, 4, {1, 1}, 64, 2, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q2_0, GGML_TYPE_F16));

    // q8_0 KV cases: decode and prompt batches, KV pad, permuted KV, feature flags, and long context
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 1},   113,   1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 1},  1024,   1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 1},  1024,   1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0, {0, 2, 1, 3}));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 2},  1025,   1, true, true,  8, 30, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 1},  1025,  64, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0, {0, 2, 1, 3}));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 1}, 16384,   1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0));

    // MLA shape: the V cache is a sub-view of the K cache, with quantized KV
    test_cases.emplace_back(new test_flash_attn_ext(576, 512, 1, {8, 1},  113,   1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0, {0, 1, 2, 3}, true, true));
    test_cases.emplace_back(new test_flash_attn_ext(576, 512, 1, {8, 1}, 1024,   1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0, {0, 1, 2, 3}, true, true));
    test_cases.emplace_back(new test_flash_attn_ext(576, 512, 1, {8, 1}, 1024,  64, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0, {0, 1, 2, 3}, true, true));

    // Sparse mask hint: supported decode/prefill layouts and dense fallbacks.
    test_cases.emplace_back(new test_flash_attn_ext(512, 512, 1, { 8, 1}, 4096, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false,  512));
    test_cases.emplace_back(new test_flash_attn_ext(512, 512, 1, { 8, 2}, 4096, 3, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false,  768));
    test_cases.emplace_back(new test_flash_attn_ext(576, 512, 1, {16, 1}, 4096, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, true,   512));
    test_cases.emplace_back(new test_flash_attn_ext(576, 512, 1, {16, 2}, 4096, 2, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, true,   768));
    test_cases.emplace_back(new test_flash_attn_ext(512, 512, 1, { 8, 1}, 4096, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false, 2304));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 1, { 8, 1}, 4096, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false,  512));
    test_cases.emplace_back(new test_flash_attn_ext(128, 128, 1, { 8, 1}, 4096, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false,  512));

    test_cases.emplace_back(new test_flash_attn_ext(128, 128, 1, { 8, 1}, 4096, 4, true, false, 8.0f, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false,  512));

    // sparse mask with large batch size
    test_cases.emplace_back(new test_flash_attn_ext(512, 512, 1, { 8, 1}, 4096, 64, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false,  512));
    test_cases.emplace_back(new test_flash_attn_ext(512, 512, 1, { 8, 1}, 4096, 64, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false, 2048));
    test_cases.emplace_back(new test_flash_attn_ext(576, 512, 1, {16, 1}, 4096, 64, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, true,   512));
    test_cases.emplace_back(new test_flash_attn_ext(576, 512, 1, {16, 1}, 4096, 64, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, true,  2048));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 1, { 8, 1}, 4096, 64, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false,  512));
    test_cases.emplace_back(new test_flash_attn_ext(128, 128, 1, { 8, 1}, 4096, 64, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false,  512));
    test_cases.emplace_back(new test_flash_attn_ext(128, 128, 1, { 8, 1}, 4096, 64, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false, 2048));

    // sparse attn (qwen4 shape - gqa 12)
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {12, 1}, 4096,  1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false,  512));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {12, 1}, 8192, 64, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false,  512));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 1, {12, 2}, 8192, 67, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false,  512));

    // sparse mask + quantized cache
    test_cases.emplace_back(new test_flash_attn_ext(128, 128, 1, { 8, 1}, 4096,  1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0, {0, 1, 2, 3}, true, false, 512));
    test_cases.emplace_back(new test_flash_attn_ext(128, 128, 1, { 8, 1}, 4096, 64, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0, {0, 1, 2, 3}, true, false, 512));

    // Qwen QSA: 256/256, gqa 12, budget 2048.
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {12, 1}, 8192, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false, 2048));

    // more V-is-sub-view-of-K cases: other head shapes, and full views with equal head sizes
    test_cases.emplace_back(new test_flash_attn_ext(320, 256, 1, {32, 1}, 512, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, true));
    test_cases.emplace_back(new test_flash_attn_ext(192, 128, 4, {8, 1},  512, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, true));
    test_cases.emplace_back(new test_flash_attn_ext(128, 128, 8, {4, 1},  512, 8, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, true));
    test_cases.emplace_back(new test_flash_attn_ext(64,  64,  4, {1, 1},  512, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0, {0, 1, 2, 3}, true, true));

    // large-KV F16 cases (Qwen3.6-27B geometry and a llama-class control): the upstream matrix
    // stops at kv=1024, blind to long-context FA bugs (e.g. the oneDNN SDPA ordering race on BMG).
    for (int64_t kv : { 4096, 16384 }) {
        test_cases.emplace_back(new test_flash_attn_ext(256, 256, 4, {6, 1}, kv, 512, true, false, 0, 0,
                                                        GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
        test_cases.emplace_back(new test_flash_attn_ext(128, 128, 8, {4, 1}, kv, 512, true, false, 0, 0,
                                                        GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    }

    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 4, {6, 1},   512, 512, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 4, {6, 1},  4096,  64, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 4, {6, 1},  4096,  16, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 4, {2, 1},  4096, 512, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 4, {4, 1},  4096, 512, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {12, 1}, 4096, 512, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));

    // dense-allocated (non-view) quant K/V at batch >= 64, in cache and native layouts
    test_cases.emplace_back(new test_flash_attn_ext(64, 64, 4, {1, 1}, 512, 75, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0, {0, 2, 1, 3}, false));
    test_cases.emplace_back(new test_flash_attn_ext(64, 64, 4, {4, 1}, 512, 75, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0, {0, 2, 1, 3}, false));
    test_cases.emplace_back(new test_flash_attn_ext(64, 64, 4, {1, 1}, 1024, 75, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0, {0, 2, 1, 3}, false));
    test_cases.emplace_back(new test_flash_attn_ext(64, 64, 4, {1, 1}, 512, 75, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0, {0, 1, 2, 3}, false));

    // FLASH_ATTN_EXT MMA: non-pow2 head size and MLA K/V view.
    test_cases.emplace_back(new test_flash_attn_ext(192, 128, 8, {8, 1}, 4096, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(576, 512, 1, {20, 1}, 512, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, true));

    // FLASH_ATTN_EXT MMA, swizzled K/V tiles, power-of-two stride: nbatch_K2 = 32, 64, 128, 256.
    test_cases.emplace_back(new test_flash_attn_ext( 64,  64, 8, {8, 1}, 4096,  4, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(128, 128, 8, {4, 1}, 4096,  8, true,  true, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 4, {2, 1}, 1024, 32, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(512, 512, 4, {2, 1}, 1024,  4, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));

    test_cases.emplace_back(new test_cross_entropy_loss     (GGML_TYPE_F32, {   10, 5, 4, 3}));
    test_cases.emplace_back(new test_cross_entropy_loss     (GGML_TYPE_F32, {30000, 1, 1, 1}));
    test_cases.emplace_back(new test_cross_entropy_loss_back(GGML_TYPE_F32, {   10, 5, 4, 3}));
    test_cases.emplace_back(new test_cross_entropy_loss_back(GGML_TYPE_F32, {30000, 1, 1, 1}));

    test_cases.emplace_back(new test_opt_step_adamw(GGML_TYPE_F32, {10, 5, 4, 3}));
    test_cases.emplace_back(new test_opt_step_sgd(GGML_TYPE_F32, {10, 5, 4, 3}));

    for (ggml_type type : base_types) {
        for (bool with_gate : {false, true}) {
            for (bool use_id : {false, true}) {
                for (bool b : {false, true}) {
                    if (!use_id && b) {
                        continue;
                    }
                    for (bool with_bias : {false, true}) {
                        if (!with_gate && !with_bias) {
                            continue;
                        }
                        for (ggml_glu_op glu_op : {GGML_GLU_OP_SWIGLU, GGML_GLU_OP_GEGLU, GGML_GLU_OP_SWIGLU_CLAMP}) {
                            if (!with_bias && glu_op == GGML_GLU_OP_SWIGLU_OAI) {
                                continue;
                            }
                            if (!with_gate && glu_op != GGML_GLU_OP_SWIGLU) {
                                continue;
                            }
                            for (bool with_lane_scale : {false, true}) {
                                if (with_lane_scale && type != GGML_TYPE_NVFP4) {
                                    continue;
                                }
                                test_cases.emplace_back(new test_mul_mat_vec_fusion(type, glu_op, 1, 32, 256,
                                    use_id, 16, 8, b, with_bias, with_gate, with_lane_scale));
                                test_cases.emplace_back(new test_mul_mat_vec_fusion(type, glu_op, 1, 32, 256,
                                    use_id, 16, 8, b, with_bias, with_gate, with_lane_scale, {1, 1}));
                                // multi-token batches (spec decoding)
                                for (int64_t m_batch : { 2, 4, 8 }) {
                                    test_cases.emplace_back(new test_mul_mat_vec_fusion(type, glu_op, m_batch, 32, 256,
                                        use_id, 16, 8, b, with_bias, with_gate, with_lane_scale, {1, 1}));
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    for (bool b : {false, true}) {
        test_cases.emplace_back(new test_mul_mat_vec_fusion(GGML_TYPE_IQ2_S, GGML_GLU_OP_SWIGLU_CLAMP, 1, 32, 256,
            true, 16, 8, b, false, true, false));
    }

    // Fused row-pair coverage: minimum rows, an even pair, and an odd tail.
    // TODO: the max_nmse_err() for these cases is not estimated correctly causing sporadic false failures.
    //for (ggml_glu_op glu_op : { GGML_GLU_OP_SWIGLU, GGML_GLU_OP_GEGLU }) {
    //    for (int64_t m_batch : { 2, 3, 4 }) {
    //        for (int64_t rows : { 1, 2, 3 }) {
    //            test_cases.emplace_back(new test_mul_mat_vec_fusion(GGML_TYPE_Q4_K, glu_op, m_batch, rows, 256,
    //                false, 16, 8, false, false, true, false, { 1, 1 }));
    //        }
    //    }
    //}

    // Both sides of the same row-count boundary as above, on the fused path.
    for (int64_t rows : {6271, 6272, 6273}) {
        test_cases.emplace_back(new test_mul_mat_vec_fusion(GGML_TYPE_Q4_K, GGML_GLU_OP_SWIGLU, 2, rows, 256,
            false, 16, 8, false, false, true, false, { 1, 1 }));
    }

    for (auto gate : {GATING_FUNC_SOFTMAX, GATING_FUNC_SIGMOID, GATING_FUNC_SOFTMAX_WEIGHT, GATING_FUNC_SQRT_SOFTPLUS}) {
        for (bool with_norm : {false, true}) {
            for (bool bias_probs : {false, true}) {
                for (float scale_w : {0.0f, 2.0f}) {
                    test_cases.emplace_back(new test_topk_moe({8, 22, 1, 1}, 4, with_norm, bias_probs, gate, scale_w));
                    test_cases.emplace_back(new test_topk_moe({31, 22, 1, 1}, 8, with_norm, bias_probs, gate, scale_w));
                    test_cases.emplace_back(new test_topk_moe({32, 22, 1, 1}, 8, with_norm, bias_probs, gate, scale_w));
                    test_cases.emplace_back(new test_topk_moe({40, 22, 1, 1}, 8, with_norm, bias_probs, gate, scale_w));
                    test_cases.emplace_back(new test_topk_moe({71, 22, 1, 1}, 8, with_norm, bias_probs, gate, scale_w));
                    test_cases.emplace_back(new test_topk_moe({128, 1, 1, 1}, 128, with_norm, bias_probs, gate, scale_w));
                    test_cases.emplace_back(new test_topk_moe({129, 1, 1, 1}, 128, with_norm, bias_probs, gate, scale_w));
                    test_cases.emplace_back(new test_topk_moe({160, 4, 1, 1}, 160, with_norm, bias_probs, gate, scale_w));
                    test_cases.emplace_back(new test_topk_moe({256, 22, 1, 1}, 6, with_norm, bias_probs, gate, scale_w)); // Used by DeepSeek-V4
                    test_cases.emplace_back(new test_topk_moe({288, 22, 1, 1}, 8, with_norm, bias_probs, gate, scale_w)); // Used by StepFun 3.7
                    // rows at and just past the limit where one block still covers all rows
                    test_cases.emplace_back(new test_topk_moe({32, 8, 1, 1}, 4, with_norm, bias_probs, gate, scale_w));
                    test_cases.emplace_back(new test_topk_moe({32, 8, 1, 1}, 8, with_norm, bias_probs, gate, scale_w));
                    test_cases.emplace_back(new test_topk_moe({32, 9, 1, 1}, 8, with_norm, bias_probs, gate, scale_w));
                }
            }
        }
    }

    // Cover the supported boundaries, common k = 8 shapes, interleaved views and adds, and k = 16 fallback.
    test_cases.emplace_back(new test_moe_reduce(63,  2, 17));
    test_cases.emplace_back(new test_moe_reduce(2048, 8, 128));
    test_cases.emplace_back(new test_moe_reduce(2048, 8, 128, false, true));
    test_cases.emplace_back(new test_moe_reduce(63,   12, 33, true,  true, true));
    test_cases.emplace_back(new test_moe_reduce(2048, 15, 40, false, true));
    test_cases.emplace_back(new test_moe_reduce(2048, 16, 32, false, true));

    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 32, 128, 1, 1));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 32, 16, 1, 1));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 32, 16, 1, 1, 1, true, true));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 32, 16, 1, 1, 1, false, true));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 16, 64, 1, 2));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64, 4, 1));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64, 4, 2));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 8, 32, 4, 2, 2));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64, 4, 2, 1, true));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64, 4, 1, 1, true));
    // KDA (vector gate)
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64, 1, 1, 1, false, true));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64, 1, 2, 1, false, true));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 16, 1, 2, 1, false, true));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 32, 4, 1, 1, false, true));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64, 4, 2, 1, false, true));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 8, 32, 4, 2, 2, false, true));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64, 4, 2, 1, true,  true));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 16, 4, 2, 1, true,  true));
    // chunked path: multi-chunk and non-multiple-of-chunk-size (chunk_size=64 GDN, 16 KDA)
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64,  64, 1));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64, 127, 1));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64, 256, 1));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64,  65, 1));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64, 100, 1));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64, 200, 1));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64, 127, 2));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64,  64, 1, 1, false, true));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64,  33, 1, 1, false, true));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64, 100, 1, 1, false, true));

    // K > 1: output keeps the last min(n_tokens, K) per-token snapshots, ordered most-recent-first
    // (slot 0 = final state, slot s = state s tokens back).
    // exact-match cases (K == n_seq_tokens):
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 16,   2, 1, 1, false, false, /*K=*/2));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 32,   4, 1, 1, false, false, /*K=*/4));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64,   4, 2, 1, false, false, /*K=*/4));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 8, 128,  4, 1, 1, false, false, /*K=*/4));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64,   4, 2, 1, false, true,  /*K=*/4));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 8, 32,   4, 2, 2, false, true,  /*K=*/4));
    // overflow: n_tokens > K — only the last K snapshots kept.
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 32,   8, 1, 1, false, false, /*K=*/3));
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 64,  16, 2, 1, false, false, /*K=*/4));

    // gdn + cache cpy fusion (K > 1)
    test_cases.emplace_back(new test_gated_delta_net_cache_fusion(GGML_TYPE_F32, 4, 32,   2, 1, 2));
    test_cases.emplace_back(new test_gated_delta_net_cache_fusion(GGML_TYPE_F32, 4, 64,   4, 1, 2));
    test_cases.emplace_back(new test_gated_delta_net_cache_fusion(GGML_TYPE_F32, 4, 32,   4, 1, 4));
    test_cases.emplace_back(new test_gated_delta_net_cache_fusion(GGML_TYPE_F32, 8, 32,   4, 2, 4));
    test_cases.emplace_back(new test_gated_delta_net_cache_fusion(GGML_TYPE_F32, 4, 32,   8, 1, 4));

#if 0
    // these tests are disabled to save execution time, sbut they can be handy for debugging
    test_cases.emplace_back(new test_llama(2, true));
    test_cases.emplace_back(new test_llama(1));
    test_cases.emplace_back(new test_llama(2));
    test_cases.emplace_back(new test_falcon(1));
    test_cases.emplace_back(new test_falcon(2));
#endif

    // lightning_indexer
    for (int kv : { 256 }) {
        for (int bs : { 1, 512 }) {
            for (int nh : { 32, 64 }) {
                for (auto [ns, nm] : { std::pair{1, 1}, std::pair{4, 4}, std::pair{4, 1} }) {
                    for (ggml_type type_K : {GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_BF16, GGML_TYPE_Q8_0, GGML_TYPE_Q5_1, GGML_TYPE_Q5_0, GGML_TYPE_Q4_1, GGML_TYPE_Q4_0, GGML_TYPE_IQ4_NL}) {
                        test_cases.emplace_back(new test_lightning_indexer(128, nh, kv, bs, ns, nm, type_K));
                    }
                }
            }
        }
    }

    for (int kv : { 1, 7, 8, 63, 64, 65 }) {
        for (ggml_type type_K : {GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_BF16, GGML_TYPE_Q8_0, GGML_TYPE_Q5_1, GGML_TYPE_Q5_0, GGML_TYPE_Q4_1, GGML_TYPE_Q4_0}) {
            test_cases.emplace_back(new test_lightning_indexer(128, 64, kv, 32, 4, 1, type_K));
        }
    }

    return test_cases;
}
#ifdef _MSC_VER
#pragma optimize("", on)
#endif

// Test cases for performance evaluation: should be representative of real-world use cases
static std::vector<std::unique_ptr<test_case>> make_test_cases_perf() {
    std::vector<std::unique_ptr<test_case>> test_cases;

    // ArifiLabs Escha-W2 fused linear (lane-164): real 27B projection shapes across the
    // decode->prefill ncols range. Cost is strictly linear in ncols (measured on a 780M:
    // 1.15 us*1000/col for escha2 5120x12288, 2.12 for escha3 17408x5120, from ncols 1/8/64),
    // which is what put an unguarded ncols=2048 dispatch at ~4.3 s, past the GPU watchdog
    // (F-124). 1/8/64 pins the per-column slope in ~5 min; larger ncols adds no information
    // and costs hours, because n_runs is sized from bytes moved, not from decode work.
    for (int64_t ncols : {1, 8, 64}) {
        test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA2, 5120, 12288, ncols));
        test_cases.emplace_back(new test_escha_mm(GGML_TYPE_ESCHA3, 17408, 5120, ncols));
    }

    // SWIGLU at a 27B-class FFN width, fused [gate|up] vs split operands
    // note: same bytes either way, so a backend that indexes them differently shows it here
    for (ggml_type type : {GGML_TYPE_F16, GGML_TYPE_F32}) {
        for (int64_t n_tokens : {512, 2048}) {
            test_cases.emplace_back(new test_glu(GGML_GLU_OP_SWIGLU, type, { 2*17408, n_tokens, 1, 1 }, 0, false));
            test_cases.emplace_back(new test_glu_split(GGML_GLU_OP_SWIGLU, type, { 17408, n_tokens, 1, 1 }, 0));
        }
    }

    // CONT of a 0<->2 permute at DeepSeek-V4 lightning-indexer shapes:
    // indexer_kq is [n_kv, n_tokens, n_head=64] and gets ggml_cont(ggml_permute(.., 2,1,0,3)).
    for (int64_t n_kv : { 1024, 1280, 2048, 2304 }) {
        test_cases.emplace_back(new test_cont(
            GGML_TYPE_F32, {n_kv, 64, 64, 1}, false, {2, 1, 0, 3}));
    }
    for (int64_t n_kv : { 2048, 2304 }) {
        test_cases.emplace_back(new test_cont(
            GGML_TYPE_F32, {n_kv, 512, 64, 1}, false, {2, 1, 0, 3}));
    }

    // LEAKY_RELU at FFN activation width, for direct comparison with RELU
    for (int64_t n_tokens : {512, 2048}) {
        test_cases.emplace_back(new test_leaky_relu(GGML_TYPE_F32, { 17408, n_tokens, 1, 1 }, 0.1f));
    }

    // Conv2d: K=CRS=NPQ=4096 matmul performance
    uint32_t                        iwh_idx  = 0;
    uint32_t                        kwh_idx  = 1;
    uint32_t                        Cout_idx = 2;
    uint32_t                        Cin_idx  = 3;
    uint32_t                        B_idx    = 4;
    std::vector<std::array<int, 5>> cases    = {
  //{IWH, KWH, Cout, Cin, B}
  // K=CRS=NPQ=4096 conv2d matmul performance
        {19,   4, 4096, 256, 16},
 // K=128, CRS=128, NPQ=4096
        { 19,  4, 128,  8,   16},
 // K=130, CRS=128, NPQ=4096
        { 19,  4, 130,  8,   16},
 // Edge case: K x CRS is small
        { 19,  2, 4,    4,   16},
 // A ConvNet's first layer
        { 224, 3, 8,    3,   1 },
 // A ConvNet's first layer with 2x2 convolution, and 1 channel
        { 224, 2, 8,    1,   1 },
 // A ConvNet's first layer with 2x2 convolution, and 1 channel, several images in the batch
        { 224, 2, 8,    1,   8 },
 // A middle layer of a ConvNet
        { 58,  3, 64,   32,  1 },
 // A middle layer of a ConvNet, several images in the batch
        { 58,  3, 64,   32,  8 },
 // A deep layer of a ConvNet, several images in the batch
        { 16,  3, 512,  128, 8 },
 // High resolution output (large NPQ)
        {1536, 3, 64,   32,  1 },
    };

    for (auto kernel_type : {GGML_TYPE_F32, GGML_TYPE_F16}) {
        for (auto act_case : cases) {
            // Direct CONV_2D
            test_cases.emplace_back(new test_conv_2d(
                { act_case[iwh_idx], act_case[iwh_idx], act_case[Cin_idx], act_case[B_idx] },
                { act_case[kwh_idx], act_case[kwh_idx], act_case[Cin_idx], act_case[Cout_idx] },
                kernel_type, 1, 1, 0, 0, 1, 1, false));   // bool cwhn = false
            test_cases.emplace_back(new test_conv_2d(
                { act_case[iwh_idx], act_case[iwh_idx], act_case[Cin_idx], act_case[B_idx] },
                { act_case[kwh_idx], act_case[kwh_idx], act_case[Cin_idx], act_case[Cout_idx] },
                kernel_type, 1, 1, 0, 0, 1, 1, true));    // bool cwhn = true
        }
    }

    struct conv3d_perf_case {
        int N, IC, ID, IH, IW, OC, KD, KH, KW, s0, s1, s2, p0, p1, p2, d0, d1, d2;
    };

    const std::vector<conv3d_perf_case> conv3d_cases = {
        {1,  320, 8,  38,  26, 1280, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1280, 8,  38,  26, 1280, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1,  320, 8,  76,  52, 1280, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1, 1280, 8,  76,  52, 1280, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1,  320, 8, 152, 104, 1280, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1},
#if 0
        // too slow on some devices
        {1, 1280, 8, 152, 104, 1280, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1,  320, 4, 304, 208, 1280, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1},
        {1,  640, 4, 304, 208, 1280, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1},
#endif
    };

    for (ggml_type kernel_type : {GGML_TYPE_F32, GGML_TYPE_F16}) {
        for (const conv3d_perf_case & c : conv3d_cases) {
            test_cases.emplace_back(new test_conv_3d(
                c.N, c.IC, c.ID, c.IH, c.IW,
                c.OC, c.KD, c.KH, c.KW,
                c.s0, c.s1, c.s2, c.p0, c.p1, c.p2, c.d0, c.d1, c.d2,
                kernel_type));
        }
    }

    test_cases.emplace_back(new test_bin_bcast(ggml_add, GGML_TYPE_F32, {4096, 1, 1, 1}, {1,   1, 1, 1}));
    test_cases.emplace_back(new test_bin_bcast(ggml_add, GGML_TYPE_F32, {4096, 1, 1, 1}, {1, 512, 1, 1}));

    test_cases.emplace_back(new test_cpy(GGML_TYPE_F32,  GGML_TYPE_F16,  {512, 3072, 1, 1}));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F32,  GGML_TYPE_F32,  {8192, 512, 2, 1}, {-1,-1,-1,-1}, {0, 2, 1, 3}));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F32,  GGML_TYPE_F32,  {3072, 512, 2, 1}, {-1,-1,-1,-1}, {0, 2, 1, 3}));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F32,  GGML_TYPE_Q4_0, {8192, 512, 2, 1}));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_Q4_0, GGML_TYPE_F32,  {8192, 512, 2, 1}));

    test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_F32, {768*1024, 256, 1, 1}, {-1,-1,-1,-1}, {1, 0, 2, 3}, {0, 0, 0, 0}));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F16, GGML_TYPE_F16, {768*1024, 256, 1, 1}, {-1,-1,-1,-1}, {1, 0, 2, 3}, {0, 0, 0, 0}));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F16, GGML_TYPE_F16, {768, 1024, 256, 1}, {-1,-1,-1,-1}, {1, 0, 2, 3}, {0, 0, 0, 0}));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_BF16, GGML_TYPE_BF16, {768, 1024, 256, 1}, {-1,-1,-1,-1}, {1, 0, 2, 3}, {0, 0, 0, 0}));

    test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_F32, {768*1024, 256, 1, 1}, {-1,-1,-1,-1}, {0, 0, 0, 0}, {0, 0, 0, 0}, true));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_F32, {768, 1024, 256, 1}, {-1,-1,-1,-1}, {0, 0, 0, 0}, {0, 0, 0, 0}, true));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F16, GGML_TYPE_F16, {768*1024, 256, 1, 1}, {-1,-1,-1,-1}, {0, 0, 0, 0}, {0, 0, 0, 0}, true));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_F16, GGML_TYPE_F16, {768, 1024, 256, 1}, {-1,-1,-1,-1}, {0, 0, 0, 0}, {0, 0, 0, 0}, true));
    test_cases.emplace_back(new test_cpy(GGML_TYPE_BF16, GGML_TYPE_BF16, {768, 1024, 256, 1}, {-1,-1,-1,-1}, {0, 0, 0, 0}, {0, 0, 0, 0}, true));

    // ring-repair 2026-08-24: recurrent-bank plane writes - cpy into an OFFSET view of a larger
    // buffer (the conv/ssm snapshot bank shape: [row, seqs] planes at row offsets). KFLIP showed
    // Vulkan corrupts these while offset-0 writes are clean; these cases pin it at op level.
    for (int64_t off : {1, 3, 7}) {
        test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_F32, {12288, 1, 1, 1}, {-1,-1,-1,-1},
            {0,0,0,0}, {0,0,0,0}, false, {12288, 8, 1, 1}, off));
        test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_F32, {5760, 2, 1, 1}, {-1,-1,-1,-1},
            {0,0,0,0}, {0,0,0,0}, false, {5760, 16, 1, 1}, off));
        test_cases.emplace_back(new test_cpy(GGML_TYPE_F32, GGML_TYPE_F32, {12, 3, 1, 1}, {-1,-1,-1,-1},
            {0,0,0,0}, {0,0,0,0}, false, {12, 24, 1, 1}, off));
    }

    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {4096, 4096, 5, 1}, false, false, GGML_TYPE_F32, {1, 1}, 1.0f, 0.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {12888, 256, 5, 1}, false, false, GGML_TYPE_F32, {1, 1}, 1.0f, 0.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {77, 4096, 5, 1}, false, false, GGML_TYPE_F32, {1, 1}, 1.0f, 0.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {1024, 1024, 10, 1}, false, false, GGML_TYPE_F32, {1, 1}, 1.0f, 0.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {77, 1024, 10, 1}, false, false, GGML_TYPE_F32, {1, 1}, 1.0f, 0.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {256, 256, 20, 1}, false, false, GGML_TYPE_F32, {1, 1}, 1.0f, 0.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {64, 64, 20, 1}, false, false, GGML_TYPE_F32, {1, 1}, 1.0f, 0.0f));
    test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {77, 64, 20, 1}, false, false, GGML_TYPE_F32, {1, 1}, 1.0f, 0.0f));

    test_cases.emplace_back(new test_argmax(GGML_TYPE_F32, {32, 10, 1, 1}));
    test_cases.emplace_back(new test_argmax(GGML_TYPE_F32, {1024, 10, 1, 1}));
    test_cases.emplace_back(new test_argmax(GGML_TYPE_F32, {32000, 512, 1, 1}));

    test_cases.emplace_back(new test_pad_reflect_1d(GGML_TYPE_F32, {512, 34, 2, 1}));
    test_cases.emplace_back(new test_pad_reflect_1d(GGML_TYPE_F32, {3000, 80, 1, 1}));
    test_cases.emplace_back(new test_pad_reflect_1d(GGML_TYPE_F32, {3000, 80, 4, 1}));
    test_cases.emplace_back(new test_pad_reflect_1d(GGML_TYPE_F32, {3000, 384, 1, 1}));
    test_cases.emplace_back(new test_pad_reflect_1d(GGML_TYPE_F32, {3000, 384, 4, 1}));

    // SNAKE activation fusion at BigVGAN scale (T=7680 = 24 kHz x 320 ms, C=192)
    test_cases.emplace_back(new test_snake_fuse(GGML_TYPE_F32,  {7680, 192, 1, 1}));
    test_cases.emplace_back(new test_snake_fuse(GGML_TYPE_F16,  {7680, 192, 1, 1}));
    test_cases.emplace_back(new test_snake_fuse(GGML_TYPE_BF16, {7680, 192, 1, 1}));

    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F32, 16416, 1, 128, {8,  1}, {4, 1}, {0, 2, 1, 3}));
    test_cases.emplace_back(new test_mul_mat(GGML_TYPE_F16, GGML_TYPE_F32, 128, 1, 16416, {8,  1}, {4, 1}, {0, 1, 2, 3}, 2*16416));

    // FWHT tests
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F32, 128, 1, 128));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F32, 64, 1, 64));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F32, 256, 1, 256));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F32, 128, 32, 128));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F32, 64, 2048, 64));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F32, 128, 2048, 128));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F32, 256, 2048, 256));
    test_cases.emplace_back(new test_mul_mat_hadamard(GGML_TYPE_F32, GGML_TYPE_F32, 512, 2048, 512));

    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 64, 64, 4, 4 }, { 32, 64, 4, 4 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 128, 128, 4, 2 }, { 32, 128, 4, 2 }));
    // qwen3next with CHUNK_SIZE 64
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 64, 64, 8, 32 }, { 64, 64, 8, 32 }));
    // qwen3next with CHUNK_SIZE 128
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 128, 128, 4, 32 }, { 128, 128, 4, 32 }));
    test_cases.emplace_back(new test_solve_tri(GGML_TYPE_F32, { 256, 256, 4, 2 }, { 128, 256, 4, 2 }));

    test_cases.emplace_back(new test_tri(GGML_TRI_TYPE_LOWER, GGML_TYPE_F32, { 256, 256, 4, 4 }));
    test_cases.emplace_back(new test_tri(GGML_TRI_TYPE_UPPER_DIAG, GGML_TYPE_F32, { 1024, 1024, 8, 4 }));

    test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, { 128, 128, 4, 4 }));
    test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, { 2048, 16, 5, 4 }));
    test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, { 20000, 10, 4, 1 }));

    for (int bs : {1, 2, 3, 4, 5, 8, 512}) {
        for (ggml_type type_a : all_types) {
            for (ggml_type type_b : {GGML_TYPE_F32}) {
                test_cases.emplace_back(new test_mul_mat(type_a, type_b, 4096, bs, 14336, {1,  1}, {1, 1}));
            }
        }
    }

    // Q4_K multi-column mat-vec
    for (int64_t m : {4096, 6144, 6272, 14336}) {
        for (int bs : {1, 2, 3, 4, 8}) {
            test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q4_K, GGML_TYPE_F32, m, bs, 4096, {1, 1}, {1, 1}));
        }
    }

    // ArifiLabs lane-209: the 27B ffn row (17408x5120) at the plain / DFlash2-verify widths, for the types the
    // seat and the Unleashed quality winners actually carry there (q4_K = the yardstick shader at 96-100% of
    // best on the 780M). `-p "m=17408"` narrows perf to these 24 rows.
    for (int bs : {1, 2, 3, 4, 5, 8}) {
        for (ggml_type type_a : {GGML_TYPE_Q4_K, GGML_TYPE_IQ4_XS, GGML_TYPE_IQ3_S, GGML_TYPE_Q4_0_ROCMFP4_FAST}) {
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32, 17408, bs, 5120, {1,  1}, {1, 1}));
        }
    }

    // ArifiLabs lane-249 / R61: the 27B GSQ-RCO iquant types on the shapes that file ACTUALLY
    // serves them at, at every NUM_COLS the spec constant reaches.
    //
    // The two FFN orientations are where IQ3_S (144 tensors), IQ4_XS (96) and IQ3_XXS (78) live in
    // that file; its output.weight is Q4_K (Sol MEDIUM review 2026-09-16 §A), so the lm_head shape
    // is NOT an iquant shape on this file. The block above covered 17408x5120 for iq3_s / iq4_xs at
    // bs {1,2,3,4,5,8} only -- no iq3_xxs at all, no 5120x17408, and no n=6/n=7. This closes those
    // holes so the width sweep of the three types has no gap and a sibling control at every cell.
    //
    // READ THESE AS DIAGNOSTICS, NOT AS A VERDICT. The R50 brief names both FFN shapes
    // launch/issue-bound in this microbench ("33-39 GB/s at every width -- do not chase those
    // rows"), a cost the served graph pipelines away. What they can honestly answer is the
    // RELATIVE width question: three types on the same framework, same shape, same rounds.
    //
    // NO ENV IS NEEDED for the three iquant rows: GGML_VK_FORCE_MMVQ=1 sets quantize_y, the q8_1
    // pipeline getter returns nullptr for these types, and the caller falls straight back to the
    // f32 dequant shader with quantize_y=false. They measure mul_mat_vec_iq3_s.comp /
    // mul_mat_vec_iq3_xxs.comp / mul_mat_vec_iq4_xs.comp unconditionally. The q4_K control DOES
    // move with the env, and at n>=5, k<=8192 it is on MMVQ+hoist by default (lane-235 / R48b).
    //
    // Filters: -p "m=17408" / -p "m=5120" / -p "m=248320".
    for (int bs : {1, 2, 3, 4, 5, 6, 7, 8}) {
        for (ggml_type type_a : {GGML_TYPE_IQ3_S, GGML_TYPE_IQ3_XXS, GGML_TYPE_IQ4_XS, GGML_TYPE_Q4_K}) {
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32,  17408, bs,  5120, {1, 1}, {1, 1}));
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32,   5120, bs, 17408, {1, 1}, {1, 1}));
            // Synthetic lm_head shape: correctness coverage and a width reference on a shape the
            // microbench reads cleanly. It is NOT this file's served lm_head (that one is Q4_K).
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32, 248320, bs,  5120, {1, 1}, {1, 1}));
        }
    }

    // ArifiLabs lane-230 / R46: the REAL 27B S-X8 decode shapes. The 27B .sx8v43 file carries 128
    // gate/up matrices at 17408x5120 and 64 down matrices at 5120x17408 in S-X8, plus the
    // 248320x5120 embedding/output pair; every other projection is Q8_0. Q8_0 and Q4_K ride along
    // as the two controls the R46 analysis pairs S-X8 against. n=6 and n=7 fill the two widths the
    // lane-224 receipts left empty, so the mat-vec width sweep has no hole.
    //
    // lane-232 / R47a kept ONE copy of this block at the R46b rebase: R47a had copied it verbatim
    // off R46 because R46 was not then an ancestor, and here it is. It is also the sweep's target:
    // with GGML_VK_SX8_MMV_ROWS / GGML_VK_SX8_MMV_WG set, one binary runs rows 1/2/4/8 x
    // workgroups 64/128/256 over these rows with no rebuild.
    for (int bs : {1, 2, 3, 4, 5, 6, 7, 8}) {
        for (ggml_type type_a : {GGML_TYPE_SX8, GGML_TYPE_Q8_0, GGML_TYPE_Q4_K}) {
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32,  17408, bs,  5120, {1, 1}, {1, 1}));
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32,   5120, bs, 17408, {1, 1}, {1, 1}));
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32, 248320, bs,  5120, {1, 1}, {1, 1}));
            // 4096x14336 already exists for all_types at bs 1..5 and 8; add only the two holes.
            if (bs == 6 || bs == 7) {
                test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32, 4096, bs, 14336, {1, 1}, {1, 1}));
            }
        }
    }

    // ArifiLabs lane-243 / R52b: the PREFILL width on the 27B FFN SHAPE, which perf mode did not
    // have. Perf mode DID already time S-X8 mul_mm: an EARLIER bs loop (not the block immediately
    // above) runs {1,2,3,4,5,8,512} over all_types, which contains GGML_TYPE_SX8, so 4096x14336 at
    // n=512 was already a timed mul_mm row. What was missing is THIS shape -- m = 17408, k = 5120,
    // the row the R52b pairing is quoted on. q8_0 is the paired control. The eval list carries the
    // same two rows as CORRECTNESS cases.
    for (ggml_type type_a : {GGML_TYPE_SX8, GGML_TYPE_Q8_0}) {
        test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32, 17408, 512, 5120, {1, 1}, {1, 1}));
    }

    // ArifiLabs lane-246 / R60: the 4B prefill FFN shapes across the SMALL-n band, as PERF rows.
    // These are the discriminator the lane was opened on. The chain139 cooled ABBA profile read
    // S-X8 mul_mm at m=9216 n=128 k=2560 as 15-27% slower than the q8_0 row -- but that pairing is
    // ACROSS TWO PROCESSES, and the byte-identical q8_0 attention rows are themselves 13-15%
    // slower in the S-X8 process, so the cross-run pairing cannot separate a kernel effect from a
    // run-level one. Timing both types at both shapes at n = 32..512 inside ONE binary and ONE
    // process removes that confound: any residual n-dependent gap here is the tile decode, and a
    // flat gap is not. The eval list carries the same rows as CORRECTNESS cases.
    for (int n : {32, 64, 128, 256, 512}) {
        for (ggml_type type_a : {GGML_TYPE_SX8, GGML_TYPE_Q8_0}) {
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32, 9216, n, 2560, {1, 1}, {1, 1}));
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32, 2560, n, 9216, {1, 1}, {1, 1}));
        }
    }

    // ArifiLabs lane-234 / R48: the real 27B Q5_K decode shapes as PERF rows, the target of the
    // R48 activation-hoist pairing. Q5_K carries 33.5% of the interactive Q4_K_XL 27B file by
    // bytes (5.41 GiB, 141 tensors) and 31% of the measured decode graph, at only 84% of this
    // box's best line - the fattest single lever in the graph.
    //
    // Q4_K rides along as the control, for the reason the lane-209 block above states: it is the
    // yardstick shader at 96-100% of best on the 780M, it shares mul_mat_vec_base.glsl and the
    // tmpsh reduction with Q5_K, and NOTHING in R48 touches its shader. A round in which Q4_K
    // moves is a round that measured the box, not the kernel, and must be thrown out rather than
    // interpreted. Q6_K rides along for the opposite reason: it is a protected win
    // (vulkan-q6k-matvec-direct-scales) that must not regress, and it never enters the MMVQ
    // framework at all (ggml_vk_should_use_mmvq returns false for it off Intel), so it also
    // witnesses that the R48 routing switch did not leak outside Q5_K.
    //
    // Filter: -p "type_a=q5_K,type_b=f32,m=17408" and the two sibling shapes. n=1..8 spans the
    // route split - MMVQ at n=1, f32 dequant at n=2..8 - so the two arms of the pairing are read
    // off different rows of the same table, not the same row.
    for (int bs : {1, 2, 3, 4, 5, 6, 7, 8}) {
        for (ggml_type type_a : {GGML_TYPE_Q5_K, GGML_TYPE_Q4_K, GGML_TYPE_Q6_K}) {
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32,  17408, bs,  5120, {1, 1}, {1, 1}));
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32,   5120, bs, 17408, {1, 1}, {1, 1}));
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32, 248320, bs,  5120, {1, 1}, {1, 1}));
        }
    }

    // ArifiLabs lane-248 / R57 step 0: the k-sweep FALSIFIER for the q4_K wide-width knee.
    //
    // R50 (lane-239) left the n=3 -> n=4 drop on q4_K MMVQ (~16% at 248320x5120) attributed to a
    // capacity crossing: the per-workgroup q8_1 B tile is ~5.4 KB per column at k=5120, so n=4
    // crosses a 16 KB L0/L1. CHECK-R50-FABLE finding 8 named the one measurement that kills that
    // premise without touching a shader -- the SAME shape at SMALL k. At k=1024 the B tile is
    // ~1.1 KB per column (4.4 KB at n=4), far under any per-CU cache on gfx1103, so a capacity
    // mechanism MUST move the knee. If the drop is still there at k=1024, capacity is dead and the
    // q4_K cheaper-dot lane proceeds as instruction cuts only.
    //
    // k=5120 at this m is already covered by the two blocks above; only 1024 and 2048 are new.
    // n=1..5 spans both sides of the knee AND gives the two low widths that show whether the knee
    // MOVES with k (a capacity threshold would move it; an issue ceiling would not).
    // These rows only reach the MMVQ shader with GGML_VK_FORCE_MMVQ=1 (ggml_vk_should_use_mmvq
    // returns true unconditionally at mmvq_mode == 1, ggml-vulkan.cpp:9531-9536); without the
    // force they measure the f32 dequant shader, which is the sweep's paired control arm.
    for (int64_t k : {1024, 2048}) {
        for (int bs : {1, 2, 3, 4, 5}) {
            test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q4_K, GGML_TYPE_F32, 248320, bs, k, {1, 1}, {1, 1}));
        }
    }
    // The step-0 table (r57-evidence/10-ksweep.txt) came back with the drop SHRINKING as k falls:
    // the n=3->4 step costs 2.68 us per MB of A at k=5120 and 0.88 at k=2048, while n=2->3 costs
    // 0.69 / 0.78 at the two k -- i.e. the knee is k-DEPENDENT, which is what a capacity threshold
    // looks like and what an issue ceiling does not. The decisive follow-up is whether the knee
    // MOVES to the width where the k=2048 B tile reaches the same size: q8_1 is ~1.125 bytes per
    // weight, so the per-column tile is 5.76 KB at k=5120 (n=4 -> 23 KB) and 2.30 KB at k=2048,
    // which reaches 23 KB only at n=10. n=6..8 extends the k=2048 row far enough to see it move.
    // (k=1024 is excluded on purpose: at K_PER_ITER*BLOCK_SIZE = 1024 each workgroup runs ONE
    // iteration, and the fixed-cost fit over k=2048/5120 predicts 2111 us against 4101 measured --
    // that cell is launch-bound and cannot resolve a loop-behaviour question.)
    for (int bs : {6, 7, 8}) {
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q4_K, GGML_TYPE_F32, 248320, bs, 2048, {1, 1}, {1, 1}));
    }
    // It DID move: largest step-down at n=7 for k=2048 against n=4 for k=5120
    // (r57-evidence/13-knee-table.txt). k=3072 is the third point, for two reasons. (a) A single
    // working-set threshold does not fit the two points exactly -- k=5120 is still healthy at
    // n=3 = 17.3 KB while k=2048 has already collapsed at n=7 = 15.8 KB -- so the threshold, if
    // there is one, needs locating rather than asserting. (b) n=7 is the width R46 blamed for a
    // "NUM_COLS == 7 tail defect in the mmvq framework"; a cliff that appears at n=7 for one k and
    // not another cannot be that defect, and a third k says so or refutes it.
    for (int bs : {1, 2, 3, 4, 5, 6, 7, 8}) {
        test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q4_K, GGML_TYPE_F32, 248320, bs, 3072, {1, 1}, {1, 1}));
    }

    // ArifiLabs lane-248 / R57 decider: the MARGINAL VERIFY ROW, both regimes, one binary.
    //
    // R63 §3a measures the served Q4_K_XL DFlash2 line's marginal verify row at 43.0 ms = 0.177
    // plain-token-equivalents, which caps the line at 10.9 t/s even with a drafter that is never
    // wrong; 15-20 t/s needs that row near ~15 ms. So the quantity a kernel lane must move is the
    // per-COLUMN cost, not the n=1 rate. This block gives it a table: the Q4_K_XL type mix
    // (q4_K / q5_K / q6_K / iq4_xs) on the three dominant decode shapes, at n = 1..16 -- across
    // the mat-vec / GEMM boundary (mul_mat_vec_max_cols = 8), so the marginal us per extra column
    // can be read in BOTH regimes and the crossing found. With GGML_ARIFI_MMV_MAX_COLS the low
    // widths can be re-timed on the GEMM path out of the same binary.
    //
    // n = 1..8 for q4_K/q5_K/q6_K on these shapes already exists in the blocks above; the rows
    // that were missing are every n >= 9 and iq4_xs outside 17408x5120.
    for (int bs : {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}) {
        for (ggml_type type_a : {GGML_TYPE_Q4_K, GGML_TYPE_Q5_K, GGML_TYPE_Q6_K, GGML_TYPE_IQ4_XS}) {
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32,  17408, bs,  5120, {1, 1}, {1, 1}));
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32,   5120, bs, 17408, {1, 1}, {1, 1}));
            test_cases.emplace_back(new test_mul_mat(type_a, GGML_TYPE_F32, 248320, bs,  5120, {1, 1}, {1, 1}));
        }
    }

    // ArifiLabs lane-253 / R65: the MMVQ width-knee DISCRIMINATOR rows.
    //
    // The q4_K mat-vec knees at NUM_COLS 4 on the 248320x5120 row (79.5/77.1/73.1/61.4 GB/s-of-A at
    // n=1..4, R61 30-perf-table.txt, 6 rounds; 16.4% reproduced by R57). Two models fit that single
    // column equally well and are indistinguishable at k=5120:
    //
    //   H-CAP   - the per-workgroup B working set, NUM_COLS * k * 1.125 B of q8_1, crosses a fixed
    //             per-CU capacity C. The knee at n=4 and not n=3 brackets C in [17.28, 23.04) KB.
    //   H-ISSUE - a k-independent ceiling at four columns.
    //
    // They separate under k, and ONLY upward. H-CAP puts the knee at floor(C/(k*1.125))+1, which is
    // n=2 uniquely across the whole bracket at k=10240 and k=12288; H-ISSUE keeps it at n=4 at every
    // k. R57 swept k DOWNWARD (1024/2048/3072) where H-CAP predicts only a null, and every one of
    // those k gives num_iters < 4 so the mul_mat_vecq.comp:146 unroll body is never entered - the
    // confound R57's own n=7 cliff lives in. BLOCK_SIZE is 64 on this device (the DMMV_WG_SIZE_LARGE
    // branch is NVIDIA/Intel-only), so col_stride is 1024 and every k here gives num_iters >= 4.
    //
    // m = 17408 keeps the sweep affordable enough to interleave rounds; the k=5120 column is the
    // anchor that ties it back to the published 248320x5120 table. n is capped at 5: n=6..8 change
    // NUM_ROWS 1 -> 4 (rm_int_n) and n=7 carries R57's separate width-locked cliff, and neither is
    // allowed near the discriminator.
    //
    // Filter: -p "type_a=q4_K,type_b=f32,m=17408".
    // m = 248320 is NOT a convenience here, it is the only m that measures anything: R65's first
    // 0b round ran the same six k at m=17408 and came back at 34-36 GB/s-of-A flat, against 79.5 at
    // n=1 on the embd/out shape. At 50 MB of A the dispatch is launch-bound, the bus is never the
    // limiter, and the width-4 knee is simply not present to be moved - the k=5120 column showed
    // 6.3% where the published table shows 16.4%. Receipt: r65-evidence/22-ksweep-table.txt.
    // The 17408 rows are kept beside it as the negative control that says so.
    for (int k : { 4096, 5120, 6144, 8192, 10240, 12288 }) {
        for (int bs : {1, 2, 3, 4, 5}) {
            test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q4_K, GGML_TYPE_F32,  17408, bs, k, {1, 1}, {1, 1}));
            test_cases.emplace_back(new test_mul_mat(GGML_TYPE_Q4_K, GGML_TYPE_F32, 248320, bs, k, {1, 1}, {1, 1}));
        }
    }

    // The FIXED-n row-regime cell needs no rows of its own: the lane-234 block above already emits
    // q4_K at 17408x5120 and 248320x5120 for n=1..8, and GGML_ARIFI_MMVQ_WIDE_ROWS_FROM moves
    // NUM_ROWS under those same rows. Filter it with
    //   -p "type_a=q4_K,type_b=f32,m=(17408|248320),n=[45],k=5120"
    // (-p is a std::regex over vars(), so alternation works).

    // qwen3-30b-a3b
    for (int bs : {1, 4, 8, 32, 64, 128, 256, 512}) {
        for (ggml_type type_a : {GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_Q4_0, GGML_TYPE_Q8_0, GGML_TYPE_Q4_K, GGML_TYPE_Q6_K, GGML_TYPE_IQ2_XS, GGML_TYPE_IQ4_XS}) {
            for (ggml_type type_b : {GGML_TYPE_F32}) {
                test_cases.emplace_back(new test_mul_mat_id(type_a, type_b, 128, 8, false, 768, bs, 2048));
                test_cases.emplace_back(new test_mul_mat_id_fusion(type_a, type_b, 128, 8, false, 768, bs, 2048, 1));
            }
        }
    }

    for (int bs : {1, 4, 8, 32, 64, 128, 256, 512}) {
        for (ggml_type type_a : {GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_Q4_0, GGML_TYPE_Q8_0, GGML_TYPE_Q4_K, GGML_TYPE_Q6_K, GGML_TYPE_IQ2_XS, GGML_TYPE_IQ4_XS}) {
            for (ggml_type type_b : {GGML_TYPE_F32}) {
                test_cases.emplace_back(new test_mul_mat_id(type_a, type_b, 32, 4, false, 1792, bs, 2048));
                test_cases.emplace_back(new test_mul_mat_id_fusion(type_a, type_b, 32, 4, false, 1792, bs, 2048, 1));
            }
        }
    }

    // arifi R18 / R15-C3: Ornith-1.5 35B-A3B expert shape (qwen35moe, 256 experts, 8 used, n_embd 2048,
    // n_ff_exp 512): gate/up = 512 rows x k 2048, down = 2048 rows x k 512. Q4_K is the file's expert
    // type (down is Q4_K on 20 layers, Q6_K on 21). bs 1 = decode, 3 = DFlash2 n-max-2 verify, <= 8 is
    // the mul_mat_vec_id width gate. `-p "n_mats=256"` narrows perf to these rows.
    for (int bs : {1, 2, 3, 4, 8}) {
        for (ggml_type type_a : {GGML_TYPE_Q4_K, GGML_TYPE_Q6_K}) {
            test_cases.emplace_back(new test_mul_mat_id(type_a, GGML_TYPE_F32, 256, 8, false,  512, bs, 2048));
            test_cases.emplace_back(new test_mul_mat_id(type_a, GGML_TYPE_F32, 256, 8, false, 2048, bs,  512));
        }
    }


    // gpt-oss-20b
    for (int bs : {1, 4, 8, 512}) {
        for (ggml_type type_a : {GGML_TYPE_MXFP4}) {
            for (ggml_type type_b : {GGML_TYPE_F32}) {
                test_cases.emplace_back(new test_mul_mat_id(type_a, type_b, 32, 4, false, 2880, bs, 2880));
                test_cases.emplace_back(new test_mul_mat_id_fusion(type_a, type_b, 32, 4, false, 2880, bs, 2880, 1));
            }
        }
    }

    for (int K : {3, 5}) {
        for (int IC : {256, 2560}) {
            for (int IW_IH : {32, 64, 256}) {
                if (IC == 2560 && IW_IH == 256) {
                    // too big
                    continue;
                }
                test_cases.emplace_back(new test_im2col(GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_F32, {IW_IH, IW_IH, IC, 1}, {K, K, IC, 1}, 1, 1, 1, 1, 1, 1, true));
            }
        }
    }

    // Qwen3-VL-8B https://github.com/ggml-org/llama.cpp/issues/17012
    test_cases.emplace_back(new test_flash_attn_ext(72, 72, 16, {1, 1}, 5776, 5776, false, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));

    // Sparse flash attention (n_kv_max hint) decode across KV depths.
    // Shapes: 576/512 DeepSeek MLA, 512/512 DeepSeek-V4/GLM-5.2, 256/256 gqa12 Qwen QSA.
    for (int64_t kv : {4096, 16384, 32768}) {
        test_cases.emplace_back(new test_flash_attn_ext(512, 512, 1, { 8, 1}, kv, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false,  512));
        test_cases.emplace_back(new test_flash_attn_ext(576, 512, 1, {16, 1}, kv, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, true,   512));
        test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {12, 1}, kv, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false, 2048));
    }

    test_cases.emplace_back(new test_flash_attn_ext(64, 64, 8, {8, 1}, 7680, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(64, 64, 8, {8, 1}, 7680, 4, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(64, 64, 8, {8, 1}, 7680,   1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q4_0, GGML_TYPE_Q4_0));
    test_cases.emplace_back(new test_flash_attn_ext(64, 64, 8, {8, 1}, 7680, 512, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q4_0, GGML_TYPE_Q4_0));
    test_cases.emplace_back(new test_flash_attn_ext(64, 64, 8, {8, 1}, 7680,   1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0));
    test_cases.emplace_back(new test_flash_attn_ext(64, 64, 8, {8, 1}, 7680, 512, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0));

    // sparse decode at long context
    test_cases.emplace_back(new test_flash_attn_ext(512, 512, 1, { 8, 1}, 49152, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false,    0));
    test_cases.emplace_back(new test_flash_attn_ext(512, 512, 1, { 8, 1}, 49152, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false, 2048));
    // gemma-4-26b-a4b global-attn layers: head_count_kv=2, 16 query heads (gqa_ratio=8)
    test_cases.emplace_back(new test_flash_attn_ext(512, 512, 2, { 8, 1}, 49152, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false,    0));
    test_cases.emplace_back(new test_flash_attn_ext(512, 512, 2, { 8, 1}, 49152, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, false, 2048));
    test_cases.emplace_back(new test_flash_attn_ext(576, 512, 1, {16, 1}, 49152, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, true,     0));
    test_cases.emplace_back(new test_flash_attn_ext(576, 512, 1, {16, 1}, 49152, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, true,  2048));

    // q8_0 KV cases with long context (decode and prompt)
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 1},   128, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 1},   512, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 1},  1024, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 1},  2048, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 1},  4096, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 1}, 10000, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 1}, 20000, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 1}, 10000, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 1}, 20000, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 1}, 10000, 512, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 1}, 20000, 512, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_Q8_0, GGML_TYPE_Q8_0));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 1}, 10000, 512, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 2, {16, 1}, 20000, 512, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));

    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 4, {6, 1}, 4096, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 4, {6, 1}, 4096, 512, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 4, {6, 1}, 16384, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 4, {6, 1}, 16384, 512, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 4, {6, 1}, 65536, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 4, {6, 1}, 65536, 512, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 4, {6, 1}, 131072, 1, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));
    test_cases.emplace_back(new test_flash_attn_ext(256, 256, 4, {6, 1}, 131072, 512, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16));

    for (int kv : { 4096, 8192, 16384,32768, 65536, }) {
        for (int hs : { 64, 128, 256, 576, }) {
            const int  hsv    = hs == 576 ? 512 : hs;
            const bool v_view = hs == 576;
            for (int nr : { 1, 4, 8, }) {
                for (int nb : { 1, 4096, }) {
                    test_cases.emplace_back(new test_flash_attn_ext(hs, hsv, 8, {nr, 1}, kv, nb, true, false, 0, 0, GGML_PREC_F32, GGML_TYPE_F16, GGML_TYPE_F16, {0, 1, 2, 3}, true, v_view));
                }
            }
        }
    }

    for (int col : {8192, 16384, 32768, 65536, 131072, 262144, 524288}) {
        for (int rows : {1, 4, 16}){
            test_cases.emplace_back(new test_soft_max(GGML_TYPE_F32, {col, rows, 1, 1}, false,  false,  GGML_TYPE_F32, {1, 1}, 1.0f, 0.0f));
        }
    }

    test_cases.emplace_back(new test_conv_2d_dw({512, 512, 256, 1}, {3, 3, 1, 256}, GGML_TYPE_F32, 1, 1, 1, false));
    test_cases.emplace_back(new test_conv_2d_dw({512, 512, 256, 1}, {3, 3, 1, 256}, GGML_TYPE_F32, 1, 1, 1, true));
    test_cases.emplace_back(new test_conv_2d_dw({112, 112, 32,  1}, {3, 3, 1, 32},  GGML_TYPE_F32, 1, 1, 1, false));
    test_cases.emplace_back(new test_conv_2d_dw({112, 112, 32,  1}, {3, 3, 1, 32},  GGML_TYPE_F32, 1, 1, 1, true));
    test_cases.emplace_back(new test_conv_2d_dw({56,  56,  128, 1}, {5, 5, 1, 128}, GGML_TYPE_F32, 2, 2, 1, false));
    test_cases.emplace_back(new test_conv_2d_dw({56,  56,  128, 1}, {5, 5, 1, 128}, GGML_TYPE_F32, 2, 2, 1, true));

    for (ggml_type kernel_type : {GGML_TYPE_F32, GGML_TYPE_F16}) {
        test_cases.emplace_back(new test_conv_transpose_2d({256, 256, 256, 1}, {3, 3, 16, 256}, 1, kernel_type));
        test_cases.emplace_back(new test_conv_transpose_2d({16, 16, 16, 1}, {3, 3, 8, 16}, 1, kernel_type));
        test_cases.emplace_back(new test_conv_transpose_2d({10, 10, 9, 1}, {3, 3, 1, 9}, 2, kernel_type));
    }

    // Memory bound overlap-add of the GEMM + col2im_1d transposed conv path, real vocoder stage shapes
    test_cases.emplace_back(new test_col2im_1d(GGML_TYPE_F32, 16, 512, 2048, 8, 0));
    test_cases.emplace_back(new test_col2im_1d(GGML_TYPE_F32, 4, 128, 65536, 2, 0));
    test_cases.emplace_back(new test_col2im_1d(GGML_TYPE_F16, 16, 512, 2048, 8, 0));

    test_cases.emplace_back(new test_mean(GGML_TYPE_F32, {256, 256, 3, 1}));


    for (int n_token : {1, 512}) {
        test_cases.emplace_back(new test_add_id(GGML_TYPE_F32, GGML_TYPE_F32, 2880, 128, 4, n_token));
        test_cases.emplace_back(new test_add_id(GGML_TYPE_F32, GGML_TYPE_F32, 2880, 32, 4, n_token));
    }

    for (bool fw : {true, false}) { // fw == forward
        for (ggml_type type : {GGML_TYPE_F32, GGML_TYPE_F16}) {
            for (bool ff : {false, true}) { // freq_factors
                for (float v : { 0, 1 }) {
                    test_cases.emplace_back(new test_rope(type, {128,  32, 512, 1}, 128, GGML_ROPE_TYPE_NORMAL, 512, 1.0f, 0.0f, 1.0f, ff, v, fw)); // llama 7B
                    test_cases.emplace_back(new test_rope(type, {128,  64, 512, 1}, 128, GGML_ROPE_TYPE_NORMAL, 512, 1.0f, 0.0f, 1.0f, ff, v, fw)); // llama 65B
                    test_cases.emplace_back(new test_rope(type, { 80,  32, 512, 1},  20, GGML_ROPE_TYPE_NEOX, 512, 1.0f, 0.0f, 1.0f, ff, v, fw)); // neox (stablelm)
                    test_cases.emplace_back(new test_rope(type, { 64,   8, 512, 1},  64, GGML_ROPE_TYPE_NEOX, 512, 1.0f, 0.0f, 1.0f, ff, v, fw)); // neox (falcon 40B)
                    test_cases.emplace_back(new test_rope(type, {128,  12, 512, 1}, 128, GGML_ROPE_TYPE_MROPE,  512, 1.0f, 0.0f, 1.0f, ff, v, fw)); // rope_multi,m-rope (qwen2vl 2B)
                    test_cases.emplace_back(new test_rope(type, {128,  12, 512, 1}, 128, GGML_ROPE_TYPE_IMROPE,  512, 1.0f, 0.0f, 1.0f, ff, v, fw)); // rope_multi,imrope (qwen3vl 2B)
                    test_cases.emplace_back(new test_rope(type, { 80,  16, 2, 1},  80, GGML_ROPE_TYPE_VISION, 512, 1.0f, 0.0f, 1.0f, ff, v, fw)); // rope_multi,m-rope (qwen2vl ViT)
                }
            }
        }
    }

    // Real-model RoPE: F32 forward, packed Q, 512-token prefill.
    test_cases.emplace_back(new test_rope(GGML_TYPE_F32, {256,  8, 512, 1},  64, GGML_ROPE_TYPE_IMROPE, 512, 1.0f, 0.0f, 1.0f, false, 0, true)); // qwen3.5 0.8B
    test_cases.emplace_back(new test_rope(GGML_TYPE_F32, {256, 16, 512, 1},  64, GGML_ROPE_TYPE_IMROPE, 512, 1.0f, 0.0f, 1.0f, false, 0, true)); // qwen3.5 4B
    test_cases.emplace_back(new test_rope(GGML_TYPE_F32, {256,  8, 512, 1}, 256, GGML_ROPE_TYPE_NEOX,   512, 1.0f, 0.0f, 1.0f, false, 0, true)); // gemma4 E2B sliding
    test_cases.emplace_back(new test_rope(GGML_TYPE_F32, {512,  8, 512, 1}, 128, GGML_ROPE_TYPE_NEOX,   512, 1.0f, 0.0f, 1.0f, true,  0, true)); // gemma4 E4B global

    std::vector<std::array<int64_t, 4>> reduce_rows_cases = {
        { 8192, 1,    1, 1 },
        { 8192, 8192, 1, 1 },
        { 128,  8192, 1, 1 },
    };

    for (auto it: reduce_rows_cases){
        test_cases.emplace_back(new test_mean(GGML_TYPE_F32, it));
        test_cases.emplace_back(new test_sum_rows(GGML_TYPE_F32, it));
        test_cases.emplace_back(new test_sum(GGML_TYPE_F32, it));
    }

    test_cases.emplace_back(new test_argsort(GGML_TYPE_F32, {65000,  16, 1, 1}));
    test_cases.emplace_back(new test_argsort(GGML_TYPE_F32, {200000, 1,  1, 1}));
    test_cases.emplace_back(new test_argsort(GGML_TYPE_F32, {200000, 16, 1, 1}));

    test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {2, 1, 1, 1}, 1));
    // widths around the tiling threshold
    for (auto cols : {4096, 8192, 12288, 16384, 24576, 32768, 65536, 131072}) {
        for (auto nrows : {1, 16}) {
            test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {cols, nrows, 1, 1}, 16));
        }
    }
    for (auto k : {1, 4, 8, 10, 16, 32, 40, 400}) {
        for (auto nrows : {1, 16}) {
            for (auto cols : {k, 1000, 65000, 200000}) {
                test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {cols, nrows, 1, 1}, k));
            }
        }
    }

    // qwen4exp sparse-attention indexer: nrows = n_tokens/n_stream, so tg gives nrows==1.
    // Sweep nrows to expose how much of the device a single row leaves idle.
    for (auto cols : {8192, 32768, 131072}) {
        for (auto nrows : {1, 2, 4, 8, 16, 32}) {
            test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {cols, nrows, 1, 1}, 2048));
        }
    }
    // backend sampler: one row of the vocab (llama-sampler.cpp top_k)
    for (auto k : {20, 40}) {
        test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {151936, 1, 1, 1}, k));
    }

    // short rows, many of them: MoE routing and group selection. The opposite corner from
    // the indexer, and the one where a work-group per row is the wasteful choice.
    for (auto cols : {2, 16, 128, 1024}) {
        for (auto nrows : {1024, 8192}) {
            for (auto k : {1, 2, 8, 16, 32}) {
                if (k <= cols) {
                    test_cases.emplace_back(new test_top_k(GGML_TYPE_F32, {cols, nrows, 1, 1}, k));
                }
            }
        }
    }

    for (auto nrows : {1, 4, 8, 16}) {
        for (auto cols : {128, 1024, 4096, 8192, 16384, 32768, 65536, 131072, 200000, 2000000}) {
            test_cases.emplace_back(new test_cumsum(GGML_TYPE_F32, {cols, nrows, 1, 1}));
        }
    }

    // Examples from granite-4.0-h-1b/ggml-model-Q8_0.gguf
    test_cases.emplace_back(new test_ssm_conv(GGML_TYPE_F32, {515, 3328, 1, 1}, {4, 3328, 1, 1})); // prefill
    test_cases.emplace_back(new test_ssm_conv(GGML_TYPE_F32, {937, 8192, 1, 1}, {4, 8192, 1, 1})); // prefill
    test_cases.emplace_back(new test_ssm_conv(GGML_TYPE_F32, {4,   3328, 1, 1}, {4, 3328, 1, 1})); // generate
    test_cases.emplace_back(new test_ssm_conv_bias_silu(GGML_TYPE_F32, {515, 3328, 1, 1}, {4, 3328, 1, 1}, true));  // prefill
    test_cases.emplace_back(new test_ssm_conv_bias_silu(GGML_TYPE_F32, {4,   3328, 1, 1}, {4, 3328, 1, 1}, true));  // generate
    test_cases.emplace_back(new test_ssm_scan(GGML_TYPE_F32, 128, 64, 48, 1, 512, 1)); // prefill
    test_cases.emplace_back(new test_ssm_scan(GGML_TYPE_F32, 128, 64, 48, 1, 1,   1)); // generate
    test_cases.emplace_back(new test_ssm_scan(GGML_TYPE_F32, 128, 80, 128, 1, 512, 1)); // Nemotron-9B prefill
    test_cases.emplace_back(new test_ssm_scan(GGML_TYPE_F32, 128, 80, 128, 1, 1,   1)); // Nemotron-9B generate

    // acc
    test_cases.emplace_back(new test_acc(GGML_TYPE_F32, {256, 17, 1, 1}, {256, 16, 1, 1}, -1));
    test_cases.emplace_back(new test_acc(GGML_TYPE_F32, {256, 17, 2, 3}, {256, 16, 2, 3}, -1));
    test_cases.emplace_back(new test_acc(GGML_TYPE_F32, {256, 17, 2, 3}, {128, 16, 2, 3}, -1));
    test_cases.emplace_back(new test_acc(GGML_TYPE_F32, {256, 17, 2, 3}, {256, 16, 2, 3}, 1));
    test_cases.emplace_back(new test_acc(GGML_TYPE_F32, {256, 17, 2, 3}, {128, 16, 2, 3}, 2));
    test_cases.emplace_back(new test_acc(GGML_TYPE_F32, {256, 17, 2, 3}, {64, 16, 2, 3}, 3));

    // GATED_DELTA_NET: realistic model configurations
    // TG: n_seq_tokens=1 (autoregressive)
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 32, 128, 1, 1));   // Qwen3.5-like: 32 heads, d=128
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 16, 64,  1, 1));   // smaller model
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 32, 128, 1, 1, 1, false, true)); // KDA
    // PP: n_seq_tokens=64,256 (prompt processing)
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 32, 128, 64, 1));  // PP-64
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 32, 128, 256, 1)); // PP-256
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 32, 128, 512, 1)); // PP-512
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 32, 128, 1024, 1)); // PP-1024
    // Small model configs (fewer heads = less GPU occupancy for autoregressive)
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 128, 64, 1));   // 4h PP-64
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 128, 256, 1));  // 4h PP-256
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 128, 512, 1));  // 4h PP-512
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 4, 128, 1024, 1)); // 4h PP-1024
    test_cases.emplace_back(new test_gated_delta_net(GGML_TYPE_F32, 32, 128, 64, 1, 1, false, true)); // KDA PP-64

    // lightning_indexer
    for (int kv : { 256, 4096, 65536 }) {
        for (int bs : { 1, 512, 2048 }) {
            for (int nh : { 32, 64 }) {
                for (int ns : { 1, 4 }) {
                    for (ggml_type type_K : {GGML_TYPE_F32, GGML_TYPE_F16, GGML_TYPE_BF16, GGML_TYPE_Q8_0, GGML_TYPE_Q5_1, GGML_TYPE_Q5_0, GGML_TYPE_Q4_1, GGML_TYPE_Q4_0, GGML_TYPE_IQ4_NL}) {
                        test_cases.emplace_back(new test_lightning_indexer(128, nh, kv, bs, ns, ns, type_K));
                    }
                }
            }
        }
    }

    // launch-overhead isolation: single L2_NORM launch vs batched siblings at the GDN
    // production shape (strided qkv views) -- perf-mode only, the eval list has its own
    // 2/4-wide coverage
    for (int n : { 128, 256 }) {
        test_cases.emplace_back(new test_l2_norm(GGML_TYPE_F32, { n, 16, 16, 1 }, 1e-12f, false, false));
        test_cases.emplace_back(new test_l2_norm_batch(GGML_TYPE_F32, { n, 16, 16, 1 }, 2, 1e-12f, true));
        test_cases.emplace_back(new test_l2_norm_batch(GGML_TYPE_F32, { n, 16, 16, 1 }, 4, 1e-12f, true));
    }


    return test_cases;
}

static std::vector<std::unique_ptr<test_case>> make_test_cases_from_file(const char * path) {
    std::ifstream f(path);

    if (!f.is_open()) {
        throw std::runtime_error("Unable to read test file");
    }

    std::vector<std::unique_ptr<test_case>> test_cases;

    std::string line;

    while (std::getline(f, line)) {
        std::istringstream iss(line);

        ggml_op op;
        ggml_type type;
        std::array<int64_t, 4> ne;
        std::array<int32_t, GGML_MAX_OP_PARAMS / sizeof(int32_t)> op_params = {};
        std::string name;
        uint64_t tmp;

        iss >> tmp;
        op = (ggml_op)tmp;
        iss >> tmp;
        type = (ggml_type)tmp;

        for (size_t i = 0; i < 4; i++) {
            iss >> ne[i];
        }

        iss >> tmp;
        for (size_t i = 0; i < tmp && i < op_params.size(); i++) {
            iss >> op_params[i];
        }

        iss >> tmp;

        size_t num_src = std::min((uint64_t)GGML_MAX_SRC, tmp);
        std::vector<input_tensor> sources(num_src);
        for (size_t i = 0; i < num_src; i++) {
            input_tensor& src = sources[i];
            iss >> tmp;
            src.type = (ggml_type)tmp;

            for (size_t i = 0; i < 4; i++) {
                iss >> src.ne[i];
            }
            for (size_t i = 0; i < 4; i++) {
                iss >> src.nb[i];
            }
        }

        iss >> name;

        if (name.length() == 1 && name[0] == '-') {
            name = "";
        }

        test_cases.emplace_back(new test_generic_op(op, type, ne, op_params, sources, std::move(name)));
    }

    return test_cases;
}

// ---- FA vec (Q,NE): forced-config numerical slice (Metal only) ----
using set_fa_vec_override_t   = void (*)(int, int);
using clear_fa_vec_override_t = void (*)(void);

// NL = 32/NE must divide both dk/4 and dv/4.
static std::vector<int> fa_vec_legal_ne(int dk, int dv) {
    std::vector<int> r;
    for (int ne : {1, 2, 4}) {
        const int nl = 32 / ne;
        if ((dk/4) % nl == 0 && (dv/4) % nl == 0) {
            r.push_back(ne);
        }
    }
    return r;
}

static bool op_names_filter_selects(const char * op_names_filter, const char * op_name) {
    if (op_names_filter == nullptr) {
        return true;
    }
    for (const auto & entry : op_filter_entries(op_names_filter)) {
        // a full test case string is matched by its op name prefix
        const auto lparen_pos = entry.find_first_of('(');
        const auto op_entry = lparen_pos != std::string_view::npos ? entry.substr(0, lparen_pos) : entry;
        if (op_filter_entry_matches(op_entry, op_name)) {
            return true;
        }
    }
    return false;
}

// Covers padded rows, sinks, kvpad, multi-SIMDgroup reduction, quantized K/V, and MLA views.
// The override is backend-global, so this runs after all parallel workers have joined.
// ============================================================================
// ArifiLabs lane-230 / R46b B2: S-X8 mat-vec against an INDEPENDENT reference.
//
// The default `test` mode compares a backend against the CPU BACKEND. For S-X8
// that reference is itself lossy: the CPU traits give S-X8 vec_dot_type Q8_1
// (ggml-cpu.c ~711), so the CPU arm quantizes the ACTIVATIONS to q8_1 before the
// dot, while the Vulkan mat-vec dequantizes the weights and multiplies in f32.
// Two consequences, both of which blocked B2:
//
//   * the CPU refuses S-X8 x f16 entirely (src1 must be f32 or the vec_dot type),
//     so the twelve f16-activation descriptors print "not supported [CPU]" and
//     cover nothing;
//   * the 5e-4 NMSE bound in `test` mode is dominated by the CPU's own q8_1
//     activation error, not by the kernel under test. At m=1,n=1 the NMSE has a
//     SINGLE output element to normalize over, so that reference error is not
//     averaged down and the case sits on the bound - which is why those ten
//     descriptors were dropped rather than explained.
//
// This mode replaces the reference instead of relaxing the bound. Weights are
// decoded by dequantize_row_sx8() (ggml's own S-X8 decoder, which is what the
// shader header names as its reference) and the dot product is accumulated in
// DOUBLE against exact activations. Neither arm of that reference is the kernel
// under test. Inputs are fixed and thread-count independent, so a case that
// fails here fails on the checker's machine too.
//
// It is a SEPARATE mode on purpose: `test`'s descriptor set stays byte-identical,
// so the candidate-versus-baseline sweep comparison is unaffected by this work.
// ============================================================================

struct sx8_ref_case {
    ggml_type type_b;
    int64_t   m;
    int64_t   n;
    int64_t   k;
    const char * group;
};

// Deterministic by (row, i) only: no random engine, no thread slicing, so the
// same bytes are generated on every machine and every run. init_tensor_uniform
// cannot be used here - it seeds from std::random_device and splits the range
// over hardware_concurrency() threads.
static float sx8_ref_value(int64_t row, int64_t i) {
    uint64_t h = (uint64_t)(row + 1) * 0x9E3779B97F4A7C15ull
               + (uint64_t)(i   + 1) * 0xBF58476D1CE4E5B9ull;
    h ^= h >> 29;
    h *= 0x94D049BB133111EBull;
    h ^= h >> 32;
    // uniform in [-1, 1), the same range init_tensor_uniform uses
    return (float)((double)(h >> 11) * (1.0 / 9007199254740992.0) * 2.0 - 1.0);
}

static void sx8_ref_fill_f32(std::vector<float> & dst, int64_t nrows, int64_t ncols, int64_t row_base) {
    dst.resize((size_t)(nrows * ncols));
    for (int64_t r = 0; r < nrows; r++) {
        for (int64_t i = 0; i < ncols; i++) {
            dst[(size_t)(r * ncols + i)] = sx8_ref_value(row_base + r, i);
        }
    }
}

// out = A(m,k) * B(n,k)^T in double, from the QUANTIZED weight bytes actually
// uploaded to the device and the exact activations. `ref_abs` is the sum of the
// TERM MAGNITUDES of the same dot product: ref_abs >> |ref| is exactly the
// cancellation condition, and it is what the near-zero split keys on. It cannot
// be recovered from the output tensor afterwards, which is why it is produced
// here rather than derived from `ref`.
static void sx8_ref_compute(const sx8_ref_case & c,
                            const std::vector<uint8_t> & a_q,
                            const std::vector<float>   & b_f32,
                            std::vector<double>        & ref,
                            std::vector<double>        & ref_abs) {
    const ggml_type_traits * tr = ggml_get_type_traits(GGML_TYPE_SX8);
    GGML_ASSERT(tr->to_float != nullptr);
    const size_t row_sz = ggml_row_size(GGML_TYPE_SX8, c.k);

    ref.assign((size_t)(c.m * c.n), 0.0);
    ref_abs.assign((size_t)(c.m * c.n), 0.0);

    auto worker = [&](int64_t r0, int64_t r1) {
        std::vector<float> w((size_t) c.k);
        for (int64_t i = r0; i < r1; i++) {
            tr->to_float(a_q.data() + (size_t) i * row_sz, w.data(), c.k);
            for (int64_t j = 0; j < c.n; j++) {
                const float * bv = b_f32.data() + (size_t)(j * c.k);
                double s  = 0.0;
                double sa = 0.0;
                for (int64_t t = 0; t < c.k; t++) {
                    const double term = (double) w[t] * (double) bv[t];
                    s  += term;
                    sa += std::fabs(term);
                }
                ref    [(size_t)(j * c.m + i)] = s;
                ref_abs[(size_t)(j * c.m + i)] = sa;
            }
        }
    };

    // Row ranges are independent, so threading changes nothing about the result.
    const int64_t nth = std::min<int64_t>(std::max<int64_t>(1, (int64_t) N_THREADS), c.m);
    if (nth <= 1) {
        worker(0, c.m);
        return;
    }
    std::vector<std::future<void>> tasks;
    for (int64_t t = 0; t < nth; t++) {
        tasks.push_back(std::async(std::launch::async, worker, t*c.m/nth, (t+1)*c.m/nth));
    }
    for (auto & t : tasks) {
        t.get();
    }
}

// Runs one case's graph on `backend` from host-side inputs. Returns false only
// if the backend declines the op or allocation/compute fails.
static bool sx8_ref_run(ggml_backend_t backend, const sx8_ref_case & c,
                        const std::vector<uint8_t> & a_q,
                        const std::vector<uint8_t> & b_raw,
                        std::vector<float> & out_f,
                        std::string & why) {
    ggml_init_params params = {
        /* .mem_size   = */ ggml_tensor_overhead()*8 + ggml_graph_overhead(),
        /* .mem_base   = */ NULL,
        /* .no_alloc   = */ true,
    };
    ggml_context_ptr ctx(ggml_init(params));
    GGML_ASSERT(ctx);

    ggml_tensor * a   = ggml_new_tensor_2d(ctx.get(), GGML_TYPE_SX8, c.k, c.m);
    ggml_tensor * b   = ggml_new_tensor_2d(ctx.get(), c.type_b,      c.k, c.n);
    ggml_tensor * out = ggml_mul_mat(ctx.get(), a, b);
    ggml_set_name(a, "a");
    ggml_set_name(b, "b");
    ggml_set_name(out, "out");

    if (!ggml_backend_supports_op(backend, out)) {
        why = "not supported";
        return false;
    }

    ggml_backend_buffer_ptr buf(ggml_backend_alloc_ctx_tensors(ctx.get(), backend));
    if (buf == NULL) {
        why = "alloc failed";
        return false;
    }

    GGML_ASSERT(a_q.size()   == ggml_nbytes(a));
    GGML_ASSERT(b_raw.size() == ggml_nbytes(b));
    ggml_backend_tensor_set(a, a_q.data(),   0, a_q.size());
    ggml_backend_tensor_set(b, b_raw.data(), 0, b_raw.size());

    ggml_cgraph * gf = ggml_new_graph(ctx.get());
    ggml_build_forward_expand(gf, out);
    if (ggml_backend_graph_compute(backend, gf) != GGML_STATUS_SUCCESS) {
        why = "compute failed";
        return false;
    }

    out_f.resize((size_t) ggml_nelements(out));
    ggml_backend_tensor_get(out, out_f.data(), 0, ggml_nbytes(out));
    return true;
}

struct sx8_ref_metrics {
    double nmse     = 0.0;   // the sweep's metric: mse(got, ref) / mse(got, 0)
    double max_abs  = 0.0;
    double max_rel  = 0.0;   // over elements that are NOT near zero (see below)
    double rms_ref  = 0.0;
    size_t n_nearzero    = 0;
    double nearzero_abs  = 0.0;
    bool   nonfinite     = false;
};

// Near-zero elements are reported SEPARATELY rather than folded into max_rel: a
// cancelling dot product can land arbitrarily close to zero, and its relative
// error then says nothing about the arithmetic.
//
// The split is PER ELEMENT against that element's own term-magnitude sum:
// |ref[i]| < 1e-3 * ref_abs[i], i.e. the dot product lost three or more digits
// to cancellation. A split against the output RMS would be inert on exactly the
// cases that need it - at m=1,n=1 there is one output, so |ref| < 1e-3*rms(ref)
// is |ref| < 1e-3*|ref|, never true.
static sx8_ref_metrics sx8_ref_measure(const std::vector<float>  & got,
                                       const std::vector<double> & ref,
                                       const std::vector<double> & ref_abs) {
    sx8_ref_metrics mt;
    const size_t n = ref.size();
    GGML_ASSERT(got.size() == n);
    GGML_ASSERT(ref_abs.size() == n);

    double sum_sq = 0.0;
    for (size_t i = 0; i < n; i++) {
        sum_sq += ref[i]*ref[i];
    }
    mt.rms_ref = std::sqrt(sum_sq / (double) n);

    double mse_g_r = 0.0;
    double mse_g_0 = 0.0;
    for (size_t i = 0; i < n; i++) {
        const double g = (double) got[i];
        const double r = ref[i];
        if (!std::isfinite(g)) {
            mt.nonfinite = true;
        }
        const double d = std::fabs(g - r);
        mse_g_r += (g - r)*(g - r);
        mse_g_0 += g*g;
        mt.max_abs = std::max(mt.max_abs, d);
        if (std::fabs(r) < 1e-3 * ref_abs[i]) {
            mt.n_nearzero++;
            mt.nearzero_abs = std::max(mt.nearzero_abs, d);
        } else if (r != 0.0) {
            mt.max_rel = std::max(mt.max_rel, d / std::fabs(r));
        }
    }
    mt.nmse = mse_g_0 > 0.0 ? mse_g_r / mse_g_0 : (mse_g_r > 0.0 ? INFINITY : 0.0);
    return mt;
}

// The route a case ACTUALLY takes, and the bound that follows from it.
//
// Requesting q8_1 is not the same as executing it. In the Vulkan backend the
// activations are quantized only when
//   ggml-vulkan.cpp:11087  quantize_y = integer_dot_product && src1->type == GGML_TYPE_F32
//                                       && contiguous && (ne11*ne10) % 4 == 0
//                                       && ggml_vk_should_use_mmvq(...)
// so an f16 ACTIVATION never reaches the q8_1 kernel, whatever GGML_ARIFI_SX8_MMVQ
// says. Selecting the bound from the environment flag instead of the executed
// route relaxed the twelve f16 cases from 1e-9 to 5e-4 - a 500,000x weaker gate
// on a path that is not q8_1 at all. The route is therefore decided per case.
enum sx8_ref_route {
    SX8_ROUTE_DEQUANT = 0,   // f32 dequant mat-vec shader
    SX8_ROUTE_Q8_1    = 1,   // q8_1 integer-dot mat-vec shader
    SX8_ROUTE_UNKNOWN = 2,   // ArifiLabs lane-235 / R48b: decided by the device, see below
};

// mul_mat_vec_max_cols in ggml-vulkan.cpp: above it, ggml_vk_mul_mat() takes the
// matmul path, which never quantizes the activations.
static const int64_t sx8_ref_mmv_max_cols = 8;

// Mirrors the backend predicate exactly, including the character test at
// ggml-vulkan.cpp:10914 (sx8_mmvq_env[0] == '1', NOT atoi() != 0 - they disagree
// on "2" and "01") and the GGML_VK_DISABLE_MMVQ / GGML_VK_FORCE_MMVQ overrides
// evaluated before it at 8297-8302.
// ArifiLabs lane-235 / R48b phase 2 changed the DEFAULT: with the MMVQ A-side hoist live
// (device-probed ON for AMD), S-X8 takes the q8_1 route at every width and GGML_ARIFI_SX8_MMVQ=0
// is what hands it back to the whole-block f32 path. GGML_ARIFI_MMVQ_ROUTE=legacy restores the old
// opt-in rule, and GGML_ARIFI_MMVQ_A_HOIST=0 removes the hoist and with it the new route.
//
// The hoist default is a VENDOR probe, and this test has no vendor. Sniffing the backend
// description for "AMD"/"Radeon" was refused: it is brittle across driver naming and re-derives a
// vendor_id check the backend already did. So every PINNED cell is mirrored exactly and the one
// genuinely unknown cell -- new routing, hoist state not pinned by an env var -- is reported as
// UNKNOWN and settled after measurement by the route witness. That costs no gate strength: the
// witness gap is eight orders wide (worst dequant 1.5e-13, smallest q8_1 1.3e-5), so a reading
// cannot sit in it by accident, and each route is still scored under its own bound.
static sx8_ref_route sx8_ref_requested_route() {
    if (getenv("GGML_VK_DISABLE_MMVQ")) {
        return SX8_ROUTE_DEQUANT;
    }
    if (getenv("GGML_VK_FORCE_MMVQ")) {
        return SX8_ROUTE_Q8_1;
    }
    const char * env = getenv("GGML_ARIFI_SX8_MMVQ");

    const char * hoist = getenv("GGML_ARIFI_MMVQ_A_HOIST");
    const char * route = getenv("GGML_ARIFI_MMVQ_ROUTE");
    const bool   legacy_rules = (hoist != nullptr && hoist[0] == '0') ||
                                (route != nullptr && strcmp(route, "legacy") == 0);
    if (legacy_rules) {
        // Pre-R48b: only an exact '1' admits the type. The character test, not atoi() != 0 --
        // they disagree on "2" and "01", and the backend uses the character test.
        return (env != nullptr && env[0] == '1') ? SX8_ROUTE_Q8_1 : SX8_ROUTE_DEQUANT;
    }
    if (env != nullptr && env[0] == '0') {
        // R48b routing, type handed back explicitly.
        return SX8_ROUTE_DEQUANT;
    }
    // R48b routing on a device whose hoist default this test cannot see: q8_1 on AMD, dequant
    // elsewhere. Settled per case by the witness.
    return SX8_ROUTE_UNKNOWN;
}

static sx8_ref_route sx8_ref_route_of(const sx8_ref_case & c, sx8_ref_route requested) {
    if (requested == SX8_ROUTE_DEQUANT)       return SX8_ROUTE_DEQUANT;
    if (c.type_b != GGML_TYPE_F32)            return SX8_ROUTE_DEQUANT;  // 11087: src1 must be f32
    if ((c.n * c.k) % 4 != 0)                 return SX8_ROUTE_DEQUANT;  // 11087: (ne11*ne10) % 4
    if (c.n > sx8_ref_mmv_max_cols)           return SX8_ROUTE_DEQUANT;  // 11851: matmul path
    return requested;                                                    // Q8_1 or UNKNOWN
}

// The bound. NOT the sweep's 5e-4 on the dequant route: that number exists to
// absorb the CPU reference's own q8_1 activation error and would be meaningless
// against an exact reference. The S-X8 dequant mat-vec multiplies decoded weights
// by exact activations in f32 and accumulates over k <= 5120 terms, so the
// expected NMSE is f32 round-off, order 1e-13..1e-12. 1e-9 is three orders above
// that and still four orders BELOW the dequantization error a wrong decode would
// produce (a single mis-decoded 6-bit level moves one term by ~1 part in 64 of
// its range). The q8_1 integer-dot route quantizes the activations itself and is
// expected to sit near 1e-4; it is scored under its own bound so the two routes
// are never scored on one number.
static double sx8_ref_bound(sx8_ref_route route) {
    return route == SX8_ROUTE_Q8_1 ? 5e-4 : 1e-9;
}

// The route WITNESS: the predicate above is a prediction of backend behaviour,
// so a case that claims q8_1 must also LOOK like q8_1. Activation quantization
// costs about 8 bits per element and cannot hide: measured on this box,
//   dequant route:  nmse 2.4e-15 .. 1.5e-13  (receipts 10, 11, and the f16 cells of 14)
//   q8_1 route:     nmse 1.3e-05 .. 6.6e-05  (receipts 13 and the f32 cells of 14)
// eight orders apart (worst dequant 1.5e-13 to smallest q8_1 1.3e-5). A
// q8_1-labelled case measuring below this floor did NOT
// execute q8_1 - which is exactly what an unsupported integer_dot_product device,
// or a future routing change, would produce - and is reported as a ROUTE MISMATCH
// failure rather than silently passing under the relaxed bound. 1e-8 sits ~68000x
// above the worst dequant reading and ~1300x below the smallest q8_1 reading.
// The floor is vacuous under PLANT_RED (corruption lifts every case far above it),
// so it is only applied on clean runs.
static double sx8_ref_route_floor(sx8_ref_route route) {
    return route == SX8_ROUTE_Q8_1 ? 1e-8 : 0.0;
}

// The verdict, as one pure function of the measurement, so the classification can
// be checked without a device. THREE outcomes, deliberately independent:
//   nonfinite      - the measurement itself is not a number. A NUMERICAL failure.
//                    It is NOT a route mismatch: nothing was measured, so nothing
//                    can be said about which route ran.
//   route_mismatch - a FINITE, in-bound value BELOW the q8_1 witness floor: the
//                    case claimed q8_1 and measured dequant-grade error.
//   pass           - in bound and not a route mismatch.
// The earlier predicate was `floor_ <= 0.0 || nmse >= floor_`. With a NaN nmse
// that comparison is false, so a nonfinite measurement on the q8_1 route was
// charged to the route-mismatch counter - contradicting the ledger header this
// file prints. Fail-closed was preserved, the attributed cause was not.
struct sx8_ref_verdict {
    bool pass           = false;
    bool route_mismatch = false;
    bool nonfinite      = false;
};

static sx8_ref_verdict sx8_ref_classify(double nmse, bool nonfinite_out, double bound, double floor_) {
    sx8_ref_verdict v;
    // Two independent sources of "not a number": a nonfinite OUTPUT element
    // (nonfinite_out), and a nonfinite nmse, which sx8_ref_measure() also emits as
    // +INFINITY when the output energy is zero while the error is not.
    v.nonfinite         = nonfinite_out || !std::isfinite(nmse);
    const bool in_bound = !v.nonfinite && nmse <= bound;
    v.route_mismatch    = in_bound && floor_ > 0.0 && nmse < floor_;
    v.pass              = in_bound && !v.route_mismatch;
    return v;
}

// The deterministic seam: no device, no shapes, just the classification contract.
// It runs once at the top of the slice and prints its result into the receipt, so
// "a nonfinite measurement is a numerical failure, never a route mismatch" is a
// checked line in the evidence instead of a claim in a comment.
static bool sx8_ref_classify_selftest() {
    const double B = sx8_ref_bound(SX8_ROUTE_Q8_1);        // 5e-4
    const double F = sx8_ref_route_floor(SX8_ROUTE_Q8_1);  // 1e-8
    const double D = sx8_ref_bound(SX8_ROUTE_DEQUANT);     // 1e-9
    struct sx8_ref_classify_tcase {
        const char * name;
        double nmse;
        bool   nonfinite_out;
        double bound;
        double floor_;
        bool   pass;
        bool   route_mismatch;
        bool   nonfinite;
    };
    const sx8_ref_classify_tcase ts[] = {
        // q8_1 route, witness floor active
        { "q8_1 in band",        1.3e-5,   false, B, F,   true,  false, false },
        { "q8_1 at floor",       1e-8,     false, B, F,   true,  false, false },
        { "q8_1 above bound",    1.0e-3,   false, B, F,   false, false, false },
        { "q8_1 below floor",    1.5e-13,  false, B, F,   false, true,  false },
        { "q8_1 nmse NaN",       NAN,      false, B, F,   false, false, true  },
        { "q8_1 nmse +Inf",      INFINITY, false, B, F,   false, false, true  },
        { "q8_1 nonfinite out",  1.3e-5,   true,  B, F,   false, false, true  },
        // dequant route, floor disabled
        { "dequant in bound",    1.5e-13,  false, D, 0.0, true,  false, false },
        { "dequant above bound", 1.0e-8,   false, D, 0.0, false, false, false },
        { "dequant nmse NaN",    NAN,      false, D, 0.0, false, false, true  },
        // PLANT_RED disables the floor on the q8_1 route as well
        { "plant, floor off",    1.5e-13,  false, B, 0.0, true,  false, false },
    };
    const int n_total = (int) (sizeof(ts) / sizeof(ts[0]));
    int       n_bad   = 0;
    for (const sx8_ref_classify_tcase & t : ts) {
        const sx8_ref_verdict v = sx8_ref_classify(t.nmse, t.nonfinite_out, t.bound, t.floor_);
        if (v.pass != t.pass || v.route_mismatch != t.route_mismatch || v.nonfinite != t.nonfinite) {
            printf("    \033[1;31mCLASSIFY SELF-CHECK FAILED\033[0m: '%s' got pass=%d mismatch=%d nonfinite=%d,"
                   " expected pass=%d mismatch=%d nonfinite=%d\n",
                   t.name, (int) v.pass, (int) v.route_mismatch, (int) v.nonfinite,
                   (int) t.pass, (int) t.route_mismatch, (int) t.nonfinite);
            n_bad++;
        }
    }
    printf("  classify:  self-check %d/%d OK%s - a nonfinite measurement is a NUMERICAL failure and is\n"
           "             never counted as a route mismatch; route mismatch means a finite in-bound value\n"
           "             below the q8_1 witness floor\n",
           n_total - n_bad, n_total, n_bad ? " \033[1;31m(FAILED)\033[0m" : "");
    return n_bad == 0;
}

// Planted-red injection. The earlier version flipped ONE byte in the whole
// weight buffer, which makes the injected error energy scale as 1/(m*k): it went
// red at k=1056 but sat at nmse 5e-5..2e-4 at k=5120, i.e. INSIDE the q8_1 path's
// 5e-4 bound. The strength must not depend on the shape, so corrupt one level
// byte in EVERY 30-byte S-X8 block: the corrupted fraction is then a fixed
// 2 weights in 32, for every m and every k.
//
// Why the magnitude clears BOTH bounds, worst case and not on average:
//   - qh[j>>1] holds the high nibble of two 6-bit levels. XOR 0x88 flips bit 3
//     of each nibble, i.e. bit 5 of each level, so both levels move by EXACTLY
//     32 of 64 steps. There is no weak input: unlike XOR 0xFF (nibble -> 15-nibble)
//     the displacement does not depend on the stored value.
//   - dequantize_row_sx8() decodes value = rlo + step*level with step = (r1-r0)/63
//     over a sub-range whose width is R, R/4, R/4 or R/2 of the block range R
//     (config strategies s=0..3). The WORST strategy still displaces the weight by
//     32/63 * R/4 ~= 0.13*R.
//   - Inputs are uniform in [-1,1), so per corrupted term the squared error is
//     >= (0.13*R)^2 with R ~ 1.5 typical, against a mean squared term of ~1/3.
//     At a 1/16 corrupted fraction that floors the NMSE near 7e-3: >10x above the
//     q8_1 bound of 5e-4 and seven orders above the dequant bound of 1e-9. The
//     bounds themselves are untouched - this only guarantees the probe crosses them.
//
// Device-copy-only (the caller passes its own copy, never the memoized a_q) and
// deterministic: no RNG, the byte chosen per block is a function of the block index.
static void sx8_ref_plant_red(std::vector<uint8_t> & a_dev) {
    const size_t bs = ggml_type_size(GGML_TYPE_SX8);
    // Pinned to the .sx8v43 container: dmin,dmax (2x2 B), config (1 B), qh (16 B),
    // ql (8 B), coeff (1 B). A size change means the offset below is stale.
    GGML_ASSERT(bs == 30 && ggml_blck_size(GGML_TYPE_SX8) == 32);
    const size_t qh_off = 5, qh_len = 16;
    GGML_ASSERT(a_dev.size() % bs == 0 && a_dev.size() > 0);

    size_t n_blocks = 0;
    for (size_t off = 0; off + bs <= a_dev.size(); off += bs) {
        a_dev[off + qh_off + (n_blocks % qh_len)] ^= 0x88;
        n_blocks++;
    }
    GGML_ASSERT(n_blocks == a_dev.size() / bs);
}

static bool run_sx8_reference_slice(ggml_backend_t backend, ggml_backend_t backend_cpu) {
    const sx8_ref_route requested = sx8_ref_requested_route();

    printf("S-X8 independent-reference slice on %s\n", ggml_backend_name(backend));
    printf("  reference: dequantize_row_sx8() + double-precision dot, exact activations\n");
    printf("  inputs:    fixed, deterministic by (row, index); no RNG, no thread slicing\n");
    printf("  request:   q8_1 activation quantization %s\n",
           requested == SX8_ROUTE_Q8_1    ? "REQUESTED (pinned by the environment)" :
           requested == SX8_ROUTE_DEQUANT ? "not requested" :
           "DEVICE DEFAULT (R48b routing: q8_1 where the A-side hoist is on, dequant elsewhere) - "
           "route inferred per case from the witness");
    printf("  routing:   q8_1 requires an f32 activation (ggml-vulkan.cpp:11087); f16 activations stay on\n");
    printf("             the dequant route and are scored at %.3g even when q8_1 is requested. q8_1 cases\n",
           sx8_ref_bound(SX8_ROUTE_DEQUANT));
    printf("             are scored in [%.3g, %.3g]: the lower edge is the route witness, a q8_1 case that\n",
           sx8_ref_route_floor(SX8_ROUTE_Q8_1), sx8_ref_bound(SX8_ROUTE_Q8_1));
    printf("             measures below it did not execute q8_1 and is a ROUTE MISMATCH, not a pass.\n");

    // Device-free proof of the classification contract, before any case runs.
    const bool classify_ok = sx8_ref_classify_selftest();

    // Groups run cheapest-first so a truncated run still leaves usable evidence,
    // and GGML_ARIFI_SX8REF_GROUPS (substring match) selects a subset.
    std::vector<sx8_ref_case> cases;
    // The ten m=1 reproducers, restored with fixed inputs. These are the ones
    // commit G removed from `test` mode; this is now their only home.
    // ArifiLabs lane-235 / R48b phase 2: widened from {1,2,3,4,8} to every mat-vec width. S-X8 now
    // takes the q8_1 route at EVERY width by default, so n=5,6,7 -- previously unreachable on this
    // path and therefore never checked against an independent reference -- are now served widths.
    for (int64_t k : {1056, 5120}) {
        for (int64_t n : {1, 2, 3, 4, 5, 6, 7, 8}) {
            cases.push_back({GGML_TYPE_F32, 1, n, k, "m1"});
        }
    }
    for (int64_t m : {7, 4095, 17408}) {
        // The twelve f16-activation descriptors the `test` sweep cannot execute,
        // each next to its f32 twin. The twin is the same shape on the same data
        // over a path the CPU backend CAN run: it is the control that separates
        // "f16 activation handling is wrong" from "the reference is wrong".
        for (int64_t n : {1, 2, 3, 4, 5, 6, 7, 8}) {
            cases.push_back({GGML_TYPE_F32, m, n, 5120, "f32-twin"});
            cases.push_back({GGML_TYPE_F16, m, n, 5120, "f16"});
        }
    }

    const char * groups = getenv("GGML_ARIFI_SX8REF_GROUPS");
    if (groups != nullptr) {
        printf("  groups:    filtered to '%s'\n", groups);
    }
    // Planted-red reachability: corrupt ONE weight byte on the device copy only,
    // after the reference has been computed. A mode that cannot go red proves
    // nothing when it is green.
    const char * plant_env = getenv("GGML_ARIFI_SX8REF_PLANT_RED");
    const int    plant     = plant_env ? atoi(plant_env) : 0;
    if (plant) {
        printf("  PLANTED RED: one level byte corrupted per S-X8 block on the device copy; every case MUST fail\n");
    }

    int n_run = 0, n_fail = 0, n_declined = 0;
    int n_q8_1 = 0, n_dequant = 0, n_route_mismatch = 0, n_nonfinite = 0;

    // Cases are grouped by (m,k) above, so one slot removes the repeated fill and
    // SX8 quantize of the same 17408x5120 weight matrix across its eight cases.
    int64_t memo_m = -1, memo_k = -1;
    std::vector<float>   a_f32;
    std::vector<uint8_t> a_q;

    for (const sx8_ref_case & c : cases) {
        if (groups != nullptr && strstr(groups, c.group) == nullptr) {
            continue;
        }

        if (c.m != memo_m || c.k != memo_k) {
            sx8_ref_fill_f32(a_f32, c.m, c.k, 0);
            a_q.assign(ggml_row_size(GGML_TYPE_SX8, c.k) * (size_t) c.m, 0);
            // single call, no imatrix: the block layout must be byte-reproducible
            ggml_quantize_chunk(GGML_TYPE_SX8, a_f32.data(), a_q.data(), 0,
                                c.m, c.k, nullptr);
            memo_m = c.m;
            memo_k = c.k;
        }

        std::vector<float> b_f32;
        sx8_ref_fill_f32(b_f32, c.n, c.k, 1 << 20);   // a different row base than A

        std::vector<uint8_t> b_raw;
        if (c.type_b == GGML_TYPE_F16) {
            b_raw.resize(b_f32.size() * sizeof(ggml_fp16_t));
            ggml_fp32_to_fp16_row(b_f32.data(), (ggml_fp16_t *) b_raw.data(), (int64_t) b_f32.size());
            // The reference must see the activations the DEVICE sees, so round
            // the f32 copy through f16 as well. f16 -> f32 is exact.
            for (size_t i = 0; i < b_f32.size(); i++) {
                b_f32[i] = ggml_fp16_to_fp32(((const ggml_fp16_t *) b_raw.data())[i]);
            }
        } else {
            b_raw.resize(b_f32.size() * sizeof(float));
            memcpy(b_raw.data(), b_f32.data(), b_raw.size());
        }

        std::vector<double> ref, ref_abs;
        sx8_ref_compute(c, a_q, b_f32, ref, ref_abs);

        // After the reference: the device gets different bytes than the reference saw.
        std::vector<uint8_t> a_dev = a_q;
        if (plant) {
            sx8_ref_plant_red(a_dev);
        }

        std::vector<float> got;
        std::string why;
        const bool ok = sx8_ref_run(backend, c, a_dev, b_raw, got, why);

        const sx8_ref_route predicted = sx8_ref_route_of(c, requested);

        printf("  MUL_MAT(type_a=sx8,type_b=%s,m=%" PRId64 ",n=%" PRId64 ",k=%" PRId64 ",grp=%s,route=%s): ",
               ggml_type_name(c.type_b), c.m, c.n, c.k, c.group,
               predicted == SX8_ROUTE_Q8_1 ? "q8_1" :
               predicted == SX8_ROUTE_DEQUANT ? "dequant" : "device");

        if (!ok) {
            // A decline is a FAILURE of coverage in this mode: the whole point is
            // that every listed descriptor executes against a reference.
            printf("\033[1;31mDECLINED\033[0m (%s) [%s]\n", why.c_str(), ggml_backend_name(backend));
            n_declined++;
            n_fail++;
            continue;
        }

        const sx8_ref_metrics mt = sx8_ref_measure(got, ref, ref_abs);
        n_run++;

        // ArifiLabs lane-235 / R48b: settle the one cell the environment does not pin. The witness
        // gap is eight orders wide, so this reads which shader ran rather than guessing: a reading
        // at or above the floor is q8_1-grade activation error, below it is f32-exact. Both routes
        // keep their own bound, and the case is labelled INFERRED so no receipt can mistake it for
        // a pinned route. Under PLANT_RED the corruption lifts every case above the floor, so an
        // unpinned red run is scored under the q8_1 bound -- the weaker of the two, which is the
        // safe direction for a mode whose planted cases must ALL fail.
        const bool          inferred = (predicted == SX8_ROUTE_UNKNOWN);
        const sx8_ref_route route    = inferred
                                       ? (mt.nmse >= sx8_ref_route_floor(SX8_ROUTE_Q8_1)
                                          ? SX8_ROUTE_Q8_1 : SX8_ROUTE_DEQUANT)
                                       : predicted;
        const double        bound    = sx8_ref_bound(route);
        // The witness cannot judge a route it just derived from the same number.
        const double        floor_   = (plant || inferred) ? 0.0 : sx8_ref_route_floor(route);

        // The CPU backend arm, where it exists: the calibration column. It is
        // NOT a pass/fail gate - it is the number that shows how much of the
        // sweep's 5e-4 budget the reference itself was spending.
        std::string cpu_col = "cpu=n/a";
        if (backend_cpu != nullptr) {
            std::vector<float> got_cpu;
            std::string why_cpu;
            if (sx8_ref_run(backend_cpu, c, a_dev, b_raw, got_cpu, why_cpu)) {
                const sx8_ref_metrics mc = sx8_ref_measure(got_cpu, ref, ref_abs);
                char buf[128];
                snprintf(buf, sizeof(buf), "cpu_nmse=%.3e cpu_absmax=%.3e", mc.nmse, mc.max_abs);
                cpu_col = buf;
            } else {
                cpu_col = "cpu=" + why_cpu;
            }
        }

        // One classification, the same function the self-check above exercised.
        const sx8_ref_verdict v    = sx8_ref_classify(mt.nmse, mt.nonfinite, bound, floor_);
        const bool            pass = v.pass;
        printf("route=%s%s bound=%.3g nmse=%.3e absmax=%.3e relmax=%.3e rms=%.3e nz=%zu/%zu nz_absmax=%.3e %s %s%s\n",
               route == SX8_ROUTE_Q8_1 ? "q8_1" : "dequant", inferred ? "(INFERRED)" : "", bound,
               mt.nmse, mt.max_abs, mt.max_rel, mt.rms_ref,
               mt.n_nearzero, ref.size(), mt.nearzero_abs, cpu_col.c_str(),
               pass ? "\033[1;32mOK\033[0m" : "\033[1;31mFAIL\033[0m",
               v.route_mismatch
                   ? " \033[1;31mROUTE MISMATCH\033[0m (q8_1 claimed, dequant-grade error measured)"
                   : (v.nonfinite
                          ? " \033[1;31mNONFINITE\033[0m (numerical failure; the route is not judged)"
                          : ""));

        if (route == SX8_ROUTE_Q8_1) {
            n_q8_1++;
        } else {
            n_dequant++;
        }
        if (v.route_mismatch) {
            n_route_mismatch++;
        }
        if (v.nonfinite) {
            n_nonfinite++;
        }
        if (!pass) {
            n_fail++;
        }
    }

    printf("  S-X8 independent-reference slice: %d cases executed, %d declined, %d failed\n",
           n_run, n_declined, n_fail);
    // The route ledger: what was requested, what executed, and under which bound.
    printf("  ROUTES: %d executed on q8_1 (bound %.3g, witness floor %.3g), %d on dequant (bound %.3g); %d route mismatch, %d nonfinite%s\n",
           n_q8_1, sx8_ref_bound(SX8_ROUTE_Q8_1), plant ? 0.0 : sx8_ref_route_floor(SX8_ROUTE_Q8_1),
           n_dequant, sx8_ref_bound(SX8_ROUTE_DEQUANT), n_route_mismatch, n_nonfinite,
           plant ? " (witness disabled under plant)" : "");
    if (requested == SX8_ROUTE_Q8_1 && n_q8_1 == 0) {
        printf("  \033[1;31mNOTE\033[0m: q8_1 was requested but no selected case can take it - f16 activations are dequant-only\n");
    }
    if (plant) {
        // Under plant, rc=1 is the EXPECTED outcome, so rc alone cannot tell
        // "all red" from "partly red". Emit the count the receipt is read on.
        const int n_total = n_run + n_declined;
        printf("  PLANTED RED reachability: %d/%d cases went red\n", n_fail, n_total);
        if (n_fail != n_total || n_total == 0) {
            printf("  \033[1;31mPLANTED RED NOT REACHED\033[0m: %d case(s) stayed inside the bound under corruption\n",
                   n_total - n_fail);
        }
    }
    // A broken classifier invalidates every verdict above it, so it is a failure of
    // the slice even when every case passed.
    return n_fail == 0 && classify_ok;
}

static bool run_fa_vec_slice(ggml_backend_t backend, ggml_backend_t backend_cpu, const char * op_names_filter) {
    const char * LLAMA_TEST_FA_VEC_DISABLE = getenv("LLAMA_TEST_FA_VEC_DISABLE");
    if (LLAMA_TEST_FA_VEC_DISABLE) {
        return true;
    }

    if (!op_names_filter_selects(op_names_filter, "FLASH_ATTN_EXT")) {
        return true;
    }

    auto * reg = ggml_backend_dev_backend_reg(ggml_backend_get_device(backend));

    auto set_ov   = (set_fa_vec_override_t)   ggml_backend_reg_get_proc_address(reg, "ggml_backend_metal_tuning_set_fa_vec_override");
    auto clear_ov = (clear_fa_vec_override_t) ggml_backend_reg_get_proc_address(reg, "ggml_backend_metal_tuning_clear_fa_vec_override");
    if (!set_ov || !clear_ov) {
        return true;  // not the Metal backend: nothing to force
    }

    printf("Running FA vec slice tests (env LLAMA_TEST_FA_VEC_DISABLE=1 to skip)\n");

    struct shape_t { int dk, dv; };
    const shape_t   shapes[] = { { 128, 128 }, { 576, 512 } };  // mainstream head size + MLA shared K/V view
    const int       ne01_pts[] = { 1, 3 };                      // decode, and padded rows for Q=2 and Q=4
    const int       ne11_pts[] = { 512, 4097 };                 // nsg=1, and nsg>=2 together with kvpad
    const ggml_type types[]    = { GGML_TYPE_F16, GGML_TYPE_Q4_0 };

    int n_run = 0;
    int n_fail = 0;
    for (auto s : shapes) {
        for (int ne : fa_vec_legal_ne(s.dk, s.dv)) {
            for (int Q : { 1, 2, 4 }) {
                for (ggml_type type_kv : types) {
                    for (bool sinks : { false, true }) {
                        for (int ne01 : ne01_pts) {
                            for (int ne11 : ne11_pts) {
                                set_ov(Q, ne);
                                test_flash_attn_ext tc(s.dk, s.dv, /*nh=*/4, { 1, 1 }, /*kv=*/ne11, /*nb=*/ne01,
                                                       /*mask=*/true, sinks, 0.0f, 0.0f, GGML_PREC_F32,
                                                       type_kv, type_kv);
                                auto st = tc.eval(backend, backend_cpu, "FLASH_ATTN_EXT", nullptr);
                                clear_ov();

                                if (st == test_status_t::FAIL) {
                                    printf("  FAIL fa_vec slice: dk=%d dv=%d Q=%d ne=%d type=%s ne01=%d ne11=%d sinks=%d\n",
                                           s.dk, s.dv, Q, ne, ggml_type_name(type_kv), ne01, ne11, (int) sinks);
                                    n_fail++;
                                }
                                n_run++;
                            }
                        }
                    }
                }
            }
        }
    }

    printf("  fa_vec (Q,NE) slice: %d cases run, %d failed\n", n_run, n_fail);

    return n_fail == 0;
}

static bool test_backend(ggml_backend_t backend, ggml_backend_dev_t dev, test_mode mode, const char * op_names_filter, const char * params_filter,
                         printer * output_printer, const char * test_file_path, int parallel_workers) {
    auto filter_test_cases = [](std::vector<std::unique_ptr<test_case>> & test_cases, const char * params_filter) {
        if (params_filter == nullptr) {
            return;
        }

        std::regex params_filter_regex(params_filter);

        for (auto it = test_cases.begin(); it != test_cases.end();) {
            if (!std::regex_search((*it)->vars(), params_filter_regex)) {
                it = test_cases.erase(it);
                continue;
            }

            it++;
        }
    };

    if (mode == MODE_SX8REF) {
        ggml_backend_ptr backend_cpu(ggml_backend_init_by_type(GGML_BACKEND_DEVICE_TYPE_CPU, NULL));
        GGML_UNUSED(dev);
        return run_sx8_reference_slice(backend, backend_cpu.get());
    }

    std::vector<std::unique_ptr<test_case>> test_cases;

    if (test_file_path == nullptr) {
        switch (mode) {
        case MODE_SX8REF:
        case MODE_TEST:
        case MODE_GRAD:
        case MODE_SUPPORT:
            test_cases = make_test_cases_eval();
            break;
        case MODE_PERF:
            test_cases = make_test_cases_perf();
            break;
        }
    } else {
        test_cases = make_test_cases_from_file(test_file_path);
    }

    filter_test_cases(test_cases, params_filter);

    if (mode == MODE_TEST) {
        ggml_backend_ptr backend_cpu(ggml_backend_init_by_type(GGML_BACKEND_DEVICE_TYPE_CPU, NULL));
        if (backend_cpu == NULL) {
            test_operation_info info("", "", "CPU");
            info.set_error("backend", "Failed to initialize CPU backend");
            output_printer->print_operation(info);
            return false;
        }
        // Use reference implementation on the CPU backend for comparison
        using ggml_backend_cpu_set_use_ref_t = void (*)(ggml_backend_t, bool);
        auto * reg = ggml_backend_dev_backend_reg(ggml_backend_get_device(backend_cpu.get()));
        auto * set_use_ref = (ggml_backend_cpu_set_use_ref_t) ggml_backend_reg_get_proc_address(reg, "ggml_backend_cpu_set_use_ref");
        if (set_use_ref) {
            set_use_ref(backend_cpu.get(), true);
        }

        std::atomic<size_t> n_ok = 0;
        std::atomic<size_t> tests_run = 0;
        std::atomic<size_t> n_not_supported = 0;   // F-112: declined cases, counted not swallowed
        std::atomic<size_t> n_filtered = 0;
        // F-112, second half: the run-level rule cannot see a SINGLE op family that executed
        // nothing inside a big green sweep — which is exactly the shape SET_ROWS_TURBO4 hid in.
        // Per-op tallies make that visible at the end of every run.
        std::map<std::string, std::pair<size_t, size_t>> per_op;   // op -> {executed, declined}
        std::mutex per_op_mutex;
        // lane-150: per-op is NOT fine enough. The generic test_set_rows / test_cpy classes report
        // one op_desc ("SET_ROWS", "CPY") for EVERY destination type they sweep, so a whole weight
        // format with zero executed cells hides behind the types that pass in the same bucket.
        // Measured at lane-150's base: all six TQ ids had 24 declined / 0 executed SET_ROWS cases
        // each — 144 invisible cells — while the bucket printed "338/343 tests passed". Only
        // TQ4_1S was ever caught, and only because it happened to have a bespoke class with its
        // own op_desc. This second tally keys on the tensor type named in vars(), so a per-TYPE
        // hole inside a mixed op is named the same way a per-op hole is.
        // KNOWN LIMITS, named rather than implied (lane-150 checker, findings 1/2/8):
        //   * ONE axis per case. A bucket with any executing cell stays silent, so a hole on a
        //     second axis still hides — CPY[type_dst=tq4_1s] has 3 passing same-type memcpy cells,
        //     which is why this tally would NOT have caught the tq4_1s->f32 read-back hole, and
        //     SET_ROWS[type_dst=X] mixes f32-src and f16-src cells.
        //   * Only these four keys. type_K / type_V / type_b / type_kernel / type_input carry no
        //     bucket, so FLASH_ATTN_EXT — the largest type-parameterised op, and the KV-cache path —
        //     is NOT covered here.
        std::map<std::string, std::pair<size_t, size_t>> per_op_type;
        // Pull the type this case is really about out of the vars() string. Order matters: the
        // destination is what a write-path cell is about, the source is what a read-path cell is.
        auto type_bucket = [](const std::string & op, const std::string & vars) -> std::string {
            for (const char * key : { "type_dst=", "type=", "type_a=", "type_src=" }) {
                size_t p = vars.find(key);
                // The match must START a var name, or "type=" matches inside "dst_type=",
                // "pool_type=", "kernel_type=", "tri_type=" and labels a POOLING MODE as a tensor
                // type (checker finding 3). Walk on until the match is at the start or after ','.
                while (p != std::string::npos && p != 0 && vars[p - 1] != ',' && vars[p - 1] != ' ') {
                    p = vars.find(key, p + 1);
                }
                if (p == std::string::npos) {
                    continue;
                }
                const size_t b = p + strlen(key);
                const size_t e = vars.find(',', b);
                return op + "[" + key + vars.substr(b, e == std::string::npos ? e : e - b) + "]";
            }
            return std::string();   // no type in vars: the per-op tally already covers it
        };
        std::vector<std::string> failed_tests;
        std::mutex failed_tests_mutex;

        // Each worker grabs a chunk of cases at a time. The chunk shrinks as we
        // run out of work so that a few slow tests at the tail get spread across
        // workers instead of landing on one unlucky thread.
        constexpr size_t MAX_TESTS_PER_ITER = 100;
        std::atomic<size_t> test_idx = 0;

        const auto & next_chunk = [&](size_t & my_begin, size_t & my_end) {
            const size_t cur = test_idx.load(std::memory_order_relaxed);
            const size_t remaining = cur < test_cases.size() ? test_cases.size() - cur : 0;
            const size_t chunk = std::max<size_t>(1, std::min<size_t>(MAX_TESTS_PER_ITER, remaining / parallel_workers));
            my_begin = test_idx.fetch_add(chunk);
            my_end = std::min(my_begin + chunk, test_cases.size());
        };

        const auto & run_tests = [&](ggml_backend_t b, ggml_backend_t b_cpu) {
            size_t my_begin, my_end;
            next_chunk(my_begin, my_end);
            while (my_begin < test_cases.size()) {
                for (size_t i = my_begin; i < my_end; ++i) {
                    auto & test = test_cases[i];
                    test_status_t status = test->eval(b, b_cpu, op_names_filter, output_printer);
                    if (status == test_status_t::SKIPPED || status == test_status_t::NOT_SUPPORTED) {
                        if (status == test_status_t::NOT_SUPPORTED) {
                            n_not_supported++;
                            std::lock_guard<std::mutex> guard(per_op_mutex);
                            per_op[test->current_op_name].second++;
                            const std::string tb = type_bucket(test->current_op_name, test->vars());
                            if (!tb.empty()) {
                                per_op_type[tb].second++;
                            }
                        } else {
                            n_filtered++;
                        }
                        continue;
                    }
                    {
                        std::lock_guard<std::mutex> guard(per_op_mutex);
                        per_op[test->current_op_name].first++;
                        const std::string tb = type_bucket(test->current_op_name, test->vars());
                        if (!tb.empty()) {
                            per_op_type[tb].first++;
                        }
                    }
                    tests_run++;
                    if (status == test_status_t::OK) {
                        n_ok++;
                    } else if (status == test_status_t::FAIL) {
                        std::lock_guard<std::mutex> guard(failed_tests_mutex);
                        failed_tests.push_back(test->current_op_name + "(" + test->vars() + ")");
                    }
                }
                next_chunk(my_begin, my_end);
            }
        };

        if (parallel_workers <= 1) {
            // Reuse the outer backend / backend_cpu so we don't pay an
            // extra CPU backend init.
            run_tests(backend, backend_cpu.get());
        } else {
            std::atomic<size_t> workers_started = 0;

            const auto & eval_worker = [&]() {
                ggml_backend_ptr b(ggml_backend_dev_init(dev, NULL));
                if (b == NULL) {
                    return;
                }

                ggml_backend_ptr b_cpu(ggml_backend_init_by_type(GGML_BACKEND_DEVICE_TYPE_CPU, NULL));
                if (b_cpu == NULL) {
                    return;
                }

                if (set_use_ref) {
                    set_use_ref(b_cpu.get(), true);
                }
                workers_started++;
                run_tests(b.get(), b_cpu.get());
            };

            std::vector<std::thread> threads;
            threads.reserve(parallel_workers);
            for (int i = 0; i < parallel_workers; ++i) {
                threads.emplace_back(eval_worker);
            }
            for (auto & t : threads) {
                t.join();
            }

            if (workers_started == 0 && !test_cases.empty()) {
                return false;
            }
        }

        test_summary_info summary(n_ok, tests_run, false);
        summary.tests_not_supported = n_not_supported;
        summary.tests_filtered      = n_filtered;
        output_printer->print_summary(summary);
        output_printer->print_failed_tests(failed_tests);

        const bool slice_ok = run_fa_vec_slice(backend, backend_cpu.get(), op_names_filter);
        // F-112: name every op whose cases were ALL declined. In a full sweep the run-level rule
        // below cannot fire (thousands of other cases execute), so without this line an op family
        // with zero executed coverage is invisible behind a green banner — the lane-148 defect.
        {
            std::vector<std::string> zero_exec;
            for (const auto & kv : per_op) {
                if (kv.second.first == 0 && kv.second.second > 0) {
                    zero_exec.push_back(kv.first + "(" + std::to_string(kv.second.second) + ")");
                }
            }
            if (!zero_exec.empty()) {
                printf("  ZERO EXECUTED COVERAGE for %zu op(s) — every case declined: ",
                       zero_exec.size());
                for (size_t i = 0; i < zero_exec.size(); i++) {
                    printf("%s%s", i ? ", " : "", zero_exec[i].c_str());
                }
                printf("\n");
            }
        }

        // lane-150: the same rule one level finer — op x type. An entry here that is NOT already
        // named by the per-op line above is a format that has zero executed coverage for this op
        // while its neighbours in the same bucket pass. That is the shape the whole TQ family sat
        // in for weeks. Declining is allowed; declining INVISIBLY is not.
        {
            std::vector<std::string> zero_exec;
            for (const auto & kv : per_op_type) {
                if (kv.second.first != 0 || kv.second.second == 0) {
                    continue;
                }
                const std::string op = kv.first.substr(0, kv.first.find('['));
                const auto it = per_op.find(op);
                if (it != per_op.end() && it->second.first == 0) {
                    continue;   // the whole op is already named above; do not print it twice
                }
                zero_exec.push_back(kv.first + "(" + std::to_string(kv.second.second) + ")");
            }
            if (!zero_exec.empty()) {
                printf("  ZERO EXECUTED COVERAGE for %zu op x type cell(s) inside otherwise-"
                       "executing ops — every case declined: ", zero_exec.size());
                for (size_t i = 0; i < zero_exec.size(); i++) {
                    printf("%s%s", i ? ", " : "", zero_exec[i].c_str());
                }
                printf("\n");
            }
        }

        // F-112: a filter that matched cases, all of which were declined, executed NOTHING. That is
        // not a pass. (A filter that matched nothing at all — n_filtered only — stays a no-op, which
        // is how the per-op sweep enumerates ops.)
        if (tests_run == 0 && n_not_supported > 0) {
            return false;
        }

        return n_ok == tests_run && slice_ok;
    }

    if (mode == MODE_GRAD) {
        test_cases.erase(
            std::remove_if(test_cases.begin(), test_cases.end(), [](const std::unique_ptr<test_case> & tc) {
                return tc->run_whole_graph();
            }),
            test_cases.end()
        );

        size_t n_ok = 0;
        for (auto & test : test_cases) {
            if (test->eval_grad(backend, op_names_filter, output_printer)) {
                n_ok++;
            }
        }
        output_printer->print_summary(test_summary_info(n_ok, test_cases.size(), false));

        return n_ok == test_cases.size();
    }

    if (mode == MODE_PERF) {
        for (auto & test : test_cases) {
            test->eval_perf(backend, op_names_filter, output_printer);
        }
        return true;
    }

    if (mode == MODE_SUPPORT) {
        // Filter out fusion cases
        test_cases.erase(
            std::remove_if(test_cases.begin(), test_cases.end(), [](const std::unique_ptr<test_case> & tc) {
                return tc->run_whole_graph();
            }),
            test_cases.end()
        );

        for (auto & test : test_cases) {
            test->eval_support(backend, op_names_filter, output_printer);
        }
        return true;
    }

    GGML_ABORT("fatal error");
}

static void list_all_ops() {
    printf("GGML operations:\n");
    std::set<std::string> all_ops;

    for (int i = 1; i < GGML_OP_COUNT; i++) {
        all_ops.insert(ggml_op_name((enum ggml_op)i));
    }
    for (int i = 0; i < GGML_UNARY_OP_COUNT; i++) {
        all_ops.insert(ggml_unary_op_name((enum ggml_unary_op)i));
    }
    for (int i = 0; i < GGML_GLU_OP_COUNT; i++) {
        all_ops.insert(ggml_glu_op_name((enum ggml_glu_op)i));
    }
    for (const auto & op : all_ops) {
        printf("  %s\n", op.c_str());
    }
    printf("\nTotal: %zu operations\n", all_ops.size());
}

static void show_test_coverage() {
    std::set<std::string> all_ops;
    for (int i = 1; i < GGML_OP_COUNT; i++) {
        auto op = (enum ggml_op)i;
        if (op == GGML_OP_VIEW      ||
            op == GGML_OP_RESHAPE   ||
            op == GGML_OP_PERMUTE   ||
            op == GGML_OP_TRANSPOSE ||
            op == GGML_OP_CONT      ||
            op == GGML_OP_GLU       ||
            op == GGML_OP_UNARY) {
            continue;
        }
        all_ops.insert(ggml_op_name(op));
    }
    for (int i = 0; i < GGML_UNARY_OP_COUNT; i++) {
        all_ops.insert(ggml_unary_op_name((enum ggml_unary_op)i));
    }
    for (int i = 0; i < GGML_GLU_OP_COUNT; i++) {
        all_ops.insert(ggml_glu_op_name((enum ggml_glu_op)i));
    }
    auto test_cases = make_test_cases_eval();
    // Filter out fusion cases
    test_cases.erase(
        std::remove_if(test_cases.begin(), test_cases.end(), [](const std::unique_ptr<test_case> & tc) {
            return tc->run_whole_graph();
        }),
        test_cases.end()
    );

    std::set<std::string> tested_ops;

    ggml_init_params params = {
        /* .mem_size = */ ggml_tensor_overhead()*128 + ggml_graph_overhead(),
        /* .mem_base = */ NULL,
        /* .no_alloc = */ true,
    };

    for (auto & test_case : test_cases) {
        ggml_context_ptr ctx(ggml_init(params));
        if (ctx) {
            test_case->mode = MODE_TEST;
            ggml_tensor * out = test_case->build_graph(ctx.get());
            if (out && out->op != GGML_OP_NONE) {
                if (out->op == GGML_OP_UNARY) {
                    tested_ops.insert(ggml_unary_op_name(ggml_get_unary_op(out)));
                } else if (out->op == GGML_OP_GLU) {
                    tested_ops.insert(ggml_glu_op_name(ggml_get_glu_op(out)));
                } else {
                    tested_ops.insert(ggml_op_name(out->op));
                }
            }
        }
    }
    std::set<std::string> covered_ops;
    std::set<std::string> uncovered_ops;
    for (const auto & op : all_ops) {
        if (tested_ops.count(op) > 0) {
            covered_ops.insert(op);
        } else {
            uncovered_ops.insert(op);
        }
    }

    printf("Operations covered by tests (%zu):\n", covered_ops.size());
    for (const auto & op : covered_ops) {
        printf("  ✓ %s\n", op.c_str());
    }
    printf("\nOperations without tests (%zu):\n", uncovered_ops.size());
    for (const auto & op : uncovered_ops) {
        printf("  ✗ %s\n", op.c_str());
    }

    printf("\nCoverage Summary:\n");
    printf("  Total operations: %zu\n", all_ops.size());
    printf("  Tested operations: %zu\n", covered_ops.size());
    printf("  Untested operations: %zu\n", uncovered_ops.size());
    printf("  Coverage: %.1f%%\n", (double)covered_ops.size() / all_ops.size() * 100.0);
}

#ifdef GGML_USE_VULKAN
// lane-230 / R46b B7a: correctness of the Vulkan cumulative per-heap reservation ledger.
// Device-free, model-free, timing-free: it drives the same ledger and the same RAII guard that
// ggml_vk_create_buffer() uses. Declared here rather than in a public header because this is an
// internal test seam, not backend API.
// R58 lane-244: Vulkan's own ceiling on memory types (VK_MAX_MEMORY_TYPES). Spelled locally so
// this test needs no Vulkan headers of its own.
#define VK_MAX_MEMORY_TYPES_LOCAL 32

extern "C" {
    // R58 lane-244: memory-type bandwidth probe seam (see ggml-vulkan.cpp).
    void     ggml_vk_memtype_force(int type_index);
    int      ggml_vk_memtype_enumerate(uint32_t * types, uint32_t * heaps, uint32_t * flags,
                                       uint64_t * heap_sizes, int max_types);
    int      ggml_vk_memtype_buffer_info(uint64_t size, uint32_t * type_bits,
                                         uint64_t * max_buffer_size, uint64_t * max_chunk);
    void     ggml_vk_memtype_probe_line(uint32_t type, uint32_t heap, uint32_t flags,
                                        uint64_t bytes, double gpu_read_gbs, double cpu_memcpy_gbs);

    bool     ggml_vk_heapres_reserve(uint32_t heap, uint64_t bytes, uint64_t budget);
    bool     ggml_vk_heapres_release(uint32_t heap, uint64_t bytes);
    bool     ggml_vk_heapres_would_fit(uint32_t heap, uint64_t bytes, uint64_t budget);
    uint64_t ggml_vk_heapres_reserved(uint32_t heap);
    void     ggml_vk_heapres_reset(void);
    int      ggml_vk_heapres_first_fit(const uint32_t * heaps, const uint64_t * budgets, int n,
                                       uint64_t bytes, uint32_t fail_mask);
    void     ggml_vk_heapres_destroy_held(void);
    int      ggml_vk_heapres_held_count(void);
    bool     ggml_vk_heapres_two_ledgers_isolated(uint32_t heap, uint64_t bytes, uint64_t budget);
    int      ggml_vk_heapres_fault_probe(int stage, uint64_t size, int host_visible,
                                         uint64_t * reserved_before, uint64_t * reserved_after,
                                         int * mapped, int * threw, uint64_t * driver_usage_after,
                                         int * bda_supported, int * bda_executed);
    int      ggml_vk_heapres_destroy_order_probe(uint64_t size,
                                                 uint64_t * reserved_before, uint64_t * reserved_alive,
                                                 uint64_t * reserved_at_driver_free,
                                                 uint64_t * reserved_after);
    // R46b B7b — per-buffer-type batch planning.
    uint64_t ggml_vk_plan_driver_alloc_calls(void);
    void     ggml_vk_plan_set_fault(int stage, int skip);
    int      ggml_vk_heapres_plan_search(const uint32_t * cand_heaps, const uint64_t * cand_bytes,
                                         const int * cand_counts, int n_items,
                                         const uint64_t * budgets, int n_heaps, int * out_choice);
    // R46b B7c — bounded host split and the staging reserve.
    int      ggml_vk_heapres_plan_search_split(const uint32_t * cand_heaps, const uint64_t * cand_bytes,
                                               const int * cand_host, const int * cand_counts, int n_items,
                                               const uint64_t * budgets, int n_heaps,
                                               uint64_t host_split_max,
                                               int * out_choice, uint64_t * out_host_bytes);
    void     ggml_vk_heapres_staging_exact_once(uint32_t heap, uint64_t bytes, uint64_t other_bytes,
                                                uint64_t budget, int * first_ok, int * second_ok,
                                                uint64_t * reserved_after_first,
                                                uint64_t * reserved_after_second);
    int      ggml_vk_staging_reserve_probe(uint64_t * bytes, uint32_t * heap, uint64_t * reserved_on_heap,
                                           uint64_t * heap_size);
    int      ggml_vk_plan_probe_limits(uint64_t * max_chunk, uint64_t * total_heap,
                                       uint64_t * reserved_now, int * used_maintenance4);
    int      ggml_vk_plan_probe_begin(const uint64_t * sizes, int n, int * status);
    void     ggml_vk_plan_probe_free_all(void);
    void     ggml_vk_plan_probe_free_one(int handle);
    uint64_t ggml_vk_plan_device_reserved(void);
    // production buffer type, the one a model load allocates weights from
    ggml_backend_buffer_type_t ggml_backend_vk_buffer_type(size_t dev_num);
}

// mirrors enum ggml_backend_plan_status (ggml-backend-impl.h), which is not a public header
enum {
    HEAPRES_PLAN_FEASIBLE      = 0,
    HEAPRES_PLAN_INFEASIBLE    = 1,
    HEAPRES_PLAN_INDETERMINATE = 2,
};

static int heapres_failures = 0;

static void heapres_check(bool ok, const char * what) {
    printf("  %s %s\n", ok ? "OK  " : "FAIL", what);
    if (!ok) {
        heapres_failures++;
    }
}

static int heapres_ledger_tests() {
    const uint64_t MiB = 1024ull * 1024ull;

    printf("Vulkan heap reservation ledger (R46b B7a)\n");

    // 1. exact fit succeeds, one byte over refuses.
    ggml_vk_heapres_reset();
    heapres_check(ggml_vk_heapres_reserve(0, 100 * MiB, 100 * MiB), "exact fit reserved");
    heapres_check(ggml_vk_heapres_reserved(0) == 100 * MiB, "exact fit accounted");
    ggml_vk_heapres_reset();
    heapres_check(!ggml_vk_heapres_reserve(0, 100 * MiB + 1, 100 * MiB), "one byte over refused");
    heapres_check(ggml_vk_heapres_reserved(0) == 0, "refused reservation left no bytes");

    // cumulative: the second allocation sees the first. This is the whole point of B7a - the old
    // per-allocation test admitted both.
    ggml_vk_heapres_reset();
    heapres_check(ggml_vk_heapres_reserve(0, 60 * MiB, 100 * MiB), "first 60MiB of 100MiB reserved");
    heapres_check(!ggml_vk_heapres_reserve(0, 60 * MiB, 100 * MiB), "second 60MiB refused (cumulative)");
    heapres_check(ggml_vk_heapres_reserved(0) == 60 * MiB, "ledger unchanged by the refusal");

    // stock behavior preserved: an empty ledger admits exactly what heap.size >= requirement admits.
    ggml_vk_heapres_reset();
    heapres_check(ggml_vk_heapres_would_fit(0, 100 * MiB, 100 * MiB), "empty ledger: fit == stock accept");
    heapres_check(!ggml_vk_heapres_would_fit(0, 100 * MiB + 1, 100 * MiB), "empty ledger: over == stock reject");

    // 2. cumulative reservations across threads cannot exceed the budget.
    {
        ggml_vk_heapres_reset();
        const uint64_t block   = 8 * MiB;
        const int      allowed = 5;
        const int      threads = 16;
        std::atomic<int> granted(0);
        std::vector<std::thread> workers;
        for (int t = 0; t < threads; t++) {
            workers.emplace_back([&]() {
                if (ggml_vk_heapres_reserve(0, block, block * allowed)) {
                    granted++;
                }
            });
        }
        for (auto & w : workers) {
            w.join();
        }
        heapres_check(granted.load() == allowed, "exactly budget/block threads were granted");
        heapres_check(ggml_vk_heapres_reserved(0) == block * allowed, "threaded total == budget, not over");
    }

    // 3. a failed allocation attempt rolls back before the next candidate is tried.
    {
        ggml_vk_heapres_reset();
        const uint32_t heaps[3]   = { 0, 0, 1 };
        const uint64_t budgets[3] = { 10 * MiB, 10 * MiB, 10 * MiB };
        // candidates 0 and 1 (same heap) throw; without rollback heap 0 would still hold 20MiB and
        // candidate 1 could not even have been reserved.
        const int idx = ggml_vk_heapres_first_fit(heaps, budgets, 3, 10 * MiB, 0x3);
        heapres_check(idx == 2, "walk fell through to the third candidate");
        heapres_check(ggml_vk_heapres_reserved(0) == 0, "failed candidates left heap 0 at zero");
        heapres_check(ggml_vk_heapres_reserved(1) == 10 * MiB, "winning candidate holds exactly one reservation");
        heapres_check(ggml_vk_heapres_held_count() == 1, "exactly one reservation is owned");

        // 4. destruction releases exactly once, and a second destruction is a no-op.
        ggml_vk_heapres_destroy_held();
        heapres_check(ggml_vk_heapres_reserved(1) == 0, "destruction released the reservation");
        ggml_vk_heapres_destroy_held();
        heapres_check(ggml_vk_heapres_reserved(1) == 0, "second destruction released nothing more");

        // every candidate refused: nothing held, nothing reserved.
        ggml_vk_heapres_reset();
        const int none = ggml_vk_heapres_first_fit(heaps, budgets, 3, 10 * MiB, 0x7);
        heapres_check(none == -1, "all candidates failing yields no winner");
        heapres_check(ggml_vk_heapres_reserved(0) == 0 && ggml_vk_heapres_reserved(1) == 0,
                      "all candidates failing left the ledger empty");
        heapres_check(ggml_vk_heapres_held_count() == 0, "all candidates failing held nothing");
    }

    // 5. double release / underflow is refused and leaves the ledger intact.
    ggml_vk_heapres_reset();
    heapres_check(ggml_vk_heapres_reserve(2, 32 * MiB, 64 * MiB), "reserved 32MiB on heap 2");
    heapres_check(ggml_vk_heapres_release(2, 32 * MiB), "first release accepted");
    heapres_check(!ggml_vk_heapres_release(2, 32 * MiB), "second release refused");
    heapres_check(ggml_vk_heapres_reserved(2) == 0, "double release did not corrupt the counter");
    heapres_check(ggml_vk_heapres_reserve(2, 16 * MiB, 64 * MiB), "reserved 16MiB on heap 2");
    heapres_check(!ggml_vk_heapres_release(2, 17 * MiB), "over-release refused");
    heapres_check(ggml_vk_heapres_reserved(2) == 16 * MiB, "over-release left the counter untouched");

    // 6. two memory types on one heap share one counter.
    // (the ledger is keyed by heapIndex; types 3 and 7 reporting heap 3 is the case this models)
    ggml_vk_heapres_reset();
    heapres_check(ggml_vk_heapres_reserve(3, 40 * MiB, 64 * MiB), "type A on heap 3 reserved");
    heapres_check(!ggml_vk_heapres_reserve(3, 40 * MiB, 64 * MiB), "type B on heap 3 sees type A's bytes");
    heapres_check(ggml_vk_heapres_reserved(3) == 40 * MiB, "one counter for both types");

    // 7. separate heaps are independent.
    ggml_vk_heapres_reset();
    heapres_check(ggml_vk_heapres_reserve(4, 64 * MiB, 64 * MiB), "heap 4 filled");
    heapres_check(ggml_vk_heapres_reserve(5, 64 * MiB, 64 * MiB), "heap 5 unaffected by heap 4");
    heapres_check(ggml_vk_heapres_reserved(4) == 64 * MiB && ggml_vk_heapres_reserved(5) == 64 * MiB,
                  "per-heap counters are independent");

    // 8. arithmetic overflow fails closed.
    ggml_vk_heapres_reset();
    heapres_check(ggml_vk_heapres_reserve(6, UINT64_MAX - 1, UINT64_MAX), "UINT64_MAX-1 reserved");
    heapres_check(!ggml_vk_heapres_reserve(6, 2, UINT64_MAX), "2 more refused instead of wrapping");
    heapres_check(ggml_vk_heapres_reserved(6) == UINT64_MAX - 1, "overflow attempt left the counter intact");
    ggml_vk_heapres_reset();
    heapres_check(!ggml_vk_heapres_reserve(7, UINT64_MAX, 1 * MiB), "UINT64_MAX against a small budget refused");

    // out-of-range heap fails closed.
    heapres_check(!ggml_vk_heapres_reserve(4096, 1, UINT64_MAX), "out-of-range heap refused");

    // 9. R46b B7a-R2 (finding 3): two ledgers with the SAME numeric heap index are isolated.
    // This models two Vulkan devices that both report heap 0/1; the production ledgers are
    // vk_device_struct members, one per device, which is exactly what this exercises.
    ggml_vk_heapres_reset();
    heapres_check(ggml_vk_heapres_two_ledgers_isolated(0, 64 * MiB, 64 * MiB),
                  "same heap index on two ledgers: no cross-talk, no cross-release");
    heapres_check(ggml_vk_heapres_two_ledgers_isolated(1, 4 * MiB, 4 * MiB),
                  "same heap index on two ledgers: holds for heap 1 too");

    ggml_vk_heapres_reset();
    return 0;
}

// R46b B7a-R2 (finding 4): post-allocation cleanup, through the PRODUCTION allocator.
//
// A fault is injected inside ggml_vk_create_buffer() after allocateMemory / mapMemory /
// bindBufferMemory / getBufferAddress. Each must leave the device ledger exactly where it started
// AND must actually free the VkDeviceMemory. The ledger equality is the direct assertion; the leak
// probe at the end is a black-box check of the driver-side free that the ledger cannot see.
static void heapres_fault_tests() {
    printf("Vulkan post-allocation cleanup, production path (R46b B7a-R2)\n");

    const uint64_t MiB  = 1024ull * 1024ull;
    const uint64_t size = 64 * MiB;

    uint64_t before = 0, after = 0, usage = 0;
    int mapped = 0, threw = 0, bda_supported = 0, bda_executed = 0;

    // stage 0: no fault. Establishes that the probe works, that the allocation really is
    // host-visible and really gets mapped (otherwise the after-map injection below would be a
    // vacuous pass), and that ordinary destruction releases the reservation.
    const int rc = ggml_vk_heapres_fault_probe(0, size, 1, &before, &after, &mapped, &threw, &usage,
                                               &bda_supported, &bda_executed);
    if (rc != 0) {
        printf("  SKIPPED: no Vulkan device, post-allocation cleanup NOT covered by this run\n");
        return;
    }
    heapres_check(threw == 0, "no fault: buffer created");
    heapres_check(mapped == 1, "no fault: allocation was host-visible and really mapped");
    heapres_check(after == before, "no fault: destruction released the reservation");

    // R46b B7a-R3 (finding 2): BDA coverage is claimed only against a runtime witness that
    // device.getBufferAddress() really executed. On a device that reports no bufferDeviceAddress
    // the stage-4 assertions below still run - they cover post-`buf->size` cleanup, which is what
    // proves the guard and ~vk_buffer_struct cannot both free - but the BDA-named claim is
    // reported UNAVAILABLE instead of being silently counted as green.
    const bool bda_covered = (bda_supported == 1);
    if (bda_covered) {
        heapres_check(bda_executed == 1,
                      "no fault: device reports bufferDeviceAddress AND getBufferAddress really executed");
    } else {
        printf("  UNAVAILABLE: device reports no bufferDeviceAddress; getBufferAddress never runs here,\n");
        printf("               so stage 4 below covers post-size cleanup ONLY, not the BDA path\n");
    }

    // R46b B7a-R3 (finding 1): ordering. The ledger must still count this buffer's bytes at the
    // instant vkFreeMemory is called, i.e. capacity is released only after the driver has the
    // memory back. An "after == before" check cannot see this; the witness can.
    {
        uint64_t ob = 0, alive = 0, at_free = 0, oa = 0;
        heapres_check(ggml_vk_heapres_destroy_order_probe(size, &ob, &alive, &at_free, &oa) == 0,
                      "destroy order: probe ran");
        heapres_check(alive > ob, "destroy order: a live buffer holds its bytes in the ledger");
        heapres_check(at_free == alive,
                      "destroy order: ledger still counted the bytes when vkFreeMemory was called");
        heapres_check(at_free != oa,
                      "destroy order: capacity was NOT released before the driver memory was freed");
        heapres_check(oa == ob, "destroy order: released exactly once afterwards");
    }

    const bool have_driver_usage = (usage != UINT64_MAX);
    if (!have_driver_usage) {
        printf("  NOTE: VK_EXT_memory_budget unavailable - the driver-side leak assertion below is skipped\n");
    }

    struct { int stage; const char * name; } stages[] = {
        { 1, "after allocateMemory"   },
        { 2, "after mapMemory"        },
        { 3, "after bindBufferMemory" },
        { 4, "after getBufferAddress" },
    };

    for (const auto & s : stages) {
        before = after = 0;
        mapped = threw = bda_executed = 0;
        heapres_check(ggml_vk_heapres_fault_probe(s.stage, size, 1, &before, &after, &mapped, &threw, &usage,
                                                  &bda_supported, &bda_executed) == 0,
                      (std::string("fault ") + s.name + ": probe ran").c_str());
        heapres_check(threw == 1, (std::string("fault ") + s.name + ": create_buffer threw").c_str());
        heapres_check(after == before,
                      (std::string("fault ") + s.name + ": ledger rolled back exactly").c_str());
        if (s.stage >= 2) {
            heapres_check(mapped == 1,
                          (std::string("fault ") + s.name + ": the map really happened first").c_str());
        }
        if (s.stage == 4) {
            if (bda_covered) {
                heapres_check(bda_executed == 1,
                              "fault after getBufferAddress: getBufferAddress really executed before the fault");
            } else {
                printf("  UNAVAILABLE: fault after getBufferAddress ran, but BDA is unsupported here -\n");
                printf("               this stage proves post-size cleanup only\n");
            }
        }
    }

    // Leak probe. The ledger cannot detect a leaked VkDeviceMemory - it rolls back correctly
    // either way - so the instrument is the DRIVER's own accounting (VK_EXT_memory_budget
    // heapUsage), read outside the allocator. Repeatedly failing right after allocateMemory must
    // leave the driver's in-use bytes where they started; without the cleanup they climb by the
    // full cumulative size.
    //
    // (Driver refusal is deliberately NOT the instrument: this driver happily hands out many GB of
    // never-touched allocations, so an out-of-memory check here would pass while leaking.)
    {
        const uint64_t leak_size  = 256 * MiB;
        const int      iterations = 48;   // 12 GiB cumulative if leaked
        bool           all_ran    = true;
        uint64_t       usage_before = 0, usage_after = 0;
        {
            uint64_t b = 0, a = 0;
            int m = 0, t = 0, bs = 0, be = 0;
            ggml_vk_heapres_fault_probe(1, leak_size, 1, &b, &a, &m, &t, &usage_before, &bs, &be);
        }
        for (int i = 0; i < iterations; i++) {
            uint64_t b = 0, a = 0;
            int m = 0, t = 0, bs = 0, be = 0;
            if (ggml_vk_heapres_fault_probe(1, leak_size, 1, &b, &a, &m, &t, &usage_after, &bs, &be) != 0 || t != 1 || a != b) {
                all_ran = false;
                break;
            }
        }
        heapres_check(all_ran, "leak probe: 48 injected post-allocation failures all rolled back");

        if (have_driver_usage) {
            const uint64_t grew = usage_after > usage_before ? usage_after - usage_before : 0;
            heapres_check(grew < leak_size,
                          "leak probe: driver in-use bytes did not grow (the memory was really freed)");
            printf("    driver heapUsage %llu -> %llu (leak would be ~%llu)\n",
                   (unsigned long long) usage_before, (unsigned long long) usage_after,
                   (unsigned long long) (leak_size * iterations));
        }

        uint64_t b = 0, a = 0, u = 0;
        int m = 0, t = 0, bs = 0, be = 0;
        heapres_check(ggml_vk_heapres_fault_probe(0, leak_size, 1, &b, &a, &m, &t, &u, &bs, &be) == 0 && t == 0,
                      "leak probe: a clean allocation still succeeds afterwards");
        heapres_check(a == b, "leak probe: ledger back to its starting value");
    }
}

// ---------------------------------------------------------------------------------------------
// R46b B7b — per-buffer-type batch allocation planning.
//
// Half 1 drives vk_heap_ledger::reserve_plan() on the TEST's ledger: candidate backtracking,
// shared heap types, separate heaps, overflow, atomicity, the search bound.
// Half 2 drives the PRODUCTION caller, ggml_backend_alloc_ctx_tensors_from_buft(), the same
// function a model load allocates its weight buffers through - once per (buffer type, context)
// group. The unit under test is therefore ONE such batch, never a whole model load.
//
// R46b B7b-R4: the five printed labels and check strings below (half 1's banner, and half 2's
// banner, skip line and two checks) previously read "whole-load"/"whole load" and were inaccurate.
// R4 corrected them and updated the matching quotations in OPUS-R46-B7B-R2-REPORT.md. Read them as
// follows: half 1's banner names a SYNTHETIC ledger exercise that allocates nothing at all, and
// half 2's strings name the one batch that case allocates - never a whole model load.
// OPUS-R46-B7B-R3-TEXT-REPORT.md §5 is historical on this point.
// ---------------------------------------------------------------------------------------------

static int heapres_plan_search(const std::vector<std::vector<std::pair<uint32_t, uint64_t>>> & items,
                               const std::vector<uint64_t> & budgets,
                               std::vector<int> & choice) {
    std::vector<uint32_t> heaps;
    std::vector<uint64_t> bytes;
    std::vector<int>      counts;
    for (const auto & item : items) {
        counts.push_back((int) item.size());
        for (const auto & c : item) {
            heaps.push_back(c.first);
            bytes.push_back(c.second);
        }
    }
    choice.assign(items.size(), -1);
    return ggml_vk_heapres_plan_search(heaps.data(), bytes.data(), counts.data(), (int) items.size(),
                                       budgets.data(), (int) budgets.size(), choice.data());
}

static void heapres_plan_ledger_tests() {
    const uint64_t MiB = 1024ull * 1024ull;
    printf("Vulkan per-buffer-type batch plan reservation (R46b B7b prerequisite)\n");

    std::vector<int> choice;

    // 1. Backtracking. Greedy would put item 0 on heap 0 (its first choice) and then item 1, whose
    //    ONLY option is heap 0, would have nowhere to go - even though a complete assignment
    //    exists. The search must undo item 0 and move it to heap 1.
    ggml_vk_heapres_reset();
    {
        const int res = heapres_plan_search({ { {0, 100 * MiB}, {1, 100 * MiB} },
                                              { {0, 100 * MiB} } },
                                            { 100 * MiB, 100 * MiB }, choice);
        heapres_check(res == HEAPRES_PLAN_FEASIBLE, "backtracking: a complete assignment was found");
        heapres_check(choice[0] == 1, "backtracking: item 0 gave up its first choice");
        heapres_check(choice[1] == 0, "backtracking: item 1 got the heap it needed");
        heapres_check(ggml_vk_heapres_reserved(0) == 100 * MiB && ggml_vk_heapres_reserved(1) == 100 * MiB,
                      "backtracking: both heaps charged exactly once");
    }

    // 2. Impossible plan: nothing is charged, so the load can be refused with the ledger intact.
    ggml_vk_heapres_reset();
    {
        const int res = heapres_plan_search({ { {0, 100 * MiB} }, { {0, 100 * MiB} } },
                                            { 150 * MiB }, choice);
        heapres_check(res == HEAPRES_PLAN_INFEASIBLE, "impossible plan refused");
        heapres_check(ggml_vk_heapres_reserved(0) == 0, "impossible plan charged nothing (all or nothing)");
    }

    // 3. Shared heap types: two DIFFERENT memory types that live on ONE heap share that heap's
    //    budget. The plan must not treat a type index as if it had its own capacity.
    ggml_vk_heapres_reset();
    {
        // heapres_plan_search gives every candidate a distinct type index; both point at heap 3.
        const int res = heapres_plan_search({ { {3, 40 * MiB} }, { {3, 40 * MiB} } },
                                            { 0, 0, 0, 64 * MiB }, choice);
        heapres_check(res == HEAPRES_PLAN_INFEASIBLE, "shared heap: two types cannot each spend the heap");
        heapres_check(ggml_vk_heapres_reserved(3) == 0, "shared heap: refusal charged nothing");
    }

    // 4. Separate heaps stay independent.
    ggml_vk_heapres_reset();
    {
        const int res = heapres_plan_search({ { {0, 40 * MiB} }, { {1, 40 * MiB} } },
                                            { 64 * MiB, 64 * MiB }, choice);
        heapres_check(res == HEAPRES_PLAN_FEASIBLE, "separate heaps: both placed");
        heapres_check(ggml_vk_heapres_reserved(0) == 40 * MiB && ggml_vk_heapres_reserved(1) == 40 * MiB,
                      "separate heaps: each heap charged its own item only");
    }

    // 5. Already-live reservations are respected: a plan cannot spend bytes another buffer holds.
    ggml_vk_heapres_reset();
    heapres_check(ggml_vk_heapres_reserve(0, 60 * MiB, 100 * MiB), "live reservation taken first");
    {
        const int res = heapres_plan_search({ { {0, 60 * MiB} } }, { 100 * MiB }, choice);
        heapres_check(res == HEAPRES_PLAN_INFEASIBLE, "plan refused against a live reservation");
        heapres_check(ggml_vk_heapres_reserved(0) == 60 * MiB, "refusal left the live reservation alone");
    }

    // 6. Overflow fails closed, both directions.
    ggml_vk_heapres_reset();
    {
        const int res = heapres_plan_search({ { {0, UINT64_MAX} } }, { 1 * MiB }, choice);
        heapres_check(res == HEAPRES_PLAN_INFEASIBLE, "UINT64_MAX item against a small budget refused");
        heapres_check(ggml_vk_heapres_reserved(0) == 0, "overflow attempt charged nothing");
    }
    ggml_vk_heapres_reset();
    heapres_check(ggml_vk_heapres_reserve(0, UINT64_MAX - 1, UINT64_MAX), "heap 0 filled to UINT64_MAX-1");
    {
        const int res = heapres_plan_search({ { {0, 2} } }, { UINT64_MAX }, choice);
        heapres_check(res == HEAPRES_PLAN_INFEASIBLE, "2 more refused instead of wrapping");
        heapres_check(ggml_vk_heapres_reserved(0) == UINT64_MAX - 1, "wrap attempt left the counter intact");
    }

    // 7. Out-of-range heap fails closed rather than indexing past the ledger.
    ggml_vk_heapres_reset();
    {
        const int res = heapres_plan_search({ { {4096, 1} } }, { 1 * MiB }, choice);
        heapres_check(res == HEAPRES_PLAN_INFEASIBLE, "out-of-range heap candidate refused");
    }

    // 8. Search bound: 30 free items followed by one that can never be placed makes the exhaustive
    //    walk enormous. The PLANNER must answer INDETERMINATE (decide nothing, charge nothing), not
    //    a refusal: "I did not finish deciding" and "no assignment exists" are different facts and
    //    the ledger must not conflate them.
    //
    //    R46b B7b-R2 (check finding 5): what the CALLER does with INDETERMINATE is a separate
    //    policy and is not "allocate unplanned" for a multi-buffer set - ggml-alloc.c refuses
    //    that case before the first allocation rather than placing part of a set it cannot complete
    //    (see test-alloc.cpp: test_plan_indeterminate_never_partially_allocates). A single-buffer
    //    allocation still falls through unchanged.
    ggml_vk_heapres_reset();
    {
        std::vector<std::vector<std::pair<uint32_t, uint64_t>>> items;
        for (int i = 0; i < 30; i++) {
            // distinct sizes, so these items are NOT interchangeable and the symmetry break below
            // cannot collapse them - this is the genuine worst case
            items.push_back({ {0, (uint64_t) (i + 1)}, {1, (uint64_t) (i + 1)} });
        }
        items.push_back({ {2, 1} });   // heap 2 has zero budget: unplaceable
        const int res = heapres_plan_search(items, { 1 * MiB, 1 * MiB, 0 }, choice);
        heapres_check(res == HEAPRES_PLAN_INDETERMINATE, "search bound reported INDETERMINATE, not a refusal");
        heapres_check(ggml_vk_heapres_reserved(0) == 0 && ggml_vk_heapres_reserved(1) == 0,
                      "abandoned search charged nothing");
    }

    // 9. The same shape with INTERCHANGEABLE items must still be DECIDED. A real load hands the
    //    planner dozens of identical weight buffers; without symmetry breaking that is the same
    //    combinatorial explosion as case 8 and the feature would never decide anything on a real
    //    model. (Found by the production test below, not by inspection.)
    ggml_vk_heapres_reset();
    {
        std::vector<std::vector<std::pair<uint32_t, uint64_t>>> items;
        for (int i = 0; i < 40; i++) {
            items.push_back({ {0, 1 * MiB}, {1, 1 * MiB} });
        }
        const int res = heapres_plan_search(items, { 8 * MiB, 8 * MiB }, choice);
        heapres_check(res == HEAPRES_PLAN_INFEASIBLE,
                      "40 interchangeable items over 2 heaps: decided, not abandoned");
        heapres_check(ggml_vk_heapres_reserved(0) == 0 && ggml_vk_heapres_reserved(1) == 0,
                      "interchangeable refusal charged nothing");
    }
    ggml_vk_heapres_reset();
    {
        std::vector<std::vector<std::pair<uint32_t, uint64_t>>> items;
        for (int i = 0; i < 12; i++) {
            items.push_back({ {0, 1 * MiB}, {1, 1 * MiB} });
        }
        const int res = heapres_plan_search(items, { 8 * MiB, 8 * MiB }, choice);
        heapres_check(res == HEAPRES_PLAN_FEASIBLE,
                      "12 interchangeable items spill onto the second heap");
        heapres_check(ggml_vk_heapres_reserved(0) == 8 * MiB && ggml_vk_heapres_reserved(1) == 4 * MiB,
                      "interchangeable spill charged 8MiB + 4MiB, exactly once each");
    }

    // 10. The real model-load shape: many buffers of DIFFERENT sizes that together do not fit.
    //     Symmetry breaking cannot help here (nothing is interchangeable), so without the
    //     total-capacity precheck this walk hits its bound and answers INDETERMINATE - which now
    //     turns the commonest real case (model bigger than memory) into a refusal with no
    //     explanation instead of a decided, reported infeasibility. It must be DECIDED.
    ggml_vk_heapres_reset();
    {
        std::vector<std::vector<std::pair<uint32_t, uint64_t>>> items;
        for (int i = 0; i < 30; i++) {
            const uint64_t bytes = (uint64_t) (i + 1) * MiB;   // 1..30 MiB, all distinct
            items.push_back({ {0, bytes}, {1, bytes} });
        }
        const int res = heapres_plan_search(items, { 64 * MiB, 64 * MiB }, choice);   // 128 < 465
        heapres_check(res == HEAPRES_PLAN_INFEASIBLE,
                      "unequal buffers past total capacity: DECIDED, not abandoned");
        heapres_check(ggml_vk_heapres_reserved(0) == 0 && ggml_vk_heapres_reserved(1) == 0,
                      "capacity refusal charged nothing");
    }
    // ... and the same unequal shape that DOES fit must still be planned, not refused.
    ggml_vk_heapres_reset();
    {
        std::vector<std::vector<std::pair<uint32_t, uint64_t>>> items;
        for (int i = 0; i < 8; i++) {
            const uint64_t bytes = (uint64_t) (i + 1) * MiB;   // 36 MiB total
            items.push_back({ {0, bytes}, {1, bytes} });
        }
        const int res = heapres_plan_search(items, { 24 * MiB, 24 * MiB }, choice);
        heapres_check(res == HEAPRES_PLAN_FEASIBLE, "unequal buffers that fit are planned");
        heapres_check(ggml_vk_heapres_reserved(0) + ggml_vk_heapres_reserved(1) == 36 * MiB,
                      "unequal plan charged its total exactly once");
    }

    ggml_vk_heapres_reset();
}

// ---------------------------------------------------------------------------------------------
// R46b B7c - bounded host split and the staging reserve.
//
// Synthetic ledger exercise: no device, no model, no allocation. A candidate here is
// (heap, bytes, host), where host marks a placement on memory that is NOT DEVICE_LOCAL.
// ---------------------------------------------------------------------------------------------

struct heapres_split_cand {
    uint32_t heap;
    uint64_t bytes;
    bool     host;
};

static int heapres_split_search(const std::vector<std::vector<heapres_split_cand>> & items,
                                const std::vector<uint64_t> & budgets,
                                uint64_t host_split_max,
                                std::vector<int> & choice,
                                uint64_t & host_bytes) {
    std::vector<uint32_t> heaps;
    std::vector<uint64_t> bytes;
    std::vector<int>      host;
    std::vector<int>      counts;
    for (const auto & item : items) {
        counts.push_back((int) item.size());
        for (const auto & c : item) {
            heaps.push_back(c.heap);
            bytes.push_back(c.bytes);
            host.push_back(c.host ? 1 : 0);
        }
    }
    choice.assign(items.size(), -1);
    host_bytes = 0;
    return ggml_vk_heapres_plan_search_split(heaps.data(), bytes.data(), host.data(), counts.data(),
                                             (int) items.size(), budgets.data(), (int) budgets.size(),
                                             host_split_max, choice.data(), &host_bytes);
}

static void heapres_host_split_tests() {
    const uint64_t MiB = 1024ull * 1024ull;
    printf("Vulkan bounded host split + staging reserve (R46b B7c)\n");

    std::vector<int> choice;
    uint64_t host_bytes = 0;

    // Three 8 MiB buffers; heap 0 is DEVICE_LOCAL and holds exactly one of them, heap 1 is
    // host-visible and could hold all three. What stops it is the bound, and only the bound.
    const std::vector<std::vector<heapres_split_cand>> three_buffers = {
        { {0, 8 * MiB, false}, {1, 8 * MiB, true} },
        { {0, 8 * MiB, false}, {1, 8 * MiB, true} },
        { {0, 8 * MiB, false}, {1, 8 * MiB, true} },
    };
    const std::vector<uint64_t> budgets = { 8 * MiB, 100 * MiB };

    // 1. Exactly at the bound: 16 MiB of split admitted when the bound is 16 MiB.
    ggml_vk_heapres_reset();
    {
        const int res = heapres_split_search(three_buffers, budgets, 16 * MiB, choice, host_bytes);
        heapres_check(res == HEAPRES_PLAN_FEASIBLE, "split exactly at the bound is admitted");
        heapres_check(host_bytes == 16 * MiB, "split reports exactly the bytes it spilled");
        heapres_check(ggml_vk_heapres_reserved(0) == 8 * MiB && ggml_vk_heapres_reserved(1) == 16 * MiB,
                      "split charged 8MiB device + 16MiB host, once each");
        // Deterministic spill selection: plan-order first-fit. Device candidates come first for
        // every buffer, so the buffers that spill are the SUFFIX of plan order.
        heapres_check(choice[0] == 0 && choice[1] == 1 && choice[2] == 1,
                      "spill selection is plan-order first-fit: the suffix spills");
    }

    // 2. One byte over the bound: refused, and nothing is charged anywhere.
    ggml_vk_heapres_reset();
    {
        const int res = heapres_split_search(three_buffers, budgets, 16 * MiB - 1, choice, host_bytes);
        heapres_check(res == HEAPRES_PLAN_INFEASIBLE, "one byte past the bound is refused");
        heapres_check(ggml_vk_heapres_reserved(0) == 0 && ggml_vk_heapres_reserved(1) == 0,
                      "a refused split charged nothing");
        heapres_check(host_bytes == 0, "a refused split reports no host bytes");
    }

    // 3. Bound 0 forbids host placement outright: the set that needed a split is refused even
    //    though the host heap has room.
    ggml_vk_heapres_reset();
    {
        const int res = heapres_split_search(three_buffers, budgets, 0, choice, host_bytes);
        heapres_check(res == HEAPRES_PLAN_INFEASIBLE, "bound 0 forbids any host placement");
        heapres_check(ggml_vk_heapres_reserved(1) == 0, "bound 0 left the host heap untouched");
    }

    // 4. The staging reserve precedes admission. The same batch and the same DEFAULT bound shape
    //    (free bytes on the host heap), with the reserve already charged: the weights can no longer
    //    reach into it, and the reserve survives the refusal intact.
    ggml_vk_heapres_reset();
    {
        heapres_check(ggml_vk_heapres_reserve(1, 90 * MiB, 100 * MiB), "staging reserve charged first");
        const uint64_t free_after_reserve = 100 * MiB - 90 * MiB;
        const int res = heapres_split_search(three_buffers, budgets, free_after_reserve, choice, host_bytes);
        heapres_check(res == HEAPRES_PLAN_INFEASIBLE, "weights cannot be admitted into the staging reserve");
        heapres_check(ggml_vk_heapres_reserved(1) == 90 * MiB, "the reserve is intact after the refusal");
    }
    //    ... and with the reserve small enough, the same batch is admitted and stays clear of it.
    ggml_vk_heapres_reset();
    {
        heapres_check(ggml_vk_heapres_reserve(1, 20 * MiB, 100 * MiB), "smaller staging reserve charged first");
        const int res = heapres_split_search(three_buffers, budgets, 100 * MiB - 20 * MiB, choice, host_bytes);
        heapres_check(res == HEAPRES_PLAN_FEASIBLE, "a split that fits beside the reserve is admitted");
        heapres_check(ggml_vk_heapres_reserved(1) == 20 * MiB + 16 * MiB,
                      "reserve plus split, both charged, neither consuming the other");
    }

    // 5. A batch whose candidates are all DEVICE_LOCAL is decided exactly as B7b decided it: no
    //    candidate is a host split, so the bound - even 0 - cannot alter the assignment or the
    //    charges. This is the candidate SHAPE the bulk-large-heap policy contributes; it is NOT that
    //    policy's production path, which prepends its attempt and then falls through to the stock
    //    chain (host-visible flag sets included). That path cannot be driven from a test at all:
    //    ggml_vk_placement_bulk_large_heap() caches its answer in a function-local static, so the
    //    environment cannot flip it after the first call in this process.
    ggml_vk_heapres_reset();
    {
        std::vector<std::vector<std::pair<uint32_t, uint64_t>>> plain;
        std::vector<std::vector<heapres_split_cand>>           split;
        for (int i = 0; i < 6; i++) {
            const uint64_t bytes = (uint64_t) (i + 1) * MiB;
            plain.push_back({ {0, bytes}, {1, bytes} });
            split.push_back({ {0, bytes, false}, {1, bytes, false} });
        }
        std::vector<int> plain_choice;
        const int res_plain = heapres_plan_search(plain, { 12 * MiB, 12 * MiB }, plain_choice);
        const uint64_t plain_0 = ggml_vk_heapres_reserved(0);
        const uint64_t plain_1 = ggml_vk_heapres_reserved(1);

        ggml_vk_heapres_reset();
        const int res_split = heapres_split_search(split, { 12 * MiB, 12 * MiB }, 0, choice, host_bytes);
        heapres_check(res_plain == res_split, "device-only candidates: same verdict with the bound on");
        heapres_check(plain_choice == choice, "device-only candidates: identical assignment");
        heapres_check(plain_0 == ggml_vk_heapres_reserved(0) && plain_1 == ggml_vk_heapres_reserved(1),
                      "device-only candidates: identical charges");
        heapres_check(host_bytes == 0, "device-only candidates: no host bytes reported");
    }

    // 6. Host-ness is part of what makes two buffers interchangeable. Both items here have the same
    //    heaps and the same sizes in the same positions, but opposite host flags, and the ONLY
    //    assignment inside the bound needs item 1 to choose position 0 while item 0 chose position 1.
    //    A symmetry break that ignored the host flag would call them interchangeable, forbid that,
    //    and report a refusal it never proved.
    ggml_vk_heapres_reset();
    {
        const std::vector<std::vector<heapres_split_cand>> mixed = {
            { {0, 4 * MiB, true},  {1, 4 * MiB, false} },
            { {0, 4 * MiB, false}, {1, 4 * MiB, true}  },
        };
        const int res = heapres_split_search(mixed, { 4 * MiB, 4 * MiB }, 4 * MiB, choice, host_bytes);
        heapres_check(res == HEAPRES_PLAN_FEASIBLE, "differing host-ness is not interchangeable");
        heapres_check(choice[0] == 1 && choice[1] == 0, "both buffers took the device-local option");
        heapres_check(host_bytes == 0, "the assignment inside the bound spilled nothing");
    }

    // 7. The staging reserve releases exactly once. `other` is a second live reservation on the same
    //    heap: a second release would be large enough to succeed against it and would steal it, so
    //    this cannot be satisfied by the ledger's underflow refusal alone.
    ggml_vk_heapres_reset();
    {
        int first_ok = 0, second_ok = 0;
        uint64_t after_first = 0, after_second = 0;
        ggml_vk_heapres_staging_exact_once(5, 256 * MiB, 512 * MiB, 1024 * MiB,
                                           &first_ok, &second_ok, &after_first, &after_second);
        heapres_check(first_ok == 1, "staging reserve released once");
        heapres_check(after_first == 512 * MiB, "release gave back exactly the reserve");
        heapres_check(second_ok == 0, "a second release is refused");
        heapres_check(after_second == 512 * MiB, "a second release stole nothing");
    }

    ggml_vk_heapres_reset();

    // 8. The REAL device: the reserve is charged during device creation, before any buffer type has
    //    planned or allocated. This is the first probe in the suite that touches a device.
    {
        uint64_t bytes = 0, reserved_on_heap = 0, heap_size = 0;
        uint32_t heap = UINT32_MAX;
        if (ggml_vk_staging_reserve_probe(&bytes, &heap, &reserved_on_heap, &heap_size) == 0) {
            heapres_check(bytes > 0, "device staging reserve is non-zero by default");
            heapres_check(heap != UINT32_MAX, "device staging reserve names a host-visible heap");
            heapres_check(bytes <= heap_size, "device staging reserve fits its heap");
            heapres_check(reserved_on_heap >= bytes,
                          "the reserve is already charged before any bulk admission");
            printf("  note staging reserve = %llu B on heap %u (heap size %llu B)\n",
                   (unsigned long long) bytes, heap, (unsigned long long) heap_size);
        } else {
            printf("  note no Vulkan device: device staging reserve probe skipped\n");
        }
    }
}

// Builds `n` meta tensors and lets the loader's own split arithmetic turn them into buffers.
//
// `vary` controls the SHAPE of the resulting buffer set, and the distinction matters:
//   vary == false - every tensor is exactly `chunk`, so every buffer is identical. Interchangeable,
//                   which is the easy case for the planner.
//   vary == true  - the sizes cycle, so the buffers come out UNEQUAL. That is what a real model
//                   load looks like (tensors are packed until the next one would overflow the
//                   buffer), and unequal buffers are exactly the case a symmetry break cannot help
//                   with. A corpus of same-size buffers would hide that.
static ggml_context * heapres_plan_make_ctx(uint64_t chunk, int n, bool vary) {
    struct ggml_init_params params = {
        /* .mem_size   = */ ggml_tensor_overhead() * (size_t) (n + 2),
        /* .mem_buffer = */ NULL,
        /* .no_alloc   = */ true,
    };
    ggml_context * ctx = ggml_init(params);
    if (ctx == NULL) {
        return NULL;
    }
    for (int i = 0; i < n; i++) {
        uint64_t bytes = chunk;
        if (vary) {
            switch (i % 3) {
                case 1:  bytes = chunk * 3 / 4; break;
                case 2:  bytes = chunk / 2;     break;
                default: bytes = chunk;         break;
            }
        }
        ggml_new_tensor_1d(ctx, GGML_TYPE_F32, (int64_t) (bytes / sizeof(float)));
    }
    return ctx;
}

static void heapres_plan_production_tests() {
    printf("Vulkan per-buffer-type batch planning, production path (R46b B7b prerequisite)\n");

    const uint64_t MiB = 1024ull * 1024ull;

    uint64_t max_chunk = 0, total_heap = 0, reserved_now = 0;
    int      used_m4 = 0;
    if (ggml_vk_plan_probe_limits(&max_chunk, &total_heap, &reserved_now, &used_m4) != 0) {
        printf("  SKIPPED: no Vulkan device, per-buffer-type batch planning NOT covered by this run\n");
        return;
    }
    printf("    buffer chunk=%llu B, all heaps=%llu B, requirements via %s\n",
           (unsigned long long) max_chunk, (unsigned long long) total_heap,
           used_m4 ? "VK_KHR_maintenance4 (no VkBuffer created)" : "a temporary VkBuffer");

    ggml_backend_buffer_type_t buft = ggml_backend_vk_buffer_type(0);
    heapres_check(buft != NULL, "production Vulkan buffer type available");
    if (buft == NULL || max_chunk == 0) {
        return;
    }

    // 1. An impossible COMPLETE plan must fail before the FIRST driver allocation. A NULL return
    //    does not prove that; the vkAllocateMemory counter does.
    //
    //    The buffers here are deliberately UNEQUAL (vary = true), which is the real model-load
    //    shape and the one the backtracking walk is worst at. An equal-size corpus would pass this
    //    test through the symmetry break and never exercise the path a real model takes.
    {
        uint64_t n64 = (total_heap / max_chunk) * 2 + 8;
        if (n64 > 2048) {
            n64 = 2048;   // still far past capacity on any heap layout this rig reports
        }
        const int n = (int) n64;
        ggml_context * ctx = heapres_plan_make_ctx(max_chunk, n, /* vary */ true);
        heapres_check(ctx != NULL, "impossible load: context built");
        if (ctx != NULL) {
            const uint64_t allocs_before   = ggml_vk_plan_driver_alloc_calls();
            const uint64_t reserved_before = ggml_vk_plan_device_reserved();
            ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors_from_buft(ctx, buft);
            heapres_check(buf == NULL, "impossible load: refused");
            heapres_check(ggml_vk_plan_driver_alloc_calls() == allocs_before,
                          "impossible load: NOT ONE vkAllocateMemory was issued");
            heapres_check(ggml_vk_plan_device_reserved() == reserved_before,
                          "impossible load: the device ledger is untouched");
            ggml_backend_buffer_free(buf);
            ggml_free(ctx);

            // R46b B7b-R2: refusing is not enough - the planner must have DECIDED that this set is
            // infeasible, not merely run out of search budget. The two answers refuse the same load
            // today but they are different facts: "no complete assignment exists" is reportable and
            // stable, "I gave up" is neither, and the O(n) total-capacity precheck is what turns the
            // commonest real shape (model bigger than memory, all buffers unequal) into the former.
            std::vector<uint64_t> unequal((size_t) n, max_chunk);
            for (size_t i = 0; i < unequal.size(); i++) {
                switch (i % 3) {
                    case 1:  unequal[i] = max_chunk * 3 / 4; break;
                    case 2:  unequal[i] = max_chunk / 2;     break;
                    default: unequal[i] = max_chunk;         break;
                }
            }
            int status_impossible = -1;
            ggml_vk_plan_probe_begin(unequal.data(), (int) unequal.size(), &status_impossible);
            heapres_check(status_impossible == HEAPRES_PLAN_INFEASIBLE,
                          "impossible load: DECIDED infeasible, not abandoned at the search bound");
            ggml_vk_plan_probe_free_all();
        }
    }

    // 2. A load that fits is charged exactly once per buffer and gives all of it back.
    {
        const int n = 2;
        // equal sizes here on purpose: this is the one assertion that checks exact byte arithmetic
        ggml_context * ctx = heapres_plan_make_ctx(max_chunk, n, /* vary */ false);
        heapres_check(ctx != NULL, "feasible load: context built");
        if (ctx != NULL) {
            const uint64_t reserved_before = ggml_vk_plan_device_reserved();
            ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors_from_buft(ctx, buft);
            heapres_check(buf != NULL, "feasible load: allocated");
            if (buf != NULL) {
                const uint64_t live = ggml_vk_plan_device_reserved() - reserved_before;
                // exactly n buffers' worth: a double charge would be ~2n, a lost charge ~0
                heapres_check(live >= (uint64_t) n * max_chunk && live < (uint64_t) n * max_chunk + 16 * MiB,
                              "feasible load: charged exactly once per buffer, no double charge");
                ggml_backend_buffer_free(buf);
            }
            heapres_check(ggml_vk_plan_device_reserved() == reserved_before,
                          "feasible load: destruction released every byte exactly once");
            ggml_free(ctx);
        }
    }

    // 3. Mid-plan failure. Buffer 0 is built, buffer 1 fails inside the production allocator. The
    //    load must leave NOTHING behind: the built buffer's own charge, the failed buffer's
    //    adopted charge and the untouched entries' reservations must all be back.
    {
        const int n = 5;
        // unequal buffers again: the rollback has to work on the real load shape, not just on a
        // set the planner finds easy
        ggml_context * ctx = heapres_plan_make_ctx(max_chunk, n, /* vary */ true);
        heapres_check(ctx != NULL, "mid-plan failure: context built");
        if (ctx != NULL) {
            const uint64_t reserved_before = ggml_vk_plan_device_reserved();
            const uint64_t allocs_before   = ggml_vk_plan_driver_alloc_calls();
            ggml_vk_plan_set_fault(1 /* after allocateMemory */, 1 /* let buffer 0 through */);
            ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors_from_buft(ctx, buft);
            ggml_vk_plan_set_fault(0, 0);
            heapres_check(buf == NULL, "mid-batch failure: the whole batch failed, not just one buffer");
            heapres_check(ggml_vk_plan_driver_alloc_calls() >= allocs_before + 2,
                          "mid-plan failure: the failure really happened part way through");
            heapres_check(ggml_vk_plan_device_reserved() == reserved_before,
                          "mid-plan failure: every reservation rolled back, consumed and unconsumed");
            ggml_backend_buffer_free(buf);
            ggml_free(ctx);
        }
    }

    // 4. Competing plans on ONE device cannot both pass against the same capacity. plan_begin
    //    allocates no driver memory at all, so this is a pure capacity test.
    {
        uint64_t n64 = (total_heap / max_chunk) * 3 / 4;
        if (n64 < 2) {
            n64 = 2;
        }
        if (n64 > 2048) {
            n64 = 2048;
        }
        const int n = (int) n64;
        std::vector<uint64_t> sizes((size_t) n, max_chunk);

        const uint64_t allocs_before = ggml_vk_plan_driver_alloc_calls();
        int status_a = -1, status_b = -1, status_b2 = -1;

        const int a = ggml_vk_plan_probe_begin(sizes.data(), n, &status_a);
        heapres_check(status_a == HEAPRES_PLAN_FEASIBLE, "competing plans: plan A took three quarters of capacity");
        ggml_vk_plan_probe_begin(sizes.data(), n, &status_b);
        heapres_check(status_b == HEAPRES_PLAN_INFEASIBLE,
                      "competing plans: plan B refused against plan A's reservations");
        ggml_vk_plan_probe_free_one(a);
        ggml_vk_plan_probe_begin(sizes.data(), n, &status_b2);
        heapres_check(status_b2 == HEAPRES_PLAN_FEASIBLE,
                      "competing plans: plan B fits once plan A is released");
        heapres_check(ggml_vk_plan_driver_alloc_calls() == allocs_before,
                      "competing plans: planning allocated no driver memory");

        const uint64_t reserved_with_b = ggml_vk_plan_device_reserved();
        ggml_vk_plan_probe_free_all();
        heapres_check(reserved_with_b > ggml_vk_plan_device_reserved(),
                      "competing plans: an open plan really held capacity");
    }

    // 5. Everything above must leave the device ledger exactly where a fresh device starts.
    uint64_t after_chunk = 0, after_total = 0, after_reserved = 0;
    int      after_m4 = 0;
    ggml_vk_plan_probe_limits(&after_chunk, &after_total, &after_reserved, &after_m4);
    heapres_check(after_reserved == reserved_now, "batch planning left no bytes reserved");
}

// ---------------------------------------------------------------------------------------------
// R58 lane-244 — MEMORY TYPE BANDWIDTH PROBE (`test-backend-ops memtype`).
//
// One number in GB/s per Vulkan memory type: how fast the GPU READS weights placed in that type.
// This is the bandwidth-envelope program's row-1 "attainable bus" probe and its row-2 per-region
// probe in one tool, and it is the measurement the host-split placement question turns on - on
// this box the beyond-reservation bytes can live in memory type 1 (HOST_VISIBLE|HOST_COHERENT) or
// type 3 (the same plus HOST_CACHED), and today's choice between them is made by enumeration
// order, never by a measurement.
//
// What is timed: an f16 mat-vec (MUL_MAT with n=1) over `bytes` of weights, i.e. exactly the
// shader path a plain decode token runs, built as an ordinary ggml graph on the ordinary Vulkan
// backend. Placement is pinned with ggml_vk_memtype_force() so every buffer of that graph lands
// in the type under test. Weights are split into chunks of at most the backend's own maximum
// buffer size - a single multi-GiB tensor would be refused by the driver, so the receipt prints
// the chunking it used.
//
// The maker never runs this: HQ runs it in a quiet window, one type at a time.
// ---------------------------------------------------------------------------------------------
static int memtype_probe_run(uint32_t type, uint32_t heap, uint32_t flags,
                             uint64_t want_bytes, uint64_t max_chunk, int rounds) {
    ggml_backend_dev_t dev = nullptr;
    for (size_t i = 0; i < ggml_backend_dev_count(); i++) {
        ggml_backend_dev_t d = ggml_backend_dev_get(i);
        if (strncmp(ggml_backend_dev_name(d), "Vulkan", 6) == 0) {
            dev = d;
            break;
        }
    }
    if (dev == nullptr) {
        printf("memtype: no Vulkan device, skipping\n");
        return 1;
    }

    // One row of the mat-vec. 4096 is a real hidden size and keeps every chunk a whole number of
    // rows, so no chunk is a ragged shape the shader would treat differently from the others.
    const int64_t K = 4096;
    const uint64_t row_bytes   = (uint64_t) K * sizeof(ggml_fp16_t);
    uint64_t       chunk_bytes = (max_chunk / row_bytes) * row_bytes;
    if (chunk_bytes == 0) {
        printf("memtype %u: max chunk %llu B is smaller than one %lld-wide f16 row, skipping\n",
               type, (unsigned long long) max_chunk, (long long) K);
        return 1;
    }
    if (chunk_bytes > want_bytes) {
        chunk_bytes = (want_bytes / row_bytes) * row_bytes;
    }
    if (chunk_bytes == 0) {
        printf("memtype %u: requested %llu B is smaller than one row, skipping\n",
               type, (unsigned long long) want_bytes);
        return 1;
    }
    const int64_t  rows_per_chunk = (int64_t) (chunk_bytes / row_bytes);
    const int      n_chunks       = (int) std::max<uint64_t>(1, want_bytes / chunk_bytes);
    const uint64_t total_bytes    = (uint64_t) n_chunks * chunk_bytes;

    printf("memtype %u: heap %u flags 0x%x, %d chunk(s) x %llu B = %llu B, %lld rows/chunk, %d rounds\n",
           type, heap, flags, n_chunks, (unsigned long long) chunk_bytes,
           (unsigned long long) total_bytes, (long long) rows_per_chunk, rounds);
    fflush(stdout);

    // The pin covers every allocation made between here and the restore below.
    ggml_vk_memtype_force((int) type);

    ggml_backend_t backend = ggml_backend_dev_init(dev, nullptr);
    if (backend == nullptr) {
        ggml_vk_memtype_force(-1);
        printf("memtype %u: backend init failed\n", type);
        return 1;
    }

    struct ggml_init_params ip = {
        /* .mem_size   = */ ggml_tensor_overhead() * (size_t) (2 * n_chunks + 8) + ggml_graph_overhead(),
        /* .mem_buffer = */ NULL,
        /* .no_alloc   = */ true,
    };
    ggml_context * ctx = ggml_init(ip);
    ggml_tensor * v = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, K, 1);
    ggml_cgraph * gf = ggml_new_graph(ctx);
    std::vector<ggml_tensor *> W((size_t) n_chunks);
    for (int c = 0; c < n_chunks; c++) {
        W[(size_t) c] = ggml_new_tensor_2d(ctx, GGML_TYPE_F16, K, rows_per_chunk);
        ggml_build_forward_expand(gf, ggml_mul_mat(ctx, W[(size_t) c], v));
    }

    ggml_backend_buffer_t buf = ggml_backend_alloc_ctx_tensors_from_buft(ctx, ggml_backend_dev_buffer_type(dev));
    if (buf == nullptr) {
        ggml_free(ctx);
        ggml_backend_free(backend);
        ggml_vk_memtype_force(-1);
        // REFUSED is a result, not a failure: a type whose heap cannot hold the span is exactly
        // what the enumeration verdict is about.
        printf("memtype %u: REFUSED - no room for %llu B in this type\n",
               type, (unsigned long long) total_bytes);
        return 0;
    }

    // Host-side fill pattern, reused for every chunk. It doubles as the CPU memcpy source, so the
    // CPU number is the memcpy of the SAME span the GPU then reads.
    std::vector<uint8_t> host_a((size_t) chunk_bytes, 0x3c);   // 0x3c3c = 1.0585938 in f16
    std::vector<uint8_t> host_b((size_t) chunk_bytes);
    for (int c = 0; c < n_chunks; c++) {
        ggml_backend_tensor_set(W[(size_t) c], host_a.data(), 0, (size_t) chunk_bytes);
    }
    std::vector<float> ones((size_t) K, 1.0f);
    ggml_backend_tensor_set(v, ones.data(), 0, (size_t) K * sizeof(float));

    double cpu_best = 0.0;
    for (int r = 0; r < rounds; r++) {
        const int64_t t0 = ggml_time_us();
        for (int c = 0; c < n_chunks; c++) {
            memcpy(host_b.data(), host_a.data(), (size_t) chunk_bytes);
        }
        const int64_t t1 = ggml_time_us();
        const double gbs = (double) total_bytes / ((double) (t1 - t0) * 1e-6) / 1e9;
        cpu_best = std::max(cpu_best, gbs);
    }

    // One discarded warm-up: the first compute compiles pipelines and fills descriptor pools, and
    // timing that would report the compile, not the bus.
    ggml_backend_graph_compute(backend, gf);

    double gpu_best = 0.0;
    for (int r = 0; r < rounds; r++) {
        const int64_t t0 = ggml_time_us();
        ggml_backend_graph_compute(backend, gf);
        const int64_t t1 = ggml_time_us();
        const double gbs = (double) total_bytes / ((double) (t1 - t0) * 1e-6) / 1e9;
        gpu_best = std::max(gpu_best, gbs);
    }

    ggml_backend_buffer_free(buf);
    ggml_free(ctx);
    ggml_backend_free(backend);
    ggml_vk_memtype_force(-1);

    ggml_vk_memtype_probe_line(type, heap, flags, total_bytes, gpu_best, cpu_best);
    return 0;
}

static int memtype_main(uint64_t want_bytes, int rounds, const char * types_csv) {
    ggml_time_init();

    uint32_t types[VK_MAX_MEMORY_TYPES_LOCAL];
    uint32_t heaps[VK_MAX_MEMORY_TYPES_LOCAL];
    uint32_t flags[VK_MAX_MEMORY_TYPES_LOCAL];
    uint64_t heap_sizes[VK_MAX_MEMORY_TYPES_LOCAL];

    const int n = ggml_vk_memtype_enumerate(types, heaps, flags, heap_sizes, VK_MAX_MEMORY_TYPES_LOCAL);
    if (n < 0) {
        printf("memtype: no Vulkan device\n");
        return 1;
    }

    uint32_t type_bits = 0;
    uint64_t max_buffer_size = 0, max_chunk = 0;
    if (ggml_vk_memtype_buffer_info(1024 * 1024, &type_bits, &max_buffer_size, &max_chunk) != 0) {
        printf("memtype: buffer requirements unavailable\n");
        return 1;
    }
    printf("memtype: %d types, storage-buffer memoryTypeBits=0x%x, max_buffer_size=%llu B, max_chunk=%llu B\n",
           n, type_bits, (unsigned long long) max_buffer_size, (unsigned long long) max_chunk);

    for (int i = 0; i < n; i++) {
        // Eligibility is measured, not assumed: a type outside memoryTypeBits can never hold one
        // of our storage buffers, so probing it would time a placement that cannot happen.
        if (!(type_bits & (1u << types[i]))) {
            printf("memtype %u: ineligible for storage buffers (outside memoryTypeBits), skipped\n", types[i]);
            continue;
        }
        if (types_csv != nullptr) {
            char want[32];
            snprintf(want, sizeof(want), "%u", types[i]);
            bool listed = false;
            for (const char * p = strstr(types_csv, want); p != nullptr; p = strstr(p + 1, want)) {
                const bool lb = (p == types_csv) || (p[-1] == ',');
                const bool rb = (p[strlen(want)] == '\0') || (p[strlen(want)] == ',');
                if (lb && rb) { listed = true; break; }
            }
            if (!listed) {
                continue;
            }
        }
        memtype_probe_run(types[i], heaps[i], flags[i], want_bytes, max_chunk, rounds);
    }
    return 0;
}

static int heapres_main() {
    heapres_ledger_tests();
    heapres_plan_ledger_tests();
    heapres_host_split_tests();
    heapres_fault_tests();
    heapres_plan_production_tests();
    printf("%s: %s\n", "heapres", heapres_failures == 0 ? "all tests passed" : "FAILURES");
    return heapres_failures == 0 ? 0 : 1;
}
#endif // GGML_USE_VULKAN

static void usage(char ** argv) {
    printf("Usage: %s [mode] [options]\n\n", argv[0]);
    printf("Valid modes:\n");
    printf("  test     (default) compare with CPU backend for correctness\n");
    printf("  grad     compare gradients from backpropagation with method of finite differences\n");
    printf("  perf     performance evaluation\n");
    printf("  support  probe backend operation support\n");
    printf("  sx8ref   S-X8 mat-vec against an independent double-precision reference\n");
#ifdef GGML_USE_VULKAN
    printf("  heapres  Vulkan per-heap reservation ledger + post-allocation cleanup\n");
    printf("      - memtype [--bytes N] [--rounds R] [--types 1,3] (GPU read GB/s per Vulkan memory type)\n");
#endif
    printf("\n");
    printf("Options:\n");
    printf("  -o <op|regex,..>            comma separated list of exact op names (as given by ggml_op_desc()),\n");
    printf("                              full test case strings, and/or regexes matched against the op name\n");
    printf("  -b <backend>                run tests on the given backend (e.g. CPU, MTL0, CUDA0)\n");
    printf("  -p <params regex>           filter test cases by a regex matched against their params\n");
    printf("  --output <console|sql|csv>  output format (default: console)\n");
    printf("  --list-ops                  list all available GGML operations\n");
    printf("  --show-coverage             show test coverage\n");
    printf("  --test-file <path>          read test operators from a test file generated by test-export-graph-ops\n");
    printf("  -j <n>                      run tests using <n> parallel worker threads (default: 1, test mode only)\n\n");
    printf("Examples:\n");
    printf("  %s -j 8\n", argv[0]);
    printf("  %s -o ADD,MUL_MAT\n", argv[0]);
    printf("  %s -o ADD -p 'type=f16.*perm1=0'\n", argv[0]);
    printf("  %s -b MTL0 -o 'DSV4.*'\n", argv[0]);
    printf("  %s -b CUDA0 -o 'ADD(type=f16,ne=[1,1,1,1],nr=[32,1,1,1],nf=1,perm1=0,src_overlap=0)'\n", argv[0]);
    printf("  %s perf -o 'MUL_MAT.*'\n", argv[0]);
}

int main(int argc, char ** argv) {
    test_mode mode = MODE_TEST;
    output_formats output_format = CONSOLE;
    const char * op_names_filter = nullptr;
    const char * backend_filter = nullptr;
    const char * params_filter = nullptr;
    const char * test_file_path = nullptr;
    int parallel_workers = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "test") == 0) {
            mode = MODE_TEST;
        } else if (strcmp(argv[i], "perf") == 0) {
            mode = MODE_PERF;
        } else if (strcmp(argv[i], "grad") == 0) {
            mode = MODE_GRAD;
        } else if (strcmp(argv[i], "support") == 0) {
            mode = MODE_SUPPORT;
        } else if (strcmp(argv[i], "sx8ref") == 0) {
            mode = MODE_SX8REF;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                op_names_filter = argv[++i];
            } else {
                usage(argv);
                return 1;
            }
        } else if (strcmp(argv[i], "-b") == 0) {
            if (i + 1 < argc) {
                backend_filter = argv[++i];
            } else {
                usage(argv);
                return 1;
            }
        } else if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 < argc) {
                params_filter = argv[++i];
            } else {
                usage(argv);
                return 1;
            }
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 < argc) {
                if (!output_format_from_str(argv[++i], output_format)) {
                    usage(argv);
                    return 1;
                }
            } else {
                usage(argv);
                return 1;
            }
#ifdef GGML_USE_VULKAN
        } else if (strcmp(argv[i], "heapres") == 0) {
            // R46b B7a: returns before any backend/device enumeration. The ledger half needs no
            // device; the fault half acquires a Vulkan device itself and skips loudly without one.
            return heapres_main();
        } else if (strcmp(argv[i], "memtype") == 0) {
            // R58 lane-244. Optional: --bytes <N> (default 4 GiB), --rounds <R> (default 3),
            // --types <csv of memory type indices> (default: every eligible type).
            uint64_t     bytes     = 4ull * 1024 * 1024 * 1024;
            int          rounds    = 3;
            const char * types_csv = nullptr;
            for (int j = i + 1; j < argc; j++) {
                if (strcmp(argv[j], "--bytes") == 0 && j + 1 < argc) {
                    bytes = strtoull(argv[++j], nullptr, 10);
                } else if (strcmp(argv[j], "--rounds") == 0 && j + 1 < argc) {
                    rounds = atoi(argv[++j]);
                } else if (strcmp(argv[j], "--types") == 0 && j + 1 < argc) {
                    types_csv = argv[++j];
                } else {
                    usage(argv);
                    return 1;
                }
            }
            if (bytes == 0 || rounds <= 0) {
                usage(argv);
                return 1;
            }
            return memtype_main(bytes, rounds, types_csv);
#endif
        } else if (strcmp(argv[i], "--list-ops") == 0) {
            list_all_ops();
            return 0;
        } else if (strcmp(argv[i], "--show-coverage") == 0) {
            show_test_coverage();
            return 0;
        } else if (strcmp(argv[i], "--test-file") == 0) {
            if (i + 1 < argc) {
                test_file_path = argv[++i];
            } else {
                usage(argv);
                return 1;
            }
        } else if (strcmp(argv[i], "-j") == 0) {
            if (i + 1 < argc) {
                parallel_workers = atoi(argv[++i]);
                if (parallel_workers < 1) {
                    usage(argv);
                    return 1;
                }
            } else {
                usage(argv);
                return 1;
            }
        } else {
            usage(argv);
            return 1;
        }
    }

    // load and enumerate backends
    ggml_backend_load_all();

    // F-109: the corpus must cover every implemented quantized type before anything is claimed
    // about it. This runs before a single case, so a missing family fails loudly, not silently.
    if (!check_all_types_coverage()) {
        printf("\033[1;31mtest-backend-ops: all_types coverage parity FAILED\033[0m\n");
        return 1;
    }

    // Create printer for output format
    std::unique_ptr<printer> output_printer = create_printer(output_format);
    if (output_printer) {
        output_printer->print_header();
    }

    output_printer->print_testing_start(testing_start_info(ggml_backend_dev_count()));

    size_t n_ok = 0;

    for (size_t i = 0; i < ggml_backend_dev_count(); i++) {
        ggml_backend_dev_t dev = ggml_backend_dev_get(i);

        if (backend_filter != NULL && strcmp(backend_filter, ggml_backend_dev_name(dev)) != 0) {
            output_printer->print_backend_init(
                backend_init_info(i, ggml_backend_dev_count(), ggml_backend_dev_name(dev), true, "Skipping"));
            n_ok++;
            continue;
        }

        if (backend_filter == NULL && ggml_backend_dev_type(dev) == GGML_BACKEND_DEVICE_TYPE_CPU && mode != MODE_GRAD) {
            output_printer->print_backend_init(backend_init_info(
                i, ggml_backend_dev_count(), ggml_backend_dev_name(dev), true, "Skipping CPU backend"));
            n_ok++;
            continue;
        }

        ggml_backend_ptr backend(ggml_backend_dev_init(dev, NULL));
        GGML_ASSERT(backend != NULL);

        ggml_backend_reg_t reg = ggml_backend_dev_backend_reg(dev);
        auto ggml_backend_set_n_threads_fn = (ggml_backend_set_n_threads_t) ggml_backend_reg_get_proc_address(reg, "ggml_backend_set_n_threads");
        if (ggml_backend_set_n_threads_fn) {
            ggml_backend_set_n_threads_fn(backend.get(), std::max<int>(1, N_THREADS/2));
        }

        size_t free, total;  // NOLINT
        ggml_backend_dev_memory(dev, &free, &total);
        output_printer->print_backend_init(backend_init_info(i, ggml_backend_dev_count(), ggml_backend_dev_name(dev),
                                                             false, "", ggml_backend_dev_description(dev),
                                                             total / 1024 / 1024, free / 1024 / 1024, true));

        bool ok = test_backend(backend.get(), dev, mode, op_names_filter, params_filter, output_printer.get(), test_file_path, parallel_workers);

        if (ok) {
            n_ok++;
        }
        output_printer->print_backend_status(
            backend_status_info(ggml_backend_name(backend.get()), ok ? test_status_t::OK : test_status_t::FAIL));
    }

    ggml_quantize_free();

    if (output_printer) {
        output_printer->print_footer();
    }

    output_printer->print_overall_summary(
        overall_summary_info(n_ok, ggml_backend_dev_count(), n_ok == ggml_backend_dev_count()));

    if (n_ok != ggml_backend_dev_count()) {
        return 1;
    }

    return 0;
}
