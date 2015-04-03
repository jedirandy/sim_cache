#ifndef CACHESIM_HPP
#define CACHESIM_HPP

#include <cinttypes>
#include <stdint.h>
#include <math.h>
#include <memory>
#include <deque>
#include <algorithm>
#include <iostream>
#include <unordered_map>

#define NONE -1

struct cache_stats_t {
	uint64_t accesses;
	uint64_t reads;
	uint64_t read_misses;
	uint64_t read_misses_combined;
	uint64_t writes;
	uint64_t write_misses;
	uint64_t write_misses_combined;
	uint64_t misses;
	uint64_t hit_time;
	uint64_t miss_penalty;
	double miss_rate;
	double avg_access_time;
	uint64_t storage_overhead;
	double storage_overhead_ratio;
};

void cache_access(char rw, uint64_t address, cache_stats_t* p_stats);
void setup_cache(uint64_t c, uint64_t b, uint64_t s, uint64_t v, char st,
		char r);
void complete_cache(cache_stats_t *p_stats);

static const uint64_t DEFAULT_C = 15; /* 32KB Cache */
static const uint64_t DEFAULT_B = 5; /* 32-byte blocks */
static const uint64_t DEFAULT_S = 3; /* 8 blocks per set */
static const uint64_t DEFAULT_V = 2; /* 4 victim blocks */

static const char BLOCKING = 'B';
static const char SUBBLOCKING = 'S';
static const char DEFAULT_ST = BLOCKING;

static const char LRU = 'L';
static const char NMRU_FIFO = 'N';
static const char DEFAULT_R = LRU;

/** Argument to cache_access rw. Indicates a load */
static const char READ = 'r';
/** Argument to cache_access rw. Indicates a store */
static const char WRITE = 'w';

enum Result {
	HIT,
	HIT_VC,
	MISS
};

/*
 * Block
 */
struct Block {
	uint64_t tag;
	uint8_t valid;
	bool is_null;
	Block(uint64_t tag, uint8_t valid);
	Block();
	virtual ~Block() {
	}
};

class Address {
private:
	static uint64_t tag_mask;
	static uint64_t index_mask;
	static uint64_t offset_mask;
	static uint64_t tag_offset;
	static uint64_t index_offset;

public:
	static uint64_t tag_bits;
	static uint64_t index_bits;
	static uint64_t offset_bits;
	uint64_t tag;
	uint64_t index;
	uint64_t offset;
	bool is_valid;

	Address(uint64_t address);
	Address(uint64_t tag, uint64_t index, uint64_t offset, bool is_valid);
	Address();
	virtual ~Address() {
	}

	static void init(uint64_t tag_bits, uint64_t index_bits,
			uint64_t offset_bits);
	static Address flat_address(const Address& adr); // convert to full-associative address
	static uint64_t get_tag(uint64_t address);
	static uint64_t get_index(uint64_t address);
	static uint64_t get_offset(uint64_t address);
	static uint8_t which_half(uint64_t offset); // determine which half of the sub-block from the offset
};
/*
 * Cache set
 */
class CacheSet {
protected:
	uint64_t num_blocks;
	std::deque<uint64_t> block_queue;
	std::unordered_map<uint64_t, Block> block_map;
	char replace_policy;
	char storage_policy;

public:
	int64_t index;
	uint64_t mru_tag;
	CacheSet(uint64_t num_blocks, char rpl_p, char sto_p, int64_t index);
	virtual ~CacheSet() {
	}
	bool is_full();

	// add a block from the address, return popped address and block
	virtual std::pair<Address, Block> add(const Address& address);
	virtual std::pair<Address, Block> add(const Address& address,
			const Block& block);
	virtual Block fetch(const Address& address);
	virtual int64_t remove(const Address& address);
	virtual int64_t remove(uint64_t tag);
	virtual bool access(const Address& address);
	virtual Block evict();
	virtual void print_queue();

	size_t get_size();
};

class VictimCache: public CacheSet {
public:
	VictimCache(uint64_t num_blocks);
	virtual ~VictimCache() {
	}

	virtual std::pair<Address, Block> add(const Address& address) override;
	virtual std::pair<Address, Block> add(const Address& address,
			const Block& block) override;
	virtual bool access(const Address& address) override;
	virtual Block fetch(const Address &address) override;
	virtual int64_t remove(const Address& address) override;
};

class Cache {
private:
	static uint64_t cache_size; // cache size in bytes
	static uint64_t block_size;
	static uint64_t num_blocks; // number of blocks per set
	static uint64_t num_victim_blocks;
	static char replace_policy;
	static char storage_policy;
	std::unordered_map<uint64_t, CacheSet> sets;
	std::shared_ptr<CacheSet> victim;
public:
	Cache(uint64_t cache_size, uint64_t block_size, uint64_t num_blocks,
			uint64_t num_victim_blocks, char storage_policy, char replace_policy);
	virtual ~Cache() {
	}

	Result access(char rw, uint64_t address);
	void update_stats(cache_stats_t* p_stats);
};

#endif /* CACHESIM_HPP */
