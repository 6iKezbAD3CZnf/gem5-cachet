#ifndef __CACHET_BASE_CTRL_HH__
#define __CACHET_BASE_CTRL_HH__

#define AT_START 0x400000000
#define MAC_START 0x400040000
#define CNT_START 0x480040000
#define MT_START 0x490040000
#define RT_START 0x4924d2480

#define HASH_CYCLE 40
#define TICK_PER_CYCLE 1000

#include <queue>

#include "mem/port.hh"
#include "mem/request.hh"
#include "params/BaseCtrl.hh"
#include "sim/sim_object.hh"

namespace gem5
{

class BaseCtrl : public SimObject
{
  public:
    class CPUSidePort: public ResponsePort
    {
      private:
        BaseCtrl *ctrl;
        bool needRetry;
        PacketPtr blockedPacket;

      public:
        CPUSidePort(const std::string& name, BaseCtrl* _ctrl):
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
        BaseCtrl *ctrl;
        bool waitingRetry;
        std::queue<PacketPtr> packetQueue;

      public:
        MemSidePort(const std::string& name, BaseCtrl* _ctrl):
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

    PacketPtr createPkt(
            Addr addr,
            size_t size,
            uint32_t flags,
            uint16_t requestorid,
            bool isRead
            );
    virtual void processFinishOperation();
    EventFunctionWrapper finishOperation;

    virtual bool handleRequest(PacketPtr pkt);
    virtual bool handleResponse(PacketPtr pkt);
    virtual Tick handleAtomic(PacketPtr pkt);
    virtual void handleFunctional(PacketPtr pkt);
    AddrRangeList getAddrRanges() const;
    void handleRangeChange();

    CPUSidePort cpuSidePort;
    MemSidePort memSidePort;

    BaseCtrl(const BaseCtrlParams &p);
    virtual Port& getPort(const std::string &if_name,
        PortID idx=InvalidPortID) override;
};

} // namespace gem5

#endif // __CACHET_BASE_CTRL_HH__
