#include "papi_wrapper.hpp"

#include <fstream>
#include <functional>
#include <iterator>
#include <sstream>

namespace {

std::tuple<std::vector<std::string>, std::vector<int>> read_events_from_file(
    const std::string &filePath) {
	std::vector<int> codes;
	std::vector<std::string> names;
	std::ifstream infile(filePath);

	if (!infile.is_open()) {
		std::cerr << "Error: Could not open file " << filePath
			  << std::endl;
		return {};  // empty
	}

	std::string line;
	while (std::getline(infile, line)) {
		if (!line.empty()) {
			int code = PAPI_NULL;
			int ret = PAPI_event_name_to_code(line.c_str(), &code);
			if (ret != PAPI_OK) {
				std::cerr
				    << "PAPI_event_name_to_code failed with "
				    << static_cast<std::string>(
					   PAPI_strerror(ret))
				    << std::endl;
			}
			codes.push_back(code);
			names.push_back(line);
		}
	}
	infile.close();
	return {names, codes};
}

template <typename VectorType>
std::string vector_to_str_line(const VectorType &vec) {
	std::ostringstream oss;
	for (auto i = 0; i < vec.size(); ++i) {
		if (0 == i)
			oss << vec[i];
		else
			oss << "," << vec[i];
	}
	if (oss.str().empty()) std::cerr << "string empty" << std::endl;
	return oss.str();
}

}  // namespace

void EventCounter::init(const std::string &path) {
	int ret = PAPI_library_init(PAPI_VER_CURRENT);
	if (ret != PAPI_VER_CURRENT) {
		std::string err = "PAPIWrapper Constructor failed with " +
				  static_cast<std::string>(PAPI_strerror(ret));
		throw std::runtime_error(err);
	}

	event_set = PAPI_NULL;
	ret = PAPI_create_eventset(&event_set);

	if (ret != PAPI_OK) {
		std::string err = "PAPI create event set failed with " +
				  static_cast<std::string>(PAPI_strerror(ret));
		throw std::runtime_error(err);
	}

	try {
		auto events = read_events_from_file(path);
		event_names = std::move(std::get<0>(events));
		event_codes = std::move(std::get<1>(events));

	} catch (const std::exception &e) {
		throw;
	}

	if (event_names.empty() || event_codes.empty()) {
		throw std::runtime_error("Failed to read papi event file ");
	}

	n_events = event_names.size();
	values.reserve(iterations);
	cycles.reserve(iterations);
	tmp_values = std::move(std::vector<CounterType>(n_events));

	auto num_counters = PAPI_num_hwctrs();
	if (event_codes.size() > num_counters) {
		std::cerr << "Requeted " << event_codes.size()
			  << " counters but only " << num_counters
			  << "are available" << std::endl;
		throw std::runtime_error("counter err");
	}

	for (auto i = 0; i < event_codes.size(); ++i) {
		ret = PAPI_add_event(event_set, event_codes[i]);
		if (ret != PAPI_OK) {
			char name[PAPI_MAX_STR_LEN];
			PAPI_event_code_to_name(event_codes[i], name);
			std::string err =
			    "Failed to add event " + std::string{name} +
			    ". Reason: " +
			    static_cast<std::string>(PAPI_strerror(ret));
			throw std::runtime_error(err);
		}
	}
}

std::string EventCounter::names_to_line() const {
	return vector_to_str_line(event_names);
}

std::vector<std::vector<long long>> EventCounter::get_raw_values() const {
	std::vector<std::vector<long long>> raw(n_events);

	for (size_t i = 0; i < n_events; ++i) {
		std::vector<long long> tmp;
		tmp.reserve(iterations);

		for (const auto &v : values) {
			tmp.push_back(v[i]);
		}
		raw[i] = tmp;
	}
	return raw;
}
