# BUILD for pcm-exporter
load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")
# load("@rules_foreign_cc//tools/build_defs:cmake.bzl", "cmake_external")
# load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

filegroup(
    name = "all",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

cmake(
    name = "pcm_with_cmake",
    lib_source = "@pcm//:all",
    out_include_dir = "include",
    # out_lib_dir="lib",
    cache_entries = {
        "CMAKE_C_FLAGS": "-fPIC",
        "CMAKE_BUILD_TYPE": "Release",
        "CMAKE_INSTALL_PREFIX": "$INSTALLDIR",
        "CMAKE_INSTALL_SBINDIR": "$INSTALLDIR/bin",
        "CMAKE_INSTALL_LIBDIR": "$INSTALLDIR/lib",
        "CMAKE_LIBRARY_OUTPUT_DIRECTORY": "$INSTALLDIR/lib",
    },
    # out_static_libs = ["libpcm.a"],
    # out_shared_libs = ["libpcm.so"],
    out_binaries = [
        "pcm",
        "pcm-numa",
        "pcm-latency",
        "pcm-power",
        "pcm-msr",
        "pcm-memory",
        "pcm-tsx",
        "pcm-pcie",
        "pcm-core",
        "pcm-iio",
        "pcm-lspci",
        "pcm-pcicfg",
        "pcm-mmio",
        "pcm-tpmi",
        "pcm-raw",
        "pcm-accel",
        "pcm-sensor-server",
        "pcm-sensor",
    ] + ["pcm-daemon", "pcm-client"],
    targets = [
        "PCM_STATIC",
        "PCM_SHARED",
    ] + [
        "pcm",
        "pcm-numa",
        "pcm-latency",
        "pcm-power",
        "pcm-msr",
        "pcm-memory",
        "pcm-tsx",
        "pcm-pcie",
        "pcm-core",
        "pcm-iio",
        "pcm-lspci",
        "pcm-pcicfg",
        "pcm-mmio",
        "pcm-tpmi",
        "pcm-raw",
        "pcm-accel",
        "pcm-sensor-server",
        "pcm-sensor",
    ] +["daemon", "client"],
    visibility = ["//visibility:public"],
    build_args = [
        "-j16",
         "--verbose",
    ],
    install=True,
)
