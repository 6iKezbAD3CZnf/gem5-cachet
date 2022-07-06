#include "cachet/ct_write.hh"

#include "base/trace.hh"
#include "debug/CTWrite.hh"

namespace gem5
{

CTWrite::CTWrite(const CTWriteParams &p) :
    SimObject(p),
    requestOperation([this]{ processRequestOperation(); }, name()),
    cpuSidePort(name() + ".cpu_side_port", this),
    memSidePort(name() + ".mem_side_port", this),
    requestPkt(nullptr),
    responseTimes(0)
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
        DPRINTF(CTWrite, "rejected\n");
        waitingRetry = true;
        packetQueue.push(pkt);
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
    assert(!packetQueue.empty());

    PacketPtr pkt = packetQueue.front();
    packetQueue.pop();

    waitingRetry = false;
    sendPacket(pkt);
}

void
CTWrite::MemSidePort::recvRangeChange()
{
    ctrl->handleRangeChange();
}

void
CTWrite::processRequestOperation()
{
    Addr offset = requestPkt->getAddr() >> 8 << 5;
    PacketPtr macPkt = createPkt(
            MAC_START + offset,
            8,
            requestPkt->req->getFlags(),
            requestPkt->req->requestorId(),
            false
            );
    memSidePort.sendPacket(macPkt);

    offset = (macPkt->getAddr() - MAC_START) >> 11 << 8;
    PacketPtr cntPkt = createPkt(
            CNT_START + offset,
            64,
            macPkt->req->getFlags(),
            macPkt->req->requestorId(),
            false
            );
    memSidePort.sendPacket(cntPkt);

    offset = (cntPkt->getAddr() - CNT_START) >> 11 << 8;
    PacketPtr mtPkt = createPkt(
            MT_START + offset,
            64,
            cntPkt->req->getFlags(),
            cntPkt->req->requestorId(),
            false
            );
    memSidePort.sendPacket(mtPkt);

    PacketPtr pkt = mtPkt;
    Addr layer_start = MT_START;
    Addr layer_size = 0x10000000 >> 3;
    for (int i=0; i<7; i++) {
        offset = (pkt->getAddr() - layer_start) >> 11 << 8;
        pkt = createPkt(
                layer_start + layer_size + offset,
                64,
                pkt->req->getFlags(),
                pkt->req->requestorId(),
                false
                );
        memSidePort.sendPacket(pkt);
        layer_start += layer_size;
        layer_size >>= 3;
    }
}

bool
CTWrite::handleRequest(PacketPtr pkt)
{
    if (requestPkt) {
        return false;
    }

    panic_if(
            pkt->getAddr() >= AT_START,
            "Data pkt whose address is over 16GiB"
            );
    DPRINTF(CTWrite, "Got request for addr %#x\n", pkt->getAddr());

    requestPkt = pkt;
    schedule(
            requestOperation,
            curTick() + HASH_CYCLE * TICK_PER_CYCLE
            );
    return true;
}

bool
CTWrite::handleResponse(PacketPtr pkt)
{
    assert(requestPkt);
    DPRINTF(CTWrite, "Got response for %#x\n", pkt->print());

    responseTimes++;
    if (responseTimes >= 10) {
        requestPkt = nullptr;
        responseTimes = 0;
        cpuSidePort.sendPacket(pkt);
        cpuSidePort.trySendRetry();
    }

    return true;
}

Tick
CTWrite::handleAtomic(PacketPtr pkt)
{
    PacketPtr metaPkt = createPkt(
            AT_START,
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
            AT_START,
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
