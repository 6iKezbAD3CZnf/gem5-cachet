#include "cachet/ct_read.hh"

#include "base/trace.hh"
#include "debug/CTRead.hh"

namespace gem5
{

CTRead::CTRead(const CTReadParams &p) :
    SimObject(p),
    cpuSidePort(name() + ".cpu_side_port", this),
    memSidePort(name() + ".mem_side_port", this),
    blocked(false)
{
    DPRINTF(CTRead, "Constructing\n");
}

void
CTRead::CPUSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    if (!sendTimingResp(pkt)) {
        blockedPacket = pkt;
    }
}

void
CTRead::CPUSidePort::trySendRetry()
{
    if (needRetry && blockedPacket == nullptr) {
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
    return ctrl->handleAtomic(pkt);
}

void
CTRead::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    ctrl->handleFunctional(pkt);
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
    assert(blockedPacket != nullptr);

    PacketPtr pkt = blockedPacket;
    blockedPacket= nullptr;

    sendPacket(pkt);
}

void
CTRead::MemSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    bool needsResponse = pkt->needsResponse();

    if (sendTimingReq(pkt)) {
        if (!needsResponse) {
            ctrl->blocked = false;
        }
    } else {
        blockedPacket = pkt;
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
    assert(blockedPacket != nullptr);

    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

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
    if (blocked) {
        return false;
    }

    DPRINTF(CTRead, "Got request for addr %#x\n", pkt->getAddr());

    blocked = true;
    memSidePort.sendPacket(pkt);
    return true;
}

bool
CTRead::handleResponse(PacketPtr pkt)
{
    assert(blocked);
    DPRINTF(CTRead, "Got response for addr %#x\n", pkt->getAddr());

    blocked = false;
    cpuSidePort.sendPacket(pkt);
    cpuSidePort.trySendRetry();
    return true;
}

Tick
CTRead::handleAtomic(PacketPtr pkt)
{
    return memSidePort.sendAtomic(pkt);
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
