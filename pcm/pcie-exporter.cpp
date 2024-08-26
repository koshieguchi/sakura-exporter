#include <unistd.h>
#include <signal.h>
#include <math.h>
#include <iomanip>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <assert.h>
#include "pcie-exporter.h"

#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include <memory>
#include <thread>
#include <chrono>
#include <csignal>

using namespace pcm;

// Factory function
IPlatform *IPlatform::getPlatform(PCM *m, bool csv, bool print_bandwidth, bool print_additional_info, uint32 delay)
{
  switch (m->getCPUModel())
  {
  case PCM::SRF:
    std::cout << "Birch" << std::endl;
    return new BirchStreamPlatform(m, csv, print_bandwidth, print_additional_info, delay);
  case PCM::SPR:
  case PCM::EMR:
    std::cout << "Eagle" << std::endl;
    return new EagleStreamPlatform(m, csv, print_bandwidth, print_additional_info, delay);
  case PCM::ICX:
  case PCM::SNOWRIDGE:
    std::cout << "Whitley" << std::endl;
    return new WhitleyPlatform(m, csv, print_bandwidth, print_additional_info, delay);
  case PCM::SKX:
    std::cout << "Purley" << std::endl;
    return new PurleyPlatform(m, csv, print_bandwidth, print_additional_info, delay);
  case PCM::BDX_DE:
  case PCM::BDX:
  case PCM::KNL:
  case PCM::HASWELLX:
    std::cout << "Grantley" << std::endl;
    return new GrantleyPlatform(m, csv, print_bandwidth, print_additional_info, delay);
  case PCM::IVYTOWN:
  case PCM::JAKETOWN:
    std::cout << "Bromolow" << std::endl;
    return new BromolowPlatform(m, csv, print_bandwidth, print_additional_info, delay);
  default:
    return nullptr;
  }
}

volatile bool keep_running = true;

void signalHandler(int signum)
{
  std::cout << "\nInterrupt signal (" << signum << ") received. Shutting down gracefully..." << std::endl;
  keep_running = false;
}

int main(int argc, char *argv[])
{
  if (print_version(argc, argv))
    exit(EXIT_SUCCESS);

  // Register signal handler for graceful shutdown
  std::signal(SIGINT, signalHandler);
  std::signal(SIGTERM, signalHandler);

  // Create a Prometheus exporter
  prometheus::Exposer exposer{"127.0.0.1:9402"};

  // Create a metrics registry
  auto registry = std::make_shared<prometheus::Registry>();

  // Add the metrics registry to the exposer
  exposer.RegisterCollectable(registry);

  // Create gauge metrics for PCIe bandwidths
  auto &pcie_bandwidth_family = prometheus::BuildGauge()
                                    .Name("pcie_bandwidth")
                                    .Help("PCIe bandwidth in bytes per second")
                                    .Register(*registry);

  auto &read_bw_gauge = pcie_bandwidth_family.Add({{"direction", "read"}});
  auto &write_bw_gauge = pcie_bandwidth_family.Add({{"direction", "write"}});

  // Create the platform
  double delay = 1.0; // Default delay of 1 second
  bool csv = false;
  bool print_bandwidth = true;
  bool print_additional_info = false;
  PCM *m = PCM::getInstance();
  if (!m || !m->good())
  {
    std::cerr << "Can't access PCM counters or failed to initialize PCM." << std::endl;
    exit(EXIT_FAILURE);
  }

  unique_ptr<IPlatform> platform(IPlatform::getPlatform(m, csv, print_bandwidth, print_additional_info, (uint)(delay * 1000)));
  if (!platform)
  {
    std::cerr << "Unsupported CPU model or failed to create platform." << std::endl;
    exit(EXIT_FAILURE);
  }

  // Start the Prometheus exporter
  std::cout << "\n------\n[INFO] Starting Prometheus exporter on port: 9402" << std::endl;

  // Monitoring loop
  while (keep_running)
  {
    platform->getEvents();

    double read_bw = platform->getReadBw();
    double write_bw = platform->getWriteBw();

    // Update Prometheus gauges
    read_bw_gauge.Set(read_bw);
    write_bw_gauge.Set(write_bw);

    // Reset the counters
    platform->cleanup();

    // Wait for the specified delay (10s)
    std::this_thread::sleep_for(std::chrono::seconds(10));
  }

  std::cout << "[INFO] Exporter stopped. Exiting program." << std::endl;
  exit(EXIT_SUCCESS);
}
