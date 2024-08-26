#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include "cpucounters.h"
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/gauge.h>

using namespace pcm;

// Common PCIe events
enum class PCIeEvent
{
  PCIRdCur,
  ItoM,
  ItoMCacheNear,
  UCRdF,
  WiL,
  WCiL,
  WCiLF,
  RFO,
  CRd,
  DRd,
  PRd
};

// Generic Platform class
class GenericPlatform
{
protected:
  PCM *m_pcm;
  std::vector<std::vector<uint64_t>> eventSample;
  std::vector<PCIeEvent> supportedEvents;
  uint32_t m_socketCount;

public:
  GenericPlatform(PCM *m, const std::vector<PCIeEvent> &events)
      : m_pcm(m), supportedEvents(events), m_socketCount(m->getNumSockets())
  {
    eventSample.resize(m_socketCount);
    for (auto &socket : eventSample)
    {
      socket.resize(events.size() * 2, 0); // *2 for miss and hit counters
    }
  }

  virtual ~GenericPlatform() = default;

  virtual void getEvents()
  {
    // To be implemented in derived classes
  }

  virtual uint64_t getReadBw()
  {
    // To be implemented in derived classes
    return 0;
  }

  virtual uint64_t getWriteBw()
  {
    // To be implemented in derived classes
    return 0;
  }

protected:
  virtual uint64_t event(uint socket, bool isMiss, PCIeEvent idx)
  {
    size_t index = static_cast<size_t>(idx) * 2 + (isMiss ? 0 : 1);
    return eventSample[socket][index];
  }
};

// BirchStream Platform
class BirchStreamPlatform : public GenericPlatform
{
public:
  BirchStreamPlatform(PCM *m) : GenericPlatform(m, {PCIeEvent::PCIRdCur, PCIeEvent::ItoM, PCIeEvent::ItoMCacheNear,
                                                    PCIeEvent::UCRdF, PCIeEvent::WiL, PCIeEvent::WCiL, PCIeEvent::WCiLF}) {}

  void getEvents() override
  {
    // Implement event gathering for BirchStream
    // This is a placeholder implementation
    for (size_t socket = 0; socket < m_socketCount; ++socket)
    {
      for (size_t i = 0; i < supportedEvents.size(); ++i)
      {
        eventSample[socket][i * 2] = m_pcm->getPCIeCounterData(socket, i * 2);         // Miss
        eventSample[socket][i * 2 + 1] = m_pcm->getPCIeCounterData(socket, i * 2 + 1); // Hit
      }
    }
  }

  uint64_t getReadBw() override
  {
    uint64_t readBw = 0;
    for (uint socket = 0; socket < m_socketCount; socket++)
    {
      readBw += event(socket, true, PCIeEvent::PCIRdCur) + event(socket, false, PCIeEvent::PCIRdCur);
    }
    return readBw * 64ULL;
  }

  uint64_t getWriteBw() override
  {
    uint64_t writeBw = 0;
    for (uint socket = 0; socket < m_socketCount; socket++)
    {
      writeBw += event(socket, true, PCIeEvent::ItoM) + event(socket, false, PCIeEvent::ItoM) +
                 event(socket, true, PCIeEvent::ItoMCacheNear) + event(socket, false, PCIeEvent::ItoMCacheNear);
    }
    return writeBw * 64ULL;
  }
};

// Prometheus exporter class
class PrometheusExporter
{
private:
  prometheus::Exposer exposer;
  std::shared_ptr<prometheus::Registry> registry;
  prometheus::Family<prometheus::Gauge> &pcie_bandwidth_family;
  prometheus::Gauge &read_bw_gauge;
  prometheus::Gauge &write_bw_gauge;

public:
  PrometheusExporter(const std::string &address)
      : exposer(address),
        registry(std::make_shared<prometheus::Registry>()),
        pcie_bandwidth_family(
            prometheus::BuildGauge()
                .Name("pcie_bandwidth")
                .Help("PCIe bandwidth in bytes per second")
                .Register(*registry)),
        read_bw_gauge(pcie_bandwidth_family.Add({{"direction", "read"}})),
        write_bw_gauge(pcie_bandwidth_family.Add({{"direction", "write"}}))
  {
    exposer.RegisterCollectable(registry);
  }

  void updateMetrics(uint64_t readBw, uint64_t writeBw)
  {
    read_bw_gauge.Set(static_cast<double>(readBw));
    write_bw_gauge.Set(static_cast<double>(writeBw));
  }
};

// Factory function to create the appropriate platform
std::unique_ptr<GenericPlatform> createPlatform(PCM *m)
{
  switch (m->getCPUModel())
  {
  case PCM::SRF:
  case PCM::SPR:
  case PCM::EMR:
    return std::make_unique<BirchStreamPlatform>(m);
  // Add cases for other platforms as needed
  default:
    throw std::runtime_error("Unsupported CPU model");
  }
}

int main()
{
  PCM *m = PCM::getInstance();
  if (!m)
  {
    std::cerr << "Cannot access CPU counters" << std::endl;
    return 1;
  }

  std::unique_ptr<GenericPlatform> platform;
  try
  {
    platform = createPlatform(m);
  }
  catch (const std::exception &e)
  {
    std::cerr << "Error creating platform: " << e.what() << std::endl;
    return 1;
  }

  PrometheusExporter exporter("127.0.0.1:9402");
  std::cout << "\n------\n[INFO] Starting Prometheus exporter on port: 9402" << std::endl;

  while (true)
  {
    platform->getEvents();

    uint64_t readBw = platform->getReadBw();
    uint64_t writeBw = platform->getWriteBw();

    exporter.updateMetrics(readBw, writeBw);

    std::this_thread::sleep_for(std::chrono::seconds(10));
  }

  return 0;
}
