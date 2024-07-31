## Install

**This repository can be built only on linux environment.**

```bash
# Build Mannually
go mod init github.com/koshieguchi/sakura-exporter/infiniband
go get github.com/prometheus/procfs
go build -o sakura-exporter main.go

# Build with Bazel
# Update WORKSPACE file based on go.mod
bazel run //:gazelle -- update-repos -from_file=infiniband/go.mod
# Generate BUILD files automatically
bazel run //:gazelle
bazel build //infiniband
bazel run //infiniband
```

新たにモジュールを追加した場合は、以下のコマンドを実行してください。

```bash
go get github.com/...
go mod tidy
```

debug
```bash
bazel info
```

## Output

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
[Bazelを使ってみる その１（Goのビルド） - Carpe Diem](https://christina04.hatenablog.com/entry/using-bazel-to-build-go)
