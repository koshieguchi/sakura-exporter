// pcm-memory-exporter.cpp
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/gauge.h>
#include "cpucounters.h"
#include "utils.h"
#include "pcm-memory-exporter.h"

using namespace std;
using namespace pcm;

PCM_MAIN_NOTHROW;

int mainThrows(int argc, char *argv[])
{
	if (print_version(argc, argv))
		exit(EXIT_SUCCESS);

	set_signal_handlers();

	cout << "\n Intel(r) Performance Counter Monitor " << PCM_VERSION << "\n";
	cout << "\n This utility measures memory bandwidth and exports metrics via Prometheus\n\n";

	// Initialize PCM
	PCM *m = PCM::getInstance();
	if (m->program() != PCM::Success)
	{
		cerr << "PCM couldn't start. Please check if another instance of PCM is running.\n";
		exit(EXIT_FAILURE);
	}

	uint32 numSockets = m->getNumSockets();

	// Set up Prometheus Exposer
	prometheus::Exposer exposer{"127.0.0.1:9404"};

	// Create a metrics registry
	auto registry = std::make_shared<prometheus::Registry>();

	// Add the metrics registry to the exposer
	exposer.RegisterCollectable(registry);

	// Create gauge metrics for memory bandwidth
	auto &memory_family = prometheus::BuildGauge()
							  .Name("pcm_memory_bandwidth_bytes_per_second")
							  .Help("PCM Memory Bandwidth in bytes per second")
							  .Register(*registry);

	// Create system-level metrics
	auto &systemReadBandwidth = memory_family.Add({{"type", "read"}, {"level", "system"}});
	auto &systemWriteBandwidth = memory_family.Add({{"type", "write"}, {"level", "system"}});
	auto &systemTotalBandwidth = memory_family.Add({{"type", "total"}, {"level", "system"}});

	// Create per-socket metrics
	std::vector<prometheus::Gauge *> socketReadBandwidth(numSockets);
	std::vector<prometheus::Gauge *> socketWriteBandwidth(numSockets);
	std::vector<prometheus::Gauge *> socketTotalBandwidth(numSockets);

	for (uint32 i = 0; i < numSockets; ++i)
	{
		socketReadBandwidth[i] = &memory_family.Add({{"socket", std::to_string(i)}, {"type", "read"}, {"level", "socket"}});
		socketWriteBandwidth[i] = &memory_family.Add({{"socket", std::to_string(i)}, {"type", "write"}, {"level", "socket"}});
		socketTotalBandwidth[i] = &memory_family.Add({{"socket", std::to_string(i)}, {"type", "total"}, {"level", "socket"}});
	}

	cout << "\n------\n[INFO] Starting Prometheus exporter on port: 9404" << std::endl;

	MainLoop mainLoop;
	double delay = 1.0; // Sampling interval in seconds

	mainLoop([&]()
			 {
        // Collect counter states before the delay
        SystemCounterState sysBeforeState = getSystemCounterState();
        std::vector<SocketCounterState> sktBeforeState(numSockets);
        for (uint32 i = 0; i < numSockets; ++i)
        {
            sktBeforeState[i] = getSocketCounterState(i);
        }

        // Sleep for the specified delay
        MySleepMs(static_cast<int>(delay * 1000));

        // Collect counter states after the delay
        SystemCounterState sysAfterState = getSystemCounterState();
        std::vector<SocketCounterState> sktAfterState(numSockets);
        for (uint32 i = 0; i < numSockets; ++i)
        {
            sktAfterState[i] = getSocketCounterState(i);
        }

        // Calculate system-level bandwidth
        double sysReadBandwidth = getBytesReadFromMC(sysBeforeState, sysAfterState) / delay;
        double sysWriteBandwidth = getBytesWrittenToMC(sysBeforeState, sysAfterState) / delay;
        double sysTotalBandwidth = sysReadBandwidth + sysWriteBandwidth;

        // Update system-level Prometheus metrics
        systemReadBandwidth.Set(sysReadBandwidth);
        systemWriteBandwidth.Set(sysWriteBandwidth);
        systemTotalBandwidth.Set(sysTotalBandwidth);

        // Calculate and update per-socket bandwidth metrics
        for (uint32 i = 0; i < numSockets; ++i)
        {
            double sktReadBandwidth = getBytesReadFromMC(sktBeforeState[i], sktAfterState[i]) / delay;
            double sktWriteBandwidth = getBytesWrittenToMC(sktBeforeState[i], sktAfterState[i]) / delay;
            double sktTotalBandwidth = sktReadBandwidth + sktWriteBandwidth;

            socketReadBandwidth[i]->Set(sktReadBandwidth);
            socketWriteBandwidth[i]->Set(sktWriteBandwidth);
            socketTotalBandwidth[i]->Set(sktTotalBandwidth);
        }

        return true; });

	// Clean up PCM resources
	m->cleanup();

	return 0;
}
