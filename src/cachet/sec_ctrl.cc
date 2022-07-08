#include "cachet/sec_ctrl.hh"

#include "base/trace.hh"
#include "debug/SecCtrl.hh"

namespace gem5
{

SecCtrl::SecCtrl(const SecCtrlParams &p) :
    BaseCtrl(p),
    readPort(name() + ".read_port", this),
    writePort(name() + ".write_port", this),
    state(Idle),
    requestPkt(nullptr),
    needsResponse(false),
    responsePkt(nullptr),
    readFinished(false),
    writeFinished(false)
{
    DPRINTF(SecCtrl, "Constructing\n");
}

void
SecCtrl::processFinishOperation()
{
    DPRINTF(SecCtrl, "finish process\n");
    if (responsePkt) {
        cpuSidePort.sendPacket(responsePkt);
    }
    state = Idle;
    requestPkt = nullptr;
    needsResponse = false;
    responsePkt = nullptr;
    readFinished = false;
    writeFinished = false;
    cpuSidePort.trySendRetry();
}

bool
SecCtrl::handleRequest(PacketPtr pkt)
{
    if (state != Idle) {
        return false;
    }

    DPRINTF(SecCtrl, "Got request for %#x\n", pkt->print());

    PacketPtr readPkt = createPkt(
            pkt->getAddr(),
            1,
            pkt->req->getFlags(),
            pkt->req->requestorId(),
            true
            );
    switch (state) {
        case Idle:
            requestPkt = pkt;
            needsResponse = pkt->needsResponse();
            readPort.sendPacket(readPkt);
            if (pkt->isRead()) {
                state = Read;
                memSidePort.sendPacket(pkt);
            } else {
                state = Write;
            }
            break;

        default:
            assert(false);
    }

    return true;
}

bool
SecCtrl::handleResponse(PacketPtr pkt)
{
    assert(state != Idle);
    DPRINTF(SecCtrl, "Got response for %#x\n", pkt->print());

    PacketPtr writePkt = createPkt(
            requestPkt->getAddr(),
            1,
            requestPkt->req->getFlags(),
            requestPkt->req->requestorId(),
            false
            );
    switch (state) {
        case Idle:
            assert(false);

        case Read:
            if (pkt->getAddr() >= AT_START) {
                assert(pkt->isRead());
                readFinished = true;
            } else {
                responsePkt = pkt;
            }

            if (needsResponse) {
                if (responsePkt && readFinished) {
                    schedule(
                            finishOperation,
                            curTick() + HASH_CYCLE*TICK_PER_CYCLE
                            );
                }
            } else {
                if (readFinished) {
                    schedule(
                            finishOperation,
                            curTick() + HASH_CYCLE*TICK_PER_CYCLE
                            );
                }
            }

            break;

        case Write:
            if (pkt->getAddr() >= AT_START) {
                if (pkt->isRead()) {
                    readFinished = true;
                    memSidePort.sendPacket(requestPkt);
                    writePort.sendPacket(writePkt);
                } else {
                    writeFinished = true;
                }
            } else {
                responsePkt = pkt;
            }

            if (needsResponse) {
                if (responsePkt && readFinished && writeFinished) {
                    schedule(finishOperation, curTick());
                }
            } else {
                if (readFinished && writeFinished) {
                    schedule(finishOperation, curTick());
                }
            }

            break;
    }

    return true;
}

Tick
SecCtrl::handleAtomic(PacketPtr pkt)
{
    PacketPtr readPkt = createPkt(
            pkt->getAddr(),
            1,
            pkt->req->getFlags(),
            pkt->req->requestorId(),
            true
            );
    if (!readPort.sendAtomic(readPkt)) {
        return false;
    }
    if (!pkt->isRead()) {
        PacketPtr writePkt = createPkt(
                pkt->getAddr(),
                1,
                pkt->req->getFlags(),
                pkt->req->requestorId(),
                false
                );
        if (!writePort.sendAtomic(writePkt)) {
            return false;
        }
    }
    return memSidePort.sendAtomic(pkt);
}

void
SecCtrl::handleFunctional(PacketPtr pkt)
{
    PacketPtr readPkt = createPkt(
            pkt->getAddr(),
            1,
            pkt->req->getFlags(),
            pkt->req->requestorId(),
            true
            );
    readPort.sendFunctional(readPkt);
    if (!pkt->isRead()) {
        PacketPtr writePkt = createPkt(
                pkt->getAddr(),
                1,
                pkt->req->getFlags(),
                pkt->req->requestorId(),
                false
                );
        writePort.sendFunctional(writePkt);
    }
    memSidePort.sendFunctional(pkt);
}

Port &
SecCtrl::getPort(const std::string &if_name, PortID idx)
{
    panic_if(idx != InvalidPortID, "This object doesn't support vector ports");

    if (if_name == "read_port") {
        return readPort;
    } else if (if_name == "write_port") {
        return writePort;
    } else {
        return BaseCtrl::getPort(if_name, idx);
    }
}

} // namespace gem5
