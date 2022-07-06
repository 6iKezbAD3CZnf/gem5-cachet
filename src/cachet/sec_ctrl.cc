#include "cachet/sec_ctrl.hh"

#include "base/trace.hh"
#include "debug/SecCtrl.hh"

namespace gem5
{

SecCtrl::SecCtrl(const SecCtrlParams &p) :
    SimObject(p),
    cpuSidePort(name() + ".cpu_side_port", this),
    memPort(name() + ".mem_port", this),
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
SecCtrl::CPUSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    if (!sendTimingResp(pkt)) {
        blockedPacket = pkt;
    }
}

void
SecCtrl::CPUSidePort::trySendRetry()
{
    if (needRetry && blockedPacket == nullptr) {
        needRetry = false;
        DPRINTF(SecCtrl, "Sending retry req for %d\n", id);
        sendRetryReq();
    }
}

AddrRangeList
SecCtrl::CPUSidePort::getAddrRanges() const
{
    return ctrl->getAddrRanges();
}

Tick
SecCtrl::CPUSidePort::recvAtomic(PacketPtr pkt)
{
    return ctrl->handleAtomic(pkt);
}

void
SecCtrl::CPUSidePort::recvFunctional(PacketPtr pkt)
{
    ctrl->handleFunctional(pkt);
}

bool
SecCtrl::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    if (!ctrl->handleRequest(pkt)) {
        needRetry = true;
        return false;
    } else {
        return true;
    }
}

void
SecCtrl::CPUSidePort::recvRespRetry()
{
    assert(blockedPacket != nullptr);

    PacketPtr pkt = blockedPacket;
    blockedPacket= nullptr;

    sendPacket(pkt);
}

void
SecCtrl::MemSidePort::sendPacket(PacketPtr pkt)
{
    panic_if(blockedPacket != nullptr, "Should never try to send if blocked!");

    if (!sendTimingReq(pkt)) {
        blockedPacket = pkt;
    }
}

bool
SecCtrl::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    return ctrl->handleResponse(pkt);
}

void
SecCtrl::MemSidePort::recvReqRetry()
{
    assert(blockedPacket != nullptr);

    PacketPtr pkt = blockedPacket;
    blockedPacket = nullptr;

    sendPacket(pkt);
}

void
SecCtrl::MemSidePort::recvRangeChange()
{
    if (name() == "system.sec_ctrl.mem_port") {
        ctrl->handleRangeChange();
    }
}

void
SecCtrl::finishProcess()
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
                memPort.sendPacket(pkt);
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
                    finishProcess();
                }
            } else {
                if (readFinished) {
                    finishProcess();
                }
            }

            break;

        case Write:
            if (pkt->getAddr() >= AT_START) {
                if (pkt->isRead()) {
                    readFinished = true;
                    memPort.sendPacket(requestPkt);
                    writePort.sendPacket(writePkt);
                } else {
                    writeFinished = true;
                }
            } else {
                responsePkt = pkt;
            }

            if (needsResponse) {
                if (responsePkt && readFinished && writeFinished) {
                    finishProcess();
                }
            } else {
                if (readFinished && writeFinished) {
                    finishProcess();
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
    return memPort.sendAtomic(pkt);
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
    memPort.sendFunctional(pkt);
}

AddrRangeList
SecCtrl::getAddrRanges() const
{
    DPRINTF(SecCtrl, "Sending new ranges\n");
    return memPort.getAddrRanges();
}

void
SecCtrl::handleRangeChange()
{
    cpuSidePort.sendRangeChange();
}

Port &
SecCtrl::getPort(const std::string &if_name, PortID idx)
{
    panic_if(idx != InvalidPortID, "This object doesn't support vector ports");

    if (if_name == "cpu_side_port") {
        return cpuSidePort;
    } else if (if_name == "mem_port") {
        return memPort;
    } else if (if_name == "read_port") {
        return readPort;
    } else if (if_name == "write_port") {
        return writePort;
    } else {
        return SimObject::getPort(if_name, idx);
    }
}

} // namespace gem5
