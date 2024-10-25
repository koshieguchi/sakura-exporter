#!/bin/bash -eux

# fetch third-party dependencies
(
  # Install prometheus-cpp
  if [ ! -d "prometheus-cpp" ]; then
    git clone --recursive https://github.com/jupp0r/prometheus-cpp.git --depth 1
  fi

  mkdir -p prometheus-cpp/_build
  cd prometheus-cpp/_build

  cmake .. -DBUILD_SHARED_LIBS=OFF -DENABLE_PUSH=OFF -DENABLE_COMPRESSION=OFF -DCMAKE_BUILD_TYPE=Release # run cmake
  cmake --build . --parallel $(($(nproc)/2)) # build
  ctest -V # run tests
  cmake --install . # install the libraries and headers
) &
(
  # Install pcm
  if [ ! -d "pcm" ]; then
    git clone --recursive https://github.com/intel/pcm.git --depth 1
  fi

  mkdir -p pcm/build
  cd pcm/build

  cmake ..
  cmake --build . --target PCM_STATIC --parallel $(($(nproc)/2)) # build
) &

# wait for both to finish
wait

mkdir -p ./bin

g++ -fsanitize=address -g -o ./bin/pcm-pcie-exporter.out pcie-exporter.cpp \
  -I. \
  -I./pcm/src \
  -L./pcm/build/src \
  ./pcm/build/src/libpcm.a \
  /usr/local/lib/libprometheus-cpp-pull.a \
  /usr/local/lib/libprometheus-cpp-core.a \
  -lz
g++ -fsanitize=address -g -o ./bin/pcm-iio-exporter.out iio-exporter.cpp \
  -I. \
  -I./pcm/src \
  ./pcm/build/src/libpcm.a \
  /usr/local/lib/libprometheus-cpp-pull.a \
  /usr/local/lib/libprometheus-cpp-core.a \
  -lz
g++ -fsanitize=address -g -o ./bin/pcm-memory-exporter.out pcm-memory-exporter.cpp \
  -I. \
  -I./pcm/src \
  ./pcm/build/src/libpcm.a \
  /usr/local/lib/libprometheus-cpp-pull.a \
  /usr/local/lib/libprometheus-cpp-core.a \
  -lz

# sudo env LD_LIBRARY_PATH=$(pwd)/pcm/build/lib:/usr/local/lib64:${LD_LIBRARY_PATH:-} ./bin/print_pcm_env.out
# sudo env LD_LIBRARY_PATH=$(pwd)/pcm/build/lib:/usr/local/lib64:${LD_LIBRARY_PATH:-} ./bin/pcie-exporter.out
# sudo env LD_LIBRARY_PATH=$(pwd)/pcm/build/lib:/usr/local/lib64:${LD_LIBRARY_PATH:-} ./bin/iio-exporter.out
# sudo env LD_LIBRARY_PATH=$(pwd)/pcm/build/lib:/usr/local/lib64:${LD_LIBRARY_PATH:-} ./bin/pcm-iio.out -csv -i=1
