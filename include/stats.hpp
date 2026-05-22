#ifndef __STATS_HPP__
#define __STATS_HPP__
#include <algorithm>
#include <vector>

namespace stats {

template <typename T>
double median(std::vector<T>& vec) {
	const size_t n = vec.size();
	const size_t mid = n / 2;

	std::sort(vec.begin(), vec.end());

	if (n % 2 == 1) return vec[mid];

	return (vec[mid] + vec[mid - 1]) / 2.0;
}

template <typename T>
T min(std::vector<T>& vec) {
	return *std::min_element(vec.begin(), vec.end());
}

template <typename T>
T max(std::vector<T>& vec) {
	return *std::max_element(vec.begin(), vec.end());
}
}  // namespace stats
#endif
