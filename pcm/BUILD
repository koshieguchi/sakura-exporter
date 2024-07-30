cc_binary(
    name = "print_pcm_env",
    srcs = ["print_pcm_env.cpp"],
    copts = ["-fsanitize=address", "-g", "-Iexternal/pcm/src"],
    linkopts = [
        "-fsanitize=address",
        "-L$(locations //:pcm_with_cmake) -lpcm"
    ],
    deps = ["//third_party:pcm_with_cmake"],
)
