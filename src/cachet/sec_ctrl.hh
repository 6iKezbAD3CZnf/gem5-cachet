#ifndef __CACHET_SEC_CTRL_HH__
#define __CACHET_SEC_CTRL_HH__

#include "cachet/base_ctrl.hh"
#include "params/SecCtrl.hh"

namespace gem5
{

class SecCtrl : public BaseCtrl
{
  public:
    enum State
    {
        Idle,
        Read,
        Write
    };

    void processFinishOperation() override;

    bool handleRequest(PacketPtr pkt) override;
    bool handleResponse(PacketPtr pkt) override;
    Tick handleAtomic(PacketPtr pkt) override;
    void handleFunctional(PacketPtr pkt) override;
    AddrRangeList getAddrRanges() const override;
    void handleRangeChange() override;

    MemSidePort readPort;
    MemSidePort writePort;

    State state;
    PacketPtr requestPkt;
    bool needsResponse;
    PacketPtr responsePkt;
    bool readFinished;
    bool writeFinished;

    SecCtrl(const SecCtrlParams &p);
    virtual Port& getPort(const std::string &if_name,
        PortID idx=InvalidPortID) override;
};

} // namespace gem5

#endif // __CACHET_SEC_CTRL_HH__
