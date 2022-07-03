#ifndef __CACHET_SEC_CTRL_HH__
#define __CACHET_SEC_CTRL_HH__

#include "mem/port.hh"
#include "params/SecCtrl.hh"
#include "sim/sim_object.hh"

namespace gem5
{

class SecCtrl : public SimObject
{
  private:
    class CPUSidePort: public ResponsePort
    {
      private:
        SecCtrl *ctrl;
        bool needRetry;
        PacketPtr blockedPacket;

      public:
        CPUSidePort(const std::string& name, SecCtrl* _ctrl):
          ResponsePort(name, _ctrl),
          ctrl(_ctrl),
          needRetry(false),
          blockedPacket(nullptr)
        {}

        void sendPacket(PacketPtr pkt);
        void trySendRetry();
        AddrRangeList getAddrRanges() const override;

      protected:
        Tick recvAtomic(PacketPtr pkt) override;
        void recvFunctional(PacketPtr pkt) override;
        bool recvTimingReq(PacketPtr pkt) override;
        void recvRespRetry() override;
    };

    class MemSidePort: public RequestPort
    {
      private:
        SecCtrl *ctrl;
        PacketPtr blockedPacket;

      public:
        MemSidePort(const std::string& name, SecCtrl* _ctrl):
          RequestPort(name, _ctrl),
          ctrl(_ctrl),
          blockedPacket(nullptr)
        {}

        void sendPacket(PacketPtr ptr);

      protected:
        bool recvTimingResp(PacketPtr pkt) override;
        void recvReqRetry() override;
        void recvRangeChange() override;
    };

    bool handleRequest(PacketPtr pkt);
    bool handleResponse(PacketPtr pkt);
    Tick handleAtomic(PacketPtr pkt);
    void handleFunctional(PacketPtr pkt);
    AddrRangeList getAddrRanges() const;
    void handleRangeChange();

    CPUSidePort cpuSidePort;
    MemSidePort memSidePort;
    bool blocked;

  public:
    SecCtrl(const SecCtrlParams &p);

    Port& getPort(const std::string &if_name,
        PortID idx=InvalidPortID) override;
};

} // namespace gem5

#endif // __CACHET_SEC_CTRL_HH__
