#include "cachet/cache_tree.hh"

#include "base/trace.hh"
#include "debug/CacheTree.hh"

namespace gem5
{

CacheTree::CacheTree(const CacheTreeParams &p) :
    MTWrite(p)
{
    DPRINTF(CacheTree, "Constructing\n");
}

bool
CacheTree::handleResponse(PacketPtr pkt)
{
    assert(requestPkt);
    DPRINTF(CacheTree, "Got response for %#x\n", pkt->print());

    // Cnt and MT pkt case
    if (pkt->getAddr() >= CNT_START) {
        responsePkt = pkt;

        if (pkt->req->getAccessDepth() == 0) {
            schedule(finishOperation, curTick() + 5*HASH_CYCLE*TICK_PER_CYCLE);
        } else {
            schedule(nextMTOperation, curTick() + HASH_CYCLE*TICK_PER_CYCLE);
        }
    }

    return true;
}

} // namespace gem5
