## Install

### Infiniband exporter

**Install Go 1.22.3**

```sh
wget https://go.dev/dl/go1.22.3.linux-amd64.tar.gz
sudo tar -C /usr/local -xzf go1.22.3.linux-amd64.tar.gz
echo 'export PATH=$PATH:/usr/local/go/bin' >> ~/.bashrc
source ~/.bashrc
go version
```

```bash
# Build Mannually
go mod init github.com/koshieguchi/sakura-exporter/infiniband
go get github.com/prometheus/procfs
go build -o infiniband infiniband-exporter.go

# Build with Bazel
# Update WORKSPACE file based on go.mod
bazel run //:gazelle -- update-repos -from_file=infiniband/go.mod
# Generate BUILD files automatically
bazel run //:gazelle
bazel build //infiniband
bazel run //infiniband
```

If you add new dependencies, you need to update WORKSPACE file and run gazelle command.

```bash
go get github.com/...
go mod tidy
```

debug

```bash
bazel info
```

### PCM exporters

```bash
sudo apt install -y libasan8 cmake zlib1g-dev
cd pcm
sudo ./build.sh
```

## Output

| name                | port | endpoint | description             |
| ------------------- | ---- | -------- | ----------------------- |
| infiniband exporter | 9401 | /metrics | Infiniband Port Metrics |
| pcm-pcie exporter   | 9402 | /metrics | PCM PCIe Metrics        |
| pcm-iio exporter    | 9403 | /metrics | PCM IIO Metrics         |
| pcm-memory exporter | 9404 | /metrics | PCM Memory Metrics      |

You can check the metrics by accessing each endpoint. For example:

```sh
$ curl localhost:9401/metrics
# HELP infiniband_port_receive_packets_total Total number of received packets on InfiniBand port
# TYPE infiniband_port_receive_packets_total gauge
infiniband_port_receive_packets_total{device="mlx5_0",port="1"} 4.09752796e+08
infiniband_port_receive_packets_total{device="mlx5_1",port="1"} 1.7511881e+07
infiniband_port_receive_packets_total{device="mlx5_4",port="1"} 1.75004e+07
infiniband_port_receive_packets_total{device="mlx5_5",port="1"} 1.7498324e+07
infiniband_port_receive_packets_total{device="mlx5_bond_0",port="1"} 0
# HELP infiniband_port_transmit_packets_total Total number of transmitted packets on InfiniBand port
# TYPE infiniband_port_transmit_packets_total gauge
infiniband_port_transmit_packets_total{device="mlx5_0",port="1"} 4.10805799e+08
infiniband_port_transmit_packets_total{device="mlx5_1",port="1"} 1.8183523e+07
infiniband_port_transmit_packets_total{device="mlx5_4",port="1"} 1.8186901e+07
infiniband_port_transmit_packets_total{device="mlx5_5",port="1"} 1.819127e+07
infiniband_port_transmit_packets_total{device="mlx5_bond_0",port="1"} 0
```

## Reference

- https://christina04.hatenablog.com/entry/using-bazel-to-build-go
