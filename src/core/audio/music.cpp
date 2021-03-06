#include "music.hpp"

#ifndef __EMSCRIPTEN__
#	include <SDL2/SDL_mixer.h>
#else
#	include <SDL/SDL_mixer.h>
#endif

namespace mo {
namespace audio {

#ifndef __EMSCRIPTEN__
	namespace {
		int64_t istream_seek( struct SDL_RWops *context, int64_t offset, int whence) {
			std::istream* stream = (std::istream*) context->hidden.unknown.data1;

			stream->clear();

			if ( whence == SEEK_SET )
				stream->seekg ( offset, std::ios::beg );
			else if ( whence == SEEK_CUR )
				stream->seekg ( offset, std::ios::cur );
			else if ( whence == SEEK_END )
				stream->seekg ( offset, std::ios::end );

			return stream->fail() ? -1 : static_cast<int64_t>(stream->tellg());
		}


		std::size_t istream_read(SDL_RWops *context, void *ptr, std::size_t size, std::size_t maxnum) {
			if ( size == 0 )
				return -1;

			std::istream* stream = (std::istream*) context->hidden.unknown.data1;
			stream->read( (char*)ptr, size * maxnum );

			return stream->bad() ? -1 : stream->gcount() / size;
		}

		int istream_close( SDL_RWops *context ) {
			if ( context ) {
				SDL_FreeRW( context );
			}
			return 0;
		}
	}
#endif

	Music::Music(asset::istream stream) :
	    _handle(nullptr, Mix_FreeMusic), _stream(std::make_unique<asset::istream>(std::move(stream))){

		auto id = _stream->aid();

#ifndef __EMSCRIPTEN__
		SDL_RWops *rwops = SDL_AllocRW();
		INVARIANT(rwops, "SDL_AllocRW failed");

		rwops->seek = istream_seek;
		rwops->read = istream_read;
		rwops->write = NULL;
		rwops->close = istream_close;
		rwops->hidden.unknown.data1 = _stream.get();

		_handle.reset(Mix_LoadMUS_RW(rwops, 1));

#else
		auto location = _stream->physical_location();
		_stream.reset();

		if(location.is_nothing())
			return;

		DEBUG("Load Music: "<<location.get_or_throw());
		_handle.reset(Mix_LoadMUS(location.get_or_throw().c_str()));
#endif

		if(!_handle){
			WARN("Mix_LoadMUS_RW ("<<id.str()<<") failed: " << Mix_GetError());
		}

	}

} /* namespace sound */
}
