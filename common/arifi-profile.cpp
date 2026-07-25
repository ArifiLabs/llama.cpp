#include "arifi-profile.h"

#include "log.h"

#include <cstdlib>
#include <string>
#include <vector>

// see arifi-profile.h and docs/HARDWARE-PROFILES.md

namespace {

struct arifi_setting {
    const char * env;
    const char * value;
    const char * why;
};

struct arifi_profile {
    const char * name;
    const char * what;
    std::vector<arifi_setting> settings;
};

// A profile may only list variables the engine reads at RUNTIME. Build-time options belong in
// build_time_only below, where they are reported and never set.
const std::vector<arifi_profile> & profiles() {
    static const std::vector<arifi_profile> all = {
        {
            "cpu-only",
            "no GPU backend in use - every matmul runs on the CPU",
            {
                { "GGML_ARIFI_VNNI_REPACK", "1",
                  "measured on a CPU-only path: prompt +326.7% / +441.8%, decode +19.9% / +23.6% "
                  "(two g64 models, ArifiLabs rig, 2026-07-24). Engages only for g64 Q1_0/Q2_0 weights" },
                { "GGML_SCHED_PREFETCH_EXPERTS", "0",
                  "nothing is offloaded, so there is no host-to-device transfer to hide. This is "
                  "already the default; the profile states it so a benchmark record is unambiguous" },
            },
        },
        {
            "gpu-offload",
            "weights land on a GPU - this is the shipped default, stated explicitly",
            {
                { "GGML_ARIFI_VNNI_REPACK", "0",
                  "repacked weights lose large-batch GPU-offload eligibility: measured prompt "
                  "-77.1% / -88.1% on the ArifiLabs Vulkan rig. OFF is already the shipped default, "
                  "so this profile changes no behaviour - it only makes the choice greppable" },
            },
        },
    };
    return all;
}

// Resolved at LINK time. Listing one here is a promise that the mechanism will NEVER pretend to
// set it: it is reported, and a matching environment variable is called out as inert.
struct arifi_build_time_option {
    const char * name;
    bool         compiled_in;
    const char * how;
};

const std::vector<arifi_build_time_option> & build_time_only() {
    static const std::vector<arifi_build_time_option> all = {
        { "GGML_ARIFI_ROCMFPX_FORMATS",
#ifdef GGML_ARIFI_ROCMFPX_FORMATS
          true,
#else
          false,
#endif
          "CMake option; its vec_dot lives in type_traits_cpu[] and is chosen when the binary is "
          "linked. Rebuild with -DGGML_ARIFI_ROCMFPX_FORMATS=ON to change it" },
    };
    return all;
}

bool env_is_set(const char * name) {
    const char * v = std::getenv(name);
    return v != nullptr && v[0] != '\0';
}

bool env_set(const char * name, const char * value) {
#if defined(_WIN32)
    return _putenv_s(name, value) == 0;
#else
    return setenv(name, value, /*overwrite =*/ 1) == 0;
#endif
}

} // namespace

std::string common_arifi_profile_names() {
    std::string out;
    for (const auto & p : profiles()) {
        if (!out.empty()) {
            out += ", ";
        }
        out += p.name;
    }
    return out;
}

void common_arifi_profile_warn_build_time_env() {
    for (const auto & o : build_time_only()) {
        if (env_is_set(o.name)) {
            LOG_WRN("arifi profile: %s is set in the environment and is being IGNORED - it is a "
                    "build-time option, not a runtime toggle. This binary was compiled with it %s. %s.\n",
                    o.name, o.compiled_in ? "ON" : "OFF", o.how);
        }
    }
}

bool common_arifi_profile_apply(const std::string & name) {
    const arifi_profile * chosen = nullptr;
    for (const auto & p : profiles()) {
        if (name == p.name) {
            chosen = &p;
            break;
        }
    }
    if (chosen == nullptr) {
        return false;
    }

    LOG_INF("arifi profile: '%s' selected - %s\n", chosen->name, chosen->what);
    LOG_INF("arifi profile: evidence for every line below is docs/HARDWARE-PROFILES.md and docs/OPTIONS-REGISTRY.md\n");

    for (const auto & s : chosen->settings) {
        const char * existing = std::getenv(s.env);
        if (existing != nullptr && existing[0] != '\0') {
            // an explicit choice by the caller always outranks a profile
            if (std::string(existing) == s.value) {
                LOG_INF("arifi profile:   %s=%s (already set to the profile value)\n", s.env, existing);
            } else {
                LOG_WRN("arifi profile:   %s=%s KEPT - your environment wins; the profile wanted '%s'\n",
                        s.env, existing, s.value);
            }
            continue;
        }
        if (!env_set(s.env, s.value)) {
            LOG_ERR("arifi profile:   %s could NOT be set - the profile is incomplete and the run is "
                    "not what you asked for\n", s.env);
            continue;
        }
        LOG_INF("arifi profile:   %s=%s - %s\n", s.env, s.value, s.why);
    }

    for (const auto & o : build_time_only()) {
        LOG_INF("arifi profile:   %s is build-time, compiled %s, NOT settable here - %s\n",
                o.name, o.compiled_in ? "ON" : "OFF", o.how);
    }

    LOG_INF("arifi profile: no shipped default was changed by this fork; a profile is opt-in and "
            "sets only the variables printed above\n");
    return true;
}
