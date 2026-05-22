#ifndef _CACHE_FLUSH_H_
#define _CACHE_FLUSH_H_

#ifdef __cplusplus
#define BEGIN_C_DECLS \
	extern "C"        \
	{
#define END_C_DECLS }
#else /* !__cplusplus */
#define BEGIN_C_DECLS
#define END_C_DECLS
#endif /* __cplusplus */

BEGIN_C_DECLS;

#include <stdint.h>

typedef enum levels
{
	_CF_L1_ = 1, /* flushes "only" L1 data cache */
	_CF_L2_,	 /* flushes L1 and L2 data cache */
	_CF_L3_,	 /* flushes all caches from L3 down */
	_CF_NONE_
} lvl_enum_t;

struct _cf
{
	uint8_t num_lvl;
	struct cf_lvl
	{
		uint32_t c_size;
		uint32_t c_line_size;
	} cf_lvls[_CF_L3_];
	char *cf_buf;
};

extern struct _cf _cf_obj;
extern const lvl_enum_t cache_lvls[];

int cf_init(void);
int cf_flush(lvl_enum_t);
int cf_finalize(void);
const char *lvl_to_str(const lvl_enum_t);

#ifndef asm
#define asm __asm__
#endif

#ifdef __aarch64__
#define cf_clflush(__p)                          \
	{                                            \
		asm volatile("dc civac, %0" ::"r"(__p)); \
		asm volatile("dsb ish");                 \
		asm volatile("isb");                     \
	}
#endif

#ifdef __x86_64__
#define cf_clflush(__p) \
	asm volatile("clflush %0" : "+m"(*(volatile char *)(__p)));
#endif
END_C_DECLS;
#endif /* _CACHE_FLUSH_H_ */
