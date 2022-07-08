#include "cachet/base_ctrl.hh"

#include "base/trace.hh"
#include "debug/BaseCtrl.hh"

namespace gem5
{

BaseCtrl::BaseCtrl(const BaseCtrlParams &p) :
    SimObject(p),
    finishOperation([this]{ processFinishOperation(); }, name()),
    cpuSidePort(name() + ".cpu_side_port", this),
    memSidePort(name() + ".mem_side_port", this),
    requestPkt(nullptr),
    responsePkt(nullptr)
{
    DPRINTF(BaseCtrl, "Constructing\n");
}

void
BaseCtrl::CPUSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPkt != nullptr, "Should never try to send if blocked!");

    if (!sendTimingResp(pkt)) {
        blockedPkt = pkt;
    }
}

void
BaseCtrl::CPUSidePort::trySendRetry()
{
    if (needRetry && blockedPkt == nullptr) {
        needRetry = false;
        DPRINTF(BaseCtrl, "Sending retry req for %d\n", id);
        sendRetryReq();
    }
}

AddrRangeList
BaseCtrl::CPUSidePort::getAddrRanges() const
{
    return ctrl->getAddrRanges();
}

Tick
BaseCtrl::CPUSidePort::recvAtomic(PacketPtr pkt)
{
    PacketPtr metaPkt = ctrl->createPkt(
            AT_START,
            1,
            pkt->req->getFlags(),
            pkt->req->requestorId(),
            true
            );
    return ctrl->handleAtomic(metaPkt);
}

void
BaseCtrl::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    PacketPtr metaPkt = ctrl->createPkt(
            AT_START,
            1,
            pkt->req->getFlags(),
            pkt->req->requestorId(),
            true
            );
    ctrl->handleFunctional(metaPkt);
}

bool
BaseCtrl::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    if (!ctrl->handleRequest(pkt)) {
        needRetry = true;
        return false;
    } else {
        return true;
    }
}

void
BaseCtrl::CPUSidePort::recvRespRetry()
{
    assert(blockedPkt != nullptr);

    PacketPtr pkt = blockedPkt;
    blockedPkt= nullptr;

    sendPacket(pkt);
}

void
BaseCtrl::MemSidePort::sendPacket(PacketPtr pkt)
{
    if (waitingRetry) {
        packetQueue.push(pkt);
        return;
    }

    if (sendTimingReq(pkt)) {
        if (!packetQueue.empty()) {
            PacketPtr nextPkt = packetQueue.front();
            packetQueue.pop();
            sendPacket(nextPkt);
        }
    } else {
        DPRINTF(BaseCtrl, "rejected\n");
        waitingRetry = true;
        packetQueue.push(pkt);
    }
}

bool
BaseCtrl::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    return ctrl->handleResponse(pkt);
}

void
BaseCtrl::MemSidePort::recvReqRetry()
{
    assert(!packetQueue.empty());

    PacketPtr pkt = packetQueue.front();
    packetQueue.pop();

    waitingRetry = false;
    sendPacket(pkt);
}

void
BaseCtrl::MemSidePort::recvRangeChange()
{
    ctrl->handleRangeChange();
}

PacketPtr
BaseCtrl::createPkt(
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

void
BaseCtrl::processFinishOperation() {
    requestPkt = nullptr;
    cpuSidePort.sendPacket(responsePkt);
    responsePkt = nullptr;
    cpuSidePort.trySendRetry();
    return;
}

bool
BaseCtrl::handleRequest(PacketPtr pkt)
{
    if (requestPkt) {
        return false;
    }

    DPRINTF(BaseCtrl, "Got request for %#x\n", pkt->print());

    requestPkt = pkt;
    memSidePort.sendPacket(pkt);
    return true;
}

bool
BaseCtrl::handleResponse(PacketPtr pkt)
{
    assert(requestPkt);
    DPRINTF(BaseCtrl, "Got response for %#x\n", pkt->print());

    responsePkt = pkt;
    schedule(finishOperation, curTick());

    return true;
}

Tick
BaseCtrl::handleAtomic(PacketPtr pkt)
{
    return memSidePort.sendAtomic(pkt);
}

void
BaseCtrl::handleFunctional(PacketPtr pkt)
{
    memSidePort.sendFunctional(pkt);
}

AddrRangeList
BaseCtrl::getAddrRanges() const
{
    DPRINTF(BaseCtrl, "Sending new ranges\n");
    return memSidePort.getAddrRanges();
}

void
BaseCtrl::handleRangeChange()
{
    cpuSidePort.sendRangeChange();
}

Port &
BaseCtrl::getPort(const std::string &if_name, PortID idx)
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
