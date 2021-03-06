#include "item_system.hpp"

#include <sf2/sf2.hpp>

#include <core/asset/asset_manager.hpp>
#include <core/utils/random.hpp>
#include <core/renderer/particles.hpp>

#include "item_comp.hpp"
#include "collector_comp.hpp"
#include "drop_comp.hpp"
#include "element_comp.hpp"

#include "../combat/comp/health_comp.hpp"
#include "../combat/comp/score_comp.hpp"

namespace mo {
namespace sys {
namespace item {
	struct Droprate {
		std::string item_aid;
		float chance;
		int min=1;
		int max=1;
	};

	struct Droprate_group {
		std::vector<Droprate> values;
	};
	struct Droprate_conf {
		std::vector<Droprate_group> groups;
	};

	sf2_structDef(Droprate, item_aid, chance, min, max)
	sf2_structDef(Droprate_group, values)
	sf2_structDef(Droprate_conf, groups)


	using namespace unit_literals;
	using namespace combat;
	using namespace state;

	namespace {
		auto rng = util::create_random_generator();

		glm::vec2 random_dir() {
			return rotate(glm::vec2{1,0}, Angle{util::random_real(rng, remove_unit(0_deg), remove_unit(360_deg))});
		}

		constexpr auto score_mult_max_ttl = 2_s;
	}

	Item_system::Item_system(
	        asset::Asset_manager& assets,
	        audio::Audio_ctx& audio,
	        ecs::Entity_manager& entity_manager,
	        physics::Physics_system& physics_system,
	        physics::Transform_system& transform,
	        state::State_system& state_system,
	        renderer::Particle_renderer& particles)
	    : _assets(assets),
	      _audio(audio),
	      _em(entity_manager),
	      _ts(transform),
	      _particles(particles),
	      _collectors(entity_manager.list<Collector_comp>()),
	      _collision_slot(&Item_system::_on_collision, this),
	      _drop_loot_slot(&Item_system::_drop_loot, this),
	      _pickup_sound_coin(assets.load<audio::Sound>("sound:pickup_coin"_aid)),
	      _pickup_sound_health(assets.load<audio::Sound>("sound:pickup_health"_aid)),
	      _pickup_sound_other(assets.load<audio::Sound>("sound:pickup_other"_aid))
	{
		_collision_slot.connect(physics_system.collisions);
		_drop_loot_slot.connect(state_system.state_change_events);

		entity_manager.register_component_type<Collector_comp>();
		entity_manager.register_component_type<Item_comp>();
		entity_manager.register_component_type<Drop_comp>();

		_droprates = assets.load<Droprate_conf>("cfg:drops"_aid);
		INVARIANT(!_droprates->groups.empty(), "Couldn't load drop configuration");
	}

	void Item_system::up_score_multiplicator() {
		_score_multiplicator+=1.0f;
		_score_mult_ttl = score_mult_max_ttl;
	}

	void Item_system::update(Time realtime_dt, Time dt) {
		for(auto& i : _em.list<Item_comp>()) {
			i._despawn_time_left -= dt.value();
			if(i._despawn_time_left < 0.f) {
				_em.erase(i.owner_ptr());
			}
		}

		if(_score_mult_ttl>0_s)
			_score_mult_ttl-=dt;
		else
			_score_multiplicator = std::max(1.f, _score_multiplicator - dt/1_s);

		if(_bullet_time_left>Time(0))
			_bullet_time_left -= realtime_dt;

		for(auto& c : _collectors) {
			if(c._active) {
				c._active = false;

				c.owner().get<physics::Transform_comp>().process([&](auto& t){
					if(!c._particles) {
						c._particles = _particles.create_emiter(
								t.position(),
								0_deg,
								0.1_m,
								0.2_m,
								renderer::Collision_handler::kill,
								200,
								100,
								0.25_s, 0.75_s,
								util::cerp<Angle>({0_deg}, c._far_angle/2.f),
								util::scerp<Angle>(0_deg, 0_deg),
								util::lerp<Speed_per_time>(30_m/second_2, 10_m/second_2),
								util::lerp<Angle_acceleration>(0_deg/second_2, 5_deg/second_2),
								util::lerp<glm::vec4>({0.4,0.4,0.4,0}, {0,0,0,0}, {0,0,0,0}),
								util::lerp<Position>({50_cm, 50_cm}, {5_cm, 5_cm}, {2_cm, 2_cm}),
								util::lerp<int8_t>(0, 0),
								_assets.load<renderer::Texture>("tex:particle_wind"_aid),
								true
						);
					}

					if(c._particles) {
						c._particles->update_center(t.position(), t.rotation());
						c._particles->active(true);
					}

					_ts.foreach_in_range(t.position(), t.rotation(),
					                     c._near, c._far,
					                     c._far_angle, c._near_angle,
					                    [&](ecs::Entity& e){
						util::process(e.get<physics::Transform_comp>(),
						              e.get<physics::Physics_comp>())
						>> [&](auto& tt, auto& tp){
							auto diff = remove_units(t.position()-tt.position());

							auto diff_len = glm::length(diff);
							if(diff_len>0.0f) {
								auto normal = (diff/diff_len);

								tp.apply_force(c._force * normal * 0.05 );

								constexpr auto max_body_velocity = (40_km/hour).value();
								auto vx = tp.velocity().x.value();
								auto vy = tp.velocity().y.value();
								auto speed = vx*vx + vy*vy;
								if(speed>max_body_velocity*max_body_velocity) {
									tp.velocity(tp.velocity() * (max_body_velocity/std::sqrt(speed)));
								}
							}
						};
					});

				});
			} else {
				if(c._particles) {
					c._particles->active(false);
				}
			}
		}
	}

	void Item_system::_on_collision(physics::Manifold& m) {
		if(m.is_with_object()) {
			m.a->owner().get<Item_comp>().process([&](Item_comp& item){
				if(!item._collected) {
					auto& other = m.b.comp->owner();

					if(item._joinable && other.has<Item_comp>()) {
						auto& other_item = other.get<Item_comp>().get_or_throw();
						if(other_item._joinable && !other_item._collected &&
						   item._target==other_item._target && item._element==other_item._element) {

							other_item._mod+=item._mod;
							process(item.owner().get<physics::Physics_comp>(),
							        other.get<physics::Physics_comp>())
							        >>[&](auto& a, auto& b){
								b.radius(min(a.radius() + b.radius(), 2_m));
							};

							item._collected = true;
						}

					} else if(other.has<Collector_comp>()) {
						switch(item._target) {
							case health:
								other.get<Health_comp>().process([&](Health_comp& h) {
									if(item._mod>0) {
										if(!h.damaged())
											return;

										h.heal(item._mod);
									} else
										h.damage(-item._mod);

									item._collected = true;
									_audio.play_static(*_pickup_sound_health);
								});

								break;
							case score:
								other.get<Score_comp>().process([&](Score_comp& s) {
									this->_score_mult_ttl = score_mult_max_ttl;
									s.add(item._mod);
									item._collected = true;
									_audio.play_static(*_pickup_sound_coin);
								});
								break;
							case element:
								other.get<Element_comp>().process([&](Element_comp& e) {
									if(e.add_slot(item._element, item._mod)) {
										item._collected = true;
										_audio.play_static(*_pickup_sound_other);
									}
								});
								break;

							case bullet_time:
								_bullet_time_left = Time(item._mod);
								item._collected = true;
								_audio.play_static(*_pickup_sound_other);
								break;
						}
					}

					if(item._collected)
						_em.erase(item.owner_ptr());
				}
			});
		}

	}

	void Item_system::_drop_loot(ecs::Entity& e, sys::state::State_data& s) {
		if(s.s==Entity_state::dying) { // he's dead jim
			process(e.get<Drop_comp>(),
			        e.get<physics::Transform_comp>())
			        >> [&](Drop_comp& d, physics::Transform_comp& t) {
				up_score_multiplicator();

				if(d._group>=0 && d._group<int8_t(_droprates->groups.size())) {
					auto group = _droprates->groups[d._group];

					for(auto& item : group.values) {
						if(util::random_bool(rng, item.chance)) {
							auto count = util::random_int(rng, item.min, item.max);

							if(item.item_aid=="coin" && this->_score_multiplicator>1.1f) {
								count = int(std::max(1,count) * std::min(10.f, std::max(1.f, this->_score_multiplicator) + 0.5f));
							}

							while(count-->0) {
								auto spawned = _em.emplace(asset::AID(asset::Asset_type::blueprint, item.item_aid));
								spawned->get<physics::Transform_comp>().get_or_throw().position(t.position());
								spawned->get<physics::Physics_comp>().get_or_throw().impulse(random_dir()  *5_n);
								spawned->get<Item_comp>().process([&](auto& item){
									item._despawn_time_left = util::random_real(rng, 8.f, 12.f);
								});
							}
						}
					}
				}
			};
		}
	}

}
}
}
