#define _POSIX_C_SOURCE 200112L
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(__INTEL_COMPILER)
#include "immintrin.h"
#endif

#include "cache_flush.h"

bool _cf_initialized = false;
bool _cf_finalized = false;
struct _cf _cf_obj = {.num_lvl = 0, .cf_buf = NULL};

#define _PATH_SYS_CACHE "/sys/devices/system/cpu/cpu0/cache"

static inline bool _clfsh_supported(void) {
#ifdef __aarch64__
	return true;  // clflush is not supported but for control flow we need
		      // to return true here
#else

	/* code from https://en.wikipedia.org/wiki/CPUID */
	unsigned int index = 1; /* EAX=1: proc info + feature bits */
	unsigned int reg[4];

	__asm__ __volatile__(
#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)
	    "pushq %%rbx        \n\t" /* save %rbx */
#else
	    "pushl %%ebx        \n\t" /* save %ebx */
#endif
	    "cpuid              \n\t"
	    "movl %%ebx ,%[ebx] \n\t" /* write the result into output var */
#if defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)
	    "popq %%rbx         \n\t"
#else
	    "popl %%ebx         \n\t"
#endif
	    : "=a"(reg[0]), [ebx] "=r"(reg[1]), "=c"(reg[2]), "=d"(reg[3])
	    : "a"(index));

	/* CPUID feature flag CLFSH: CPUID.01H:EDX[bit 19] */
	return ((reg[3] & (0x1 << 19)) ? true : false);
#endif
}

static inline size_t _not_power_of_two(size_t n) { return (n & (n - 1)); }

static size_t _next_power_of_two(size_t n) {
	int shift = 1;

	n -= 1;
	while (_not_power_of_two(n + 1)) {
		n |= n >> shift;
		shift <<= 1;
	}

	return (n + 1);
}

static int _dir_exist(const char *dname) {
	DIR *dir = opendir(dname);
	if (dir)
		closedir(dir);
	else
		return 0;
	return -1;
}

static int _fread_cache_info(const char *fname, const char *format,
			     unsigned int *res1, char *res2) {
	char buf[UINT8_MAX];
	FILE *fp = NULL;
	int ret = 0;

	if (access(fname, R_OK)) return -1;

	fp = fopen(fname, "r");
	if (!fp) return -1;
	if (fgets(buf, UINT8_MAX, fp) != NULL) {
		if (res1 && res2)
			sscanf(buf, format, res1, res2);
		else if (res1)
			sscanf(buf, format, res1);
		else if (res2)
			sscanf(buf, format, res2);
		else {
			fclose(fp);
			return -1;
		}
	}
	if ((ret = fclose(fp))) return -1;

	return ret;
}

static int _read_cache_configs(void) {
#ifdef __aarch64__  // lets assume we only care about A64FX for now
	_cf_obj.num_lvl = 2;
	_cf_obj.cf_lvls[0].c_line_size = 256;
	_cf_obj.cf_lvls[0].c_size = 65536;  // 64KiB
	_cf_obj.cf_lvls[1].c_line_size = 256;
	_cf_obj.cf_lvls[1].c_size = 8 * 1024 * 1024;  // 8MiB
#else
	unsigned int i = 0, c_num = 0, lvl = 0;
	char path[UINT8_MAX], c_str[UINT8_MAX];
	/* assume homogeneous cores -> only check cpu0 for number of caches */
	for (i = 0; i < UINT8_MAX; i++) {
		snprintf(path, UINT8_MAX, _PATH_SYS_CACHE "/index%u", i);
		if (!_dir_exist(path)) break;

		snprintf(path, UINT8_MAX, _PATH_SYS_CACHE "/index%u/type", i);
		if (_fread_cache_info(path, "%s", NULL, c_str)) return -1;
		if (!strcmp(c_str, "Instruction")) continue;
		c_str[0] = '\0';

		snprintf(path, UINT8_MAX, _PATH_SYS_CACHE "/index%u/level", i);
		if (_fread_cache_info(path, "%u", &c_num, NULL)) return -1;
		lvl = c_num - 1;
		c_num = 0;

		snprintf(path, UINT8_MAX,
			 _PATH_SYS_CACHE "/index%u/coherency_line_size", i);
		if (_fread_cache_info(path, "%u", &c_num, NULL)) return -1;
		_cf_obj.cf_lvls[lvl].c_line_size = c_num;
		c_num = 0;

		snprintf(path, UINT8_MAX, _PATH_SYS_CACHE "/index%u/size", i);
		if (_fread_cache_info(path, "%u%s", &c_num, c_str)) return -1;
		if (c_str[0] == '\0')
			_cf_obj.cf_lvls[lvl].c_size = c_num;
		else if (!strcmp(c_str, "K"))
			_cf_obj.cf_lvls[lvl].c_size = c_num << 10;
		else if (!strcmp(c_str, "M"))
			_cf_obj.cf_lvls[lvl].c_size = c_num << 20;
		else
			return -1;
		c_num = 0;
		c_str[0] = '\0';

		_cf_obj.num_lvl++;
	}
#endif
	return 0;
}

int cf_init(void) {
	size_t buf_alignment = 0, buf_size = 0;
	int i = 0, ret = 0;

	if (_cf_initialized || _cf_finalized) {
		fprintf(stderr,
			"cf_init called multiple times or after"
			" cf_finalize; abort");
		return -1;
	}

	if (!_clfsh_supported() || _read_cache_configs()) {
		goto ERROR;
	}

	for (i = 0; i < _cf_obj.num_lvl; i++) {
		if (buf_alignment < _cf_obj.cf_lvls[i].c_line_size)
			buf_alignment = _cf_obj.cf_lvls[i].c_line_size;
		if (buf_size < _cf_obj.cf_lvls[i].c_size)
			buf_size = _cf_obj.cf_lvls[i].c_size;
	}

	if (_not_power_of_two(buf_alignment)) {
		buf_alignment = _next_power_of_two(buf_alignment);
	}

	ret =
	    posix_memalign((void **)&(_cf_obj.cf_buf), buf_alignment, buf_size);

	if (ret || !_cf_obj.cf_buf) {
		errno = ENOMEM;
		goto ERROR;
	}

	memset(_cf_obj.cf_buf, 0, buf_size);

	_cf_initialized = true;
	return 0;

ERROR:
	(void)cf_finalize();
	return -1;
}

int cf_flush(lvl_enum_t lvl) {
	int i = 0, num_iter = 0, cl_size = 0;
	char *buf = NULL;

	if (!_cf_initialized || _cf_finalized || !_cf_obj.cf_buf) {
		fprintf(stderr,
			"cf_init not called or cf_finalize called"
			" already; abort");
		return -1;
	}

	if (_CF_NONE_ == lvl) {
		return 0;
	}

	if (lvl <= 0 || lvl > _cf_obj.num_lvl) {
		fprintf(stderr, "invalid requested cache level (%u)\n", lvl);
		errno = EINVAL;
		return errno;
	}

	buf = _cf_obj.cf_buf;
	cl_size = _cf_obj.cf_lvls[lvl - 1].c_line_size;
	num_iter = _cf_obj.cf_lvls[lvl - 1].c_size / cl_size;

	/* load and modify forward */
	for (i = 0; i < num_iter; i++, buf += cl_size) {
		*((uint32_t *)buf) ^= 0x1;

#if defined(__GNUC__)
		__builtin_prefetch(buf + cl_size, 1, 3);
#endif

#if defined(__INTEL_COMPILER)
		_mm_prefetch(buf + cl_size, _MM_HINT_T0);
#endif
	}

	/* and now evict backwards */
	for (i = 0, buf -= cl_size; i < num_iter; i++, buf -= cl_size) {
		cf_clflush(buf);
	}
	return 0;
}

int cf_finalize(void) {
	if (!_cf_initialized || _cf_finalized) {
		fprintf(stderr,
			"cf_init not called or cf_finalize called"
			" multiple times; abort");
		return -1;
	}

	_cf_finalized = true;
	if (_cf_obj.cf_buf) free(_cf_obj.cf_buf);

	return 0;
}

/* check L3 [or L2] for Xeon E5-2650 v4:
	 valgrind --tool=cachegrind [--LL=262144,8,64] <prog>
	 callgrind_annotate --auto=yes cachegrind.out.*
 */
/*
int main() {
	int i = 0, ret = 0, sum = 0;
	int *data=malloc(1024*sizeof(int));

	for (i = 0; i < 1024; i++)
		data[i] = i;

	if ((ret = cf_init()))
		exit(EXIT_FAILURE);

	for (i = 0; i < 1024; i++) {
		if ((ret = cf_flush(_CF_L2_)))
			exit(EXIT_FAILURE);
		sum += data[i];
	}

	if ((ret = cf_finalize()))
		exit(EXIT_FAILURE);

	printf("sum %d\n", sum);
	free(data);
	return 0;
}
*/
