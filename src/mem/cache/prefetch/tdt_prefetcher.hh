#ifndef __MEM_CACHE_PREFETCH_TDT_PREFETCHER_HH__
#define __MEM_CACHE_PREFETCH_TDT_PREFETCHER_HH__

#include <string>
#include <unordered_map>
#include <vector>

#include "base/cache/associative_cache.hh"
#include "base/sat_counter.hh"
#include "base/types.hh"
#include "mem/cache/prefetch/queued.hh"
#include "mem/cache/replacement_policies/replaceable_entry.hh"
#include "mem/cache/tags/indexing_policies/set_associative.hh"
#include "mem/cache/tags/tagged_entry.hh"
#include "mem/packet.hh"
#include "params/TDTPrefetcherHashedSetAssociative.hh"

namespace gem5
{

GEM5_DEPRECATED_NAMESPACE(ReplacementPolicy, replacement_policy);
namespace replacement_policy
{
    class Base;
}

struct TDTPrefetcherParams;



GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);
namespace prefetch
{

//Implementation of Best Offset Prefetcher
//Here we will create a class that will be used to include Recent requests table
class BestOffsetPrefetcher
{
private:
    Addr addr; // If this is needed
    
    std::vector<Addr> recentRequests;
    std::unordered_map<int, int> offsetTable; // offset, score
    int offsetTableSize;
    int recentRequestSize; 
    int maxScore;
    int maxRound;
    int currentRound;
    int D;

public:
    
    BestOffsetPrefetcher(int recentRequestSize, int offsetTableSize, int maxScore, int maxRound);
    
    void fillOffsetTable();
    void addRecentRequest(Addr addr);
    void testRecentRequest(Addr addrRequest, int testOffset, int blkSize);
    uint32_t computeRRTableIndex(Addr address);
    uint32_t extractTag(Addr address);
    //void updateOffsetScore(int offset, int score);
    void endLearningRound(int newBestoffset);
    int getBestOffset() const { return D; }
    // void printOffsets() const; // Method to print offsets

    // Add friend declaration to allow TDTPrefetcher access
    friend class TDTPrefetcher;
};

class TDTPrefetcherHashedSetAssociative : public TaggedSetAssociative
{
    protected:
        uint32_t extractSet(const KeyType &key) const override;
        Addr extractTag(const Addr addr) const override;

    public:
        TDTPrefetcherHashedSetAssociative(
            const TDTPrefetcherHashedSetAssociativeParams &p)
        : TaggedSetAssociative(p)
        {}

        ~TDTPrefetcherHashedSetAssociative() = default;
};

class TDTPrefetcher : public Queued
{

  protected:
   BestOffsetPrefetcher bestOffsetPrefetcher;
    const struct PCTableInfo
    {
        const int assoc;
        const int numEntries;

        TaggedIndexingPolicy* const indexingPolicy;
        replacement_policy::Base* const replacementPolicy;

        PCTableInfo(int assoc, int num_entries,
                    TaggedIndexingPolicy* indexing_policy,
                    replacement_policy::Base* repl_policy)
          : assoc(assoc), numEntries(num_entries),
            indexingPolicy(indexing_policy), replacementPolicy(repl_policy)
        {

        }
    } pcTableInfo;

    // A basic entry you can use for designing your prefetcher
    // You can extend this type of entry with more data like cycle accessed
    // Recommend to store this in a a map and look through to find matching entries
    struct TDTEntry : public TaggedEntry
    {
        TDTEntry(TagExtractor ext);
        void invalidate() override;

        

        Addr lastAddr = 0;


    };

    // This redefines an associative set as the PC Table, a Set can be indexed
    // into by a PC, so a PC will go to a single TDTEntry (or return nullptr)
    using PCTable = AssociativeCache<TDTEntry>;

    // The following can safely be ignored
    std::unordered_map<int, std::unique_ptr<PCTable>> pcTables;

    PCTable& findTable(int context);
    PCTable& allocateNewContext(int context);
    // The preceding can safely be ignored

    void notifyFill(const CacheAccessProbeArg &arg) override;

  public:
    TDTPrefetcher(const TDTPrefetcherParams &p);

    void calculatePrefetch(const PrefetchInfo &pf1,
                           std::vector<AddrPriority> &addresses,
                           const CacheAccessor &cache) override;
                           

};

}
}

#endif  //__MEM_CACHE_PREFETCH_TDT_PREFETCHER_HH__
