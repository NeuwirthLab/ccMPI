#include <CLI/CLI.hpp>
#include <vector>

#include "benchmark_runner.hpp"
#include "papi_wrapper.hpp"
#include "report.hpp"
#include "util.hpp"

#define OMPI_SKIP_MPICXX
#define MPICH_SKIP_MPICXX

#include "cache_flush.h"
#include "mpi.h"

namespace {

#ifdef USE_BLOCKING_PT2PT
#define SEND \
	MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 1, 100, MPI_COMM_WORLD)
#define RECV                                                     \
	MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 0, 100, \
		 MPI_COMM_WORLD, MPI_STATUS_IGNORE)
#else
#define SEND                                                              \
	{                                                                 \
		MPI_Request req;                                          \
		MPI_Isend((char *)msg_buffer, msg_size, MPI_CHAR, 1, 100, \
			  MPI_COMM_WORLD, &req);                          \
		MPI_Wait(&req, MPI_STATUS_IGNORE);                        \
	}
#define RECV                                                              \
	{                                                                 \
		MPI_Request req;                                          \
		MPI_Irecv((char *)msg_buffer, msg_size, MPI_CHAR, 0, 100, \
			  MPI_COMM_WORLD, &req);                          \
		MPI_Wait(&req, MPI_STATUS_IGNORE);                        \
	}
#endif

#define EXPECTED_MSG_PATTERN                                                 \
	{                                                                    \
		MPI_Request req;                                             \
		if (0 == my_rank) {                                          \
			MPI_Barrier(MPI_COMM_WORLD);                         \
			MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 1,  \
				 100, MPI_COMM_WORLD);                       \
		} else {                                                     \
			MPI_Irecv((char *)msg_buffer, msg_size, MPI_CHAR, 0, \
				  100, MPI_COMM_WORLD, &req);                \
			MPI_Barrier(MPI_COMM_WORLD);                         \
			counter.start();                                     \
			MPI_Wait(&req, MPI_STATUS_IGNORE);                   \
			counter.stop();                                      \
		}                                                            \
	}

#define UNEXPECTED_MSG_PATTERN                                               \
	{                                                                    \
		MPI_Request req;                                             \
		if (0 == my_rank) {                                          \
			MPI_Isend((char *)msg_buffer, msg_size, MPI_CHAR, 1, \
				  100, MPI_COMM_WORLD, &req);                \
			MPI_Barrier(MPI_COMM_WORLD);                         \
			MPI_Wait(&req, MPI_STATUS_IGNORE);                   \
		} else {                                                     \
			MPI_Barrier(MPI_COMM_WORLD);                         \
			counter.start();                                     \
			MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 0,  \
				 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);    \
			counter.stop();                                      \
		}                                                            \
	}

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

#ifdef __aarch64__
#define CLEAR_CACHE CLEAR_L2;
#else
#define CLEAR_CACHE CLEAR_L3;
#endif

size_t cache_line_size;

void flush_l1(EventCounter &counter, volatile char *msg_buffer,
	      const size_t &msg_size, const int &my_rank) {
	CLEAR_L1;
	WARMUP_L1I;
	if (0 == my_rank) {
		counter.start();
		SEND;
		counter.stop();
	} else {
		RECV;
	}
	BARRIER_ALL;
}

void flush_l2(EventCounter &counter, volatile char *msg_buffer,
	      const size_t &msg_size, const int &my_rank) {
	CLEAR_L2;
	WARMUP_L1I;
	if (0 == my_rank) {
		counter.start();
		SEND;
		counter.stop();
	} else {
		RECV;
	}
	BARRIER_ALL;
}

void flush_l3(EventCounter &counter, volatile char *msg_buffer,
	      const size_t &msg_size, const int &my_rank) {
	CLEAR_L3;
	WARMUP_L1I;
	if (0 == my_rank) {
		counter.start();
		SEND;
		counter.stop();
	} else {
		RECV;
	}
	BARRIER_ALL;
}

void flush_l1_expected(EventCounter &counter, volatile char *msg_buffer,
		       const size_t &msg_size, const int &my_rank) {
	CLEAR_L1;
	WARMUP_L1I;
	EXPECTED_MSG_PATTERN;
	BARRIER_ALL;
}

void flush_l2_expected(EventCounter &counter, volatile char *msg_buffer,
		       const size_t &msg_size, const int &my_rank) {
	CLEAR_L2;
	WARMUP_L1I;
	EXPECTED_MSG_PATTERN;
	BARRIER_ALL;
}

void flush_l3_expected(EventCounter &counter, volatile char *msg_buffer,
		       const size_t &msg_size, const int &my_rank) {
	CLEAR_L3;
	WARMUP_L1I;
	EXPECTED_MSG_PATTERN;
	BARRIER_ALL;
}

void flush_l1_unexpected(EventCounter &counter, volatile char *msg_buffer,
			 const size_t &msg_size, const int &my_rank) {
	CLEAR_L1;
	WARMUP_L1I;
	UNEXPECTED_MSG_PATTERN;
	BARRIER_ALL;
}

void flush_l2_unexpected(EventCounter &counter, volatile char *msg_buffer,
			 const size_t &msg_size, const int &my_rank) {
	CLEAR_L2;
	WARMUP_L1I;
	UNEXPECTED_MSG_PATTERN;
	BARRIER_ALL;
}

void flush_l3_unexpected(EventCounter &counter, volatile char *msg_buffer,
			 const size_t &msg_size, const int &my_rank) {
	CLEAR_L3;
	WARMUP_L1I;
	UNEXPECTED_MSG_PATTERN;
	BARRIER_ALL;
}

void flush_mpi_exclusive(EventCounter &counter, volatile char *msg_buffer,
			 const size_t &msg_size, const int &my_rank) {
	CLEAR_CACHE;
	prefetch_read_buf(msg_buffer, msg_size, cache_line_size);
	WARMUP_L1I;
	if (0 == my_rank) {
		counter.start();
		SEND;
		counter.stop();
	} else {
		RECV;
	}
	BARRIER_ALL;
}
void flush_mpi_exclusive_expected(EventCounter &counter,
				  volatile char *msg_buffer,
				  const size_t &msg_size, const int &my_rank) {
	CLEAR_CACHE;
	prefetch_read_buf(msg_buffer, msg_size, cache_line_size);
	WARMUP_L1I;
	EXPECTED_MSG_PATTERN;
	BARRIER_ALL;
}

void flush_mpi_exclusive_unexpected(EventCounter &counter,
				    volatile char *msg_buffer,
				    const size_t &msg_size,
				    const int &my_rank) {
	CLEAR_CACHE;
	prefetch_read_buf(msg_buffer, msg_size, cache_line_size);
	WARMUP_L1I;
	UNEXPECTED_MSG_PATTERN;
	BARRIER_ALL;
}

void flush_mpi_modify(EventCounter &counter, volatile char *msg_buffer,
		      const size_t &msg_size, const int &my_rank) {
	CLEAR_CACHE;
	prefetch_modify_buf(msg_buffer, msg_size, cache_line_size);
	WARMUP_L1I;
	if (0 == my_rank) {
		counter.start();
		SEND;
		counter.stop();
	} else {
		counter.start();
		RECV;
		counter.stop();
	}
	BARRIER_ALL;
}

void flush_mpi_modify_expected(EventCounter &counter, volatile char *msg_buffer,
			       const size_t &msg_size, const int &my_rank) {
	CLEAR_CACHE;
	prefetch_modify_buf(msg_buffer, msg_size, cache_line_size);
	WARMUP_L1I;
	EXPECTED_MSG_PATTERN;
	BARRIER_ALL;
}

void flush_mpi_modify_unexpected(EventCounter &counter,
				 volatile char *msg_buffer,
				 const size_t &msg_size, const int &my_rank) {
	CLEAR_CACHE;
	prefetch_modify_buf(msg_buffer, msg_size, cache_line_size);
	WARMUP_L1I;
	UNEXPECTED_MSG_PATTERN;
	BARRIER_ALL;
}

void flush_data(EventCounter &counter, volatile char *msg_buffer,
		const size_t &msg_size, const int &my_rank) {
	volatile char *w_buffer = new char[msg_size];
	std::fill(w_buffer, w_buffer + msg_size, 'w');

	CLEAR_CACHE;

	if (0 == my_rank) {
		MPI_Send((char *)w_buffer, msg_size, MPI_CHAR, 1, 100,
			 MPI_COMM_WORLD);
	} else {
		MPI_Recv((char *)w_buffer, msg_size, MPI_CHAR, 0, 100,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}

	WARMUP_L1I;

	if (0 == my_rank) {
		counter.start();
		SEND;
		counter.stop();
	} else {
		counter.start();
		RECV;
		counter.stop();
	}
	BARRIER_ALL;
	delete[] w_buffer;
}

void flush_data_expected(EventCounter &counter, volatile char *msg_buffer,
			 const size_t &msg_size, const int &my_rank) {
	volatile char *w_buffer = new char[msg_size];
	std::fill(w_buffer, w_buffer + msg_size, 'w');

	CLEAR_CACHE;
	if (0 == my_rank) {
		MPI_Send((char *)w_buffer, msg_size, MPI_CHAR, 1, 100,
			 MPI_COMM_WORLD);
	} else {
		MPI_Recv((char *)w_buffer, msg_size, MPI_CHAR, 0, 100,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}
	WARMUP_L1I;
	EXPECTED_MSG_PATTERN;
	BARRIER_ALL;
	delete[] w_buffer;
}

void flush_data_unexpected(EventCounter &counter, volatile char *msg_buffer,
			   const size_t &msg_size, const int &my_rank) {
	volatile char *w_buffer = new char[msg_size];
	std::fill(w_buffer, w_buffer + msg_size, 'w');

	CLEAR_CACHE;

	if (0 == my_rank) {
		MPI_Send((char *)w_buffer, msg_size, MPI_CHAR, 1, 100,
			 MPI_COMM_WORLD);
	} else {
		MPI_Recv((char *)w_buffer, msg_size, MPI_CHAR, 0, 100,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}
	WARMUP_L1I;
	UNEXPECTED_MSG_PATTERN;
	BARRIER_ALL;
	delete[] w_buffer;
}

void no_flush(EventCounter &counter, volatile char *msg_buffer,
	      const size_t &msg_size, const int &my_rank) {
	if (0 == my_rank) {
		counter.start();
		SEND;
		counter.stop();
	} else {
		RECV;
	}
	BARRIER_ALL;
}

void no_flush_expected(EventCounter &counter, volatile char *msg_buffer,
		       const size_t &msg_size, const int &my_rank) {
	EXPECTED_MSG_PATTERN;
	BARRIER_ALL;
}

void no_flush_unexpected(EventCounter &counter, volatile char *msg_buffer,
			 const size_t &msg_size, const int &my_rank) {
	UNEXPECTED_MSG_PATTERN;
	BARRIER_ALL;
}
}  // namespace

int main(int argc, char *argv[]) {
	int my_rank, n_ranks;

	size_t iterations = 1000;
	size_t min_exp = 0;
	size_t max_exp = 17;
	std::string papi_events_file;
	std::string only_bench;
	std::string send_result_file{};
	std::string recv_result_file{};
	bool raw_data = false;
	bool median_data = false;

	reporter::Reporter reporter = reporter::min_reporter;

	CLI::App app("MPI Cache Flush Benchmark");
	app.add_option("-i,--iterations", iterations,
		       "Number of timed iterations");
	app.add_option("-e,--min-exp", min_exp,
		       "Minimum exponent used to generate message sizes.");
	app.add_option("-x,--max-exp", max_exp,
		       "Maximum exponent used to generate message sizes.");
	app.add_option("-p, --papi-event-file", papi_events_file,
		       "File with PAPI event names to record");
	app.add_option("-s,--send-result-file", send_result_file,
		       "File in which send results will appear");
	app.add_option("-a,--recv-result-file", recv_result_file,
		       "File in which recv results will appear");
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
				   {"cache_line_size", cache_line_size},
				   {"my_rank", my_rank}});

	// if (0 == my_rank) {
	//	std::cout << "name,iterations,msg_size,cycles,"
	//		  << counter.names_to_line() << ","
	//		  << "operation" << std::endl;
	// }

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

	runner.add_benchmark_list(
	    {{"Flush_L1D", flush_l1},
	     {"Flush_L2D", flush_l2},
#ifndef __aarch64__
	     {"Flush_L3D", flush_l3},
#endif
	     {"Flush_L1D_expected", flush_l1_expected},
	     {"Flush_L2D_expected", flush_l2_expected},
#ifndef __aarch64__
	     {"Flush_L3D_expected", flush_l3_expected},
#endif
	     {"Flush_L1D_unexpected", flush_l1_unexpected},
	     {"Flush_L2D_unexpected", flush_l2_unexpected},
#ifndef __aarch64__
	     {"Flush_L3D_unexpected", flush_l3_unexpected},
#endif

	     {"Flush_mpi_exclusive", flush_mpi_exclusive},
	     {"Flush_mpi_exclusive_expected", flush_mpi_exclusive_expected},
	     {"Flush_mpi_exclusive_unexpected", flush_mpi_exclusive_unexpected},
	     {"Flush_mpi_modify", flush_mpi_modify},
	     {"Flush_mpi_modify_expected", flush_mpi_modify_expected},
	     {"Flush_mpi_modify_unexpected", flush_mpi_modify_unexpected},
	     {"Flush_data", flush_data},
	     {"Flush_data_expected", flush_data_expected},
	     {"Flush_data_unexpected", flush_data_unexpected},
	     {"No_flush_expected", no_flush_expected},
	     {"No_flush_unexpected", no_flush_unexpected},
	     {"No_flush", no_flush}});

	BARRIER_ALL;

	if (!only_bench.empty()) {
		runner.run(only_bench);
	} else {
		runner.run_all();
	}

	const auto result_lines = runner.get_result_lines();

	auto print_result_lines = [&result_lines,
				   &counter](const std::string &name) -> void {
		if (result_lines.empty()) return;

		std::ofstream ofs(name, std::ios::out | std::ios::app);
		if (!ofs.is_open()) MPI_Abort(MPI_COMM_WORLD, -11);
		ofs << "name,iterations,msg_size,cycles,"
		    << counter.names_to_line() << ","
		    << "operation" << std::endl;

		std::copy(result_lines.begin(), result_lines.end(),
			  std::ostream_iterator<std::string>(ofs));
		ofs.close();
	};

	if (0 == my_rank) {
#ifdef USE_BLOCKING_PT2PT
		if (send_result_file.empty())
			send_result_file = "blocking_send.csv";
#else
		if (send_result_file.empty())
			send_result_file = "nonblocking_send.csv";

#endif
		print_result_lines(send_result_file);
	} else {
#ifdef USE_BLOCKING_PT2PT
		if (recv_result_file.empty())
			recv_result_file = "blocking_recv.csv";

#else
		if (recv_result_file.empty())
			recv_result_file = "nonblocking_recv.csv";

#endif
		print_result_lines(recv_result_file);
	}

	cf_finalize();
	MPI_Finalize();
	return 0;
}
