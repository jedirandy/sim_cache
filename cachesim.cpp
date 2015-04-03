#include "cachesim.hpp"

uint64_t g_cache_size = 0; // cache size in bytes
uint64_t g_block_size = 0;
uint64_t g_num_blocks = 0; // number of blocks per set
uint64_t g_num_victim_blocks = 0;
uint64_t g_tag_bits = 0;
uint64_t g_tag_mask = 0;
uint64_t g_tag_offset = 0;
uint64_t g_index_mask = 0;
uint64_t g_index_offset = 0;
uint64_t g_offset_mask = 0;
uint64_t g_index_bits = 0;
char g_storage_policy;
char g_replace_policy;

std::map<uint64_t, CacheSet> g_sets;
std::shared_ptr<CacheSet> g_victim;

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
	g_tag_bits = 64 - c + s;
	g_index_bits = c - b - s;
	// update offsets
	g_index_offset = b;
	g_tag_offset = c - s;
	// update masks
	for (uint64_t i = 0; i < g_index_bits; ++i) {
		g_index_mask |= (1 << i);
	}
	for (uint64_t i = 0; i < 64 - c + s; ++i) {
		g_tag_mask |= (1 << i);
	}
	for (uint64_t i = 0; i < b; ++i) {
		g_offset_mask |= (1 << i);
	}

	g_victim.reset(new VictimCache(g_num_victim_blocks));
	// init cache sets
	int num_sets = g_cache_size / (g_block_size * g_num_blocks);
	for (int i = 0; i < num_sets; ++i) {
		CacheSet cs(g_num_blocks, g_storage_policy, g_replace_policy, i);
		g_sets.insert(std::make_pair(i, cs));
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

	Address adr(address);
	auto& set = g_sets.at(adr.index);
	if (set.access(adr)) {
		// hit
	} else if (g_victim->access(adr)) {
		// miss in the main cache, hit in the victim cache
		// 1. replace the cache in the MC by the hit cache in the VC
		// 2. move the replaced block to the MRU position
		auto evicted = set.add(adr);
		g_victim->remove(adr);
		if (evicted.first.is_valid)
			g_victim->add(evicted.first, evicted.second);

		if (rw == WRITE) {
			p_stats->write_misses++;
		} else {
			p_stats->read_misses++;
		}
	} else {
		// miss both
		if (rw == WRITE) {
			p_stats->write_misses++;
			p_stats->write_misses_combined++;
		} else {
			p_stats->read_misses++;
			p_stats->read_misses_combined++;
		}
		if (g_storage_policy == SUBBLOCKING) {
			// partial-miss in the MC
			Block block = set.fetch(adr);
			if (!block.is_null) {
				set.add(adr, block);
				return;
			}
			// if found a partial-miss block in VC
			// 1. update it's valid bit
			// 2. move to the main cache
			block = g_victim->fetch(adr);
			if (!block.is_null) {
				block.valid |= set.which_half(adr.offset);
				auto evicted = set.add(adr, block);
				g_victim->remove(adr);
				g_victim->add(evicted.first, evicted.second);
				return;
			}
		}
		// fetch from memory
		auto evicted = set.add(adr);
		if (evicted.first.is_valid)
			g_victim->add(evicted.first, evicted.second);
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
	p_stats->misses = p_stats->write_misses_combined
			+ p_stats->read_misses_combined;
	p_stats->miss_rate = (double) p_stats->misses / (double) p_stats->accesses;

	p_stats->hit_time = ceil(0.2 * g_num_blocks);
	if (g_storage_policy == BLOCKING) {
		p_stats->miss_penalty = ceil(
				0.2 * g_num_blocks + 50 + 0.25 * g_block_size);
	} else if (g_storage_policy == SUBBLOCKING) {
		p_stats->miss_penalty = ceil(
				0.2 * g_num_blocks + 50 + 0.25 * g_block_size / 2);
	}

	double misses = p_stats->misses;
	double hits = p_stats->accesses - p_stats->misses;
	double total = p_stats->accesses;
	p_stats->avg_access_time = (p_stats->hit_time * hits
			+ (p_stats->miss_penalty + p_stats->hit_time) * misses) / total;

	uint64_t total_num_blocks = g_cache_size / g_block_size;
	uint64_t overhead = 0;
	uint64_t valid_bits = 0;
	uint64_t controller_bits = 0;
	uint64_t dirty_bits = 1;
	if (g_storage_policy == BLOCKING) {
		valid_bits = 1;
	} else {
		valid_bits = 2;
	}
	if (g_replace_policy == LRU) {
		controller_bits = 8;
	} else {
		controller_bits = 4;
	}
	// main cache: num_block * (dirty + valid + tags + LRU controller bits)
	overhead += total_num_blocks
			* (dirty_bits + valid_bits + g_tag_bits + controller_bits);
	// victim: num_blocks * (dirty + valid + tags + index + LRU controller bits)
	overhead += g_num_victim_blocks
			* (dirty_bits + valid_bits + g_tag_bits + g_index_bits + 8);

	p_stats->storage_overhead = overhead;
	uint64_t total_bits = 0;
	total_bits = (total_num_blocks + g_num_victim_blocks) * g_block_size * 8;

	p_stats->storage_overhead_ratio = (double) overhead / (double) total_bits;
}

/*
 * Address utils
 */
uint64_t get_tag(uint64_t address) {
	return g_tag_mask & (address >> g_tag_offset);
}

uint64_t get_index(uint64_t address) {
	return g_index_mask & (address >> g_index_offset);
}

uint64_t get_offset(uint64_t address) {
	return g_offset_mask & address;
}

Address::Address(uint64_t address) {
	this->index = get_index(address);
	this->tag = get_tag(address);
	this->offset = get_offset(address);
	this->is_valid = true;
}

Address::Address() {
	this->index = 0;
	this->tag = 0;
	this->offset = 0;
	this->is_valid = false;
}

/*
 * Block
 */

Block::Block(uint64_t tag, uint8_t valid) :
		tag(tag), valid(valid), is_null(false) {
}

Block::Block() :
		tag(0), valid(0), is_null(true) {
}

/*
 * Cache Set
 */
CacheSet::CacheSet(uint64_t num_blocks, char sto_p, char rpl_p, int64_t index) :
		num_blocks(num_blocks), replace_policy(rpl_p), storage_policy(sto_p), index(
				index), mru_tag(0) {
}

bool CacheSet::is_full() {
	return block_map.size() >= this->num_blocks;
}

size_t CacheSet::get_size() {
	return block_map.size();
}

std::pair<Address, Block> CacheSet::add(const Address& address,
		const Block& block) {
	Block new_block;
	Block evicted_block;
	Address evicted_address;
	if (!address.is_valid) {
		return std::make_pair(evicted_address, new_block);
	}

	if (storage_policy == SUBBLOCKING) {
		// check if exist
		auto iter = block_map.find(address.tag);
		if (iter != block_map.end()) {
			// update valid bit
			iter->second.valid |= which_half(address.offset);
			access(address);
			return std::make_pair(evicted_address, new_block);
		}
	}

	if (is_full()) {
		// full, need to evict
		evicted_block = evict();
		evicted_address.tag = evicted_block.tag;
		evicted_address.index = address.index;
		evicted_address.is_valid = true;
	}
	if (block.is_null) {
		new_block = Block(address.tag, which_half(address.offset));
	} else {
		new_block = block;
	}
	block_map.insert(std::make_pair(address.tag, new_block));
	block_queue.emplace_back(address.tag);
	return std::make_pair(evicted_address, evicted_block);
}

std::pair<Address, Block> CacheSet::add(const Address& address) {
	Block blk;
	return add(address, blk);
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

Block CacheSet::evict() {
	auto iter = block_queue.begin();
	if (replace_policy == NMRU_FIFO && *iter == mru_tag) {
		iter++;
	}
	auto map_iter = block_map.find(*iter);
	Block block = map_iter->second;
	block_map.erase(map_iter);
	block_queue.erase(iter);

	return block;
}

bool CacheSet::access(const Address& address) {
	uint64_t tag = address.tag;
#ifdef _DEBUG
	std::cout << "accessing tag:" << address.tag << " index:" << address.index
			<< " offset:" << address.offset << " set number:" << index;
#endif
	auto iter = block_map.find(tag);
	if (iter != block_map.end()) {
		if (storage_policy == SUBBLOCKING) {
			auto block = iter->second;
			if (!(block.valid & which_half(address.offset))) {
#ifdef _DEBUG
				std::cout << " Sub-blocking miss, valid:" << block.valid
						<< " required:" << which_half(address.offset)
						<< std::endl;
#endif
				// invalid half
				return false;
			}
		}
		// hit
		if (replace_policy == LRU) {
			// move to the MRU position (front)
			auto i = std::find(block_queue.begin(), block_queue.end(), tag);
			auto copy = *i;
			block_queue.erase(i);
			block_queue.emplace_back(copy);
		} else if (replace_policy == NMRU_FIFO) {
			// update the MRU tag
			mru_tag = tag;
		}
#ifdef _DEBUG
		std::cout << " hit" << std::endl;
#endif
		return true;
	}

#ifdef _DEBUG
	std::cout << " miss" << std::endl;
#endif
	// miss
	return false;
}

Block CacheSet::fetch(const Address& address) {
	Block block;
	auto iter = block_map.find(address.tag);
	if (iter != block_map.end()) {
		block = iter->second;
	}
	return block;
}

uint8_t CacheSet::which_half(uint64_t offset) {
	if (offset < g_block_size / 2)
		return 1;
	return 2;
}

void CacheSet::print_queue() {
	for (auto e : block_queue) {
		std::cout << e << ",";
	}
	std::cout << std::endl;
}

/*
 * Victim
 */
VictimCache::VictimCache(uint64_t num_blocks) :
		CacheSet(num_blocks, BLOCKING, LRU, -1) {
}

Address VictimCache::convert_address(const Address& adr) {
	Address address;
	address.tag = ((adr.tag << (g_tag_offset - g_index_offset)) | adr.index)
			& g_tag_mask;
	address.index = adr.index;
	address.offset = adr.offset;
	address.is_valid = adr.is_valid;
	return address;
}

std::pair<Address, Block> VictimCache::add(const Address& adr) {
	return CacheSet::add(convert_address(adr));
}

std::pair<Address, Block> VictimCache::add(const Address& adr,
		const Block& block) {
	return CacheSet::add(convert_address(adr), block);
}

int64_t VictimCache::remove(const Address& adr) {
	return CacheSet::remove(convert_address(adr));
}

bool VictimCache::access(const Address& adr) {
	return CacheSet::access(convert_address(adr));
}

Block VictimCache::fetch(const Address& adr) {
	return CacheSet::fetch(convert_address(adr));
}
