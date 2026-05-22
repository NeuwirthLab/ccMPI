#pragma once
#include <cmath>
#include <cstdlib>
#include <functional>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>
#include <random>
#include <algorithm>
#include <fstream>

#include "papi_wrapper.hpp"
#include "mpi.h"

namespace benchmark {

class BenchmarkRunner {
	using Reporter = std::function<std::string(
	    EventCounter &, const std::size_t &, const std::size_t &,
	    const std::string &)>;
	using BenchFunc = std::function<void(EventCounter &, volatile char *,
					     const std::size_t &, const int &)>;
	using BenchmarkList =
	    std::initializer_list<std::pair<std::string, BenchFunc>>;
	using ParameterList =
	    std::initializer_list<std::pair<std::string, size_t>>;
	using BenchmarkTable = std::map<std::string, BenchFunc>;
	using ParameterTable = std::map<std::string, size_t>;

	BenchmarkTable benchmark_table;
	ParameterTable parameter_table;
	std::vector<std::string> result_lines;
	EventCounter counter;
	Reporter reporter;

	unsigned get_random_seed(){
		std::ifstream urandom("/dev/urandom", std::ios::in | std::ios::binary);

		if(!urandom){
			std::cerr << "Failed to open urandom" << std::endl;
			MPI_Abort(MPI_COMM_WORLD, -77);
		}

		unsigned seed;

		urandom.read(reinterpret_cast<char*>(&seed),sizeof(unsigned));
		urandom.close();
		return seed;
	}

	inline void *alloc_cacheline(size_t size, size_t cache_line_size) {
		void *ptr = nullptr;
		if (posix_memalign(&ptr, cache_line_size, size) != 0)
			throw std::bad_alloc();

		return ptr;
	}

	std::vector<size_t> generate_msg_sizes() {
		size_t max_exp = parameter_table["max_exp"];
		size_t min_exp = parameter_table["min_exp"];

		size_t dist;

		if (min_exp >= max_exp) {
			dist = 1;
			min_exp = max_exp;
		} else {
			dist = max_exp - min_exp;
		}

		std::vector<size_t> msg_sizes(dist);

		for (auto &m : msg_sizes) {
			m = std::pow(2, min_exp++);
		}

		return msg_sizes;
	}

	std::string insert_op_string(const std::string &s, const int &my_rank) {
		if(s.empty()){
			return {};
		}

		std::string cpy = s;
		std::string op = my_rank == 0 ? ",send" : ",recv";

		for (size_t pos = 0; pos < cpy.size(); ++pos) {
			if (cpy[pos] == '\n') {
				cpy.insert(pos, op);
				pos += op.size();
			}
		}

		return cpy;
	}

	void run(const std::vector<size_t> &msg_sizes, const std::string &name,
		 BenchFunc fun) {
		int my_rank = static_cast<int>(parameter_table["my_rank"]);
		size_t iterations = parameter_table["iterations"];
		size_t cache_line_size = parameter_table["cache_line_size"];
		size_t skip = parameter_table["skip"];

		if (0 == my_rank) {
			std::cerr << "Now running benchmark " << name
				  << std::endl;
		}
		for (const auto &m : msg_sizes) {
			volatile char *msg = (volatile char *)alloc_cacheline(
			    m, cache_line_size);
			for (std::size_t i = 0; i < m; ++i) msg[i] = 'c';
			for (std::size_t i = 0; i < iterations + skip; ++i) {
				fun(counter, msg, m, my_rank);
				if (i < skip ) {
					counter.clear_state();
				}
			}
			result_lines.push_back(insert_op_string(
			    reporter(counter, iterations, m, name), my_rank));
			free((void *)msg);
		}
	}

       public:
	explicit BenchmarkRunner(EventCounter &counter, Reporter reporter)
	    : counter{counter}, reporter{reporter} {};

	void add_benchmark_list(BenchmarkList benchmark_list) {
		for (const auto &i : benchmark_list) {
			benchmark_table[i.first] = i.second;
		}
	}

	void add_benchmark(const std::pair<std::string, BenchFunc> benchmark) {
		benchmark_table[benchmark.first] = benchmark.second;
	}

	void add_parameter_list(ParameterList parameter_list) {
		for (const auto &i : parameter_list) {
			parameter_table[i.first] = i.second;
		}
	}

	void add_parameter(std::pair<std::string, size_t> parameter) {
		parameter_table[parameter.first] = parameter.second;
	}

	std::vector<std::string> get_result_lines() const {
		return result_lines;
	}

	void run_all() {
		int my_rank = static_cast<int>(parameter_table["my_rank"]);
		const auto msg_sizes = generate_msg_sizes();

		std::vector<std::string> keys;
		keys.reserve(benchmark_table.size());

		for(const auto& [k,v] : benchmark_table)
			keys.push_back(k);
		
		unsigned seed = get_random_seed();

		if(my_rank == 0)
			MPI_Send(&seed,1,MPI_UNSIGNED,1,99,MPI_COMM_WORLD);
		else 
			MPI_Recv(&seed,1,MPI_UNSIGNED,0,99,MPI_COMM_WORLD,MPI_STATUS_IGNORE);

		std::mt19937 rng(seed);
		std::shuffle(keys.begin(),keys.end(),rng);
		size_t iterations = parameter_table["iterations"];
		//estimate worst case memory requirements
		result_lines.reserve(benchmark_table.size() * msg_sizes.size() * iterations);

		//for (const auto &benchmark : benchmark_table) {
		for(const auto &key : keys){
			const auto name = key;//benchmark.first;
			const auto fun = benchmark_table[key]; //benchmark.second;

			run(msg_sizes, name, fun);
		}
	}

	void run(const std::string &name) {
		auto search = benchmark_table.find(name);
		if (search == benchmark_table.end()) {
			std::cerr << name
				  << " not among the registered benchmarks"
				  << std::endl;
			return;
		}

		int my_rank = static_cast<int>(parameter_table["my_rank"]);
		const auto msg_sizes = generate_msg_sizes();

		run(msg_sizes, name, benchmark_table[name]);
	}
};

}  // namespace benchmark
