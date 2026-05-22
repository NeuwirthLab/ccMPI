#include <CLI/CLI.hpp>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
#include <dlfcn.h>
}

#include "cache_flush.h"
#include "papi_wrapper.hpp"
#include "report.hpp"
#include "util.hpp"

namespace {
size_t cache_line_size;

#ifdef __ARM_FEATURE_SVE
bool has_sve = true;
#else
bool has_sve = false;
#endif

void print_cache_info() {
	int num_lvl = _cf_obj.num_lvl;
	std::cout << "Has SVE support: " << has_sve << std::endl;
	std::cout << "Cache Levels: " << num_lvl << std::endl;
	for (int i = 0; i < num_lvl; ++i) {
		std::cout << "L" << i + 1 << ": \n";
		std::cout << "\tSize: " << _cf_obj.cf_lvls[i].c_size
			  << std::endl;
		std::cout << "\tCL_size: " << _cf_obj.cf_lvls[i].c_line_size
			  << std::endl;
	}
}

#define MEMCPY(dst, src, size) memcpy((dst), (src), (size))
//#define MEMCPY(dst, src, len)                          \
//	asm volatile("rep movsb"                       \
//		     : "=D"(dst), "=S"(src), "=c"(len) \
//		     : "0"(dst), "1"(src), "2"(len)    \
//		     : "memory")

// #endif

#define COMPILER_BARRIER() asm volatile("" ::: "memory")

#ifdef __aarch64__
#define CLEAR_CACHE CLEAR_L2;
#else
#define CLEAR_CACHE CLEAR_L3;
#endif

void memcopy_cold(EventCounter &counter, char *src, char *dst, size_t size) {
	CLEAR_CACHE;
	counter.start();
	MEMCPY(dst, src, size);
	COMPILER_BARRIER();
	counter.stop();
}

void memcopy_hot_dst(EventCounter &counter, char *src, char *dst, size_t size) {
	CLEAR_CACHE;
	prefetch_modify_buf(dst, size, cache_line_size);

	counter.start();
	MEMCPY(dst, src, size);
	COMPILER_BARRIER();
	counter.stop();
}

void memcopy_hot_src(EventCounter &counter, char *src, char *dst, size_t size) {
	CLEAR_CACHE;
	prefetch_modify_buf(src, size, cache_line_size);

	counter.start();
	MEMCPY(dst, src, size);
	COMPILER_BARRIER();
	counter.stop();
}

void memcopy_both_hot(EventCounter &counter, char *src, char *dst,
		      size_t size) {
	CLEAR_CACHE;
	prefetch_modify_buf(src, size, cache_line_size);
	prefetch_modify_buf(dst, size, cache_line_size);

	counter.start();
	MEMCPY(dst, src, size);
	COMPILER_BARRIER();
	counter.stop();
}

void memcopy_hot(EventCounter &counter, char *src, char *dst, size_t size) {
	counter.start();
	MEMCPY(dst, src, size);
	COMPILER_BARRIER();
	counter.stop();
}

void *alloc_cacheline(std::size_t size) {
	void *ptr = nullptr;
	if (posix_memalign(&ptr, cache_line_size, size) != 0)
		throw std::bad_alloc();
	return ptr;
}

}  // namespace

int main(int argc, char *argv[]) {
	int my_rank, n_ranks;

	size_t iterations = 1000;
	size_t size = 4096;
	size_t skip = 10;
	std::string papi_events_file;
	std::string only_bench;
	bool show_config = false;
	bool raw_data = false;
	bool median_data = false;
	reporter::Reporter reporter = reporter::min_reporter;

	CLI::App app("memcpy Benchmark");
	app.add_option("-i,--iterations", iterations,
		       "Number of timed iterations");
	app.add_option("-s,--size", size, "buffer size");
	app.add_option("-p, --papi-event-file", papi_events_file,
		       "File with PAPI event names to record");
	app.add_flag("-c,--cache-config", show_config,
		     "Print cache configuration");
	auto *raw_data_option = app.add_flag("-r,--raw-data", raw_data,
					     "Print per iteration results.");
	auto *median_data_option =
	    app.add_flag("-m,--median", median_data, "Report median values.");
	raw_data_option->excludes(median_data_option);

	CLI11_PARSE(app, argc, argv);

	if (show_config) {
		print_cache_info();
		return 0;
	}

	if (raw_data) {
		reporter = reporter::raw_data_reporter;
	}

	if (median_data) {
		reporter = reporter::median_reporter;
	}

	std::vector<std::pair<
	    std::string,
	    std::function<void(EventCounter &, char *, char *, size_t)>>>
	    benchmarks = {{"memcopy_cold", memcopy_cold},
			  {"memcopy_hot_dst", memcopy_hot_dst},
			  {"memcopy_hot_src", memcopy_hot_src},
			  {"memcopy_both_hot", memcopy_both_hot},
			  {"memcopy_hot", memcopy_hot}};

	EventCounter counter(iterations);

	try {
		counter.init(papi_events_file);
	} catch (const std::exception &e) {
		std::cerr << e.what() << std::endl;
		std::terminate();
	}

	if (cf_init()) {
		std::cerr << "cf_init failed" << std::endl;
		std::terminate();
	}

	std::cout << "name,iterations,size,cycles," << counter.names_to_line()
		  << std::endl;

	cache_line_size = _cf_obj.cf_lvls[0].c_line_size;
	std::vector<std::string> result_lines;
	result_lines.reserve(benchmarks.size());

	for (const auto &benchmark : benchmarks) {
		volatile char *dst = (volatile char *)alloc_cacheline(size);
		volatile char *src = (volatile char *)alloc_cacheline(size);

		std::fill(src, src + size, 'a');
		std::fill(dst, dst + size, 'b');

		for (size_t i = 0; i < iterations + skip; ++i) {
			benchmark.second(counter, (char *)src, (char *)dst,
					 size);
			if(i < skip)
				counter.clear_state();
		}
		
		result_lines.push_back(
		    reporter(counter, iterations, size, benchmark.first));
		free((void *)dst);
		free((void *)src);
	}

	std::copy(result_lines.begin(), result_lines.end(),
		  std::ostream_iterator<std::string>(std::cout));

	cf_finalize();
	return 0;
}
