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
	registry = prometheus.NewRegistry()

	portRcvData = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "infiniband_port_receive_data_total",
			Help: "Total number of received data on InfiniBand port",
		},
		[]string{"device", "port"},
	)

	portXmitData = prometheus.NewGaugeVec(
		prometheus.GaugeOpts{
			Name: "infiniband_port_transmit_data_total",
			Help: "Total number of transmitted data on InfiniBand port",
		},
		[]string{"device", "port"},
	)
)

func init() {
	registry.MustRegister(portRcvData)
	registry.MustRegister(portXmitData)
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
			if port.Counters.PortRcvData != nil {
				portRcvData.WithLabelValues(deviceName, fmt.Sprintf("%d", portNum)).Set(float64(*port.Counters.PortRcvData))
			}
			if port.Counters.PortXmitData != nil {
				portXmitData.WithLabelValues(deviceName, fmt.Sprintf("%d", portNum)).Set(float64(*port.Counters.PortXmitData))
			}
		}
	}
}

func main() {
	http.HandleFunc("/metrics", func(w http.ResponseWriter, r *http.Request) {
		collectInfiniBandMetrics()
		promhttp.HandlerFor(registry, promhttp.HandlerOpts{}).ServeHTTP(w, r)
	})

	log.Printf("Starting exporter on :9401")
	log.Fatal(http.ListenAndServe(":9401", nil))
}
