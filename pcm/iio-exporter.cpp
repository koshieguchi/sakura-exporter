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

PCM_MAIN_NOTHROW;

int mainThrows(int argc, char *argv[])
{
  if (print_version(argc, argv))
    exit(EXIT_SUCCESS);

  // null_stream nullStream;
  // check_and_set_silent(argc, argv, nullStream);

  string program = string(argv[0]);

  vector<struct iio_counter> counters;
  // bool csv = false;
  // bool human_readable = false;
  // bool show_root_port = false;
  // std::string csv_delimiter = ",";
  std::string output_file;
  double delay = PCM_DELAY_DEFAULT;
  bool list = false;
  MainLoop mainLoop;
  iio_evt_parse_context evt_ctx;
  // Map with metrics names.
  map<string, std::pair<h_id, std::map<string, v_id>>> nameMap;

  set_signal_handlers();

  // print_cpu_details();

  PCM *m = PCM::getInstance();

  PCIDB pciDB;
  load_PCIDB(pciDB);

  auto mapping = IPlatformMapping::getPlatformMapping(m->getCPUModel(), m->getNumSockets());
  if (!mapping)
  {
    cerr << "Failed to discover pci tree: unknown platform" << endl;
    exit(EXIT_FAILURE);
  }

  std::vector<struct iio_stacks_on_socket> iios;
  if (!mapping->pciTreeDiscover(iios))
  {
    exit(EXIT_FAILURE);
  }

  std::ostream *output = &std::cout;
  std::fstream file_stream;
  if (!output_file.empty())
  {
    file_stream.open(output_file.c_str(), std::ios_base::out);
    output = &file_stream;
  }

  if (list)
  {
    print_PCIeMapping(iios, pciDB, *output);
    return 0;
  }

  string ev_file_name;
  if (m->IIOEventsAvailable())
  {
    ev_file_name = "opCode-" + std::to_string(m->getCPUModel()) + ".txt";
  }
  else
  {
    cerr << "This CPU is not supported by PCM IIO tool! Program aborted\n";
    exit(EXIT_FAILURE);
  }

  map<string, uint32_t> opcodeFieldMap;
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

  evt_ctx.m = m;
  evt_ctx.ctrs.clear(); // fill the ctrs by evt_handler call back func.

  try
  {
    load_events(ev_file_name, opcodeFieldMap, iio_evt_parse_handler, (void *)&evt_ctx, nameMap);
  }
  catch (std::exception &e)
  {
    std::cerr << "Error info:" << e.what() << "\n";
    std::cerr << "Event configure file have the problem and cause the program exit, please double check it!\n";
    exit(EXIT_FAILURE);
  }

  results.resize(m->getNumSockets(), stack_content(m->getMaxNumOfIIOStacks(), ctr_data()));

  // Prometheus definition
  // Create a Prometheus exporter
  prometheus::Exposer exposer{"127.0.0.1:9403"};

  // Create a metrics registry
  auto registry = std::make_shared<prometheus::Registry>();

  // Add the metrics registry to the exposer
  exposer.RegisterCollectable(registry);

  // Create gauge metrics for PCIe bandwidths
  auto &pcm_iio_family = prometheus::BuildGauge()
                             .Name("pcm_iio")
                             .Help("PCM IIO in bytes per second")
                             .Register(*registry);

  // Add metrics to the registry
  for (const auto &socket : iios)
  {
    for (const auto &stack : socket.stacks)
    {
      const uint32_t stack_id = stack.iio_unit_id;

      for (const auto &ctr : evt_ctx.ctrs)
      {
        std::string metric_name = "socket" + std::to_string(socket.socket_id) + "_stack" + std::to_string(stack_id) + "_" + ctr.v_event_name;
        pcm_iio_family.Add({{"socket", std::to_string(socket.socket_id)}, {"stack", std::to_string(stack_id)}, {"event", ctr.v_event_name}});
      }
    }
  }

  // Start the Prometheus exporter
  std::cout << "\n------\n[INFO] Starting Prometheus exporter on port: 9403" << std::endl;

  mainLoop([&]()
           {
        collect_data(m, delay, iios, evt_ctx.ctrs);

        // Update the Prometheus metrics
        for (const auto &socket : iios)
        {
          for (const auto &stack : socket.stacks)
          {
            const uint32_t stack_id = stack.iio_unit_id;

            for (const auto &ctr : evt_ctx.ctrs)
            {
              const uint64_t value = results[socket.socket_id][stack_id][std::pair<h_id, v_id>(ctr.h_id, ctr.v_id)];
              pcm_iio_family.Add({{"socket", std::to_string(socket.socket_id)}, {"stack", std::to_string(stack_id)}, {"event", ctr.v_event_name}}).Set(value);
            }
          }
        }
        return true; });

  file_stream.close();

  exit(EXIT_SUCCESS);
}
