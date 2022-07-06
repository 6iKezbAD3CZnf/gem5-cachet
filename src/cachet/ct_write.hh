#ifndef __CACHET_CT_WRITE_HH__
#define __CACHET_CT_WRITE_HH__

#include <queue>

#include "cachet/common.hh"
#include "params/CTWrite.hh"

namespace gem5
{

class CTWrite : public SimObject
{
  private:
    class CPUSidePort: public ResponsePort
    {
      private:
        CTWrite *ctrl;
        bool needRetry;
        PacketPtr blockedPkt;

      public:
        CPUSidePort(const std::string& name, CTWrite* _ctrl):
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
        CTWrite *ctrl;
        bool waitingRetry;
        std::queue<PacketPtr> packetQueue;

      public:
        MemSidePort(const std::string& name, CTWrite* _ctrl):
          RequestPort(name, _ctrl),
          ctrl(_ctrl),
          waitingRetry(false),
          packetQueue()
        {}

        void sendPacket(PacketPtr ptr);

      protected:
        bool recvTimingResp(PacketPtr pkt) override;
        void recvReqRetry() override;
        void recvRangeChange() override;
    };

    void processRequestOperation();
    EventFunctionWrapper requestOperation;

    bool handleRequest(PacketPtr pkt);
    bool handleResponse(PacketPtr pkt);
    Tick handleAtomic(PacketPtr pkt);
    void handleFunctional(PacketPtr pkt);
    AddrRangeList getAddrRanges() const;
    void handleRangeChange();

    CPUSidePort cpuSidePort;
    MemSidePort memSidePort;
    PacketPtr requestPkt;
    int responseTimes;

  public:
    CTWrite(const CTWriteParams &p);

    Port& getPort(const std::string &if_name,
        PortID idx=InvalidPortID) override;
};

} // namespace gem5

#endif // __CACHET_CT_WRITE_HH__
