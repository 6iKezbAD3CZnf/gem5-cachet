#ifndef __CACHET_COMMON_HH__
#define __CACHET_COMMON_HH__

#define AT_START 0x400000000
#define MAC_START 0x400040000
#define CNT_START 0x480040000
#define MT_START 0x490040000
#define RT_START 0x4924d2480

#define HASH_CYCLE 40
#define TICK_PER_CYCLE 1000

#include "mem/port.hh"
#include "mem/request.hh"
#include "sim/sim_object.hh"

namespace gem5
{

PacketPtr createPkt(
        Addr addr,
        size_t size,
        uint32_t flags,
        uint16_t requestorid,
        bool isRead
        );

} // namespace gem5

#endif // __CACHET_COMMON_HH__
