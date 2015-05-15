/**************************************************************************\
 * Window & OpenGL-Context creation + management                          *
 *                                               ___                      *
 *    /\/\   __ _  __ _ _ __  _   _ _ __ ___     /___\_ __  _   _ ___     *
 *   /    \ / _` |/ _` | '_ \| | | | '_ ` _ \   //  // '_ \| | | / __|    *
 *  / /\/\ \ (_| | (_| | | | | |_| | | | | | | / \_//| |_) | |_| \__ \    *
 *  \/    \/\__,_|\__, |_| |_|\__,_|_| |_| |_| \___/ | .__/ \__,_|___/    *
 *                |___/                              |_|                  *
 *                                                                        *
 * Copyright (c) 2014 Florian Oetke                                       *
 *  Based on code of GDW-SS2014 project by Stefan Bodenschatz             *
 *  which was distributed under the MIT license.                          *
 *                                                                        *
 *  This file is part of MagnumOpus and distributed under the MIT License *
 *  See LICENSE file for details.                                         *
\**************************************************************************/

#pragma once

#include <memory>
#include <string>
#include <SDL2/SDL.h>

namespace mo {
namespace renderer {
	class Graphics_ctx {
		public:
			Graphics_ctx(const std::string& name, int width, int height, bool fullscreen);
			~Graphics_ctx();

			void start_frame();
			void end_frame(float delta_time);
			void set_clear_color(float r, float g, float b);

			auto win_width()const noexcept{return _win_width;}
			auto win_height()const noexcept{return _win_height;}

		private:
			std::string _name;
			int _win_width, _win_height;

			std::unique_ptr<SDL_Window,void(*)(SDL_Window*)> _window;
			SDL_GLContext _gl_ctx;

			float _frame_start_time = 0;
			float _delta_time_smoothed = 0;
			float _cpu_delta_time_smoothed = 0;
			float _time_since_last_FPS_output = 0;
	};

	struct Disable_depthtest {
		Disable_depthtest();
		~Disable_depthtest();
	};
}
}

