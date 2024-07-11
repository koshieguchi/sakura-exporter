package main

import (
    "fmt"
    "log"
    "net/http"

    "github.com/prometheus/client_golang/prometheus"
    "github.com/prometheus/client_golang/prometheus/promhttp"
    "github.com/prometheus/procfs/sysfs"
)

var (
    portRcvPackets = prometheus.NewGaugeVec(
        prometheus.GaugeOpts{
            Name: "infiniband_port_receive_packets_total",
            Help: "Total number of received packets on InfiniBand port",
        },
        []string{"device", "port"},
    )
)

func init() {
    prometheus.MustRegister(portRcvPackets)
}

func collectInfiniBandMetrics() {
    fs, err := sysfs.NewFS("/sys")
    if err != nil {
        log.Printf("Failed to create sysfs: %v", err)
        return
    }

    ibClass, err := fs.InfiniBandClass()
    if err != nil {
        log.Printf("Failed to get InfiniBand class: %v", err)
        return
    }

    for deviceName, device := range ibClass {
        for portNum, port := range device.Ports {
            if port.Counters.PortRcvPackets != nil {
                portRcvPackets.WithLabelValues(deviceName, fmt.Sprintf("%d", portNum)).Set(float64(*port.Counters.PortRcvPackets))
            }
        }
    }
}

func main() {
    http.HandleFunc("/metrics", func(w http.ResponseWriter, r *http.Request) {
        collectInfiniBandMetrics()
        promhttp.Handler().ServeHTTP(w, r)
    })

    log.Printf("Starting exporter on :9401")
    log.Fatal(http.ListenAndServe(":9401", nil))
}
