// pcm-memory-exporter.cpp
// SPDX-License-Identifier: BSD-3-Clause
// This code converts the CSV output to Prometheus exporter output.

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <csignal>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/gauge.h>
#include "pcm-memory-exporter.h"
#include "cpucounters.h"
#include "utils.h"

using namespace std;
using namespace pcm;

volatile bool keepRunning = true;

void signalHandler(int signum)
{
	keepRunning = false;
}

int main(int argc, char *argv[])
{
	// Set up signal handlers to gracefully handle termination
	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);

	// Print version information
	cout << "\n Intel(r) Performance Counter Monitor " << PCM_VERSION << "\n";
	cout << "\n This utility measures memory bandwidth and exports metrics via Prometheus\n\n";

	// Initialize PCM
	PCM *m = PCM::getInstance();
	if (m->program() != PCM::Success)
	{
		cerr << "PCM couldn't start. Please check if another instance of PCM is running.\n";
		exit(EXIT_FAILURE);
	}

	// Set up Prometheus Exposer
	prometheus::Exposer exposer{"127.0.0.1:8080"};
	auto registry = std::make_shared<prometheus::Registry>();
	exposer.RegisterCollectable(registry);

	// Create Gauges for metrics
	auto &memoryMetrics = prometheus::BuildGauge()
							  .Name("pcm_memory_bandwidth_bytes")
							  .Help("Memory bandwidth in bytes per second")
							  .Register(*registry);

	// System-level metrics
	auto &systemReadBandwidth = memoryMetrics.Add({{"type", "read"}, {"level", "system"}});
	auto &systemWriteBandwidth = memoryMetrics.Add({{"type", "write"}, {"level", "system"}});
	auto &systemTotalBandwidth = memoryMetrics.Add({{"type", "total"}, {"level", "system"}});

	// Per-socket metrics
	std::vector<prometheus::Gauge *> socketReadBandwidth;
	std::vector<prometheus::Gauge *> socketWriteBandwidth;
	std::vector<prometheus::Gauge *> socketTotalBandwidth;

	uint32 numSockets = m->getNumSockets();
	for (uint32 i = 0; i < numSockets; ++i)
	{
		socketReadBandwidth.push_back(&memoryMetrics.Add({{"type", "read"}, {"level", "socket"}, {"socket", std::to_string(i)}}));
		socketWriteBandwidth.push_back(&memoryMetrics.Add({{"type", "write"}, {"level", "socket"}, {"socket", std::to_string(i)}}));
		socketTotalBandwidth.push_back(&memoryMetrics.Add({{"type", "total"}, {"level", "socket"}, {"socket", std::to_string(i)}}));
	}

	// Display a message indicating that monitoring has started
	cout << "Starting memory bandwidth monitoring...\n";

	// Main data collection loop
	const double delay = 1.0; // Sampling interval in seconds

	while (keepRunning)
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

		// Optionally, print the metrics to the console for verification
		cout << fixed << setprecision(2);
		cout << "System Read Bandwidth: " << sysReadBandwidth / (1024 * 1024) << " MB/sec" << endl;
		cout << "System Write Bandwidth: " << sysWriteBandwidth / (1024 * 1024) << " MB/sec" << endl;
		cout << "System Total Bandwidth: " << sysTotalBandwidth / (1024 * 1024) << " MB/sec" << endl;

		for (uint32 i = 0; i < numSockets; ++i)
		{
			cout << "Socket " << i << " Read Bandwidth: " << socketReadBandwidth[i]->Value() / (1024 * 1024) << " MB/sec" << endl;
			cout << "Socket " << i << " Write Bandwidth: " << socketWriteBandwidth[i]->Value() / (1024 * 1024) << " MB/sec" << endl;
			cout << "Socket " << i << " Total Bandwidth: " << socketTotalBandwidth[i]->Value() / (1024 * 1024) << " MB/sec" << endl;
		}
		cout << endl;
	}

	// Clean up PCM resources
	m->cleanup();

	return 0;
}
