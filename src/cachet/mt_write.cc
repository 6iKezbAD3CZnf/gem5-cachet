#include "cachet/mt_write.hh"

#include "base/trace.hh"
#include "debug/MTWrite.hh"

namespace gem5
{

MTWrite::MTWrite(const MTWriteParams &p) :
    BaseCtrl(p),
    requestOperation([this]{ processRequestOperation(); }, name()),
    nextMTOperation([this]{ processNextMTOperation(); }, name()),
    memBypassPort(name() + ".mem_bypass_port", this),
    requestPkt(nullptr),
    responsePkt(nullptr)
{
    DPRINTF(MTWrite, "Constructing\n");
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
    memBypassPort.sendPacket(macPkt);

    offset = (macPkt->getAddr() - MAC_START) >> 11 << 8;
    PacketPtr cntPkt = createPkt(
            CNT_START + offset,
            64,
            macPkt->req->getFlags(),
            macPkt->req->requestorId(),
            false
            );
    memBypassPort.sendPacket(cntPkt);
}

void
MTWrite::processNextMTOperation()
{
    PacketPtr pkt = responsePkt;
    if (pkt->getAddr() < CNT_START) {
        assert(false);
    } else if (pkt->getAddr() >= CNT_START &&
            pkt->getAddr() < MT_START) {
        Addr offset = (pkt->getAddr() - CNT_START) >> 11 << 8;
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
    Tick ret = 0;

    Addr offset = pkt->getAddr() >> 8 << 5;
    PacketPtr macPkt = createPkt(
            MAC_START + offset,
            8,
            pkt->req->getFlags(),
            pkt->req->requestorId(),
            false
            );
    ret += memBypassPort.sendAtomic(macPkt);

    offset = (macPkt->getAddr() - MAC_START) >> 11 << 8;
    PacketPtr cntPkt = createPkt(
            CNT_START + offset,
            64,
            macPkt->req->getFlags(),
            macPkt->req->requestorId(),
            false
            );
    ret += memBypassPort.sendAtomic(cntPkt);

    pkt = cntPkt;
    offset = (pkt->getAddr() - CNT_START) >> 11 << 8;
    pkt = createPkt(
            MT_START + offset,
            64,
            pkt->req->getFlags(),
            pkt->req->requestorId(),
            false
            );
    ret += memSidePort.sendAtomic(pkt);

    Addr layer_start = MT_START;
    Addr layer_size = 0x10000000 >> 3;
    for (int i=0; i<10; i++) {
        if (layer_start == RT_START) {
            // Root
            ret += HASH_CYCLE * TICK_PER_CYCLE;
            break;
        }

        if (pkt->getAddr() < layer_start+layer_size) {
            offset = (pkt->getAddr() - layer_start) >> 11 << 8;
            pkt = createPkt(
                    layer_start + layer_size + offset,
                    64,
                    pkt->req->getFlags(),
                    pkt->req->requestorId(),
                    false
                    );
            ret += memSidePort.sendAtomic(pkt);
        }
        layer_start += layer_size;
        layer_size >>= 3;
    }

    return ret;
}

void
MTWrite::handleFunctional(PacketPtr pkt)
{
    Addr offset = pkt->getAddr() >> 8 << 5;
    PacketPtr macPkt = createPkt(
            MAC_START + offset,
            8,
            pkt->req->getFlags(),
            pkt->req->requestorId(),
            false
            );
    memBypassPort.sendFunctional(macPkt);

    offset = (macPkt->getAddr() - MAC_START) >> 11 << 8;
    PacketPtr cntPkt = createPkt(
            CNT_START + offset,
            64,
            macPkt->req->getFlags(),
            macPkt->req->requestorId(),
            false
            );
    memBypassPort.sendFunctional(cntPkt);

    pkt = cntPkt;
    offset = (pkt->getAddr() - CNT_START) >> 11 << 8;
    pkt = createPkt(
            MT_START + offset,
            64,
            pkt->req->getFlags(),
            pkt->req->requestorId(),
            false
            );
    memSidePort.sendFunctional(pkt);

    Addr layer_start = MT_START;
    Addr layer_size = 0x10000000 >> 3;
    for (int i=0; i<10; i++) {
        if (layer_start == RT_START) {
            // Root
            break;
        }

        if (pkt->getAddr() < layer_start+layer_size) {
            offset = (pkt->getAddr() - layer_start) >> 11 << 8;
            pkt = createPkt(
                    layer_start + layer_size + offset,
                    64,
                    pkt->req->getFlags(),
                    pkt->req->requestorId(),
                    false
                    );
            memSidePort.sendFunctional(pkt);
        }
        layer_start += layer_size;
        layer_size >>= 3;
    }

    return;
}

Port &
MTWrite::getPort(const std::string &if_name, PortID idx)
{
    panic_if(idx != InvalidPortID, "This object doesn't support vector ports");

    if (if_name == "mem_bypass_port") {
        return memBypassPort;
    } else {
        return BaseCtrl::getPort(if_name, idx);
    }
}

} // namespace gem5
