#ifndef __REPORTER_HPP__
#define __REPORTER_HPP__
#include <iostream>
#include <vector>

#include "stats.hpp"
namespace reporter {
using Reporter = std::function<std::string(
    EventCounter &, const size_t &, const size_t &, const std::string &)>;

std::string median_reporter(EventCounter &counter, const size_t &iterations,
			    const size_t &size, const std::string &name) {
	std::ostringstream oss;
	auto raw_values = counter.get_raw_values();
	auto raw_cycles = counter.get_raw_cycles();
	const size_t n_events = raw_values.size();

	if (raw_values.empty() || raw_cycles.empty() || n_events == 0) {
		return {};
	}

	oss << name << "," << iterations << "," << size << ","
	    << stats::median(raw_cycles) << ",";

	for (size_t n = 0; n < n_events; ++n) {
		oss << stats::median(raw_values[n]);
		if (n == n_events - 1)
			oss << std::endl;
		else
			oss << ",";
	}

	counter.clear_state();
	return oss.str();
}

std::string min_reporter(EventCounter &counter, const size_t &iterations,
			 const size_t &size, const std::string &name) {
	std::ostringstream oss;
	auto raw_values = counter.get_raw_values();
	auto raw_cycles = counter.get_raw_cycles();
	const size_t n_events = counter.get_n_events();

	if (raw_values.empty() || raw_cycles.empty() || n_events == 0) {
		return {};
	}

	oss << name << "," << iterations << "," << size << ","
	    << stats::min(raw_cycles) << ",";

	for (size_t n = 0; n < n_events; ++n) {
		oss << stats::min(raw_values[n]);
		if (n == n_events - 1)
			oss << std::endl;
		else
			oss << ",";
	}

	counter.clear_state();
	return oss.str();
}

std::string raw_data_reporter(EventCounter &counter, const size_t &iterations,
			      const size_t &size, const std::string &name) {
	std::ostringstream oss;
	const auto raw_values = counter.get_raw_values();
	const auto raw_cycles = counter.get_raw_cycles();
	const size_t n_events = counter.get_event_names().size();

	if (raw_values.empty() || raw_cycles.empty() || n_events == 0) {
		return {};
	}

	for (size_t i = 0; i < iterations; ++i) {
		oss << name << "," << i << "," << size << "," << raw_cycles[i]
		    << ",";
		for (size_t n = 0; n < n_events; ++n) {
			oss << raw_values[n][i];
			if (n == n_events - 1)
				oss << std::endl;
			else
				oss << ",";
		}
	}
	counter.clear_state();
	return oss.str();
}
}  // namespace reporter

#endif
