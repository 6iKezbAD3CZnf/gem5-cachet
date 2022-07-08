#ifndef __CACHET_CT_WRITE_HH__
#define __CACHET_CT_WRITE_HH__

#include "cachet/base_ctrl.hh"
#include "params/CTWrite.hh"

namespace gem5
{

class CTWrite : public BaseCtrl
{
  private:
    void processRequestOperation();
    EventFunctionWrapper requestOperation;

    void processFinishOperation() override;
    bool handleRequest(PacketPtr pkt) override;
    bool handleResponse(PacketPtr pkt) override;
    Tick handleAtomic(PacketPtr pkt) override;
    void handleFunctional(PacketPtr pkt) override;

    PacketPtr requestPkt;
    PacketPtr responsePkt;
    int responseTimes;

  public:
    CTWrite(const CTWriteParams &p);
};

} // namespace gem5

#endif // __CACHET_CT_WRITE_HH__
