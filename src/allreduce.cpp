#include <CLI/CLI.hpp>
#include <vector>

#include "benchmark_runner.hpp"
#include "papi_wrapper.hpp"
#include "util.hpp"

#define OMPI_SKIP_MPICXX
#define MPICH_SKIP_MPICXX

#include "cache_flush.h"
#include "mpi.h"

#define WARMUP_L1I \
	MPI_Allreduce(nullptr, nullptr, 0, MPI_CHAR, MPI_SUM, MPI_COMM_WORLD)

void flush_l1(EventCounter &counter, volatile char *msg_buffer,
	      const std::size_t &msg_size, const int &my_rank) {
	char *src_buffer = (char *)msg_buffer;
	char *dst_buffer = (char *)msg_buffer + msg_size / 2;
	CLEAR_L1;
	WARMUP_L1I;
	counter.start();
	MPI_Allreduce(src_buffer, dst_buffer, msg_size / 2, MPI_CHAR, MPI_SUM,
		      MPI_COMM_WORLD);
	counter.stop();
}

void flush_l2(EventCounter &counter, volatile char *msg_buffer,
	      const std::size_t &msg_size, const int &my_rank) {
	char *src_buffer = (char *)msg_buffer;
	char *dst_buffer = (char *)msg_buffer + msg_size / 2;
	CLEAR_L2;
	WARMUP_L1I;
	counter.start();
	MPI_Allreduce(src_buffer, dst_buffer, msg_size / 2, MPI_CHAR, MPI_SUM,
		      MPI_COMM_WORLD);
	counter.stop();
}

void flush_l3(EventCounter &counter, volatile char *msg_buffer,
	      const std::size_t &msg_size, const int &my_rank) {
	char *src_buffer = (char *)msg_buffer;
	char *dst_buffer = (char *)msg_buffer + msg_size / 2;
	CLEAR_L3;
	WARMUP_L1I;
	counter.start();
	MPI_Allreduce(src_buffer, dst_buffer, msg_size / 2, MPI_CHAR, MPI_SUM,
		      MPI_COMM_WORLD);
	counter.stop();
}

void flush_mpi_exclusive(EventCounter &counter, volatile char *msg_buffer,
			 const std::size_t &msg_size, const int &my_rank) {
	char *src_buffer = (char *)msg_buffer;
	char *dst_buffer = (char *)msg_buffer + msg_size / 2;

#ifdef __aarch64__
	CLEAR_L2;
#else
	CLEAR_L3;
#endif
	PRELOAD_MSG(msg_buffer, msg_size);
	WARMUP_L1I;
	counter.start();
	MPI_Allreduce(src_buffer, dst_buffer, msg_size / 2, MPI_CHAR, MPI_SUM,
		      MPI_COMM_WORLD);
	counter.stop();
}

void flush_mpi_modify(EventCounter &counter, volatile char *msg_buffer,
		      const std::size_t &msg_size, const int &my_rank) {
	char *src_buffer = (char *)msg_buffer;
	char *dst_buffer = (char *)msg_buffer + msg_size / 2;

#ifdef __aarch64__
	CLEAR_L2;
#else
	CLEAR_L3;
#endif
	PRELOAD_MODIFY_MSG(msg_buffer, msg_size);
	WARMUP_L1I;
	counter.start();
	MPI_Allreduce(src_buffer, dst_buffer, msg_size / 2, MPI_CHAR, MPI_SUM,
		      MPI_COMM_WORLD);
	counter.stop();
}

void flush_data(EventCounter &counter, volatile char *msg_buffer,
		const std::size_t &msg_size, const int &my_rank) {
	char *src_buffer = (char *)msg_buffer;
	char *dst_buffer = (char *)msg_buffer + msg_size / 2;

	char *w_buffer = new char[msg_size];
	std::fill(w_buffer, w_buffer + msg_size, 'w');

#ifdef __aarch64__
	CLEAR_L2;
#else
	CLEAR_L3;
#endif
	MPI_Allreduce(w_buffer, w_buffer + msg_size / 2, msg_size / 2, MPI_CHAR,
		      MPI_SUM, MPI_COMM_WORLD);
	WARMUP_L1I;
	counter.start();
	MPI_Allreduce(src_buffer, dst_buffer, msg_size / 2, MPI_CHAR, MPI_SUM,
		      MPI_COMM_WORLD);
	counter.stop();
	delete[] w_buffer;
}

void no_flush(EventCounter &counter, volatile char *msg_buffer,
	      const std::size_t &msg_size, const int &my_rank) {
	char *src_buffer = (char *)msg_buffer;
	char *dst_buffer = (char *)msg_buffer + msg_size / 2;
	WARMUP_L1I;
	counter.start();
	MPI_Allreduce(src_buffer, dst_buffer, msg_size / 2, MPI_CHAR, MPI_SUM,
		      MPI_COMM_WORLD);
	counter.stop();
}

std::string default_reporter(EventCounter &counter,
			     const std::size_t &iterations,
			     const std::size_t &msg_size,
			     const std::string &name, const int &my_rank) {
	std::ostringstream oss;
	double cycles = counter.get_cycles();
	double c_min = 0, c_max = 0, c_avg = 0;

	MPI_Reduce(&cycles, &c_min, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
	MPI_Reduce(&cycles, &c_max, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
	MPI_Reduce(&cycles, &c_avg, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

	int world_size;
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);

	if (0 == my_rank) {
		c_avg /= world_size;
		oss << name << "," << iterations << "," << msg_size / 2 << ","
		    << c_min << "," << c_avg << "," << c_max << ","
		    << counter.values_to_line();
	}
	counter.clear_state();
	return oss.str();
}

int main(int argc, char *argv[]) {
	int my_rank, n_ranks;

	size_t iterations = 1000;
	size_t min_exp = 0;
	size_t max_exp = 24;
	std::string papi_events_file;
	std::string only_bench;

	CLI::App app("MPI Cache Flush Benchmark");
	app.add_option("-i,--iterations", iterations,
		       "Number of timed iterations");
	app.add_option("-m,--min-exp", min_exp,
		       "Minimum exponent used to generate message sizes.");
	app.add_option("-x,--max-exp", max_exp,
		       "Maximum exponent used to generate message sizes.");
	app.add_option("-p, --papi-event-file", papi_events_file,
		       "File with PAPI event names to record");
	app.add_option(
	    "-o,--only-bench", only_bench,
	    "Execute only one benchmark defined by the given string.");
	CLI11_PARSE(app, argc, argv);

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &n_ranks);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

	EventCounter counter(iterations);

	try {
		counter.init(papi_events_file);
	} catch (const std::exception &e) {
		std::cerr << e.what() << std::endl;
		ABORT(-1);
	}

	benchmark::BenchmarkRunner runner(counter, default_reporter);
	runner.add_parameter_list({{"iterations", iterations},
				   {"min_exp", min_exp + 1},
				   {"max_exp", max_exp + 1},
				   {"my_rank", my_rank}});

	// print header
	if (0 == my_rank) {
		std::cout << "name,iterations,msg_size,min_cycles,avg_cycles,"
			     "max_cycles,"
			  << counter.names_to_line() << "\n";
	}

	if (cf_init()) {
		std::cerr << "cf_init failed" << std::endl;
		ABORT(-2);
	}

	std::vector<char> src(10);
	std::vector<char> dst(10);

	MPI_Barrier(MPI_COMM_WORLD);

	for (int i = 0; i < 100; ++i) {
		MPI_Allreduce(src.data(), dst.data(), 10, MPI_CHAR, MPI_SUM,
			      MPI_COMM_WORLD);
	}

	runner.add_benchmark_list({{"Flush_L1D", flush_l1},
				   {"Flush_L2D", flush_l2},
#ifndef __aarch64__
				   {"Flush_L3D", flush_l2},
#endif
				   {"Flush_mpi_exclusive", flush_mpi_exclusive},
				   {"Flush_mpi_modify", flush_mpi_modify},
				   {"Flush_data", flush_data},
				   {"No_flush", no_flush}});

	if (!only_bench.empty()) {
		runner.run(only_bench);
	} else {
		runner.run_all();
	}

	const auto result_lines = runner.get_result_lines();

	auto print_result_lines = [&result_lines](std::ostream &os) {
		std::copy(result_lines.begin(), result_lines.end(),
			  std::ostream_iterator<std::string>(os, "\n"));
	};

	if (0 == my_rank) print_result_lines(std::cout);

	cf_finalize();
	MPI_Finalize();
	return 0;
}
