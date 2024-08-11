// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2020-2022, Intel Corporation
#pragma once
// written by Roman Sudarikov

#include <iostream>
#include "cpucounters.h"
#include "utils.h"
#include <vector>
#include <array>
#include <string>
#include <initializer_list>
#include <algorithm>

#if defined(_MSC_VER)
typedef unsigned int uint;
#endif

using namespace std;
using namespace pcm;
#define NUM_SAMPLES (1)

class IPlatform
{
  void init();

public:
  IPlatform(PCM *m, bool csv, bool bandwidth, bool verbose);
  virtual void getEvents() = 0;
  virtual void cleanup() = 0;
  virtual uint64 getReadBw() = 0;
  virtual uint64 getWriteBw() = 0;
  static IPlatform *getPlatform(PCM *m, bool csv, bool bandwidth,
                                bool verbose, uint32 delay);
  virtual ~IPlatform() {}

protected:
  PCM *m_pcm;
  bool m_csv;
  bool m_bandwidth;
  bool m_verbose;
  uint m_socketCount;

  enum eventFilter
  {
    TOTAL,
    MISS,
    HIT,
    fltLast
  };

  vector<string> filterNames, bwNames;
};

void IPlatform::init()
{
  print_cpu_details();

  if (m_pcm->isSomeCoreOfflined())
  {
    cerr << "Core offlining is not supported. Program aborted\n";
    exit(EXIT_FAILURE);
  }
}

IPlatform::IPlatform(PCM *m, bool csv, bool bandwidth, bool verbose) : m_pcm(m),
                                                                       filterNames{"(Total)", "(Miss)", "(Hit)"},
                                                                       bwNames{"PCIe Rd (B)", "PCIe Wr (B)"}
{
  m_csv = csv;
  m_bandwidth = bandwidth;
  m_verbose = verbose;
  m_socketCount = m_pcm->getNumSockets();

  init();
}

/*
 * Common API to program, access and represent required Uncore counters.
 * The only difference is event opcodes and the way how bandwidth is calculated.
 */
class LegacyPlatform : public IPlatform
{
  enum
  {
    before,
    after,
    total
  };
  vector<string> eventNames;
  vector<eventGroup_t> eventGroups;
  uint32 m_delay;
  typedef vector<vector<uint64>> eventCount_t;
  array<eventCount_t, total> eventCount;

  virtual void getEvents() final;
  virtual void cleanup() final;

  uint64 getEventCount(uint socket, uint idx);
  uint eventGroupOffset(eventGroup_t &eventGroup);
  void getEventGroup(eventGroup_t &eventGroup);

public:
  LegacyPlatform(initializer_list<string> events, initializer_list<eventGroup_t> eventCodes,
                 PCM *m, bool csv, bool bandwidth, bool verbose, uint32 delay) : IPlatform(m, csv, bandwidth, verbose),
                                                                                 eventNames(events), eventGroups(eventCodes)
  {
    int eventsCount = 0;
    for (auto &group : eventGroups)
      eventsCount += (int)group.size();

    // Delay for each multiplexing group. Counters will be scaled.
    m_delay = uint32(delay / eventGroups.size() / NUM_SAMPLES);

    eventSample.resize(m_socketCount);
    for (auto &e : eventSample)
      e.resize(eventsCount);

    for (auto &run : eventCount)
    {
      run.resize(m_socketCount);
      for (auto &events_ : run)
        events_.resize(eventsCount);
    }
  };
  virtual uint64 getReadBw() = 0;
  virtual uint64 getWriteBw() = 0;

protected:
  vector<vector<uint64>> eventSample;
  virtual uint64 event(uint socket, eventFilter filter, uint idx) = 0;
};

void LegacyPlatform::cleanup()
{
  for (auto &socket : eventSample)
    fill(socket.begin(), socket.end(), 0);
}

inline uint64 LegacyPlatform::getEventCount(uint skt, uint idx)
{
  return eventGroups.size() * (eventCount[after][skt][idx] -
                               eventCount[before][skt][idx]);
}

uint LegacyPlatform::eventGroupOffset(eventGroup_t &eventGroup)
{
  uint offset = 0;
  uint grpIdx = (uint)(&eventGroup - eventGroups.data());

  for (auto iter = eventGroups.begin(); iter < eventGroups.begin() + grpIdx; iter++)
    offset += (uint)iter->size();

  return offset;
}

void LegacyPlatform::getEventGroup(eventGroup_t &eventGroup)
{
  m_pcm->programPCIeEventGroup(eventGroup);
  uint offset = eventGroupOffset(eventGroup);

  for (int run = before; run < total; run++)
  {
    for (uint skt = 0; skt < m_socketCount; ++skt)
      for (uint ctr = 0; ctr < eventGroup.size(); ++ctr)
        eventCount[run][skt][ctr + offset] = m_pcm->getPCIeCounterData(skt, ctr);
    if (run == before)
      MySleepMs(m_delay);
  }

  for (uint skt = 0; skt < m_socketCount; ++skt)
    for (uint idx = offset; idx < offset + eventGroup.size(); ++idx)
      eventSample[skt][idx] += getEventCount(skt, idx);
}

void LegacyPlatform::getEvents()
{
  for (auto &evGroup : eventGroups)
    getEventGroup(evGroup);
}

// BHS
class BirchStreamPlatform : public LegacyPlatform
{
public:
  BirchStreamPlatform(PCM *m, bool csv, bool bandwidth, bool verbose, uint32 delay) : LegacyPlatform({"PCIRdCur", "ItoM", "ItoMCacheNear", "UCRdF", "WiL", "WCiL", "WCiLF"},
                                                                                                     {
                                                                                                         {0xC8F3FE00000435, 0xC8F3FD00000435, 0xCC43FE00000435, 0xCC43FD00000435},
                                                                                                         {0xCD43FE00000435, 0xCD43FD00000435, 0xC877DE00000135, 0xC87FDE00000135},
                                                                                                         {
                                                                                                             0xC86FFE00000135,
                                                                                                             0xC867FE00000135,
                                                                                                         },
                                                                                                     },
                                                                                                     m, csv, bandwidth, verbose, delay) {
                                                                                      };
  virtual uint64 getReadBw();
  virtual uint64 getWriteBw();

private:
  enum eventIdx
  {
    PCIRdCur,
    ItoM,
    ItoMCacheNear,
    UCRdF,
    WiL,
    WCiL,
    WCiLF
  };

  enum Events
  {
    PCIRdCur_miss,
    PCIRdCur_hit,
    ItoM_miss,
    ItoM_hit,
    ItoMCacheNear_miss,
    ItoMCacheNear_hit,
    UCRdF_miss,
    WiL_miss,
    WCiL_miss,
    WCiLF_miss,
    eventLast
  };
  virtual uint64 event(uint socket, eventFilter filter, uint idx);
};

uint64 BirchStreamPlatform::event(uint socket, eventFilter filter, uint idx)
{
  uint64 event = 0;
  switch (idx)
  {
  case PCIRdCur:
    if (filter == TOTAL)
      event = eventSample[socket][PCIRdCur_miss] +
              eventSample[socket][PCIRdCur_hit];
    else if (filter == MISS)
      event = eventSample[socket][PCIRdCur_miss];
    else if (filter == HIT)
      event = eventSample[socket][PCIRdCur_hit];
    break;
  case ItoM:
    if (filter == TOTAL)
      event = eventSample[socket][ItoM_miss] +
              eventSample[socket][ItoM_hit];
    else if (filter == MISS)
      event = eventSample[socket][ItoM_miss];
    else if (filter == HIT)
      event = eventSample[socket][ItoM_hit];
    break;
  case ItoMCacheNear:
    if (filter == TOTAL)
      event = eventSample[socket][ItoMCacheNear_miss] +
              eventSample[socket][ItoMCacheNear_hit];
    else if (filter == MISS)
      event = eventSample[socket][ItoMCacheNear_miss];
    else if (filter == HIT)
      event = eventSample[socket][ItoMCacheNear_hit];
    break;
  case UCRdF:
    if (filter == TOTAL || filter == MISS)
      event = eventSample[socket][UCRdF_miss];
    break;
  case WiL:
    if (filter == TOTAL || filter == MISS)
      event = eventSample[socket][WiL_miss];
    break;
  case WCiL:
    if (filter == TOTAL || filter == MISS)
      event = eventSample[socket][WCiL_miss];
    break;
  case WCiLF:
    if (filter == TOTAL || filter == MISS)
      event = eventSample[socket][WCiLF_miss];
    break;
  default:
    break;
  }
  return event;
}

uint64 BirchStreamPlatform::getReadBw()
{
  uint64 readBw = 0;
  for (uint socket = 0; socket < m_socketCount; socket++)
    readBw += (event(socket, TOTAL, PCIRdCur));
  return (readBw * 64ULL);
}

uint64 BirchStreamPlatform::getWriteBw()
{
  uint64 writeBw = 0;
  for (uint socket = 0; socket < m_socketCount; socket++)
    writeBw += (event(socket, TOTAL, ItoM) +
                event(socket, TOTAL, ItoMCacheNear));
  return (writeBw * 64ULL);
}

// SPR
class EagleStreamPlatform : public LegacyPlatform
{
public:
  EagleStreamPlatform(PCM *m, bool csv, bool bandwidth, bool verbose, uint32 delay) : LegacyPlatform({"PCIRdCur", "ItoM", "ItoMCacheNear", "UCRdF", "WiL", "WCiL", "WCiLF"},
                                                                                                     {
                                                                                                         {0xC8F3FE00000435, 0xC8F3FD00000435, 0xCC43FE00000435, 0xCC43FD00000435},
                                                                                                         {0xCD43FE00000435, 0xCD43FD00000435, 0xC877DE00000135, 0xC87FDE00000135},
                                                                                                         {
                                                                                                             0xC86FFE00000135,
                                                                                                             0xC867FE00000135,
                                                                                                         },
                                                                                                     },
                                                                                                     m, csv, bandwidth, verbose, delay) {
                                                                                      };

  virtual uint64 getReadBw();
  virtual uint64 getWriteBw();

private:
  enum eventIdx
  {
    PCIRdCur,
    ItoM,
    ItoMCacheNear,
    UCRdF,
    WiL,
    WCiL,
    WCiLF
  };

  enum Events
  {
    PCIRdCur_miss,
    PCIRdCur_hit,
    ItoM_miss,
    ItoM_hit,
    ItoMCacheNear_miss,
    ItoMCacheNear_hit,
    UCRdF_miss,
    WiL_miss,
    WCiL_miss,
    WCiLF_miss,
    eventLast
  };
  virtual uint64 event(uint socket, eventFilter filter, uint idx);
};

uint64 EagleStreamPlatform::event(uint socket, eventFilter filter, uint idx)
{
  uint64 event = 0;
  switch (idx)
  {
  case PCIRdCur:
    if (filter == TOTAL)
      event = eventSample[socket][PCIRdCur_miss] +
              eventSample[socket][PCIRdCur_hit];
    else if (filter == MISS)
      event = eventSample[socket][PCIRdCur_miss];
    else if (filter == HIT)
      event = eventSample[socket][PCIRdCur_hit];
    break;
  case ItoM:
    if (filter == TOTAL)
      event = eventSample[socket][ItoM_miss] +
              eventSample[socket][ItoM_hit];
    else if (filter == MISS)
      event = eventSample[socket][ItoM_miss];
    else if (filter == HIT)
      event = eventSample[socket][ItoM_hit];
    break;
  case ItoMCacheNear:
    if (filter == TOTAL)
      event = eventSample[socket][ItoMCacheNear_miss] +
              eventSample[socket][ItoMCacheNear_hit];
    else if (filter == MISS)
      event = eventSample[socket][ItoMCacheNear_miss];
    else if (filter == HIT)
      event = eventSample[socket][ItoMCacheNear_hit];
    break;
  case UCRdF:
    if (filter == TOTAL || filter == MISS)
      event = eventSample[socket][UCRdF_miss];
    break;
  case WiL:
    if (filter == TOTAL || filter == MISS)
      event = eventSample[socket][WiL_miss];
    break;
  case WCiL:
    if (filter == TOTAL || filter == MISS)
      event = eventSample[socket][WCiL_miss];
    break;
  case WCiLF:
    if (filter == TOTAL || filter == MISS)
      event = eventSample[socket][WCiLF_miss];
    break;
  default:
    break;
  }
  return event;
}

uint64 EagleStreamPlatform::getReadBw()
{
  uint64 readBw = 0;
  for (uint socket = 0; socket < m_socketCount; socket++)
    readBw += (event(socket, TOTAL, PCIRdCur));
  return (readBw * 64ULL);
}

uint64 EagleStreamPlatform::getWriteBw()
{
  uint64 writeBw = 0;
  for (uint socket = 0; socket < m_socketCount; socket++)
    writeBw += (event(socket, TOTAL, ItoM) +
                event(socket, TOTAL, ItoMCacheNear));
  return (writeBw * 64ULL);
}

// ICX
class WhitleyPlatform : public LegacyPlatform
{
public:
  WhitleyPlatform(PCM *m, bool csv, bool bandwidth, bool verbose, uint32 delay) : LegacyPlatform({"PCIRdCur", "ItoM", "ItoMCacheNear", "UCRdF", "WiL"},
                                                                                                 {
                                                                                                     {0xC8F3FE00000435, 0xC8F3FD00000435, 0xCC43FE00000435, 0xCC43FD00000435},
                                                                                                     {0xCD43FE00000435, 0xCD43FD00000435, 0xC877DE00000135, 0xC87FDE00000135},
                                                                                                 },
                                                                                                 m, csv, bandwidth, verbose, delay) {
                                                                                  };

  virtual uint64 getReadBw();
  virtual uint64 getWriteBw();

private:
  enum eventIdx
  {
    PCIRdCur,
    ItoM,
    ItoMCacheNear,
    UCRdF,
    WiL,
  };

  enum Events
  {
    PCIRdCur_miss,
    PCIRdCur_hit,
    ItoM_miss,
    ItoM_hit,
    ItoMCacheNear_miss,
    ItoMCacheNear_hit,
    UCRdF_miss,
    WiL_miss,
    eventLast
  };
  virtual uint64 event(uint socket, eventFilter filter, uint idx);
};

uint64 WhitleyPlatform::event(uint socket, eventFilter filter, uint idx)
{
  uint64 event = 0;
  switch (idx)
  {
  case PCIRdCur:
    if (filter == TOTAL)
      event = eventSample[socket][PCIRdCur_miss] +
              eventSample[socket][PCIRdCur_hit];
    else if (filter == MISS)
      event = eventSample[socket][PCIRdCur_miss];
    else if (filter == HIT)
      event = eventSample[socket][PCIRdCur_hit];
    break;
  case ItoM:
    if (filter == TOTAL)
      event = eventSample[socket][ItoM_miss] +
              eventSample[socket][ItoM_hit];
    else if (filter == MISS)
      event = eventSample[socket][ItoM_miss];
    else if (filter == HIT)
      event = eventSample[socket][ItoM_hit];
    break;
  case ItoMCacheNear:
    if (filter == TOTAL)
      event = eventSample[socket][ItoMCacheNear_miss] +
              eventSample[socket][ItoMCacheNear_hit];
    else if (filter == MISS)
      event = eventSample[socket][ItoMCacheNear_miss];
    else if (filter == HIT)
      event = eventSample[socket][ItoMCacheNear_hit];
    break;
  case UCRdF:
    if (filter == TOTAL || filter == MISS)
      event = eventSample[socket][UCRdF_miss];
    break;
  case WiL:
    if (filter == TOTAL || filter == MISS)
      event = eventSample[socket][WiL_miss];
    break;
  default:
    break;
  }
  return event;
}

uint64 WhitleyPlatform::getReadBw()
{
  uint64 readBw = 0;
  for (uint socket = 0; socket < m_socketCount; socket++)
    readBw += (event(socket, TOTAL, PCIRdCur));
  return (readBw * 64ULL);
}

uint64 WhitleyPlatform::getWriteBw()
{
  uint64 writeBw = 0;
  for (uint socket = 0; socket < m_socketCount; socket++)
    writeBw += (event(socket, TOTAL, ItoM) +
                event(socket, TOTAL, ItoMCacheNear));
  return (writeBw * 64ULL);
}

// CLX, SKX
class PurleyPlatform : public LegacyPlatform
{
public:
  PurleyPlatform(PCM *m, bool csv, bool bandwidth, bool verbose, uint32 delay) : LegacyPlatform({"PCIRdCur", "RFO", "CRd", "DRd", "ItoM", "PRd", "WiL"},
                                                                                                {
                                                                                                    {0x00043c33}, // PCIRdCur_miss
                                                                                                    {0x00043c37}, // PCIRdCur_hit
                                                                                                    {0x00040033}, // RFO_miss
                                                                                                    {0x00040037}, // RFO_hit
                                                                                                    {0x00040233}, // CRd_miss
                                                                                                    {0x00040237}, // CRd_hit
                                                                                                    {0x00040433}, // DRd_miss
                                                                                                    {0x00040437}, // DRd_hit
                                                                                                    {0x00049033}, // ItoM_miss
                                                                                                    {0x00049037}, // ItoM_hit
                                                                                                    {0x40040e33}, // PRd_miss
                                                                                                    {0x40040e37}, // PRd_hit
                                                                                                    {0x40041e33}, // WiL_miss
                                                                                                    {0x40041e37}, // WiL_hit
                                                                                                },
                                                                                                m, csv, bandwidth, verbose, delay) {
                                                                                 };

  virtual uint64 getReadBw();
  virtual uint64 getWriteBw();

private:
  enum eventIdx
  {
    PCIRdCur,
    RFO,
    CRd,
    DRd,
    ItoM,
    PRd,
    WiL,
  };

  enum Events
  {
    PCIRdCur_miss,
    PCIRdCur_hit,
    RFO_miss,
    RFO_hit,
    CRd_miss,
    CRd_hit,
    DRd_miss,
    DRd_hit,
    ItoM_miss,
    ItoM_hit,
    PRd_miss,
    PRd_hit,
    WiL_miss,
    WiL_hit,
  };
  virtual uint64 event(uint socket, eventFilter filter, uint idx);
};

uint64 PurleyPlatform::event(uint socket, eventFilter filter, uint idx)
{
  uint64 event = 0;
  if (filter == TOTAL)
    event = eventSample[socket][2 * idx] +
            eventSample[socket][2 * idx + 1];
  else if (filter == MISS)
    event = eventSample[socket][2 * idx];
  else if (filter == HIT)
    event = eventSample[socket][2 * idx + 1];

  return event;
}

uint64 PurleyPlatform::getReadBw()
{
  uint64 readBw = 0;
  for (uint socket = 0; socket < m_socketCount; socket++)
    readBw += (event(socket, TOTAL, PCIRdCur) +
               event(socket, TOTAL, RFO) +
               event(socket, TOTAL, CRd) +
               event(socket, TOTAL, DRd));
  return (readBw * 64ULL);
}

uint64 PurleyPlatform::getWriteBw()
{
  uint64 writeBw = 0;
  for (uint socket = 0; socket < m_socketCount; socket++)
    writeBw += (event(socket, TOTAL, RFO) +
                event(socket, TOTAL, ItoM));
  return (writeBw * 64ULL);
}

// BDX, HSX
class GrantleyPlatform : public LegacyPlatform
{
public:
  GrantleyPlatform(PCM *m, bool csv, bool bandwidth, bool verbose, uint32 delay) : LegacyPlatform({"PCIRdCur", "RFO", "CRd", "DRd", "ItoM", "PRd", "WiL"},
                                                                                                  {
                                                                                                      {0x19e10000}, // PCIRdCur_miss
                                                                                                      {0x19e00000}, // PCIRdCur_total
                                                                                                      {0x18030000}, // RFO_miss
                                                                                                      {0x18020000}, // RFO_total
                                                                                                      {0x18110000}, // CRd_miss
                                                                                                      {0x18100000}, // CRd_total
                                                                                                      {0x18210000}, // DRd_miss
                                                                                                      {0x18200000}, // DRd_total
                                                                                                      {0x1c830000}, // ItoM_miss
                                                                                                      {0x1c820000}, // ItoM_total
                                                                                                      {0x18710000}, // PRd_miss
                                                                                                      {0x18700000}, // PRd_total
                                                                                                      {0x18f10000}, // WiL_miss
                                                                                                      {0x18f00000}, // WiL_total
                                                                                                  },
                                                                                                  m, csv, bandwidth, verbose, delay) {
                                                                                   };

  virtual uint64 getReadBw();
  virtual uint64 getWriteBw();

private:
  enum eventIdx
  {
    PCIRdCur,
    RFO,
    CRd,
    DRd,
    ItoM,
    PRd,
    WiL,
  };

  enum Events
  {
    PCIRdCur_miss,
    PCIRdCur_total,
    RFO_miss,
    RFO_total,
    CRd_miss,
    CRd_total,
    DRd_miss,
    DRd_total,
    ItoM_miss,
    ItoM_total,
    PRd_miss,
    PRd_total,
    WiL_miss,
    WiL_total,
  };
  virtual uint64 event(uint socket, eventFilter filter, uint idx);
};

uint64 GrantleyPlatform::event(uint socket, eventFilter filter, uint idx)
{
  uint64 event = 0;
  if (filter == HIT)
    if (eventSample[socket][2 * idx] < eventSample[socket][2 * idx + 1])
      event = eventSample[socket][2 * idx + 1] - eventSample[socket][2 * idx];
    else
      event = 0;
  else if (filter == MISS)
    event = eventSample[socket][2 * idx];
  else if (filter == TOTAL)
    event = eventSample[socket][2 * idx + 1];

  return event;
}

uint64 GrantleyPlatform::getReadBw()
{
  uint64 readBw = 0;
  for (uint socket = 0; socket < m_socketCount; socket++)
    readBw += (event(socket, TOTAL, PCIRdCur) +
               event(socket, TOTAL, RFO) +
               event(socket, TOTAL, CRd) +
               event(socket, TOTAL, DRd));
  return (readBw * 64ULL);
}

uint64 GrantleyPlatform::getWriteBw()
{
  uint64 writeBw = 0;
  for (uint socket = 0; socket < m_socketCount; socket++)
    writeBw += (event(socket, TOTAL, RFO) +
                event(socket, TOTAL, ItoM));
  return (writeBw * 64ULL);
}

// IVT, JKT
class BromolowPlatform : public LegacyPlatform
{
public:
  BromolowPlatform(PCM *m, bool csv, bool bandwidth, bool verbose, uint32 delay) : LegacyPlatform({"PCIeRdCur", "PCIeNSRd", "PCIeWiLF", "PCIeItoM", "PCIeNSWr", "PCIeNSWrF"},
                                                                                                  {
                                                                                                      {0x19e10000}, // PCIeRdCur_miss
                                                                                                      {0x19e00000}, // PCIeRdCur_total
                                                                                                      {0x1e410000}, // PCIeNSRd_miss
                                                                                                      {0x1e400000}, // PCIeNSRd_total
                                                                                                      {0x19410000}, // PCIeWiLF_miss
                                                                                                      {0x19400000}, // PCIeWiLF_total
                                                                                                      {0x19c10000}, // PCIeItoM_miss
                                                                                                      {0x19c00000}, // PCIeItoM_total
                                                                                                      {0x1e510000}, // PCIeNSWr_miss
                                                                                                      {0x1e500000}, // PCIeNSWr_total
                                                                                                      {0x1e610000}, // PCIeNSWrF_miss
                                                                                                      {0x1e600000}, // PCIeNSWrF_total
                                                                                                  },
                                                                                                  m, csv, bandwidth, verbose, delay) {
                                                                                   };

  virtual uint64 getReadBw();
  virtual uint64 getWriteBw();

private:
  enum eventIdx
  {
    PCIeRdCur,
    PCIeNSRd,
    PCIeWiLF,
    PCIeItoM,
    PCIeNSWr,
    PCIeNSWrF,
  };

  enum Events
  {
    PCIeRdCur_miss,
    PCIeRdCur_total,
    PCIeNSRd_miss,
    PCIeNSRd_total,
    PCIeWiLF_miss,
    PCIeWiLF_total,
    PCIeItoM_miss,
    PCIeItoM_total,
    PCIeNSWr_miss,
    PCIeNSWr_total,
    PCIeNSWrF_miss,
    PCIeNSWrF_total,
  };
  virtual uint64 event(uint socket, eventFilter filter, uint idx);
};

uint64 BromolowPlatform::event(uint socket, eventFilter filter, uint idx)
{
  uint64 event = 0;
  if (filter == HIT)
    if (eventSample[socket][2 * idx] < eventSample[socket][2 * idx + 1])
      event = eventSample[socket][2 * idx + 1] - eventSample[socket][2 * idx];
    else
      event = 0;
  else if (filter == MISS)
    event = eventSample[socket][2 * idx];
  else if (filter == TOTAL)
    event = eventSample[socket][2 * idx + 1];

  return event;
}

uint64 BromolowPlatform::getReadBw()
{
  uint64 readBw = 0;
  for (uint socket = 0; socket < m_socketCount; socket++)
    readBw += (event(socket, TOTAL, PCIeRdCur) +
               event(socket, TOTAL, PCIeNSWr));
  return (readBw * 64ULL);
}

uint64 BromolowPlatform::getWriteBw()
{
  uint64 writeBw = 0;
  for (uint socket = 0; socket < m_socketCount; socket++)
    writeBw += (event(socket, TOTAL, PCIeWiLF) +
                event(socket, TOTAL, PCIeItoM) +
                event(socket, TOTAL, PCIeNSWr) +
                event(socket, TOTAL, PCIeNSWrF));
  return (writeBw * 64ULL);
}
