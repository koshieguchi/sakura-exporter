# BUILD for pcm-exporter
load("@rules_foreign_cc//foreign_cc:defs.bzl", "cmake")

filegroup(
    name = "srcs",
    srcs = glob(["src/**"]),
    visibility = ["//visibility:public"],
)

filegroup(
    name = "all",
    srcs = glob(["**"]),
    visibility = ["//visibility:public"],
)

cmake(
    name = "pcm_with_cmake",
    # Values to be passed as -Dkey=value on the CMake command line;
    # here are serving to provide some CMake script configuration options
    #  cache_entries = {
    #      "NOFORTRAN": "on",
    #      "BUILD_WITHOUT_LAPACK": "no",
    #  },
    # lib_source = "//:srcs",
    lib_source = "@pcm//:all",
    # working_directory = "src",
    # We are selecting the resulting static library to be passed in C/C++ provider
    # as the result of the build;
    # However, the cmake_external dependants could use other artifacts provided by the build,
    # according to their CMake script
    # cmake_options = [
    #     "-DBUILD_SHARED_LIBS=OFF",  # 静的ライブラリのみをビルド
    # ],
    # build_args = [
    #     "-DBUILD_SHARED_LIBS=OFF",
    # ],
    out_static_libs = ["libpcm.a"],
    targets = ["PCM_STATIC"],
    visibility = ["//visibility:public"],
)


