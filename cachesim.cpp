#include "cachesim.hpp"

/**
 * Subroutine for initializing the cache. You many add and initialize any global or heap
 * variables as needed.
 * XXX: You're responsible for completing this routine
 *
 * @c The total number of bytes for data storage is 2^C
 * @b The size of a single cache line in bytes is 2^B
 * @s The number of blocks in each set is 2^S
 * @v The number of blocks in the victim cache is 2^V
 * @st The storage policy, BLOCKING or SUBBLOCKING (refer to project description for details)
 * @r The replacement policy, LRU or NMRU_FIFO (refer to project description for details)
 */
void setup_cache(uint64_t c, uint64_t b, uint64_t s, uint64_t v, char st,
		char r) {
	cache.reset(new Cache(c, b, s, v));
}
/**
 * Subroutine that simulates the cache one trace event at a time.
 * XXX: You're responsible for completing this routine
 *
 * @rw The type of event. Either READ or WRITE
 * @address  The target memory address
 * @p_stats Pointer to the statistics structure
 */
void cache_access(char rw, uint64_t address, cache_stats_t* p_stats) {

}

/**
 * Subroutine for cleaning up any outstanding memory operations and calculating overall statistics
 * such as miss rate or average access time.
 * XXX: You're responsible for completing this routine
 *
 * @p_stats Pointer to the statistics structure
 */
void complete_cache(cache_stats_t *p_stats) {
	// todo
}

Cache::Cache(uint64_t c, uint64_t b, uint64_t s, uint64_t v) {
	this->block_size = pow(2, c);
	this->cache_size = pow(2, b);
	this->num_blocks = pow(2, s);
	this->num_victim_blocks = pow(2, v);

	// update offsets
	this->index_offset = b;
	this->tag_offset = c - s;

	// update masks
	int set_bits = c - b - s;
	for (int i = 0; i < set_bits; ++i) {
		this->index_mask |= (1 << i);
	}
	for (int i = 0; i < 64 - c + s; ++i) {
		this->tag_mask |= (1 << i);
	}
}

uint64_t Cache::get_tag(uint64_t address) {
	return tag_mask & (address >> tag_offset);
}

uint64_t Cache::get_index(uint64_t address) {
	return index_mask & (address >> index_offset);
}

bool Cache::read(uint64_t address) {
	int index = get_index(address);
	int tag = get_tag(address);
	auto iter = sets.find(index);
	if (iter == sets.end()) {
		// set not init

	} else {
		auto set = iter->second;
		if(set.has_block(tag)) {
			// cache hit
		} else {
			// cache miss
		}

		// todo
		// update stats
	}
	// todo
	// update LRU
	return false;
}

/*
 * Cache Set
 */
bool CacheSet::isFull() {
	return false;
}

bool CacheSet::has_block(uint64_t tag) {
	if (blocks.find(tag) == blocks.end()) {
		// not exist
		return false;
	}
	return true;
}
