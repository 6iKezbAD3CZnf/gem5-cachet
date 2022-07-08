#include "cachet/mt_write.hh"

#include "base/trace.hh"
#include "debug/MTWrite.hh"

namespace gem5
{

MTWrite::MTWrite(const MTWriteParams &p) :
    SimObject(p),
    requestOperation([this]{ processRequestOperation(); }, name()),
    nextMTOperation([this]{ processNextMTOperation(); }, name()),
    finishOperation([this]{ processFinishOperation(); }, name()),
    cpuSidePort(name() + ".cpu_side_port", this),
    memSidePort(name() + ".mem_side_port", this),
    requestPkt(nullptr),
    responsePkt(nullptr)
{
    DPRINTF(MTWrite, "Constructing\n");
}

void
MTWrite::CPUSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPkt != nullptr, "Should never try to send if blocked!");

    if (!sendTimingResp(pkt)) {
        blockedPkt = pkt;
    }
}

void
MTWrite::CPUSidePort::trySendRetry()
{
    if (needRetry && blockedPkt == nullptr) {
        needRetry = false;
        DPRINTF(MTWrite, "Sending retry req for %d\n", id);
        sendRetryReq();
    }
}

AddrRangeList
MTWrite::CPUSidePort::getAddrRanges() const
{
    return ctrl->getAddrRanges();
}

Tick
MTWrite::CPUSidePort::recvAtomic(PacketPtr pkt)
{
    return ctrl->handleAtomic(pkt);
}

void
MTWrite::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    ctrl->handleFunctional(pkt);
}

bool
MTWrite::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    if (!ctrl->handleRequest(pkt)) {
        needRetry = true;
        return false;
    } else {
        return true;
    }
}

void
MTWrite::CPUSidePort::recvRespRetry()
{
    assert(blockedPkt != nullptr);

    PacketPtr pkt = blockedPkt;
    blockedPkt= nullptr;

    sendPacket(pkt);
}

void
MTWrite::MemSidePort::sendPacket(PacketPtr pkt)
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
        DPRINTF(MTWrite, "rejected\n");
        waitingRetry = true;
        packetQueue.push(pkt);
    }
}

bool
MTWrite::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    return ctrl->handleResponse(pkt);
}

void
MTWrite::MemSidePort::recvReqRetry()
{
    assert(!packetQueue.empty());

    PacketPtr pkt = packetQueue.front();
    packetQueue.pop();

    waitingRetry = false;
    sendPacket(pkt);
}

void
MTWrite::MemSidePort::recvRangeChange()
{
    ctrl->handleRangeChange();
}

void
MTWrite::processRequestOperation()
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
}

void
MTWrite::processNextMTOperation()
{
    PacketPtr pkt = responsePkt;
    if (pkt->getAddr() < CNT_START) {
        assert(false);
    } else if (pkt->getAddr() >= CNT_START &&
            pkt->getAddr() < MT_START) {
        Addr offset = (responsePkt->getAddr() - CNT_START) >> 11 << 8;
        PacketPtr mtPkt = createPkt(
                MT_START + offset,
                64,
                pkt->req->getFlags(),
                pkt->req->requestorId(),
                false
                );
        memSidePort.sendPacket(mtPkt);
        return;
    } else {
        Addr layer_start = MT_START;
        Addr layer_size = 0x10000000 >> 3;
        for (int i=0; i<10; i++) {
            if (layer_start == RT_START) {
                // Root
                schedule(
                        finishOperation,
                        curTick() + HASH_CYCLE * TICK_PER_CYCLE
                        );
                return;
            }

            if (pkt->getAddr() < layer_start+layer_size) {
                // Layer 0 is the leaf of MT
                DPRINTF(MTWrite, "send pkt in layer %d\n", i+1);
                Addr offset = (pkt->getAddr() - layer_start) >> 11 << 8;
                PacketPtr mtPkt = createPkt(
                        layer_start + layer_size + offset,
                        64,
                        pkt->req->getFlags(),
                        pkt->req->requestorId(),
                        false
                        );
                memSidePort.sendPacket(mtPkt);
                return;
            }
            layer_start += layer_size;
            layer_size >>= 3;
        }
    }

    assert(false);
}

void
MTWrite::processFinishOperation()
{
    cpuSidePort.sendPacket(responsePkt);
    requestPkt = nullptr;
    responsePkt = nullptr;
    cpuSidePort.trySendRetry();
    return;
}

bool
MTWrite::handleRequest(PacketPtr pkt)
{
    if (requestPkt) {
        return false;
    }

    panic_if(
            pkt->getAddr() >= AT_START,
            "Data pkt whose address is over 16GiB"
            );
    DPRINTF(MTWrite, "Got request for addr %#x\n", pkt->getAddr());

    requestPkt = pkt;
    schedule(
            requestOperation,
            curTick() + HASH_CYCLE * TICK_PER_CYCLE
            );
    return true;
}

bool
MTWrite::handleResponse(PacketPtr pkt)
{
    assert(requestPkt);
    DPRINTF(MTWrite, "Got response for %#x\n", pkt->print());

    // Cnt and MT pkt case
    if (pkt->getAddr() >= CNT_START) {
        responsePkt = pkt;
        schedule(nextMTOperation, curTick() + HASH_CYCLE*TICK_PER_CYCLE);
    }

    return true;
}

Tick
MTWrite::handleAtomic(PacketPtr pkt)
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
MTWrite::handleFunctional(PacketPtr pkt)
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
MTWrite::getAddrRanges() const
{
    DPRINTF(MTWrite, "Sending new ranges\n");
    return memSidePort.getAddrRanges();
}

void
MTWrite::handleRangeChange()
{
    cpuSidePort.sendRangeChange();
}

Port &
MTWrite::getPort(const std::string &if_name, PortID idx)
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
