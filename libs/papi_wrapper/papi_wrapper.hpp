#ifndef __PAPI_WRAPPER_HPP__
#define __PAPI_WRAPPER_HPP__
#include <algorithm>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

extern "C" {
#include <papi.h>
}

class EventCounter {
	using CounterType = long long;
	std::vector<std::vector<CounterType>> values;
	std::vector<CounterType> tmp_values;
	std::vector<CounterType> cycles;
	std::vector<int> event_codes;
	std::vector<std::string> event_names;
	CounterType t_start;
	int event_set;
	std::size_t n_events;
	std::size_t iterations;

       public:
	explicit EventCounter(const std::size_t &iterations)
	    : t_start{0}, iterations{iterations} {};
	~EventCounter() = default;

	void init(const std::string &path);

	inline void start() {
		PAPI_start(event_set);
		t_start = PAPI_get_real_cyc();
	}

	inline void stop() {
		CounterType t_end = (PAPI_get_real_cyc() - t_start);
		PAPI_stop(event_set, tmp_values.data());
		cycles.push_back(t_end);
		values.push_back(tmp_values);
		PAPI_reset(event_set);
	}

	void clear_state() {
		values.clear();
		cycles.clear();
	}

	std::vector<std::string> get_event_names() const { return event_names; }
	std::vector<CounterType> get_raw_cycles() const { return cycles; }
	std::vector<std::vector<CounterType>> get_raw_values() const;
	std::string names_to_line() const;
	size_t get_n_events() const { return n_events; }
};

#endif
