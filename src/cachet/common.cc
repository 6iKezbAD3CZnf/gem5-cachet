#include "cachet/common.hh"

namespace gem5
{

PacketPtr
createPkt(
        Addr addr,
        size_t size,
        uint32_t flags,
        uint16_t requestorId,
        bool isRead
        )
{
    RequestPtr req(new Request(addr, size, flags, requestorId));
    MemCmd cmd = isRead ? MemCmd::ReadReq : MemCmd::WriteReq;
    PacketPtr retPkt = new Packet(req, cmd);
    uint8_t *reqData = new uint8_t[size]; // just empty here
    retPkt->dataDynamic(reqData);

    return retPkt;
}

} // namespace gem5
