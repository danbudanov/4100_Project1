/**
 * @author Daniil Budanov
 *
 * Throughout this implementation, I make extensive use of C++ STL structures
 * and program in an object-oriented manner.
 *
 * The logic behind this is that this simulation fits the functional 
 * requirements of the assignment, but the abstracted software design allows for
 * better debugging ability. 
 *
 * The assumption is that these structures' functionality can be implemented in
 * hardware with extended area and power; for instance, an associative set in a
 * cache would have valid bits and hardware for accessing entries, but in sw
 * this functionality can be modeled as an extended stack based upon a doubly 
 * linked list.
 */
#include "cache.hpp"
#include <list>
#include <vector>
#include <algorithm>

#include <exception>

// Include for log2 function
#include <cmath> 

typedef struct cache_stats_t* stats_t;

/**
 * @brief Useful function for finding number of bits needed to represent number
 */
inline uint64_t clog2(uint64_t num)
{

    return static_cast<uint64_t>(std::ceil(std::log2(num)));
}




/**
 * @brief Object representing entry in a cache
 */
class CacheEntry
{
    protected:
        uint64_t addr;
        bool dirty;
        uint64_t c;
        uint64_t b;
        uint64_t s;

        const uint64_t ADDR_WIDTH = 64UL;

        /**
         * blank flag is set in special case where there is no data in an entry
         * this can be referenced between transfers of register data
         *
         * blank flags are only set by a default constructor for CacheEntry
         *
         * Note that blank entries are not actually stored in sets, but are
         * returned for communication purposes
         */
        bool blank = false;

        /**
         * flag to track prefetch utilization
         */
        bool prefetched = false;

        /**
         * Calculate the number of bits in the tag
         */
        inline uint64_t tagSize() const
        {
           return ADDR_WIDTH - c + s;
        }

        /**
         * Calculate the number of bits in the byte offset
         */
        inline uint64_t byteOffsetSize() const
        {
            return b;
        }

        /**
         * Calculate the number of bits in the index
         */
        inline uint64_t indexSize() const
        {
           return c - s - b;
        }

        /**
         * @brief Shift value by shiftAmount and mask out bitCount of its bits
         * @param val the value to shift and maskbit
         * @param bitCount the number of bits to mask
         * @shiftAmount how much to shift over the value
         */
        inline uint64_t shiftAndMask(uint64_t val, uint64_t bitCount,
                uint64_t shiftAmount) const
        {
            /* return (val >> shiftAmount) & ((2 << bitCount) - 1); */
            uint64_t res = (val >> shiftAmount) & ((2UL << bitCount) - 1UL);
            return res;
        }

    public:

        /**
         * The default constructor will return a blank CacheEntry
         */
        CacheEntry() : blank(true)
        {}

        /**
         * @brief initialize from complete cache description
         * @param addr desired access address
         * @param dirty if access is a write (will end up in memory)
         * @param c 2^c is total bytes in data store of cache
         * @param b 2^b bytes in a block
         * @param s 2^s blocks in a set
         */
        CacheEntry(uint64_t addr_i, bool dirty_i, uint64_t c_i, uint64_t b_i, 
                uint64_t s_i) 
            : addr(addr_i), dirty(dirty_i), c(c_i), b(b_i), s(s_i) 
        {}
        
        /**
         * Copy constructor
         */
        CacheEntry(const CacheEntry& alt)
        {
            addr = alt.addr;
            dirty = alt.dirty;
            c = alt.c;
            b = alt.b;
            s = alt.s;
        }

        /**
         * @brief Copy constructor with new (c, b, s)
         * @param alt the cache entry to copy from
         * @ c_i the new C for entry
         * @ b_i the new B for entry
         * @ s_i the new S for entry
         */
        CacheEntry(const CacheEntry& alt, uint64_t c_i, uint64_t b_i, 
                uint64_t s_i)
        {
            c = c_i;
            b = b_i;
            s = s_i;
        }

        /**
         * Assignment operator
         */
        CacheEntry& operator=(const CacheEntry& alt)
        {
            addr = alt.addr;
            c = alt.c;
            b = alt.b;
            s = alt.s;
            dirty = alt.dirty;

            return *this;
        }

        /**
         * Set a new dirty bit
         */
        void setDirty(bool dirty_i)
        {
            dirty = dirty_i;
        }

        /**
         * Set a new C
         */
        void setC(uint64_t c_i)
        {
            c = c_i;
        }

        /**
         * Set a new B
         */
        void setB(uint64_t b_i)
        {
            b = b_i;
        }

        /**
         * Set a new S
         */
        void setS(uint64_t s_i)
        {
            s = s_i;
        }

        /**
         * Update a block address for an access
         */
        void setBlockAddress(uint64_t blockAddress_i)
        {
            // Create mask for new block address region (everything but BO)
            uint64_t blockAddressMask = ((1UL<<(ADDR_WIDTH - b)) - 1UL) << b;
            addr &= ~blockAddressMask; // Clear out old block address

            // Insert new block address
            addr |= blockAddress_i << b;
        }

        /**
         * Set whether block is prefetched
         */
        void setPrefetched(bool prefetched_i)
        {
            prefetched = prefetched_i;
        }

        /**
         * Getters for address parameters
         */
        uint64_t getTag() const
        {
            return shiftAndMask(addr, tagSize(), indexSize() 
                    + byteOffsetSize());
        }

        uint64_t getIndex() const
        {
            return shiftAndMask(addr, indexSize(), byteOffsetSize());
        }

        uint64_t getByteOffset() const
        {
            return shiftAndMask(addr, byteOffsetSize(), 0);
        }

        uint64_t getBlockAddress() const
        {
            return shiftAndMask(addr, indexSize() + tagSize(), 
                    byteOffsetSize());
        }

        uint64_t getAddress() const
        {
            return addr;
        }

        uint64_t getC() const
        {
            return c;
        }

        uint64_t getB() const
        {
            return b;
        }

        uint64_t gets() const
        {
            return s;
        }

        bool isBlank() const
        {
            return blank;
        }

        bool isDirty() const
        {
            return dirty;
        }

        bool isPrefetched() const
        {
            return prefetched;
        }

        /**
         * @brief Overload overload statements for std::find()
         */
        bool operator==(const CacheEntry& rhs)
        {
            return (getTag() == rhs.getTag());
        }
        bool operator==(const uint64_t& compareTag)
        {
            return (getTag() == compareTag);
        }
}; // CacheEntry

/**
 * A cleaner way to check whether a CacheEntry is blank
 */
inline bool checkBlank(const CacheEntry& entry)
{
    return entry.isBlank();
}

/**
 * @brief Base object will hold an N-way associative set of a cache
 *
 * Due to the stack nature of LRU, the eviction policy will be enforced by
 * maintaining access entries in an N-way doubly linked list structure
 */
class CacheSet
{
    protected:
        uint64_t ways;
        uint64_t c, b, s;

        /**
         * @brief Primary data structure used for storing cache entries
         * This is the structure that will be searched for tags, etc. 
         * Note that the number of ways is 2^s
         */
        std::list<CacheEntry> set;
    public:
        CacheSet(uint64_t c_i, uint64_t b_i, uint64_t s_i) 
            : ways(1UL << s_i), c(c_i), b(b_i), s(s_i)
        {}

        /**
         * Default constructor, for parameterizing afterwards
         */
        CacheSet()
        {}


        void init(uint64_t c_i, uint64_t b_i, uint64_t s_i) 
        {
            ways = 1UL << s_i;
            c = c_i;
            b = b_i; 
            s = s_i;
        }

        uint64_t getWays()
        {
            return ways;
        }

        uint64_t getSize()
        {
            return set.size();
        }

        /**
         * Finds tag in list, removes it from list, and returns a copy the 
         * associated CacheEntry
         */
        CacheEntry retrieve(uint64_t tag)
        {
            auto foundEntryIt = std::find(set.begin(), set.end(), tag);
            if (foundEntryIt == set.end()) {
                return CacheEntry();
            } else {
                CacheEntry found = *foundEntryIt;
                set.remove(*foundEntryIt);
                return found;
            }
        }

        /**
         * Finds tag in list, returns associated CacheEntry if found,
         * Blank if not
         */
        CacheEntry seek(uint64_t tag)
        {
            auto foundEntryIt = std::find(set.begin(), set.end(), tag);
            if (foundEntryIt == set.end()) {
                return CacheEntry();
            } else {
                return *foundEntryIt;
            }
        }

        /**
         * Searches for tag in set, returns whether it exists
         */
        bool contains(uint64_t tag)
        {
            auto foundEntryIt = std::find(set.begin(), set.end(), tag);
            if (foundEntryIt == set.end()) {
                return false;
            } else {
                return true;
            }
        }

}; // CacheSet

/**
 * @brief Create an associative set with LRU policy
 * 
 * LRU is the tail of the dll representing the set
 * MRU is the head
 */
class LruSet : public CacheSet
{
    public:
        LruSet(uint64_t c_i, uint64_t b_i, uint64_t s_i)
            : CacheSet(c_i, b_i, s_i)
        {}

        /**
         * @brief Insert an entry in the LRU position, possibly evicting old LRU
         *
         * @param entry the entry to be inserted into LRU
         * @return the ejected CacheEntry if ejected, else a blank CacheEntry
         */
        CacheEntry insertLru(CacheEntry entry)
        {
            if (set.size() < this->ways) { // have space in set, so no ejection
                this->set.push_back(entry);
                return CacheEntry();
            } else { // have to evict the LRU entry
                // Copy value of LRU entry
                CacheEntry lru = this->set.back();
                this->set.pop_back();
                this->set.push_back(entry);
                return lru;
            }
        }

        /**
         * @brief Insert an entry in the MRU position, possibly evicting old LRU
         *
         * @param entry the entry to be inserted into MRU
         * @return the ejected CacheEntry if ejected, else a blank CacheEntry
         *
         * NOTE: (!!!) Make sure value is not already in cache when inserting!!!
         * can do read() (if L1 to try to access first) or seek() to see if in
         * there already
         */
        CacheEntry inserMru(CacheEntry entry)
        {
            if (set.size() < this->ways) { // have space in set, so no ejection
                this->set.push_front(entry);
                return CacheEntry();
            } else { // have to evict the LRU entry
                // Copy value of LRU entry
                CacheEntry lru = this->set.back();
                this->set.pop_back();
                this->set.push_front(entry);
                return lru;
            }
        }

        /**
         * @brief Search set for entry with given tag. Return blank if not found
         *
         * @param tag the tag to search for in set
         * @return the CacheEntry corresponding to found tag or blank CacheEntry
         *
         * If an entry matching tag is found, it is moved into MRU position
         */
        CacheEntry read(uint64_t tag)
        {
            auto foundEntryIt = std::find(this->set.begin(), this->set.end(), 
                    tag);
            if (foundEntryIt == this->set.end()) { // entry not found
                return CacheEntry();
            } else {
                CacheEntry newMru = *foundEntryIt;
                this->set.remove(newMru);
                set.push_front(newMru);
                return newMru;
            }
        }

        /**
         * @brief Attempt to writeback to LRU
         *
         * If a hit, mark block dirty and return copy of wb cache
         * If a miss, return blank
         */
        CacheEntry writeBack(uint64_t tag)
        {
            auto foundEntryIt = std::find(this->set.begin(), this->set.end(), 
                    tag);
            if (foundEntryIt == this->set.end()) { // entry not found
                return CacheEntry();
            } else {
                foundEntryIt->setDirty(true);
                CacheEntry newMru = *foundEntryIt;
                this->set.remove(newMru);
                set.push_front(newMru);
                return newMru;
            }
        }

}; // LruSet

/**
 * @brief An associative set for the victim cache
 *
 * This relies on a FIFO eviction policy.
 *
 * Any read that finds a matching tag will also remove the entry and return its
 * contents.
 */
class VictimSet : public CacheSet
{
    private: 
        uint64_t v;
    public:
        /**
         * @brief initialize a fully associative set from only b and num entries
         *
         * clog2(v) is the number of bits needed to represent desired number of
         * VC blocks
         *
         * @param v the number of blocks per victim cache
         */
        VictimSet(uint64_t v_i, uint64_t b_i) 
            : CacheSet(clog2(v_i) + b_i, b_i, clog2(v_i)), v(v_i)
        {
        }

        /**
         * Default constructor, for when parameters not yet passed in
         */
        VictimSet()
        {}

        /**
         * Initialize, with new V parameter
         */
        void init(uint64_t v_i, uint64_t b_i)
        {
            CacheSet::init(clog2(v_i) + b_i, b_i, clog2(v_i));
            v = v_i;
        }


        /**
         * Insert entry into VictimSet
         */
        CacheEntry insert(CacheEntry entry)
        {
            if (this->set.size() < this->ways) { // set not full
                this->set.push_front(entry);
                return CacheEntry();
            } else { // have to evict the FIFO entry (at tail of cache)
                // Copy value of FIFO entry
                CacheEntry fifo = this->set.back();
                this->set.pop_back();
                this->set.push_front(entry);
                return fifo;
            }
        }

        // Use CacheSet::retrieve() to get elements out of Victim Set

}; // VictimSet

class Prefetcher
{
    private:
        /**
         * Reference the L2 cache for prefetching ops
         */
        std::vector<LruSet>& prefCache;

        /**
         * evictions buffer will hold entries evicted by prefetch
         */
        std::list<CacheEntry> evictions;

        // The number of blocks to prefetch
        uint64_t k;

        // Total number of blocks in cache
        /* const uint64_t blocksPerCache; */

        /**
         * Definitions of structure of blocks in cache we prefetch to
         */
        uint64_t c, b, s;
    public:
        Prefetcher(std::vector<LruSet>& prefCache_i, uint64_t k_i, uint64_t c_i, 
                uint64_t b_i, uint64_t s_i) 
            : prefCache(prefCache_i), k(k_i), c(c_i), b(b_i), s(s_i)
        {}

        /**
         * Constructor referencing to a cache
         * Can be parameterized using init()
         */
        Prefetcher(std::vector<LruSet>& prefCache_i) : prefCache(prefCache_i)
        {}

        void init(uint64_t k_i, uint64_t c_i, uint64_t b_i, uint64_t s_i) 
        {
            k = k_i;
            c = c_i;
            b = b_i;
            s = s_i;
        }

        /**
         * @brief prefetch K blocks into cache
         *
         * K blocks with increasing block addresses will be prefetched into cache.
         * This may cause evictions from the cache, which will be stored in the
         * evictions buffer
         * Each time this runs, the evictions buffer is flushed
         *
         * @param startEntry the entry after which K of the following blocks will be
         * fetched into the cache LRU values
         */
        void prefetch(const CacheEntry& startEntry)
        {
            evictions.clear();
            
            // Create local entry whose block address can be manipulated
            CacheEntry tmp_entry = startEntry;

            // A prefetched entry cannot be dirty
            tmp_entry.setDirty(false);

            // Set prefetched flag in all prefetched entries placed into cache
            tmp_entry.setPrefetched(true);

            uint64_t tmp_blockAddress = startEntry.getBlockAddress();
            for (auto i=0UL; i<k; ++i) {
                ++tmp_blockAddress;
                // Set the incremented block address for prefetched entry
                // All tags, indexes, etc. can ba calculated off of this
                tmp_entry.setBlockAddress(tmp_blockAddress);

                // Select set of cache at the index of incremented base address
                LruSet& prefEntrySet = prefCache.at(tmp_entry.getIndex());

                // Check that set does not contain prefetched entry
                if(!prefEntrySet.contains(tmp_entry.getTag())) {
                    // Insert prefetched entry into set
                    CacheEntry evicted  = prefEntrySet.insertLru(tmp_entry);
                    if(!evicted.isBlank()) {
                        // If evictions occur, place them into evictions buffer
                        evictions.push_back(evicted);
                    }
                }
            }
        }

        /**
         * @brief pops eviction from evictions buffer
         *
         * Copies over and removes eviction from evictions buffer
         */
        CacheEntry popEviction() 
        {
            CacheEntry eviction = evictions.front();
            evictions.pop_front();
            return eviction;
        }

        bool checkEmpty()
        {
            return (evictions.empty());
        }
}; // Prefetcher

/**
 * Globals used in actual simulation
 */
uint64_t L1_C;
uint64_t L1_S;
uint64_t L2_C;
uint64_t L2_S;
uint64_t B;
uint64_t V;
uint64_t K;

uint64_t L1_NUM_SETS, L2_NUM_SETS;

/**
 * The vectors for the caches map indexes to associative sets
 */
std::vector<LruSet> l1; 
std::vector<LruSet> l2;

Prefetcher l2Prefetch(l2);

VictimSet vc;


/** @brief Function to initialize your cache structures and any globals that you might need
 *
 *  @param conf pointer to the cache configuration structure
 *
 */
void cache_init(struct cache_config_t *conf)
{
    // Set to globals to be accessible later
    L1_C = conf->c;
    L1_S = conf->s;
    L2_C = conf->C;
    L2_S = conf->S;
    B = conf->b;
    V = conf->v;
    K = conf->k;


    // Number of sets in each cache = 2^(c-s-b)
    // (number index bits) = C - S - B
    L1_NUM_SETS = 1UL << (L1_C - L1_S - B);
    L2_NUM_SETS = 1UL << (L2_C - L2_S - B);

    // Reserve space on L1 and L2 vectors equal to 2^(number index bits)
    l1.reserve(L1_NUM_SETS);
    l2.reserve(L2_NUM_SETS);

    // Allocate sets for each cache
    for (auto i=0UL; i<L1_NUM_SETS; ++i) {
        l1.push_back(LruSet(L1_C, B, L1_S));
    }

    for (auto i=0UL; i<L2_NUM_SETS; ++i) {
        l1.push_back(LruSet(L2_C, B, L2_S));
    }

    // Initialize prefetcher object, which will handle prefetching into L2
    l2Prefetch.init(K, L2_C, B, L2_S);

    // Set up victim cache
    vc.init(V, B);

    throw "done initializing!!!\n";

}

/** @brief Function to initialize your cache structures and any globals that you might need
 *
 *  @param addr The address being accessed
 *  @param rw Tell if the access is a read or a write
 *  @param stats Pointer to the cache statistics structure
 *
 */
void cache_access(uint64_t addr, char rw, struct cache_stats_t *stats)
{
}

/** @brief Function to free any allocated memory and finalize statistics
 *
 *  @param stats pointer to the cache statistics structure
 *
 */
void cache_cleanup(struct cache_stats_t *stats)
{
}
