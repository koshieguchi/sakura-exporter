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
#include "pcm-memory-exporter.h"

PCM_MAIN_NOTHROW;

int mainThrows(int argc, char *argv[])
{
  if (print_version(argc, argv))
    exit(EXIT_SUCCESS);

  null_stream nullStream2;
#ifdef PCM_FORCE_SILENT
  null_stream nullStream1;
  cout.rdbuf(&nullStream1);
  cerr.rdbuf(&nullStream2);
#else
  check_and_set_silent(argc, argv, nullStream2);
#endif

  set_signal_handlers();

  cerr << "\n";
  cerr << " Intel(r) Performance Counter Monitor: Memory Bandwidth Monitoring Utility " << PCM_VERSION << "\n";
  cerr << "\n";

  cerr << " This utility measures memory bandwidth per channel or per DIMM rank in real-time\n";
  cerr << "\n";

  double delay = -1.0;
  bool csv = false, csvheader = false, show_channel_output = true, print_update = false;
  uint32 no_columns = DEFAULT_DISPLAY_COLUMNS; // Default number of columns is 2
  char *sysCmd = NULL;
  char **sysArgv = NULL;
  int rankA = -1, rankB = -1;
  MainLoop mainLoop;

  string program = string(argv[0]);

  PCM *m = PCM::getInstance();
  assert(m);
  if (m->getNumSockets() > max_sockets)
  {
    cerr << "Only systems with up to " << max_sockets << " sockets are supported! Program aborted\n";
    exit(EXIT_FAILURE);
  }
  ServerUncoreMemoryMetrics metrics;
  metrics = m->PMMTrafficMetricsAvailable() ? Pmem : PartialWrites;

  if (argc > 1)
    do
    {
      argv++;
      argc--;
      string arg_value;

      if (check_argument_equals(*argv, {"--help", "-h", "/h"}))
      {
        print_help(program);
        exit(EXIT_FAILURE);
      }
      else if (check_argument_equals(*argv, {"-silent", "/silent"}))
      {
        // handled in check_and_set_silent
        continue;
      }
      else if (check_argument_equals(*argv, {"-csv", "/csv"}))
      {
        csv = csvheader = true;
      }
      else if (extract_argument_value(*argv, {"-csv", "/csv"}, arg_value))
      {
        csv = true;
        csvheader = true;
        if (!arg_value.empty())
        {
          m->setOutput(arg_value);
        }
        continue;
      }
      else if (mainLoop.parseArg(*argv))
      {
        continue;
      }
      else if (extract_argument_value(*argv, {"-columns", "/columns"}, arg_value))
      {
        if (arg_value.empty())
        {
          continue;
        }
        no_columns = stoi(arg_value);
        if (no_columns == 0)
          no_columns = DEFAULT_DISPLAY_COLUMNS;
        if (no_columns > m->getNumSockets())
          no_columns = m->getNumSockets();
        continue;
      }
      else if (extract_argument_value(*argv, {"-rank", "/rank"}, arg_value))
      {
        if (arg_value.empty())
        {
          continue;
        }
        int rank = stoi(arg_value);
        if (rankA >= 0 && rankB >= 0)
        {
          cerr << "At most two DIMM ranks can be monitored \n";
          exit(EXIT_FAILURE);
        }
        else
        {
          if (rank > 7)
          {
            cerr << "Invalid rank number " << rank << "\n";
            exit(EXIT_FAILURE);
          }
          if (rankA < 0)
            rankA = rank;
          else if (rankB < 0)
            rankB = rank;
          metrics = PartialWrites;
        }
        continue;
      }
      else if (check_argument_equals(*argv, {"--nochannel", "/nc", "-nc"}))
      {
        show_channel_output = false;
        continue;
      }
      else if (check_argument_equals(*argv, {"-pmm", "/pmm", "-pmem", "/pmem"}))
      {
        metrics = Pmem;
        continue;
      }
      else if (check_argument_equals(*argv, {"-all", "/all"}))
      {
        skipInactiveChannels = false;
        continue;
      }
      else if (check_argument_equals(*argv, {"-mixed", "/mixed"}))
      {
        metrics = PmemMixedMode;
        continue;
      }
      else if (check_argument_equals(*argv, {"-mm", "/mm"}))
      {
        metrics = PmemMemoryMode;
        show_channel_output = false;
        continue;
      }
      else if (check_argument_equals(*argv, {"-partial", "/partial"}))
      {
        metrics = PartialWrites;
        continue;
      }
      else if (check_argument_equals(*argv, {"-u", "/u"}))
      {
        print_update = true;
        continue;
      }
      PCM_ENFORCE_FLUSH_OPTION
#ifdef _MSC_VER
      else if (check_argument_equals(*argv, {"--uninstallDriver"}))
      {
        Driver tmpDrvObject;
        tmpDrvObject.uninstall();
        cerr << "msr.sys driver has been uninstalled. You might need to reboot the system to make this effective.\n";
        exit(EXIT_SUCCESS);
      }
      else if (check_argument_equals(*argv, {"--installDriver"}))
      {
        Driver tmpDrvObject = Driver(Driver::msrLocalPath());
        if (!tmpDrvObject.start())
        {
          tcerr << "Can not access CPU counters\n";
          tcerr << "You must have a signed  driver at " << tmpDrvObject.driverPath() << " and have administrator rights to run this program\n";
          exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
      }
#endif
      else if (check_argument_equals(*argv, {"--"}))
      {
        argv++;
        sysCmd = *argv;
        sysArgv = argv;
        break;
      }
      else
      {
        delay = parse_delay(*argv, program, (print_usage_func)print_help);
        continue;
      }
    } while (argc > 1); // end of command line parsing loop

  m->disableJKTWorkaround();
  print_cpu_details();
  const auto cpu_family_model = m->getCPUFamilyModel();
  if (!m->hasPCICFGUncore())
  {
    cerr << "Unsupported processor model (0x" << std::hex << cpu_family_model << std::dec << ").\n";
    if (m->memoryTrafficMetricsAvailable())
      cerr << "For processor-level memory bandwidth statistics please use 'pcm' utility\n";
    exit(EXIT_FAILURE);
  }
  if (anyPmem(metrics) && (m->PMMTrafficMetricsAvailable() == false))
  {
    cerr << "PMM/Pmem traffic metrics are not available on your processor.\n";
    exit(EXIT_FAILURE);
  }
  if (metrics == PmemMemoryMode && m->PMMMemoryModeMetricsAvailable() == false)
  {
    cerr << "PMM Memory Mode metrics are not available on your processor.\n";
    exit(EXIT_FAILURE);
  }
  if (metrics == PmemMixedMode && m->PMMMixedModeMetricsAvailable() == false)
  {
    cerr << "PMM Mixed Mode metrics are not available on your processor.\n";
    exit(EXIT_FAILURE);
  }
  if ((rankA >= 0 || rankB >= 0) && anyPmem(metrics))
  {
    cerr << "PMM/Pmem traffic metrics are not available on rank level\n";
    exit(EXIT_FAILURE);
  }
  if ((rankA >= 0 || rankB >= 0) && !show_channel_output)
  {
    cerr << "Rank level output requires channel output\n";
    exit(EXIT_FAILURE);
  }
  PCM::ErrorCode status = m->programServerUncoreMemoryMetrics(metrics, rankA, rankB);
  m->checkError(status);

  max_imc_channels = (pcm::uint32)m->getMCChannelsPerSocket();

  std::vector<ServerUncoreCounterState> BeforeState(m->getNumSockets());
  std::vector<ServerUncoreCounterState> AfterState(m->getNumSockets());
  uint64 BeforeTime = 0, AfterTime = 0;

  if ((sysCmd != NULL) && (delay <= 0.0))
  {
    // in case external command is provided in command line, and
    // delay either not provided (-1) or is zero
    m->setBlocked(true);
  }
  else
  {
    m->setBlocked(false);
  }

  if (csv)
  {
    if (delay <= 0.0)
      delay = PCM_DELAY_DEFAULT;
  }
  else
  {
    // for non-CSV mode delay < 1.0 does not make a lot of practical sense:
    // hard to read from the screen, or
    // in case delay is not provided in command line => set default
    if (((delay < 1.0) && (delay > 0.0)) || (delay <= 0.0))
      delay = PCM_DELAY_DEFAULT;
  }

  shared_ptr<CHAEventCollector> chaEventCollector;

  SPR_CXL = (PCM::SPR == cpu_family_model || PCM::EMR == cpu_family_model) && (getNumCXLPorts(m) > 0);
  if (SPR_CXL)
  {
    chaEventCollector = std::make_shared<CHAEventCollector>(delay, sysCmd, mainLoop, m);
    assert(chaEventCollector.get());
    chaEventCollector->programFirstGroup();
  }

  cerr << "Update every " << delay << " seconds\n";

  if (csv)
    cerr << "Read/Write values expressed in (MB/s)" << endl;

  readState(BeforeState);

  uint64 SPR_CHA_CXL_Event_Count = 0;

  BeforeTime = m->getTickCount();

  if (sysCmd != NULL)
  {
    MySystem(sysCmd, sysArgv);
  }

  mainLoop([&]()
           {
        if (enforceFlush || !csv) cout << flush;

        if (chaEventCollector.get())
        {
            chaEventCollector->multiplexEvents(BeforeState);
        }
        else
        {
            calibratedSleep(delay, sysCmd, mainLoop, m);
        }

        AfterTime = m->getTickCount();
        readState(AfterState);
        if (chaEventCollector.get())
        {
            SPR_CHA_CXL_Event_Count = chaEventCollector->getTotalCount(AfterState);
            chaEventCollector->reset();
            chaEventCollector->programFirstGroup();
            readState(AfterState); // TODO: re-read only CHA counters (performance optmization)
        }

        if (!csv) {
          //cout << "Time elapsed: " << dec << fixed << AfterTime-BeforeTime << " ms\n";
          //cout << "Called sleep function for " << dec << fixed << delay_ms << " ms\n";
        }

        if(rankA >= 0 || rankB >= 0)
          calculate_bandwidth_rank(m,BeforeState, AfterState, AfterTime - BeforeTime, csv, csvheader, no_columns, rankA, rankB);
        else
          calculate_bandwidth(m,BeforeState,AfterState,AfterTime-BeforeTime,csv,csvheader, no_columns, metrics,
                show_channel_output, print_update, SPR_CHA_CXL_Event_Count);

        swap(BeforeTime, AfterTime);
        swap(BeforeState, AfterState);

        if ( m->isBlocked() ) {
        // in case PCM was blocked after spawning child application: break monitoring loop here
            return false;
        }
        return true; });

  exit(EXIT_SUCCESS);
}
