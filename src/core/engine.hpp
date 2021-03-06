/**************************************************************************\
 * initialization, live-cycle management & glue-code                      *
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

#include <vector>
#include <memory>
#include "utils/maybe.hpp"

namespace mo {
	namespace asset {class Asset_manager;}
	namespace renderer {class Graphics_ctx;}
	namespace audio {class Audio_ctx;}
	class Configuration;
	class Input_manager;


	class Engine;

	enum class Prev_screen_policy {
		discard,
		stack,
		draw,
		update
	};

	class Screen {
		public:
			Screen(Engine& engine) : _engine(engine) {}
			Screen(const Screen&) = delete;
			Screen& operator=(const Screen&) = delete;

			virtual ~Screen()noexcept = default;

		protected:
			friend class Engine;

			virtual void _on_enter(util::maybe<Screen&> prev){}
			virtual void _on_leave(util::maybe<Screen&> next){}
			virtual void _update(float delta_time) = 0;
			virtual void _draw(float delta_time) = 0;
			virtual auto _prev_screen_policy()const noexcept -> Prev_screen_policy = 0;

			Engine& _engine;
	};

	struct Sdl_wrapper {
		Sdl_wrapper();
		~Sdl_wrapper();
	};

	extern std::string get_sdl_error();

	class Engine {
		friend class Screen;
		public:
			Engine(const std::string& title, int argc, char** argv, char** env);
			~Engine() noexcept;

			bool running() const noexcept {
				return !_quit;
			}

			void exit() noexcept {
				_quit = true;
			}

			template<class T, typename ...Args>
			auto enter_screen(Args&&... args) -> T& {
				return static_cast<T&>(enter_screen(std::make_unique<T>(*this, std::forward<Args>(args)...)));
			}

			auto enter_screen(std::unique_ptr<Screen> screen) -> Screen&;
			void leave_screen(uint8_t depth=1);
			auto current_screen() -> Screen&;

			void on_frame();

			auto& graphics_ctx()noexcept {return *_graphics_ctx;}
			auto& graphics_ctx()const noexcept {return *_graphics_ctx;}
			auto& audio_ctx() const noexcept {return *_audio_ctx;}
			auto& audio_ctx() noexcept {return *_audio_ctx;}
			auto& assets()noexcept {return *_asset_manager;}
			auto& assets()const noexcept {return *_asset_manager;}
			auto& input()noexcept {return *_input_manager;}
			auto& input()const noexcept {return *_input_manager;}

		protected:
			virtual void _on_frame(float dt) {};
			virtual auto _on_reload() -> std::tuple<bool, std::string> {return std::make_tuple(false, "");};

		protected:
			bool _quit = false;
			std::unique_ptr<asset::Asset_manager> _asset_manager;
			Sdl_wrapper _sdl;
			std::unique_ptr<renderer::Graphics_ctx> _graphics_ctx;
			std::unique_ptr<audio::Audio_ctx> _audio_ctx;
			std::unique_ptr<Input_manager> _input_manager;
			std::vector<std::shared_ptr<Screen>> _screen_stack;

			float _current_time = 0;
			float _last_time = 0;

			void _poll_events();

			struct Reload_handler;
			std::unique_ptr<Reload_handler> _rh;
	};

} /* namespace core */
