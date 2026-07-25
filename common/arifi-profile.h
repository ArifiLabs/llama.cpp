#pragma once

#include <string>

// ArifiLabs hardware profiles - see docs/HARDWARE-PROFILES.md.
//
// A profile is a NAMED, documented set of runtime environment toggles. It only ever sets
// variables the engine already reads, it never overrides one the caller set explicitly, and
// it logs every decision verbatim. Selecting no profile changes nothing: every shipped
// default is exactly what it was before this file existed.
//
// Ordering constraint: a profile must be applied BEFORE the first model load. The toggles it
// sets are latched into function-local statics the first time a tensor buffer is initialised
// (see ggml_arifi_vnni_repack_enabled() in ggml/src/ggml-cpu/repack.cpp). Applying one from
// the argument parser satisfies this; applying one after a model exists does not.

// Applies the named profile. Returns false, and leaves the environment untouched, if the name
// is not known. Logs every variable it sets, keeps, or cannot set.
bool common_arifi_profile_apply(const std::string & name);

// "cpu-only, gpu-offload" - for --help text and error messages.
std::string common_arifi_profile_names();

// Warns about environment variables that LOOK like fork toggles but are resolved at LINK time,
// so setting them in the environment does nothing. Cheap, and called whether or not a profile
// was selected, because the trap does not require a profile to fall into.
void common_arifi_profile_warn_build_time_env();
