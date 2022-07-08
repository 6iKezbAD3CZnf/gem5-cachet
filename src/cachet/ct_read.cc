#include "cachet/ct_read.hh"

#include "base/trace.hh"
#include "debug/CTRead.hh"

namespace gem5
{

CTRead::CTRead(const CTReadParams &p) :
    BaseCtrl(p),
    blocked(false),
    responcePkt(nullptr)
{
    DPRINTF(CTRead, "Constructing\n");
}

void
CTRead::processFinishOperation() {
    blocked = false;
    cpuSidePort.sendPacket(responcePkt);
    responcePkt = nullptr;
    cpuSidePort.trySendRetry();
    return;
}

bool
CTRead::handleRequest(PacketPtr pkt)
{
    if (blocked) {
        return false;
    }

    panic_if(
            pkt->getAddr() >= AT_START,
            "Data pkt whose address is over 16GiB"
            );
    DPRINTF(CTRead, "Got request for %#x\n", pkt->print());

    blocked = true;
    Addr offset = pkt->getAddr() >> 8 << 5;
    PacketPtr macPkt = createPkt(
            MAC_START + offset,
            8,
            pkt->req->getFlags(),
            pkt->req->requestorId(),
            true
            );
    memSidePort.sendPacket(macPkt);
    return true;
}

bool
CTRead::handleResponse(PacketPtr pkt)
{
    assert(blocked);
    DPRINTF(CTRead, "Got response for %#x\n", pkt->print());

    if (pkt->req->getAccessDepth() == 0) {
        // Cache Hit
        responcePkt = pkt;
        schedule(finishOperation, curTick() + HASH_CYCLE * TICK_PER_CYCLE);
        return true;
    } else {
        if (pkt->getAddr() < CNT_START) {
            Addr offset = (pkt->getAddr() - MAC_START) >> 11 << 8;
            PacketPtr cntPkt = createPkt(
                    CNT_START + offset,
                    64,
                    pkt->req->getFlags(),
                    pkt->req->requestorId(),
                    true
                    );
            memSidePort.sendPacket(cntPkt);
        } else if (pkt->getAddr() < MT_START) {
            Addr offset = (pkt->getAddr() - CNT_START) >> 11 << 8;
            PacketPtr mtPkt = createPkt(
                    MT_START + offset,
                    64,
                    pkt->req->getFlags(),
                    pkt->req->requestorId(),
                    true
                    );
            memSidePort.sendPacket(mtPkt);
        } else {
            Addr layer_start = MT_START;
            Addr layer_size = 0x10000000 >> 3;
            for (int i=0; i<10; i++) {
                if (layer_start == RT_START) {
                    // Root
                    responcePkt = pkt;
                    schedule(
                            finishOperation,
                            curTick() + HASH_CYCLE * TICK_PER_CYCLE
                            );
                    return true;
                }

                if (pkt->getAddr() < layer_start+layer_size) {
                    // Layer 0 is the leaf of MT
                    DPRINTF(CTRead, "send pkt in layer %d\n", i+1);
                    Addr offset = (pkt->getAddr() - layer_start) >> 11 << 8;
                    PacketPtr mtPkt = createPkt(
                            layer_start + layer_size + offset,
                            64,
                            pkt->req->getFlags(),
                            pkt->req->requestorId(),
                            true
                            );
                    memSidePort.sendPacket(mtPkt);
                    return true;
                }
                layer_start += layer_size;
                layer_size >>= 3;
            }

            assert(false);
        }
    }

    return true;
}

Tick
CTRead::handleAtomic(PacketPtr pkt)
{
    PacketPtr metaPkt = createPkt(
            AT_START,
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

} // namespace gem5
