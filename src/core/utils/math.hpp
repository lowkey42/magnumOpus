/**************************************************************************\
 * math helpers                                                           *
 *                                               ___                      *
 *    /\/\   __ _  __ _ _ __  _   _ _ __ ___     /___\_ __  _   _ ___     *
 *   /    \ / _` |/ _` | '_ \| | | | '_ ` _ \   //  // '_ \| | | / __|    *
 *  / /\/\ \ (_| | (_| | | | | |_| | | | | | | / \_//| |_) | |_| \__ \    *
 *  \/    \/\__,_|\__, |_| |_|\__,_|_| |_| |_| \___/ | .__/ \__,_|___/    *
 *                |___/                              |_|                  *
 *                                                                        *
 * Copyright (c) 2014 Florian Oetke                                       *
 *                                                                        *
 *  This file is part of MagnumOpus and distributed under the MIT License *
 *  See LICENSE file for details.                                         *
\**************************************************************************/

#pragma once

#include <tuple>
#include "../units.hpp"

namespace mo {
namespace util {

	template<typename Pos, typename Vel>
	auto spring(Pos source, Vel v, Pos target, float damping,
	            float freq, Time t) -> std::tuple<Pos, Vel> {
		auto f = remove_unit(1 + 2*t*damping*freq);
		auto tff = remove_unit(t*freq*freq);
		auto ttff = remove_unit(t*tff);
		auto detInv = 1.f / (f+ttff);
		auto diff = remove_units(target-source);
		return std::make_tuple(
			(f * source + t*v+ttff*target) * detInv,
			(v + tff * Vel{diff.x, diff.y}) * detInv
		);

	}

}
}