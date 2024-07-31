cc_binary(
    name = "print_pcm_env",
    srcs = ["print_pcm_env.cpp"],
    copts = ["-fsanitize=address", "-g", "-Iexternal/pcm/pcm_with_cmake/src"],
    linkopts = [
        "-fsanitize=address",
        "-Lexternal/pcm/pcm_with_cmake/lib",
        "-lpcm"
    ],
    deps = ["@pcm//:pcm_with_cmake"],
)
