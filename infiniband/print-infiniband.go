package main

import (
	"fmt"
	"log"

	"github.com/prometheus/procfs/sysfs"
)

func main() {
    // sysfsファイルシステムへのアクセスを初期化
    fs, err := sysfs.NewFS("/sys")
    if err != nil {
        log.Fatalf("Failed to create sysfs: %v", err)
    }

    // InfiniBandClassの情報を取得
    ibClass, err := fs.InfiniBandClass()
    if err != nil {
        log.Fatalf("Failed to get InfiniBand class: %v", err)
    }

    // 各デバイスとポートのport_rcv_dataを表示
    for deviceName, device := range ibClass {
        for portNum, port := range device.Ports {
            if port.Counters.PortRcvData != nil {
                fmt.Printf("Device: %s, Port: %d, Port Receive dataPortRcvData: %d\n",
                           deviceName, portNum, *port.Counters.PortRcvData)
            } else {
                fmt.Printf("Device: %s, Port: %d, Port Receive dataPortRcvData: Not available\n",
                           deviceName, portNum)
            }
        }
    }
}
