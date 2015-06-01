#ifndef UTIL_MATH_INCLUDED
	#include "math.hpp"
#endif


namespace mo {
namespace util {

	namespace {
		constexpr int32_t small_magic_prime = 337;
		constexpr int32_t magic_prime = 1'299'827;
		constexpr auto max_seed = 512;
		constexpr auto max_seed_real = 512.f;
	}

	template<typename T>
	Interpolation<T>::Interpolation(T begin, T end, Interpolation_type type,
									T max_deviation, std::vector<T> cpoints)noexcept
		: initial_value(begin), final_value(end), type(type),
		  max_deviation(max_deviation), cpoints(cpoints) {
	}

	template<typename T>
	auto Interpolation<T>::operator()(float t, int32_t seed)const noexcept -> T {
		int32_t base = seed;
		T val;
		switch(type) {
			case Interpolation_type::linear:
				val = (1-t)*initial_value + t*final_value;
				base=base + t*small_magic_prime;
				break;

			case Interpolation_type::constant:
				val = cpoints.at((seed+magic_prime) % cpoints.size());
				break;
		}

		base = (base*small_magic_prime) % max_seed;

		val += (base/max_seed_real) * max_deviation*2 - max_deviation;

		return val;
	}

	template<typename T>
	auto lerp(T begin, T end, T max_deviation) -> Interpolation<T> {
		return {begin ,end, Interpolation_type::linear, max_deviation, {}};
	}

	template<typename T>
	auto cerp(std::vector<T> values, T max_deviation) -> Interpolation<T> {
		return {T{0} ,T{0}, Interpolation_type::linear, max_deviation, std::move(values)};
	}
}
}