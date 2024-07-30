package(default_visibility = ["//visibility:public"])
load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

cmake(
    name = "pcm",
    # cache_entries = {
    #     "CMAKE_C_FLAGS": "-fPIC",
    # },
    lib_source = "@inte_pcm//:all_srcs",
    out_static_libs = ["libpcm.a"],
)

cc_library(
    name = "intel_pcm",
    srcs = ["build/lib/libpcm.a"],
    hdrs = glob(["build/include/*.h"]),
    includes = ["build/include"],
)
