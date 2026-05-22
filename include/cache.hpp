#ifndef __CACHE_HPP__
#define __CACHE_HPP__

extern "C" {
#include <cache_flush.h>
}

class CacheControl {
	std::size_t cache_line_size;
	std::size_t llc_size;
	int n_cache_level;

       public:
	CacheControl() {
		if (cf_init()) throw std::runtime_error("cf_init failed");
		n_cache_level = _cf_obj.num_lvl;
		llc_size = _cf_obj.num_lvl == 3 ? _cf_obj.cf_lvls[2].c_size
						: _cf_obj.cf_lvls[1].c_size;
		cache_line_size = _cf_obj.cf_lvls[0].c_line_size;
	}
	~CacheControl() { cf_finalize(); }
	std::size_t get_cache_line_size() const { return cache_line_size; }
	int get_n_cache_level() const { return n_cache_level; }
	void flush_cache_level(const lvl_enum_t lvl) const { cf_flush(lvl); }
	void flush_buffer(volatile void *p) const { cf_clflush(p); }
};
#endif
