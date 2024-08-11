# Makefile

# Number of parallel jobs for building
JOBS := $(shell echo $$((`nproc`/2)))

# Directories
PROMETHEUS_CPP_DIR := prometheus-cpp
PCM_DIR := pcm

# Targets
all: pcie-exporter.out pcm-iio.out iio-exporter.out

# Fetch third-party dependencies
$(PROMETHEUS_CPP_DIR)/_build:
	if [ ! -d "$(PROMETHEUS_CPP_DIR)" ]; then \
		git clone --recursive https://github.com/jupp0r/prometheus-cpp.git --depth 1; \
	fi
	mkdir -p $(PROMETHEUS_CPP_DIR)/_build
	cd $(PROMETHEUS_CPP_DIR)/_build && \
	cmake .. -DBUILD_SHARED_LIBS=ON -DENABLE_PUSH=OFF -DENABLE_COMPRESSION=OFF && \
	cmake --build . --parallel $(JOBS) && \
	ctest -V && \
	cmake --install .

$(PCM_DIR)/build:
	if [ ! -d "$(PCM_DIR)" ]; then \
		git clone --recursive https://github.com/intel/pcm.git --depth 1; \
	fi
	mkdir -p $(PCM_DIR)/build
	cd $(PCM_DIR)/build && \
	cmake .. && \
	cmake --build . --target PCM_SHARED --parallel $(JOBS)

# Build targets
pcie-exporter.out: pcie-exporter.cpp $(PROMETHEUS_CPP_DIR)/_build $(PCM_DIR)/build
	g++ -fsanitize=address -g -o pcie-exporter.out pcie-exporter.cpp \
	-I. \
	-I$(PCM_DIR)/src \
	-L$(PCM_DIR)/build/lib \
	-lpcm \
	-lprometheus-cpp-pull \
	-lprometheus-cpp-core \
	-lz

pcm-iio.out: pcm-iio.cpp $(PCM_DIR)/build
	g++ -fsanitize=address -g -o pcm-iio.out pcm-iio.cpp \
	-I$(PCM_DIR)/src \
	-L$(PCM_DIR)/build/lib \
	-lpcm

iio-exporter.out: iio-exporter.cpp $(PCM_DIR)/build
	g++ -fsanitize=address -g -o iio-exporter.out iio-exporter.cpp \
	-I$(PCM_DIR)/src \
	-L$(PCM_DIR)/build/lib \
	-lpcm

# Clean up
clean:
	rm -rf *.out $(PROMETHEUS_CPP_DIR) $(PCM_DIR)

.PHONY: all clean
