#ifndef __CACHET_CT_READ_HH__
#define __CACHET_CT_READ_HH__

#include "cachet/common.hh"
#include "params/CTRead.hh"

namespace gem5
{

class CTRead : public SimObject
{
  private:
    class CPUSidePort: public ResponsePort
    {
      private:
        CTRead *ctrl;
        bool needRetry;
        PacketPtr blockedPkt;

      public:
        CPUSidePort(const std::string& name, CTRead* _ctrl):
          ResponsePort(name, _ctrl),
          ctrl(_ctrl),
          needRetry(false),
          blockedPkt(nullptr)
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
        CTRead *ctrl;
        PacketPtr blockedPkt;

      public:
        MemSidePort(const std::string& name, CTRead* _ctrl):
          RequestPort(name, _ctrl),
          ctrl(_ctrl),
          blockedPkt(nullptr)
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
    PacketPtr requestPkt;

  public:
    CTRead(const CTReadParams &p);

    Port& getPort(const std::string &if_name,
        PortID idx=InvalidPortID) override;
};

} // namespace gem5

#endif // __CACHET_CT_READ_HH__
