[External C/C++ libraries rules - Google Docs](https://docs.google.com/document/d/1Gv452Vtki8edo_Dj9VTNJt5DA_lKTcSMwrwjJOkLaoU/edit)

```bash
g++ -fsanitize=address -g -o print_pcm_env.out print_pcm_env.cpp -I/home/koshieguchi/pcm/src -L/home/koshieguchi/pcm/build/lib -lpcm
sudo env LD_LIBRARY_PATH=/home/koshieguchi/pcm/build/lib:$LD_LIBRARY_PATH ./print_pcm_env.out
```

pcm
`/home/mdxuser/.cache/bazel/_bazel_mdxuser/429e3924ade3921e08597fad00a99977/sandbox/linux-sandbox/4/execroot/_main/bazel-out/k8-fastbuild/bin/external/pcm/pcm_with_cmake.build_tmpdir`
