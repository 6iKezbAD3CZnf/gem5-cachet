#ifndef __CACHET_CACHE_TREE_HH__
#define __CACHET_CACHE_TREE_HH__

#include "cachet/mt_write.hh"
#include "params/CacheTree.hh"

namespace gem5
{

class CacheTree : public MTWrite
{
  private:
    bool handleResponse(PacketPtr pkt) override;

  public:
    CacheTree(const CacheTreeParams &p);
};

} // namespace gem5

#endif // __CACHET_CACHE_TREE_HH__
