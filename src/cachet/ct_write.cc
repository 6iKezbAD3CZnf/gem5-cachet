#include "cachet/ct_write.hh"

#include "base/trace.hh"
#include "debug/CTWrite.hh"

namespace gem5
{

CTWrite::CTWrite(const CTWriteParams &p) :
    SimObject(p),
    cpuSidePort(name() + ".cpu_side_port", this),
    memSidePort(name() + ".mem_side_port", this),
    requestPkt(nullptr)
{
    DPRINTF(CTWrite, "Constructing\n");
}

void
CTWrite::CPUSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPkt != nullptr, "Should never try to send if blocked!");

    if (!sendTimingResp(pkt)) {
        blockedPkt = pkt;
    }
}

void
CTWrite::CPUSidePort::trySendRetry()
{
    if (needRetry && blockedPkt == nullptr) {
        needRetry = false;
        DPRINTF(CTWrite, "Sending retry req for %d\n", id);
        sendRetryReq();
    }
}

AddrRangeList
CTWrite::CPUSidePort::getAddrRanges() const
{
    return ctrl->getAddrRanges();
}

Tick
CTWrite::CPUSidePort::recvAtomic(PacketPtr pkt)
{
    return ctrl->handleAtomic(pkt);
}

void
CTWrite::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    ctrl->handleFunctional(pkt);
}

bool
CTWrite::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    if (!ctrl->handleRequest(pkt)) {
        needRetry = true;
        return false;
    } else {
        return true;
    }
}

void
CTWrite::CPUSidePort::recvRespRetry()
{
    assert(blockedPkt != nullptr);

    PacketPtr pkt = blockedPkt;
    blockedPkt= nullptr;

    sendPacket(pkt);
}

void
CTWrite::MemSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPkt != nullptr, "Should never try to send if blocked!");

    assert(pkt->needsResponse());

    if (!sendTimingReq(pkt)) {
        blockedPkt = pkt;
    }
}

bool
CTWrite::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    return ctrl->handleResponse(pkt);
}

void
CTWrite::MemSidePort::recvReqRetry()
{
    assert(blockedPkt != nullptr);

    PacketPtr pkt = blockedPkt;
    blockedPkt = nullptr;

    sendPacket(pkt);
}

void
CTWrite::MemSidePort::recvRangeChange()
{
    ctrl->handleRangeChange();
}

bool
CTWrite::handleRequest(PacketPtr pkt)
{
    if (requestPkt) {
        return false;
    }

    panic_if(
            pkt->getAddr() >= META_BORDER,
            "Data pkt whose address is over 16GiB"
            );
    DPRINTF(CTWrite, "Got request for addr %#x\n", pkt->getAddr());

    requestPkt = pkt;
    PacketPtr metaPkt = createPkt(
            META_BORDER,
            1,
            requestPkt->req->getFlags(),
            requestPkt->req->requestorId(),
            false
            );
    memSidePort.sendPacket(metaPkt);
    return true;
}

bool
CTWrite::handleResponse(PacketPtr pkt)
{
    assert(requestPkt);
    DPRINTF(CTWrite, "Got response for addr %#x\n", pkt->getAddr());

    requestPkt = nullptr;
    cpuSidePort.sendPacket(pkt);
    cpuSidePort.trySendRetry();
    return true;
}

Tick
CTWrite::handleAtomic(PacketPtr pkt)
{
    PacketPtr metaPkt = createPkt(
            META_BORDER,
            1,
            pkt->req->getFlags(),
            pkt->req->requestorId(),
            false
            );
    return memSidePort.sendAtomic(metaPkt);
}

void
CTWrite::handleFunctional(PacketPtr pkt)
{
    PacketPtr metaPkt = createPkt(
            META_BORDER,
            1,
            pkt->req->getFlags(),
            pkt->req->requestorId(),
            false
            );
    memSidePort.sendFunctional(metaPkt);
}

AddrRangeList
CTWrite::getAddrRanges() const
{
    DPRINTF(CTWrite, "Sending new ranges\n");
    return memSidePort.getAddrRanges();
}

void
CTWrite::handleRangeChange()
{
    cpuSidePort.sendRangeChange();
}

Port &
CTWrite::getPort(const std::string &if_name, PortID idx)
{
    panic_if(idx != InvalidPortID, "This object doesn't support vector ports");

    if (if_name == "cpu_side_port") {
        return cpuSidePort;
    } else if (if_name == "mem_side_port") {
        return memSidePort;
    } else {
        return SimObject::getPort(if_name, idx);
    }
}

} // namespace gem5
