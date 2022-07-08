#ifndef __CACHET_CT_READ_HH__
#define __CACHET_CT_READ_HH__

#include "cachet/base_ctrl.hh"
#include "params/CTRead.hh"

namespace gem5
{

class CTRead : public BaseCtrl
{
  private:
    void processFinishOperation() override;
    bool handleRequest(PacketPtr pkt) override;
    bool handleResponse(PacketPtr pkt) override;
    Tick handleAtomic(PacketPtr pkt) override;
    void handleFunctional(PacketPtr pkt) override;

    bool blocked;
    PacketPtr responcePkt;

  public:
    CTRead(const CTReadParams &p);
};

} // namespace gem5

#endif // __CACHET_CT_READ_HH__
