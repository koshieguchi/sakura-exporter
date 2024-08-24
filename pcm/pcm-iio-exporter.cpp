#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/gauge.h>

// 他の必要なヘッダーをインクルード
#include "cpucounters.h"
#include "pcie-exporter.h"

// PCIeデバイス情報を収集する関数
void collectPCIeInfo(PCM *m, IPlatform *platform, prometheus::Gauge &read_bw_gauge, prometheus::Gauge &write_bw_gauge)
{
  // getEventsを呼び出してPCIeのイベントを収集
  platform->getEvents();

  // 読み取りおよび書き込み帯域幅を取得
  double read_bw = platform->getReadBw();
  double write_bw = platform->getWriteBw();

  // PrometheusのGaugeに値を設定
  read_bw_gauge.Set(read_bw);
  write_bw_gauge.Set(write_bw);

  // プラットフォームのクリーンアップ
  platform->cleanup();
}

int main(int argc, char *argv[])
{
  if (print_version(argc, argv))
    exit(EXIT_SUCCESS);

  // Prometheusエクスポーザーの初期化
  prometheus::Exposer exposer{"127.0.0.1:9402"};

  // メトリクスレジストリの作成
  auto registry = std::make_shared<prometheus::Registry>();

  // メトリクスレジストリをエクスポーザーに登録
  exposer.RegisterCollectable(registry);

  // Gaugeメトリクスの作成
  auto &pcie_bandwidth_family = prometheus::BuildGauge()
                                    .Name("pcie_bandwidth")
                                    .Help("PCIe bandwidth in bytes per second")
                                    .Register(*registry);

  auto &read_bw_gauge = pcie_bandwidth_family.Add({{"direction", "read"}});
  auto &write_bw_gauge = pcie_bandwidth_family.Add({{"direction", "write"}});

  // PCMインスタンスの取得
  PCM *m = PCM::getInstance();
  double delay = 1.0; // 1秒の遅延
  bool csv = false;
  bool print_bandwidth = true;
  bool print_additional_info = false;

  // プラットフォームの作成
  std::unique_ptr<IPlatform> platform(IPlatform::getPlatform(m, csv, print_bandwidth, print_additional_info, static_cast<uint>(delay * 1000)));

  // エクスポーターの開始
  std::cout << "\n------\n[INFO] Starting Prometheus exporter on port: 9402" << std::endl;

  // 監視ループ
  while (true)
  {
    collectPCIeInfo(m, platform.get(), read_bw_gauge, write_bw_gauge);
    std::this_thread::sleep_for(std::chrono::seconds(1)); // 1秒待機
  }

  exit(EXIT_SUCCESS);
}
