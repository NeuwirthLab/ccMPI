#ifndef __UTIL_HPP__
#define __UTIL_HPP__

#include "cache_flush.h"
#define CLEAR_L1 cf_flush(cache_lvls[0])
#define CLEAR_L2 cf_flush(cache_lvls[1])
#define CLEAR_L3 cf_flush(cache_lvls[2])

static inline void flush_buffer(volatile char *ptr, const size_t len,
				const size_t cache_line_size) {
	for (volatile size_t i = 0; i < len; i += cache_line_size) {
		cf_clflush(ptr + i);
	}
}

static inline void builtin_prefetch_rfo(volatile char *ptr, const size_t len,
					const size_t cache_line_size) {
	for (volatile size_t i = 0; i < len; i += cache_line_size) {
#ifdef __aarch64__
		__asm__ volatile("prfm pstl2strm, [%0]" ::"r"((void*)&ptr[i]));
#else
		__builtin_prefetch((void *)&ptr[i], 1, 0);
#endif
	}
}

static inline void prefetch_modify_buf(volatile char *p, const size_t len,
				       const size_t cache_line_size) {
	for (volatile size_t i = 0; i < len; i += cache_line_size) {
		p[i] ^= p[i];
	}
}

static inline void prefetch_read_buf(volatile char *p, const size_t len,
				     const size_t cache_line_size) {
	volatile char t;
	for (volatile size_t i = 0; i < len; i += cache_line_size) {
		t = p[i];
	}
}
#define ABORT(code) MPI_Abort(MPI_COMM_WORLD, (code))
#define BARRIER_ALL MPI_Barrier(MPI_COMM_WORLD)
#endif
