#ifndef CACHESIM_HPP
#define CACHESIM_HPP

#include <cinttypes>
#include <stdint.h>
#include <math.h>
#include <memory>
#include <vector>
#include <map>
#include <deque>

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
 * Cache
 */
class Cache {
private:
	uint64_t cache_size;
	uint64_t block_size;
	uint64_t num_blocks; // blocks per set
	uint64_t num_victim_blocks;

	std::map<uint64_t, CacheSet> sets;
	std::map<uint64_t, Block> victims;
	std::deque<uint64_t> victim_track;

	uint64_t tag_mask;
	uint64_t tag_offset;
	uint64_t index_mask;
	uint64_t index_offset;
	uint64_t get_tag(uint64_t address);
	uint64_t get_index(uint64_t address);
public:
	Cache(uint64_t c, uint64_t b, uint64_t s, uint64_t v);
	virtual ~Cache();

	cache_stats_t stats;
	bool write(uint64_t address);
	bool read(uint64_t address);
};

/*
 * Cache set
 */
class CacheSet {
private:
	std::map<uint64_t, Block> blocks;
	std::deque<uint64_t> block_track;
public:
	CacheSet():blocks(), block_track(){};
	virtual ~CacheSet();
	bool isFull();
	bool has_block(uint64_t tag);
};

/*
 * Block
 */
class Block {
private:
	uint64_t size;
	uint64_t tag;
	uint8_t valid;
public:
	Block();
	virtual ~Block();

	virtual void evict();
};

std::shared_ptr<Cache> cache;

#endif /* CACHESIM_HPP */
