#include "cachesim.hpp"

std::shared_ptr<Cache> g_cache_ptr;
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
	uint64_t cache_size = pow(2, c);
	uint64_t block_size = pow(2, b);
	uint64_t num_blocks = pow(2, s);
	uint64_t num_victim_blocks = pow(2, v);
	g_cache_ptr.reset(
			new Cache(cache_size, block_size, num_blocks, num_victim_blocks, st,
					r));

	// update masks
	uint64_t tag_bits = 64 - c + s;
	uint64_t index_bits = c - b - s;
	uint64_t offset_bits = b;
	Address::init(tag_bits, index_bits, offset_bits);
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

	Result result = g_cache_ptr->access(rw, address);
	switch (result) {
	case HIT:
		break;
	case HIT_VC:
		if (rw == WRITE) {
			p_stats->write_misses++;
		} else {
			p_stats->read_misses++;
		}
		break;
	case MISS:
		if (rw == WRITE) {
			p_stats->write_misses++;
			p_stats->write_misses_combined++;
		} else {
			p_stats->read_misses++;
			p_stats->read_misses_combined++;
		}
		break;
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
	g_cache_ptr->update_stats(p_stats);
}

/*
 * Address
 */
uint64_t Address::tag_mask;
uint64_t Address::index_mask;
uint64_t Address::offset_mask;
uint64_t Address::tag_offset;
uint64_t Address::index_offset;
uint64_t Address::tag_bits;
uint64_t Address::index_bits;
uint64_t Address::offset_bits;

uint64_t Address::get_tag(uint64_t address) {
	return tag_mask & (address >> tag_offset);
}

uint64_t Address::get_index(uint64_t address) {
	return index_mask & (address >> index_offset);
}

uint64_t Address::get_offset(uint64_t address) {
	return offset_mask & address;
}

Address::Address(uint64_t address) {
	this->index = get_index(address);
	this->tag = get_tag(address);
	this->offset = get_offset(address);
	this->is_valid = true;
}

Address::Address() :
		tag(0), index(0), offset(0), is_valid(false) {
}

Address::Address(uint64_t tag, uint64_t index, uint64_t offset, bool is_valid) :
		tag(tag), index(index), offset(offset), is_valid(is_valid) {
}

void Address::init(uint64_t _tag_bits, uint64_t _index_bits,
		uint64_t _offset_bits) {
	tag_bits = _tag_bits;
	index_bits = _index_bits;
	offset_bits = _offset_bits;
	// set masks
	for (uint64_t i = 0; i < tag_bits; ++i) {
		tag_mask |= (1 << i);
	}
	for (uint64_t i = 0; i < index_bits; ++i) {
		index_mask |= (1 << i);
	}
	for (uint64_t i = 0; i < offset_bits; ++i) {
		offset_mask |= (1 << i);
	}
	index_offset = offset_bits;
	tag_offset = index_bits + offset_bits;
}

Address Address::flat_address(const Address& adr) {
	Address address(
			((adr.tag << (tag_offset - index_offset)) | adr.index) & tag_mask,
			adr.index, adr.offset, adr.is_valid);
	return address;
}

uint8_t Address::which_half(uint64_t offset) {
	if (offset < pow(2, offset_bits) / 2)
		return 1;
	return 2;
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
			iter->second.valid |= Address::which_half(address.offset);
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
		new_block = Block(address.tag, Address::which_half(address.offset));
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
			if (!(block.valid & Address::which_half(address.offset))) {
#ifdef _DEBUG
				std::cout << " Sub-blocking miss, valid:" << block.valid
				<< " required:" << Address::which_half(address.offset)
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

void CacheSet::print_queue() {
	for (auto e = block_queue.begin(); e!=block_queue.end(); ++e) {
		std::cout << *e << ",";
	}
	std::cout << std::endl;
}

/*
 * Victim
 */
VictimCache::VictimCache(uint64_t num_blocks) :
		CacheSet(num_blocks, BLOCKING, LRU, -1) {
}

std::pair<Address, Block> VictimCache::add(const Address& adr) {
	return CacheSet::add(Address::flat_address(adr));
}

std::pair<Address, Block> VictimCache::add(const Address& adr,
		const Block& block) {
	return CacheSet::add(Address::flat_address(adr), block);
}

int64_t VictimCache::remove(const Address& adr) {
	return CacheSet::remove(Address::flat_address(adr));
}

bool VictimCache::access(const Address& adr) {
	return CacheSet::access(Address::flat_address(adr));
}

Block VictimCache::fetch(const Address& adr) {
	return CacheSet::fetch(Address::flat_address(adr));
}

/*
 * Cache
 */
uint64_t Cache::cache_size; // cache size in bytes
uint64_t Cache::block_size;
uint64_t Cache::num_blocks; // number of blocks per set
uint64_t Cache::num_victim_blocks;
char Cache::replace_policy;
char Cache::storage_policy;

Cache::Cache(uint64_t _cache_size, uint64_t _block_size, uint64_t _num_blocks,
		uint64_t _num_victim_blocks, char _storage_policy,
		char _replace_policy) {
	cache_size = _cache_size;
	block_size = _block_size;
	num_blocks = _num_blocks;
	num_victim_blocks = _num_victim_blocks;
	storage_policy = _storage_policy;
	replace_policy = _replace_policy;
	// init victim cache
	victim.reset(new VictimCache(num_victim_blocks));
	// init cache sets
	int num_sets = cache_size / (block_size * num_blocks);
	for (int i = 0; i < num_sets; ++i) {
		CacheSet cs(num_blocks, storage_policy, replace_policy, i);
		sets.insert(std::make_pair(i, cs));
	}
}

Result Cache::access(char rw, uint64_t address) {
	Result result;
	Address adr(address);
	auto& set = sets.at(adr.index);
	if (set.access(adr)) {
		// hit
		result = HIT;
	} else if (victim->access(adr)) {
		result = HIT_VC;
		auto evicted = set.add(adr);
		victim->remove(adr);
		if (evicted.first.is_valid)
			victim->add(evicted.first, evicted.second);
	} else {
		result = MISS;
		if (storage_policy == SUBBLOCKING) {
			// partial-miss in the MC
			Block block = set.fetch(adr);
			if (!block.is_null) {
				set.add(adr, block);
				return result;
			}
			// if found a partial-miss block in VC
			// 1. update it's valid bit
			// 2. move to the main cache
			block = victim->fetch(adr);
			if (!block.is_null) {
				block.valid |= Address::which_half(adr.offset);
				auto evicted = set.add(adr, block);
				victim->remove(adr);
				victim->add(evicted.first, evicted.second);
				return result;
			}
		}
		// fetch from memory
		auto evicted = set.add(adr);
		if (evicted.first.is_valid)
			victim->add(evicted.first, evicted.second);
	}
	return result;
}

void Cache::update_stats(cache_stats_t* p_stats) {
	p_stats->misses = p_stats->write_misses_combined
			+ p_stats->read_misses_combined;
	p_stats->miss_rate = (double) p_stats->misses / (double) p_stats->accesses;

	p_stats->hit_time = ceil(0.2 * num_blocks);
	if (storage_policy == BLOCKING) {
		p_stats->miss_penalty = ceil(
				0.2 * num_blocks + 50 + 0.25 * block_size);
	} else if (storage_policy == SUBBLOCKING) {
		p_stats->miss_penalty = ceil(
				0.2 * num_blocks + 50 + 0.25 * block_size / 2);
	}

	double misses = p_stats->misses;
	double hits = p_stats->accesses - p_stats->misses;
	double total = p_stats->accesses;
	p_stats->avg_access_time = (p_stats->hit_time * hits
			+ (p_stats->miss_penalty + p_stats->hit_time) * misses) / total;

	int total_num_blocks = cache_size / block_size;
	int overhead = 0;
	int valid_bits = 0;
	int controller_bits = 0;
	int dirty_bits = 1;
	if (storage_policy == BLOCKING) {
		valid_bits = 1;
	} else {
		valid_bits = 2;
	}
	if (replace_policy == LRU) {
		controller_bits = 8;
	} else {
		controller_bits = 4;
	}
	// main cache: num_block * (dirty + valid + tags + LRU controller bits)
	overhead += total_num_blocks
			* (dirty_bits + valid_bits + Address::tag_bits + controller_bits);
	// victim: num_blocks * (dirty + valid + tags + index + LRU controller bits)
	overhead += num_victim_blocks
			* (dirty_bits + valid_bits + Address::tag_bits + Address::index_bits
					+ 8);

	p_stats->storage_overhead = overhead;
	int total_bits = (total_num_blocks + num_victim_blocks) * block_size * 8;

	p_stats->storage_overhead_ratio = (double) overhead / (double) total_bits;
}
