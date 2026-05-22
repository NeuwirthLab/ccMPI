#include <CLI/CLI.hpp>
#include <functional>
#include <iostream>
#include <vector>

#include "cache.hpp"
#include "papi_wrapper.hpp"

#define OMPI_SKIP_MPICXX
#define MPICH_SKIP_MPICXX

extern "C" {
#include "cache_flush.h"
#include "mpi.h"
}

#define ABORT(code) MPI_Abort(MPI_COMM_WORLD, (code))

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

#define CLEAR_L1 cf_flush(cache_lvls[0])
#define CLEAR_L2 cf_flush(cache_lvls[1])
#define CLEAR_L3 cf_flush(cache_lvls[2])

#define FLUSH_MSG_BUFFER(_p, __size)                                         \
	{                                                                     \
		const std::size_t c_line_size =                               \
		    static_cast<std::size_t>(_cf_obj.cf_lvls[0].c_line_size); \
		for (volatile std::size_t i = 0; i < (__size);                \
		     i += c_line_size) {                                      \
			cf_clflush((_p) + i);                                \
		}                                                             \
	}

#define LOAD_MSG_BUFFER(__p, __size)                                          \
	{                                                                     \
		volatile char p = 'c';                                        \
		const std::size_t c_line_size =                               \
		    static_cast<std::size_t>(_cf_obj.cf_lvls[0].c_line_size); \
		for (volatile std::size_t i = 0; i < (__size);                \
		     i += c_line_size) {                                      \
			p += (__p)[i];                                        \
		}                                                             \
	}

void flush_l1(EventCounter &counter, volatile char *msg_buffer,
	      const std::size_t &msg_size, const int &my_rank) {
	CLEAR_L1;
	MPI_Barrier(MPI_COMM_WORLD);
	if (0 == my_rank) {
		counter.start();
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 1, 100,
			 MPI_COMM_WORLD);
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 1, 101,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		counter.stop();
	} else {
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 0, 100,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 0, 101,
			 MPI_COMM_WORLD);
	}
}

void flush_l2(EventCounter &counter, volatile char *msg_buffer,
	      const std::size_t &msg_size, const int &my_rank) {
	CLEAR_L2;
	MPI_Barrier(MPI_COMM_WORLD);
	if (0 == my_rank) {
		counter.start();
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 1, 100,
			 MPI_COMM_WORLD);
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 1, 101,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		counter.stop();
	} else {
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 0, 100,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 0, 101,
			 MPI_COMM_WORLD);
	}
}

void flush_l3(EventCounter &counter, volatile char *msg_buffer,
	      const std::size_t &msg_size, const int &my_rank) {
	CLEAR_L3;
	MPI_Barrier(MPI_COMM_WORLD);
	if (0 == my_rank) {
		counter.start();
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 1, 100,
			 MPI_COMM_WORLD);
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 1, 101,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		counter.stop();
	} else {
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 0, 100,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 0, 101,
			 MPI_COMM_WORLD);
	}
}

void flush_l1_warmup(EventCounter &counter, volatile char *msg_buffer,
		     const std::size_t &msg_size, const int &my_rank) {
	CLEAR_L1;
	WARMUP_L1I;
	MPI_Barrier(MPI_COMM_WORLD);
	if (0 == my_rank) {
		counter.start();
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 1, 100,
			 MPI_COMM_WORLD);
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 1, 101,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		counter.stop();
	} else {
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 0, 100,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 0, 101,
			 MPI_COMM_WORLD);
	}
}

void flush_l2_warmup(EventCounter &counter, volatile char *msg_buffer,
		     const std::size_t &msg_size, const int &my_rank) {
	CLEAR_L2;
	WARMUP_L1I;
	MPI_Barrier(MPI_COMM_WORLD);
	if (0 == my_rank) {
		counter.start();
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 1, 100,
			 MPI_COMM_WORLD);
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 1, 101,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		counter.stop();
	} else {
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 0, 100,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 0, 101,
			 MPI_COMM_WORLD);
	}
}

void flush_l3_warmup(EventCounter &counter, volatile char *msg_buffer,
		     const std::size_t &msg_size, const int &my_rank) {
	CLEAR_L3;
	WARMUP_L1I;
	MPI_Barrier(MPI_COMM_WORLD);
	if (0 == my_rank) {
		counter.start();
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 1, 100,
			 MPI_COMM_WORLD);
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 1, 101,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		counter.stop();
	} else {
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 0, 100,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 0, 101,
			 MPI_COMM_WORLD);
	}
}

void flush_mpi(EventCounter &counter, volatile char *msg_buffer,
	       const std::size_t &msg_size, const int &my_rank) {
#ifndef __aarch64__
	CLEAR_L3;
#else
	CLEAR_L2;
#endif
	LOAD_MSG_BUFFER(msg_buffer, msg_size);
	MPI_Barrier(MPI_COMM_WORLD);
	if (0 == my_rank) {
		counter.start();
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 1, 100,
			 MPI_COMM_WORLD);
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 1, 101,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		counter.stop();
	} else {
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 0, 100,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 0, 101,
			 MPI_COMM_WORLD);
	}
}

void flush_data(EventCounter &counter, volatile char *msg_buffer,
		const std::size_t &msg_size, const int &my_rank) {
#ifdef __aarch64__
	volatile char *w_buffer = new char[msg_size];
	std::fill(w_buffer, w_buffer + msg_size, 'w');
	CLEAR_L2;
	MPI_Barrier(MPI_COMM_WORLD);
	if (0 == my_rank) {
		MPI_Send((char *)w_buffer, msg_size, MPI_CHAR, 1, 100,
			 MPI_COMM_WORLD);
		MPI_Recv((char *)w_buffer, msg_size, MPI_CHAR, 1, 101,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	} else {
		MPI_Recv((char *)w_buffer, msg_size, MPI_CHAR, 0, 100,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Send((char *)w_buffer, msg_size, MPI_CHAR, 0, 101,
			 MPI_COMM_WORLD);
	}
#else
	CLEAR_L3;
	MPI_Barrier(MPI_COMM_WORLD);
	if (0 == my_rank) {
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 1, 100,
			 MPI_COMM_WORLD);
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 1, 101,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	} else {
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 0, 100,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 0, 101,
			 MPI_COMM_WORLD);
	}
	FLUSH_MSG_BUFFER(msg_buffer, msg_size);
#endif
	MPI_Barrier(MPI_COMM_WORLD);
	if (0 == my_rank) {
		counter.start();
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 1, 100,
			 MPI_COMM_WORLD);
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 1, 101,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		counter.stop();
	} else {
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 0, 100,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 0, 101,
			 MPI_COMM_WORLD);
	}
#ifdef __aarch64__
	delete[] w_buffer;
#endif
}

void no_flush(EventCounter &counter, volatile char *msg_buffer,
	      const std::size_t &msg_size, const int &my_rank) {
	WARMUP_L1I;
	MPI_Barrier(MPI_COMM_WORLD);
	if (0 == my_rank) {
		counter.start();
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 1, 100,
			 MPI_COMM_WORLD);
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 1, 101,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		counter.stop();
	} else {
		MPI_Recv((char *)msg_buffer, msg_size, MPI_CHAR, 0, 100,
			 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		MPI_Send((char *)msg_buffer, msg_size, MPI_CHAR, 0, 101,
			 MPI_COMM_WORLD);
	}
}

std::string default_reporter(EventCounter &counter,
			     const std::size_t &iterations,
			     const std::size_t &msg_size,
			     const std::string &name, const int &my_rank) {
	std::ostringstream oss;
	if (my_rank != 0) return {};
	oss << name << "," << iterations << "," << msg_size << ","
	    << counter.get_cycles() << "," << counter.values_to_line();
	counter.clear_state();
	return oss.str();
}

class BenchmarkRunner {
	using BenchFunc = std::function<void(EventCounter &, volatile char *,
					     const std::size_t &, const int &)>;
	using Reporter = std::function<std::string(
	    EventCounter &, const std::size_t &, const std::size_t &,
	    const std::string &, const int &)>;
	struct Benchmark {
		std::string name;
		BenchFunc measure;
		Reporter reporter;
	};

	std::vector<Benchmark> benchmarks;
	std::vector<std::string> result_lines;
	std::size_t iterations;
	std::size_t min_exp;
	std::size_t max_exp;
	int my_rank;
	EventCounter counter;

	void print_result_lines(std::ostream &os) {
		for (const std::string &l : result_lines) {
			os << l << std::endl;
		}
	}

       public:
	explicit BenchmarkRunner(const int &my_rank,
				 const std::size_t &iterations,
				 const std::size_t &min_exp,
				 const std::size_t &max_exp,
				 EventCounter &counter)
	    : my_rank{my_rank},
	      iterations{iterations},
	      min_exp{min_exp},
	      max_exp{max_exp},
	      counter{counter} {};

	void add(const std::string &name, BenchFunc func, Reporter reporter) {
		benchmarks.push_back({name, func, reporter});
	}

	void run_all() {
		std::size_t dist = max_exp - min_exp;
		std::vector<std::size_t> msg_sizes(dist);

		result_lines.reserve(dist * benchmarks.size());

		std::generate(msg_sizes.begin(), msg_sizes.end(),
			      [&]() -> std::size_t {
				      static std::size_t s = min_exp;
				      if (0 == s) {
					      return ++s;
				      }
				      return (s *= 2);
			      });

		for (const auto &b : benchmarks) {
			if (0 == my_rank) {
				std::cerr << "Now running benchmark " << b.name
					  << std::endl;
			}
			for (const auto &m : msg_sizes) {
				volatile char *msg = new char[m];
				for (std::size_t i = 0; i < m; ++i)
					msg[i] = 'c';
				for (std::size_t i = 0; i < iterations; ++i) {
					b.measure(counter, msg, m, my_rank);
				}
				result_lines.push_back(b.reporter(
				    counter, iterations, m, b.name, my_rank));
				delete[] msg;
			}
		}
		if (0 == my_rank) {
			print_result_lines(std::cout);
		}
	}
};

int main(int argc, char *argv[]) {
	int my_rank, n_ranks;

	std::size_t iterations = 1000;
	std::size_t min_exp = 0;
	std::size_t max_exp = 24;
	std::string papi_events_file;

	CLI::App app("MPI Cache Flush Benchmark");
	app.add_option("-i,--iterations", iterations,
		       "Number of timed iterations");
	app.add_option("-m,--min-exp", min_exp,
		       "Minimum exponent used to generate message sizes.");
	app.add_option("-x,--max-exp", max_exp,
		       "Maximum exponent used to generate message sizes.");
	app.add_option("-p, --papi-event-file", papi_events_file,
		       "File with PAPI event names to record");

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

	BenchmarkRunner runner(my_rank, iterations, min_exp, max_exp, counter);

	// print header
	if (0 == my_rank) {
		std::cout << "name,iterations,msg_size,cycles,"
			  << counter.names_to_line() << std::endl;
	}

	if (cf_init()) {
		std::cerr << "cf_init failed" << std::endl;
		ABORT(-2);
	}

	std::vector<char> buf(10);

	MPI_Barrier(MPI_COMM_WORLD);

	for (int i = 0; i < 100; ++i) {
		if (0 == my_rank) {
			MPI_Send(buf.data(), 10, MPI_CHAR, 1, 101,
				 MPI_COMM_WORLD);
			MPI_Recv(buf.data(), 10, MPI_CHAR, 1, 102,
				 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
		} else {
			MPI_Recv(buf.data(), 10, MPI_CHAR, 0, 101,
				 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			MPI_Send(buf.data(), 10, MPI_CHAR, 0, 102,
				 MPI_COMM_WORLD);
		}
	}

	runner.add("Flush_L1D_cold_L1I", flush_l1, default_reporter);
	runner.add("Flush_L2D_cold_L1I", flush_l2, default_reporter);
	runner.add("Flush_L1D_hot_L1I", flush_l1_warmup, default_reporter);
	runner.add("Flush_L2D_hot_L1I", flush_l2_warmup, default_reporter);
	runner.add("Flush_mpi_cold_L1I", flush_mpi, default_reporter);
	runner.add("No_flush_hot_L1I", no_flush, default_reporter);
	runner.add("Flush_data_cold_L1I", flush_data, default_reporter);

#ifndef __aarch64__
	runner.add("Flush_L3D_hot_L1I", flush_l3_warmup, default_reporter);
	runner.add("Flush_L3D_cold_L1I", flush_l3, default_reporter);
#endif

	MPI_Barrier(MPI_COMM_WORLD);

	runner.run_all();

	MPI_Barrier(MPI_COMM_WORLD);
	cf_finalize();
	MPI_Finalize();
	return 0;
}
