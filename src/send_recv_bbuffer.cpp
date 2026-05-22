#include <CLI/CLI.hpp>
#include <cstring>
#include <vector>

extern "C" {
#include <dlfcn.h>
}

#include "benchmark_runner.hpp"
#include "papi_wrapper.hpp"
#include "report.hpp"
#include "util.hpp"

#define OMPI_SKIP_MPICXX
#define MPICH_SKIP_MPICXX

#include "cache_flush.h"
#include "mpi.h"

namespace {

#define WARMUP_L1I                                                             \
	{                                                                      \
		if (0 == my_rank) {                                            \
			MPI_Recv(nullptr, 0, MPI_CHAR, 1, 102, MPI_COMM_WORLD, \
				 MPI_STATUS_IGNORE);                           \
			MPI_Send(nullptr, 0, MPI_CHAR, 1, 101,                 \
				 MPI_COMM_WORLD);                              \
		} else {                                                       \
			MPI_Send(nullptr, 0, MPI_CHAR, 0, 102,                 \
				 MPI_COMM_WORLD);                              \
			MPI_Recv(nullptr, 0, MPI_CHAR, 0, 101, MPI_COMM_WORLD, \
				 MPI_STATUS_IGNORE);                           \
		}                                                              \
	}

typedef void (*prefetch_bounce_buffer_fun)(const size_t, const size_t);

size_t cache_line_size;

prefetch_bounce_buffer_fun prefetch_bounce_buffer;

#ifdef __aarch64__
#define CLEAR_CACHE CLEAR_L2;
#else
#define CLEAR_CACHE CLEAR_L3;
#endif

void bounce_buffer(EventCounter &counter, volatile char *msg_buffer,
		   const size_t &msg_size, const int &my_rank) {

	CLEAR_CACHE;

	if (0 == my_rank) {
		prefetch_modify_buf(msg_buffer, msg_size, cache_line_size);
		prefetch_bounce_buffer(cache_line_size, msg_size);
	}

	WARMUP_L1I;
	if (0 == my_rank) {
		counter.start();
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 1, 100,
			 MPI_COMM_WORLD);
		counter.stop();
	} else {
		counter.start();
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 0, 100,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		counter.stop();
	}
	BARRIER_ALL;
}

void cold_cache(EventCounter &counter, volatile char *msg_buffer,
		const size_t &msg_size, const int &my_rank) {
	CLEAR_CACHE;
	WARMUP_L1I;
	if (0 == my_rank) {
		counter.start();
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 1, 100,
			 MPI_COMM_WORLD);
		counter.stop();
	} else {
		counter.start();
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 0, 100,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		counter.stop();
	}
	BARRIER_ALL;
}

void hot_cache(EventCounter &counter, volatile char *msg_buffer,
	       const size_t &msg_size, const int &my_rank) {
	WARMUP_L1I;
	if (0 == my_rank) {
		counter.start();
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 1, 100,
			 MPI_COMM_WORLD);
		counter.stop();
	} else {
		counter.start();
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 0, 100,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		counter.stop();
	}
	BARRIER_ALL;
}
}  // namespace

int main(int argc, char *argv[]) {
	int my_rank, n_ranks;

	size_t iterations = 1000;
	size_t min_exp = 12;
	size_t max_exp = 13;
	size_t skip = 10;
	std::string papi_events_file;
	std::string only_bench;
	bool raw_data = false;
	bool median_data = false;

	reporter::Reporter reporter = reporter::min_reporter;

	CLI::App app("MPI Cache Flush Benchmark");
	app.add_option("-i,--iterations", iterations,
		       "Number of timed iterations");
	app.add_option("-e,--min-exp", min_exp,
		       "Minimum exponent used to generate message sizes.");
	app.add_option("-s,--skip", skip,
		       "skip couple iterations at the beginning");
	app.add_option("-x,--max-exp", max_exp,
		       "Maximum exponent used to generate message sizes.");
	app.add_option("-p, --papi-event-file", papi_events_file,
		       "File with PAPI event names to record");
	app.add_option(
	    "-o,--only-bench", only_bench,
	    "Execute only one benchmark defined by the given string.");
	auto *raw_data_option = app.add_flag("-r,--raw-data", raw_data,
					     "Print per iteration values.");
	auto *median_data_option =
	    app.add_flag("-m,--median", median_data, "Report median values.");
	raw_data_option->excludes(median_data_option);

	CLI11_PARSE(app, argc, argv);

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &n_ranks);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

	if (raw_data) reporter = reporter::raw_data_reporter;

	if (median_data) reporter = reporter::median_reporter;

	prefetch_bounce_buffer = (prefetch_bounce_buffer_fun)dlsym(
	    RTLD_DEFAULT, "MPOOL_TRACER_builtin_prefetch_bounce_buffer");

	if (!prefetch_bounce_buffer) {
		std::cerr << "Failed to connect to UCX_MPOOL_TRACER runtime"
			  << std::endl;
		ABORT(-1);
	}

	EventCounter counter(iterations);

	try {
		counter.init(papi_events_file);
	} catch (const std::exception &e) {
		std::cerr << e.what() << std::endl;
		ABORT(-1);
	}

	if (cf_init()) {
		std::cerr << "cf_init failed" << std::endl;
		ABORT(-2);
	}

	cache_line_size = _cf_obj.cf_lvls[0].c_line_size;

	benchmark::BenchmarkRunner runner(counter, reporter);
	runner.add_parameter_list({{"iterations", iterations},
				   {"min_exp", min_exp},
				   {"max_exp", max_exp},
				   {"skip", skip},
				   {"cache_line_size", cache_line_size},
				   {"my_rank", my_rank}});

	if (0 == my_rank) {
		std::cout << "name,iterations,msg_size,cycles,"
			  << counter.names_to_line() << ","
			  << "operation" << std::endl;
	}

	std::vector<char> buf(4096);

	for (int i = 0; i < 100; ++i) {
		if (0 == my_rank) {
			MPI_Send(buf.data(), 4096, MPI_CHAR, 1, 101,
				 MPI_COMM_WORLD);
		} else {
			MPI_Recv(buf.data(), 4096, MPI_CHAR, 0, 101,
				 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		}
	}

	BARRIER_ALL;

	runner.add_benchmark_list({{"cold_cache", cold_cache},
				   {"bounce_buffer", bounce_buffer},
				   {"hot_cache", hot_cache}});

	if (!only_bench.empty()) {
		runner.run(only_bench);
	} else {
		runner.run_all();
	}

	const auto result_lines = runner.get_result_lines();

	auto print_result_lines = [&result_lines](std::ostream &os) {
		std::copy(result_lines.begin(), result_lines.end(),
			  std::ostream_iterator<std::string>(os));
	};

	if (0 == my_rank) {
		print_result_lines(std::cout);
		std::fflush(stdout);
	}

	cf_finalize();
	MPI_Finalize();
	return 0;
}
