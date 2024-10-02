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

  null_stream nullStream;
  check_and_set_silent(argc, argv, nullStream);

  std::cout << "\n Intel(r) Performance Counter Monitor " << PCM_VERSION << "\n";
  std::cout << "\n This utility measures IIO information\n\n";

  string program = string(argv[0]);

  vector<struct iio_counter> counters;
  bool csv = false;
  bool human_readable = false;
  bool show_root_port = false;
  std::string csv_delimiter = ",";
  std::string output_file;
  double delay = PCM_DELAY_DEFAULT;
  bool list = false;
  MainLoop mainLoop;
  iio_evt_parse_context evt_ctx;
  // Map with metrics names.
  map<string, std::pair<h_id, std::map<string, v_id>>> nameMap;

  while (argc > 1)
  {
    argv++;
    argc--;
    std::string arg_value;
    if (check_argument_equals(*argv, {"--help", "-h", "/h"}))
    {
      print_usage(program);
      exit(EXIT_FAILURE);
    }
    else if (check_argument_equals(*argv, {"-silent", "/silent"}))
    {
      // handled in check_and_set_silent
      continue;
    }
    else if (extract_argument_value(*argv, {"-csv-delimiter", "/csv-delimiter"}, arg_value))
    {
      csv_delimiter = std::move(arg_value);
    }
    else if (check_argument_equals(*argv, {"-csv", "/csv"}))
    {
      csv = true;
    }
    else if (extract_argument_value(*argv, {"-csv", "/csv"}, arg_value))
    {
      csv = true;
      output_file = std::move(arg_value);
    }
    else if (check_argument_equals(*argv, {"-human-readable", "/human-readable"}))
    {
      human_readable = true;
    }
    else if (check_argument_equals(*argv, {"-list", "--list"}))
    {
      list = true;
    }
    else if (check_argument_equals(*argv, {"-root-port", "/root-port"}))
    {
      show_root_port = true;
    }
    else if (mainLoop.parseArg(*argv))
    {
      continue;
    }
    else
    {
      delay = parse_delay(*argv, program, (print_usage_func)print_usage);
      continue;
    }
  }

  set_signal_handlers();

  print_cpu_details();

  PCM *m = PCM::getInstance();

  PCIDB pciDB;
  load_PCIDB(pciDB);

  auto mapping = IPlatformMapping::getPlatformMapping(m->getCPUFamilyModel(), m->getNumSockets());
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
    ev_file_name = "opCode-" + std::to_string(m->getCPUFamily()) + "-" + std::to_string(m->getInternalCPUModel()) + ".txt";
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

#ifdef PCM_DEBUG
  print_nameMap(nameMap);
#endif

  results.resize(m->getNumSockets(), stack_content(m->getMaxNumOfIIOStacks(), ctr_data()));

  mainLoop([&]()
           {
        collect_data(m, delay, iios, evt_ctx.ctrs);
        vector<string> display_buffer = csv ?
            build_csv(iios, evt_ctx.ctrs, human_readable, show_root_port, csv_delimiter, nameMap) :
            build_display(iios, evt_ctx.ctrs, pciDB, nameMap);
        display(display_buffer, *output);
        return true; });

  file_stream.close();

  exit(EXIT_SUCCESS);
}
