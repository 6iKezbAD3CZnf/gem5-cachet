#include "cachet/ct_write.hh"

#include "base/trace.hh"
#include "debug/CTWrite.hh"

namespace gem5
{

CTWrite::CTWrite(const CTWriteParams &p) :
    BaseCtrl(p),
    requestOperation([this]{ processRequestOperation(); }, name()),
    requestPkt(nullptr),
    responsePkt(nullptr),
    responseTimes(0)
{
    DPRINTF(CTWrite, "Constructing\n");
}

void
CTWrite::processFinishOperation()
{
    requestPkt = nullptr;
    responseTimes = 0;
    cpuSidePort.sendPacket(responsePkt);
    responsePkt = nullptr;
    cpuSidePort.trySendRetry();
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
        responsePkt = pkt;
        schedule(finishOperation, curTick());
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

} // namespace gem5
