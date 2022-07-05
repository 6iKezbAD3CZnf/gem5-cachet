#include "cachet/ct_read.hh"

#include "base/trace.hh"
#include "debug/CTRead.hh"

namespace gem5
{

CTRead::CTRead(const CTReadParams &p) :
    SimObject(p),
    cpuSidePort(name() + ".cpu_side_port", this),
    memSidePort(name() + ".mem_side_port", this),
    requestPkt(nullptr)
{
    DPRINTF(CTRead, "Constructing\n");
}

void
CTRead::CPUSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPkt != nullptr, "Should never try to send if blocked!");

    if (!sendTimingResp(pkt)) {
        blockedPkt = pkt;
    }
}

void
CTRead::CPUSidePort::trySendRetry()
{
    if (needRetry && blockedPkt == nullptr) {
        needRetry = false;
        DPRINTF(CTRead, "Sending retry req for %d\n", id);
        sendRetryReq();
    }
}

AddrRangeList
CTRead::CPUSidePort::getAddrRanges() const
{
    return ctrl->getAddrRanges();
}

Tick
CTRead::CPUSidePort::recvAtomic(PacketPtr pkt)
{
    PacketPtr metaPkt = createPkt(
            META_BORDER,
            1,
            pkt->req->getFlags(),
            pkt->req->requestorId(),
            true
            );
    return ctrl->handleAtomic(metaPkt);
}

void
CTRead::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    PacketPtr metaPkt = createPkt(
            META_BORDER,
            1,
            pkt->req->getFlags(),
            pkt->req->requestorId(),
            true
            );
    ctrl->handleFunctional(metaPkt);
}

bool
CTRead::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    if (!ctrl->handleRequest(pkt)) {
        needRetry = true;
        return false;
    } else {
        return true;
    }
}

void
CTRead::CPUSidePort::recvRespRetry()
{
    assert(blockedPkt != nullptr);

    PacketPtr pkt = blockedPkt;
    blockedPkt= nullptr;

    sendPacket(pkt);
}

void
CTRead::MemSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPkt != nullptr, "Should never try to send if blocked!");

    assert(pkt->needsResponse());

    if (!sendTimingReq(pkt)) {
        blockedPkt = pkt;
    }
}

bool
CTRead::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    return ctrl->handleResponse(pkt);
}

void
CTRead::MemSidePort::recvReqRetry()
{
    assert(blockedPkt != nullptr);

    PacketPtr pkt = blockedPkt;
    blockedPkt = nullptr;

    sendPacket(pkt);
}

void
CTRead::MemSidePort::recvRangeChange()
{
    ctrl->handleRangeChange();
}

bool
CTRead::handleRequest(PacketPtr pkt)
{
    if (requestPkt) {
        return false;
    }

    panic_if(
            pkt->getAddr() >= META_BORDER,
            "Data pkt whose address is over 16GiB"
            );
    DPRINTF(CTRead, "Got request for addr %#x\n", pkt->getAddr());

    requestPkt = pkt;
    PacketPtr metaPkt = createPkt(
            META_BORDER,
            1,
            requestPkt->req->getFlags(),
            requestPkt->req->requestorId(),
            true
            );
    memSidePort.sendPacket(metaPkt);
    return true;
}

bool
CTRead::handleResponse(PacketPtr pkt)
{
    assert(requestPkt);
    DPRINTF(CTRead, "Got response for addr %#x\n", pkt->getAddr());

    requestPkt = nullptr;
    cpuSidePort.sendPacket(pkt);
    cpuSidePort.trySendRetry();
    return true;
}

Tick
CTRead::handleAtomic(PacketPtr pkt)
{
    PacketPtr metaPkt = createPkt(
            META_BORDER,
            1,
            pkt->req->getFlags(),
            pkt->req->requestorId(),
            true
            );
    return memSidePort.sendAtomic(metaPkt);
}

void
CTRead::handleFunctional(PacketPtr pkt)
{
    memSidePort.sendFunctional(pkt);
}

AddrRangeList
CTRead::getAddrRanges() const
{
    DPRINTF(CTRead, "Sending new ranges\n");
    return memSidePort.getAddrRanges();
}

void
CTRead::handleRangeChange()
{
    cpuSidePort.sendRangeChange();
}

Port &
CTRead::getPort(const std::string &if_name, PortID idx)
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
