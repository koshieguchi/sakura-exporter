#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <chrono>
#include <thread>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/gauge.h>
#include "cpucounters.h"
#include "utils.h"
#include "iio-exporter.h"

using namespace pcm;

class PCIeExporter
{
private:
  PCM *m;
  std::shared_ptr<prometheus::Registry> registry;
  prometheus::Exposer exposer;
  std::map<std::string, prometheus::Family<prometheus::Gauge> *> metric_families;

  void setupMetrics()
  {
    std::cout << "Setting up metrics..." << std::endl;
    metric_families["pcie_throughput"] = &prometheus::BuildGauge()
                                              .Name("pcie_throughput")
                                              .Help("PCIe throughput in bytes/second")
                                              .Register(*registry);
    std::cout << "Metrics setup completed." << std::endl;
  }

  void collectAndExportMetrics()
  {
    std::cout << "Collecting and exporting metrics..." << std::endl;

    // Initialize necessary data structures
    std::vector<struct iio_stacks_on_socket> iios;
    std::vector<struct iio_counter> ctrs;
    std::map<std::string, std::pair<h_id, std::map<std::string, v_id>>> nameMap;

    // Discover PCIe tree
    auto mapping = IPlatformMapping::getPlatformMapping(m->getCPUModel(), m->getNumSockets());
    if (!mapping)
    {
      std::cerr << "Failed to discover PCIe tree: unknown platform" << std::endl;
      return;
    }

    if (!mapping->pciTreeDiscover(iios))
    {
      std::cerr << "PCIe tree discovery failed." << std::endl;
      return;
    }

    std::cout << "PCIe tree discovery completed." << std::endl;

    // Load event configuration
    std::string ev_file_name;
    if (m->IIOEventsAvailable())
    {
      ev_file_name = "opCode-" + std::to_string(m->getCPUModel()) + ".txt";
    }
    else
    {
      std::cerr << "This CPU is not supported by PCM IIO tool! Program aborted." << std::endl;
      return;
    }

    iio_evt_parse_context evt_ctx;
    evt_ctx.m = m;
    evt_ctx.ctrs.clear(); // Clear previous counters

    std::cout << "Loading events from " << ev_file_name << "..." << std::endl;

    // Load events
    try
    {
      std::map<std::string, uint32_t> opcodeFieldMap;
      opcodeFieldMap["opcode"] = PCM::OPCODE;
      opcodeFieldMap["ev_sel"] = PCM::EVENT_SELECT;
      opcodeFieldMap["umask"] = PCM::UMASK;
      opcodeFieldMap["reset"] = PCM::RESET;
      opcodeFieldMap["edge_det"] = PCM::EDGE_DET;
      opcodeFieldMap["ignored"] = PCM::IGNORED;
      opcodeFieldMap["overflow_enable"] = PCM::OVERFLOW_ENABLE;
      opcodeFieldMap["en"] = PCM::ENABLE;
      opcodeFieldMap["invert"] = PCM::INVERT;
      opcodeFieldMap["thresh"] = PCM::THRESH;
      opcodeFieldMap["ch_mask"] = PCM::CH_MASK;
      opcodeFieldMap["fc_mask"] = PCM::FC_MASK;
      opcodeFieldMap["hname"] = PCM::H_EVENT_NAME;
      opcodeFieldMap["vname"] = PCM::V_EVENT_NAME;
      opcodeFieldMap["multiplier"] = PCM::MULTIPLIER;
      opcodeFieldMap["divider"] = PCM::DIVIDER;
      opcodeFieldMap["ctr"] = PCM::COUNTER_INDEX;

      load_events(ev_file_name, opcodeFieldMap, iio_evt_parse_handler, (void *)&evt_ctx, nameMap);
    }
    catch (std::exception &e)
    {
      std::cerr << "Error loading events: " << e.what() << "\n";
      std::cerr << "Event configure file might have issues. Exiting.\n";
      return;
    }

    std::cout << "Event loading completed." << std::endl;

    // Collect data
    std::cout << "Starting data collection..." << std::endl;
    collect_data(m, 1.0 /*delay in seconds*/, iios, evt_ctx.ctrs);
    std::cout << "Data collection completed." << std::endl;

    // Now populate Prometheus metrics
    for (const auto &socket : iios)
    {
      for (const auto &stack : socket.stacks)
      {
        const uint32_t stack_id = stack.iio_unit_id;
        for (const auto &ctr : evt_ctx.ctrs)
        {
          std::string metric_name = "pcie_" + ctr.v_event_name + "_" + stack.stack_name;
          metric_name.erase(metric_name.find_last_not_of(' ') + 1);

          std::cout << "Processing metric: " << metric_name << std::endl;

          // Ensure metric family exists or create a new one
          auto &family = prometheus::BuildGauge()
                             .Name(metric_name)
                             .Help("PCIe metric")
                             .Register(*registry);

          auto &gauge = family.Add({{"socket", std::to_string(socket.socket_id)},
                                    {"stack", stack.stack_name}});

          // Retrieve and set the metric value
          try
          {
            uint64_t value = ctr.data[0].at(socket.socket_id).at(stack_id).at(std::make_pair(ctr.h_id, ctr.v_id));
            std::cout << "Metric value: " << value << std::endl;
            gauge.Set(static_cast<double>(value));
          }
          catch (const std::out_of_range &e)
          {
            std::cerr << "Out of range error accessing metric data: " << e.what() << std::endl;
          }
        }
      }
    }
  }

public:
  PCIeExporter(PCM *pcm, const std::string &address)
      : m(pcm), registry(std::make_shared<prometheus::Registry>()), exposer(address)
  {
    exposer.RegisterCollectable(registry);
    setupMetrics();
  }

  void run(int interval_seconds)
  {
    while (true)
    {
      collectAndExportMetrics();
      std::this_thread::sleep_for(std::chrono::seconds(interval_seconds));
    }
  }
};

int main(int argc, char *argv[])
{
  PCM *m = PCM::getInstance();
  if (!m->good())
  {
    std::cerr << "Can't access PCM counters" << std::endl;
    exit(1);
  }

  std::cout << "Initializing PCIe Exporter..." << std::endl;
  PCIeExporter exporter(m, "0.0.0.0:9603");
  std::cout << "PCIe Exporter started on port 9603" << std::endl;
  exporter.run(10); // Update every 10 seconds

  return 0;
}
