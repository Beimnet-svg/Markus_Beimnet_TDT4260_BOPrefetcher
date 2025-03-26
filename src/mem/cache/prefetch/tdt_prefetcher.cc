
#include "mem/cache/prefetch/tdt_prefetcher.hh"
#include "debug/HWPrefetch.hh"
#include "mem/cache/replacement_policies/base.hh"
#include "params/TDTPrefetcher.hh"

namespace gem5
{

  GEM5_DEPRECATED_NAMESPACE(Prefetcher, prefetch);
  namespace prefetch
  {

    TDTPrefetcher::TDTEntry::TDTEntry(TagExtractor ext)
        : TaggedEntry()
    {
      registerTagExtractor(ext);
      invalidate();
    }

    void
    TDTPrefetcher::TDTEntry::invalidate()
    {
      TaggedEntry::invalidate();
    }

    TDTPrefetcher::TDTPrefetcher(const TDTPrefetcherParams &params)
        : Queued(params),
          pcTableInfo(params.table_assoc, params.table_entries,
                      params.table_indexing_policy,
                      params.table_replacement_policy)
    {
    }

    TDTPrefetcher::PCTable &
    TDTPrefetcher::findTable(int context)
    {
      auto it = pcTables.find(context);
      if (it != pcTables.end())
        return *(it->second);

      return allocateNewContext(context);
    }

    TDTPrefetcher::PCTable &
    TDTPrefetcher::allocateNewContext(int context)
    {
      assert(context == 0);
      std::string table_name = name() + ".PCTable" + std::to_string(context);
      pcTables[context].reset(new PCTable(
          table_name.c_str(),
          pcTableInfo.numEntries,
          pcTableInfo.assoc,
          pcTableInfo.replacementPolicy,
          pcTableInfo.indexingPolicy,
          TDTEntry(genTagExtractor(pcTableInfo.indexingPolicy))));

      DPRINTF(HWPrefetch, "Adding context %i with tdt4260 entries\n", context);

      return *(pcTables[context]);
    }

    void
    TDTPrefetcher::notifyFill(const CacheAccessProbeArg &arg)
    {
      // A cache line has been filled in
      addRecentRequest(arg.pkt->getAddr() - D);
      // What if the current best offset (D) has changed while the cache fill was loading? Then the D would be wrong
    }

    void
    TDTPrefetcher::BestOffsetPrefetcher::fillOffsetTable()
    {
      for (int i = 1; i <= 256; ++i)
      {
        int num = i;
        while (num % 2 == 0)
          num /= 2;
        while (num % 3 == 0)
          num /= 3;
        while (num % 5 == 0)
          num /= 5;
        if (num == 1)
        {
          offsetTable[i] = 0;
        }
      }
    }

    void
    TDTPrefetcher::BestOffsetPrefetcher::addRecentRequest(Addr addr)
    {
      if (recentRequests.size() >= recentRequestSize)
      {
        recentRequests.pop_front();
      }
      recentRequests.push_back({addr});
    }

    void
    TDTPrefetcher::BestOffsetPrefetcher::testRecentRequest(Addr addrRequest, int testOffset)
    {
      int testAdress = addrRequest - testOffset; // X -d
      for (auto &i : recentRequests)
      {
        if (testAdress == i.addr)
        {
          offsetTable[testOffset]++;
          if(offsetTable[testOffset] == maxScore){
            endLearningRound(testOffset);
          }
        }
      }
    }

    
    void
    TDTPrefetcher::BestOffsetPrefetcher::endLearningRound(int newBestoffset){
      //if newBestOffset is 0, roundMax was reached
      if(newBestoffset == 0){
        int highestValue = 0;

        for( auto &i: offsetTable){
          if(i.second > highestValue){
            highestValue = i.second;
          }
        }

        newBestoffset = offsetTable[highestValue];
      }
      D = newBestoffset;

      //reset offsTable
      for(auto &i: offsetTable){
        i.second = 0;
      }
      
    }


    void
    TDTPrefetcher::calculatePrefetch(const PrefetchInfo &pfi,
                                     std::vector<AddrPriority> &addresses,
                                     const CacheAccessor &cache)
    {
      if (!pfi.hasPC())
      {
        DPRINTF(HWPrefetch, "Ignoring request with no PC.\n");
        return;
      }

      // access_addr is the memory address (of the cache line) requested
      Addr access_addr = pfi.getAddr();

      // test one offset from the offset table, iterating each time calculatePrefetech is called
      static auto it = offsetTable.begin();
      if (it != offsetTable.end())
      {
        testRecentRequest(access_addr, it->first);
        ++it;
      }
      else
      {
        it = offsetTable.begin();
        currentRound++;
        testRecentRequest(access_addr, it->first);
        ++it;
      }

      

      // access pc is the pc of the inst that requests the cache line
      Addr access_pc = pfi.getPC();

      // context can be ignored
      int context = 0;

      // Currently implemented prefetching algorithm: Next line prefetching
      // TODO: Implement something better!
      addresses.push_back(AddrPriority(access_addr + (D * blksize), 0));


      if(currentRound == maxRound){
        endLearningRound(0);
      }
      

      // Can safely be ignored
      // Get matching storage of entries
      // Context is 0 due to single-threaded application
      PCTable &pcTable = findTable(context);

      // Get matching entry for your given PC from the PC Table
      const TDTEntry::KeyType key{access_pc, false};
      TDTEntry *entry = pcTable.findEntry(key);

      // Check if you have entry
      if (entry != nullptr)
      {
        // There is an entry for this PC
        // You might want to update information for this entry
      }
      else
      {
        // No entry for this PC
        // You might want to make an entry for this PC
      }

      // The following show you how to add an entry to PCTable for a PC
      // All slots are by default taken, you must replace a previous slot with new data
      // Find replacement victim for your new data, update information
      TDTEntry *victim = pcTable.findVictim(key);
      victim->lastAddr = access_addr;
      pcTable.insertEntry(key, victim);
    }

    uint32_t
    TDTPrefetcherHashedSetAssociative::extractSet(const KeyType &key) const
    {
      const Addr pc = key.address;
      const Addr hash1 = pc >> 1;
      const Addr hash2 = hash1 >> tagShift;
      return (hash1 ^ hash2) & setMask;
    }

    Addr
    TDTPrefetcherHashedSetAssociative::extractTag(const Addr addr) const
    {
      return addr;
    }

  } // namespace prefetch
} // namespace gem5
