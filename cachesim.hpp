#ifndef CACHESIM_HPP
#define CACHESIM_HPP

#include <cinttypes>
#include <stdint.h>
#include <math.h>
#include <memory>
#include <vector>
#include <map>
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
    double   miss_rate;
    double   avg_access_time;
    uint64_t storage_overhead;
    double   storage_overhead_ratio;
};

void cache_access(char rw, uint64_t address, cache_stats_t* p_stats);
void setup_cache(uint64_t c, uint64_t b, uint64_t s, uint64_t v, char st, char r);
void complete_cache(cache_stats_t *p_stats);

static const uint64_t DEFAULT_C = 15;   /* 32KB Cache */
static const uint64_t DEFAULT_B = 5;    /* 32-byte blocks */
static const uint64_t DEFAULT_S = 3;    /* 8 blocks per set */
static const uint64_t DEFAULT_V = 2;    /* 4 victim blocks */

static const char     BLOCKING = 'B';
static const char     SUBBLOCKING = 'S';
static const char     DEFAULT_ST = BLOCKING;

static const char     LRU = 'L';
static const char     NMRU_FIFO = 'N';
static const char     DEFAULT_R = LRU;

/** Argument to cache_access rw. Indicates a load */
static const char     READ = 'r';
/** Argument to cache_access rw. Indicates a store */
static const char     WRITE = 'w';

/*
 * Block
 */
class Block {
private:
public:
	uint64_t tag;
	uint8_t valid;
	bool is_null;
	Block(uint64_t tag, uint8_t valid): tag(tag), valid(valid), is_null(false) {};
	Block(): valid(0), is_null(true), tag(0){};
	virtual ~Block(){};
};

struct Address {
	uint64_t tag;
	uint64_t index;
	uint64_t offset;
	bool valid;
	Address(uint64_t address);
	Address();
};
/*
 * Cache set
 */
class CacheSet {
protected:
	uint64_t num_blocks;
	std::deque<uint64_t> block_queue;
	char replace_policy;
	char storage_policy;
	uint8_t which_half(uint64_t offset);
public:
	int count = 0;
	int64_t index;
	std::unordered_map<uint64_t, Block> block_map;
	CacheSet(uint64_t num_blocks, char sto_p, char rpl_p, int64_t index);
	virtual ~CacheSet(){};
	bool is_full();

	// add a block from the address, return invalid if no block is popped
	virtual Address add(const Address& address);
	virtual int64_t remove(const Address& address);
	virtual int64_t remove(uint64_t tag);
	virtual bool access(const Address& address);
	virtual Block evict();
	virtual void print_queue();

	size_t get_size();
};

class VictimCache: public CacheSet{
public:
	VictimCache(uint64_t num_blocks);
	virtual ~VictimCache(){};

	Address convert_address(const Address& address);
	virtual Address add(const Address& address) override;
	virtual bool access(const Address& address) override;
	virtual int64_t remove(const Address& address) override;
};

uint64_t get_tag(uint64_t address);
uint64_t get_index(uint64_t address);
uint64_t get_offset(uint64_t address);

#endif /* CACHESIM_HPP */
