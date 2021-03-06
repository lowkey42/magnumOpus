#include "graphics_ctx.hpp"

#include <iostream>
#include <sstream>
#include <cstdio>
#include <GL/glew.h>
#include <sf2/sf2.hpp>

#include "../utils/log.hpp"
#include "../asset/asset_manager.hpp"

namespace mo {
namespace renderer {
	namespace {
		void sdl_error_check() {
			const char *err = SDL_GetError();
			if(*err != '\0') {
				std::string errorStr(err);
				SDL_ClearError();
				FAIL("SDL: "<<errorStr);
			}
		}

	#ifndef EMSCRIPTEN
		void
	#ifdef GLAPIENTRY
		GLAPIENTRY
	#endif
		gl_debug_callback(GLenum source,GLenum type,GLuint id,GLenum severity,GLsizei length,const GLchar* message,const void*) {
			WARN(std::string(message,length)<<" (source: "<<source<<", type: "<<type<<", id: "<<id<<", severity: "<<severity<<")");
		}
	#endif


		struct Graphics_cfg {
			int width;
			int height;
			bool fullscreen;
			float max_screenshake = 0.5;
			float brightness = 1.1;
		};

		sf2_structDef(Graphics_cfg,
			width,
			height,
			fullscreen,
			max_screenshake,
			brightness
		)

	#ifndef EMSCRIPTEN
		constexpr auto default_cfg = Graphics_cfg{1920,1080,true, 0.5f, 1.2f};
	#else
		constexpr auto default_cfg = Graphics_cfg{1024,512,false, 0.5f, 1.2f};
	#endif

	}


	Graphics_ctx::Graphics_ctx(const std::string& name, asset::Asset_manager& assets)
	 : _assets(assets), _name(name), _window(nullptr, SDL_DestroyWindow) {

		auto& cfg = asset::unpack(assets.load_maybe<Graphics_cfg>("cfg:graphics"_aid)).get_or_other(
			default_cfg
		);

		_win_width = cfg.width;
		_win_height = cfg.height;
		_max_screenshake = cfg.max_screenshake;
		_brightness = cfg.brightness;
		_fullscreen = cfg.fullscreen;

		if(&cfg==&default_cfg) {
			assets.save<Graphics_cfg>("cfg:graphics"_aid, cfg);
		}

#ifndef EMSCRIPTEN
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#endif
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

		int win_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
		if(_fullscreen)
			win_flags|=SDL_WINDOW_FULLSCREEN_DESKTOP;

		_window.reset( SDL_CreateWindow(_name.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
							_win_width, _win_height, win_flags) );

		if (!_window)
			FAIL("Unable to create window");

		sdl_error_check();

		SDL_GetWindowSize(_window.get(), &_win_width, &_win_height);

		try {
			_gl_ctx = SDL_GL_CreateContext(_window.get());
			sdl_error_check();
			SDL_GL_MakeCurrent(_window.get(), _gl_ctx);
			sdl_error_check();

		} catch (const std::runtime_error& ex) {
			FAIL("Failure to create OpenGL context. This application requires a OpenGL 3.3 capable GPU. Error was: "<< ex.what());
		}

		SDL_GL_SetSwapInterval(1);

		glewExperimental = GL_TRUE;
		glewInit();

#ifndef __EMSCRIPTEN__
		INVARIANT(GLEW_VERSION_3_3, "Requested OpenGL 3.3 Context but 3.3 Features are not available.");

		if(GLEW_KHR_debug){
			glDebugMessageCallback((GLDEBUGPROC)gl_debug_callback, stderr);
		}
		else{
			WARN("No OpenGL debug log available.");
		}
#endif

		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
		set_clear_color(0.0f,0.0f,0.0f);
		SDL_GL_SetSwapInterval(1);
	}

	Graphics_ctx::~Graphics_ctx() {
		SDL_GL_DeleteContext(_gl_ctx);
	}

	void Graphics_ctx::reset_viewport()const noexcept {
		glViewport(0,0, win_width(), win_height());
	}

	void Graphics_ctx::start_frame() {
		glClearColor(_clear_color.r, _clear_color.g, _clear_color.b,1.f);
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

		_frame_start_time = SDL_GetTicks() / 1000.0f;
	}
	void Graphics_ctx::end_frame(float delta_time) {
		float smooth_factor=0.1f;
		_delta_time_smoothed=(1.0f-smooth_factor)*_delta_time_smoothed+smooth_factor*delta_time;

		float cpu_delta_time = SDL_GetTicks() / 1000.0f - _frame_start_time;
		_cpu_delta_time_smoothed=(1.0f-smooth_factor)*_cpu_delta_time_smoothed+smooth_factor*cpu_delta_time;

		_time_since_last_FPS_output+=delta_time;
		if(_time_since_last_FPS_output>=1.0f){
			_time_since_last_FPS_output=0.0f;
			std::ostringstream osstr;
			osstr<<_name<<" ("<<(int((1.0f/_delta_time_smoothed)*10.0f)/10.0f)<<" FPS, ";
			osstr<<(int(_delta_time_smoothed*10000.0f)/10.0f)<<" ms/frame, ";
			osstr<<(int(_cpu_delta_time_smoothed*10000.0f)/10.0f)<<" ms/frame [cpu])";
			SDL_SetWindowTitle(_window.get(), osstr.str().c_str());
		}
		SDL_GL_SwapWindow(_window.get());

		// unbind texture
		glActiveTexture(0);
		glBindTexture(GL_TEXTURE_2D, 0);

#ifndef SLOW_SYSTEM
		if( delta_time < 1.f/60 ) {
            SDL_Delay(Uint32(1000*(1.f/60 - delta_time)));
        }
#endif
	}
	void Graphics_ctx::set_clear_color(float r, float g, float b) {
		_clear_color = glm::vec3(r,g,b);
	}

	auto Graphics_ctx::max_screenshake()const noexcept -> float {
		return _screenshake_enabled ? _max_screenshake * 100 : 0;
	}
	void Graphics_ctx::toggle_screenschake(bool enable) {
		_screenshake_enabled = enable;
	}

	void Graphics_ctx::resolution(int width, int height, float max_screenshake) {
		Graphics_cfg cfg{_win_width, _win_height, _fullscreen, _max_screenshake};
		_assets.save<Graphics_cfg>("cfg:graphics"_aid, cfg);
	}

	Disable_depthtest::Disable_depthtest() {
		glDisable(GL_DEPTH_TEST);
	}
	Disable_depthtest::~Disable_depthtest() {
		glEnable(GL_DEPTH_TEST);
	}

	Disable_depthwrite::Disable_depthwrite() {
		glDepthMask(GL_FALSE);
	}
	Disable_depthwrite::~Disable_depthwrite() {
		glDepthMask(GL_TRUE);
	}
}
}
