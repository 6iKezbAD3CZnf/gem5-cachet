#ifndef __CACHET_MT_WRITE_HH__
#define __CACHET_MT_WRITE_HH__

#include "cachet/base_ctrl.hh"
#include "params/MTWrite.hh"

namespace gem5
{

class MTWrite : public BaseCtrl
{
  private:
    void processRequestOperation();
    EventFunctionWrapper requestOperation;
    void processNextMTOperation();
    EventFunctionWrapper nextMTOperation;

    void processFinishOperation() override;
    bool handleRequest(PacketPtr pkt) override;
    bool handleResponse(PacketPtr pkt) override;
    Tick handleAtomic(PacketPtr pkt) override;
    void handleFunctional(PacketPtr pkt) override;

    MemSidePort memBypassPort;

    PacketPtr requestPkt;
    PacketPtr responsePkt;

  public:
    MTWrite(const MTWriteParams &p);
    Port& getPort(const std::string &if_name,
        PortID idx=InvalidPortID) override;
};

} // namespace gem5

#endif // __CACHET_MT_WRITE_HH__
