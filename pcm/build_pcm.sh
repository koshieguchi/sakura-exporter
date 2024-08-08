#!/bin/bash -eux

# fetch third-party dependencies
rm -rf prometheus-cpp
git clone https://github.com/jupp0r/prometheus-cpp.git
cd prometheus-cpp
git submodule init
git submodule update

mkdir _build
cd _build

# run cmake
cmake .. -DBUILD_SHARED_LIBS=ON -DENABLE_PUSH=OFF -DENABLE_COMPRESSION=OFF

# build
cmake --build . --parallel $(nproc)

# run tests
ctest -V

# install the libraries and headers
cmake --install .

cd ../../

rm -rf pcm
git clone https://github.com/intel/pcm.git
cd pcm
mkdir -p build
cd build
cmake ..
make -j $(nproc)

cd ../../

# g++ -fsanitize=address -g -o print_pcm_env.out print_pcm_env.cpp -I./pcm/src -L./pcm/build/lib -lpcm
g++ -fsanitize=address -g -o pcie-exporter.out pcie-exporter.cpp -I./pcm/src -L./pcm/build/lib -lpcm
export LD_LIBRARY_PATH=./pcm/build/lib:$LD_LIBRARY_PATH
# sudo env LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./print_pcm_env.out
sudo env LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./pcie-exporter.out
