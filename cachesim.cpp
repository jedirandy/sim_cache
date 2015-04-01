#include "cachesim.hpp"

uint64_t g_cache_size = 0;
uint64_t g_block_size = 0;
uint64_t g_num_blocks = 0; // blocks per set
uint64_t g_num_victim_blocks = 0;

uint64_t g_tag_mask = 0;
uint64_t g_tag_offset = 0;
uint64_t g_index_mask = 0;
uint64_t g_index_offset = 0;
uint64_t g_offset_mask = 0;
uint64_t get_tag(uint64_t address);
uint64_t get_index(uint64_t address);
uint64_t get_offset(uint64_t address);

static char g_storage_policy;
static char g_replace_policy;

std::map<uint64_t, CacheSet> g_sets;
CacheSet g_victim;

Address::Address(uint64_t address) {
	this->index = get_index(address);
	this->tag = get_tag(address);
	this->offset = get_offset(address);
}

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
	g_cache_size = pow(2, c);
	g_block_size = pow(2, b);
	g_num_blocks = pow(2, s);
	g_num_victim_blocks = pow(2, v);
	g_storage_policy = st;
	g_replace_policy = r;

	// update offsets
	g_index_offset = b;
	g_tag_offset = c - s;

	// update masks
	int set_bits = c - b - s;
	for (int i = 0; i < set_bits; ++i) {
		g_index_mask |= (1 << i);
	}
	for (int i = 0; i < 64 - c + s; ++i) {
		g_tag_mask |= (1 << i);
	}
	for (int i = 0; i < b; ++i) {
		g_offset_mask |= (1 << i);
	}

	g_victim = CacheSet(g_num_victim_blocks, st, r, 0);

	// init cache sets
	int num_sets = g_cache_size / (g_block_size * g_num_blocks);
	for (int i = 0; i < num_sets; ++i) {
		CacheSet cs = CacheSet(g_num_blocks, g_storage_policy, g_replace_policy, i);
		g_sets[i] = cs;
	}
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
	p_stats->accesses++;
	if (rw == WRITE) {
		p_stats->writes++;
	} else {
		p_stats->reads++;
	}

	Address adr = Address(address);
	auto iter = g_sets.find(adr.index);
	if (iter != g_sets.end()) {
		auto set = iter->second;
		if (set.access(adr)) {
			// hit
		} else if (g_victim.access(adr)) {
			if (rw == WRITE) {
				p_stats->write_misses++;
			} else {
				p_stats->read_misses++;
			}
			// miss in main, hit in VC
			// 1. replace the cache in main by the hit cache in VC
			// 2. move the replaced block to the MRU position
			auto removed = set.add(adr);
			if (removed != NONE)
				g_victim.add(removed);
		} else {
			if (rw == WRITE) {
				p_stats->write_misses++;
				p_stats->write_misses_combined++;
			} else {
				p_stats->read_misses++;
				p_stats->read_misses_combined++;
			}
			// miss both
			// fetch
			auto removed = set.add(adr);
			if (removed != NONE)
				g_victim.add(removed);
		}
	} else {
		// set not initialized
		CacheSet cs = CacheSet(g_num_blocks, g_storage_policy, g_replace_policy, adr.index);
		g_sets[adr.index] = cs;
		cs.add(adr);

		if (rw == WRITE) {
			p_stats->write_misses++;
			p_stats->write_misses_combined++;
		} else {
			p_stats->read_misses++;
			p_stats->read_misses_combined++;
		}
	}
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
	p_stats->misses = p_stats->write_misses_combined + p_stats->read_misses_combined;
	p_stats->miss_rate = p_stats->misses / p_stats->accesses;
}

uint64_t get_tag(uint64_t address) {
	return g_tag_mask & (address >> g_tag_offset);
}

uint64_t get_index(uint64_t address) {
	return g_index_mask & (address >> g_index_offset);
}

uint64_t get_offset(uint64_t address) {
	return g_offset_mask & address;
}

/*
 * Cache Set
 */
bool CacheSet::isFull() {
	return block_map.size() >= num_blocks;
}

int64_t CacheSet::add(const Address& address) {
	if (storage_policy == SUBBLOCKING) {
		// check if exist
		auto iter = block_map.find(address.tag);
		if (iter != block_map.end()) {
			iter->second.valid |= which_half(address.offset);
		}
		// bring to the front
		access(address);
		return NONE;
	}

	if (isFull()) {
		// need to evict
		auto removed = block_queue.back();
		if (replace_policy == LRU) {
			auto iter = block_queue.end();
			remove(*iter);
		} else if (replace_policy == NMRU_FIFO) {
			auto iter = block_queue.begin();
			iter++;
			remove(*iter);
		}
		return removed;
	}
	// move to MRU
	Block block = Block(which_half(address.offset));
	block_map[address.tag] = block;
	block_queue.push_front(address.tag);
	std::cout<<block_map.size()<<std::endl;
	return NONE;
}

int64_t CacheSet::remove(uint64_t tag) {
	auto iter = block_map.find(tag);
	if (iter != block_map.end()) {
		// remove from the map
		block_map.erase(iter);
		// remove from the queue
		auto iter_q = std::find(block_queue.begin(), block_queue.end(), tag);
		block_queue.erase(iter_q);
		return tag;
	}
	// not found
	return NONE;
}

int64_t CacheSet::remove(const Address& address) {
	return remove(address.tag);
}

bool CacheSet::access(const Address& address) {
	uint64_t tag = address.tag;
	if (block_map.find(tag) != block_map.end()) {
		if (storage_policy == SUBBLOCKING) {
			auto block = block_map.find(tag)->second;
			if (!(block.valid && which_half(address.offset))) {
				// invalid half
				return false;
			}
		}
		// hit
		// move to MRU
		auto i = std::find(block_queue.begin(), block_queue.end(), tag);
		auto copy = *i;
		block_queue.erase(i);
		block_queue.push_front(copy);
		return true;
	}
	// miss
	return false;
}

uint8_t CacheSet::which_half(uint64_t offset) {
	if (offset < g_block_size / 2)
		return 1;
	return 2;
}
