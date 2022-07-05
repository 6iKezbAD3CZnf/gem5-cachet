#ifndef __CACHET_COMMON_HH__
#define __CACHET_COMMON_HH__

#define META_BORDER 0x400000000

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
