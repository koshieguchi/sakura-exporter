#!/bin/bash -eux

rm -rf pcm
git clone https://github.com/intel/pcm.git
cd pcm
mkdir -p build
cd build
cmake ..
make -j $(nproc)

cd ../../

g++ -fsanitize=address -g -o print_pcm_env.out print_pcm_env.cpp -I./pcm/src -L./pcm/build/lib -lpcm
export LD_LIBRARY_PATH=./pcm/build/lib:$LD_LIBRARY_PATH
sudo env LD_LIBRARY_PATH=$LD_LIBRARY_PATH ./print_pcm_env.out
