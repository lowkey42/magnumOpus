/**************************************************************************\
 * simple wrapper for SDL-Music                                           *
 *                                                ___                     *
 *    /\/\   __ _  __ _ _ __  _   _ _ __ ___     /___\_ __  _   _ ___     *
 *   /    \ / _` |/ _` | '_ \| | | | '_ ` _ \   //  // '_ \| | | / __|    *
 *  / /\/\ \ (_| | (_| | | | | |_| | | | | | | / \_//| |_) | |_| \__ \    *
 *  \/    \/\__,_|\__, |_| |_|\__,_|_| |_| |_| \___/ | .__/ \__,_|___/    *
 *                |___/                              |_|                  *
 *                                                                        *
 * Copyright (c) 2015 Florian Oetke & Sebastian Schalow                   *
 *                                                                        *
 *  This file is part of MagnumOpus and distributed under the MIT License *
 *  See LICENSE file for details.                                         *
\**************************************************************************/

#pragma once

#include <string>
#include <vector>
#include <stdexcept>

#include "SDL/SDL_mixer.h"

#include "../utils/log.hpp"
#include "../asset/asset_manager.hpp"

namespace mo {
namespace sound {

	struct Music_loading_failed : public asset::Loading_failed {
		explicit Music_loading_failed(const std::string& msg)noexcept : Loading_failed(msg){}
	};

	class Music {
		public:
			explicit Music(asset::istream stream) throw(Music_loading_failed);
			virtual ~Music()noexcept = default;

			Music& operator=(Music&&) noexcept = default;

			Music(const Music&) = delete;
			Music& operator=(const Music&) = delete;

			Mix_Music* getMusic() const noexcept { return _handle.get(); }

		protected:
			std::unique_ptr<Mix_Music,void(*)(Mix_Music*)> _handle;
			std::unique_ptr<asset::istream> _stream;

	};
	using Music_ptr = asset::Ptr<Music>;


} /* namespace sound */

namespace asset {
	template<>
	struct Loader<sound::Music> {
		using RT = std::shared_ptr<sound::Music>;

		static RT load(istream in) throw(Loading_failed){
			return std::make_unique<sound::Music>(std::move(in));
		}

		static void store(ostream out, const sound::Music& asset) throw(Loading_failed) {
			// TODO
			FAIL("NOT IMPLEMENTED, YET!");
		}
	};
}
}
