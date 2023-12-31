﻿#include "event_visuals.h"

#include "../../config_vars.h"

#include "../../../base/sdk.h"
#include "../../../base/global_context.h"

#include "../../../base/tools/math.h"
#include "../../../base/tools/render.h"

#include "../../../base/sdk/entity.h"

#include "../../../base/other/byte_arrays.h"

#include "../../features.h"

#include <playsoundapi.h>
#pragma comment(lib,"Winmm.lib")

void draw_beam(vector3d start, vector3d end, color clr) {
	beam_info_t beam_info;
	beam_info.type = beam_normal;
	beam_info.model_name = xor_c("sprites/purplelaser1.vmt");
	beam_info.model_index = -1;
	beam_info.start = start;
	beam_info.end = end;
	beam_info.life = 3.f;
	beam_info.fade_lenght = .1f;
	beam_info.halo_scale = 0.f;
	beam_info.amplitude = 1.f;
	beam_info.segments = 2;
	beam_info.renderable = true;
	beam_info.brightness = clr.a();
	beam_info.red = clr.r();
	beam_info.green = clr.g();
	beam_info.blue = clr.b();
	beam_info.speed = 1.f;
	beam_info.start_frame = 0;
	beam_info.frame_rate = 0.f;
	beam_info.width = 3;
	beam_info.end_width = 3;
	beam_info.flags = beam_only_no_is_once | beam_fade_in | beam_notile;

	beam_t* final_beam = interfaces::beam->create_beam_points(beam_info);

	if (final_beam)
		interfaces::beam->draw_beam(final_beam);
}

void c_event_visuals::on_item_purchase(c_game_event* event) {
	const char* event_name = event->get_name();
	if (std::strcmp(event_name, xor_c("item_purchase")) || !g_ctx.local)
		return;

	if (g_cfg.visuals.eventlog.logs & 8) {
		auto userid = event->get_int(xor_c("userid"));

		if (!userid)
			return;

		int user_id = interfaces::engine->get_player_for_user_id(event->get_int(xor_c("userid")));

		c_csplayer* player = (c_csplayer*)interfaces::entity_list->get_entity(user_id);
		if (!player)
			return;

		if (player->team() == g_ctx.local->team() || player == g_ctx.local)
			return;

		auto item_name = event->get_string(xor_c("weapon"));
		auto message = item_name + xor_str(" by ") + player->get_name();
		g_event_logger->add_message(message, event_buy);
	}
}

void c_event_visuals::on_bomb_plant(c_game_event* event) {
	const char* event_name = event->get_name();
	if (g_cfg.visuals.eventlog.logs & 16) {
		if (!std::strcmp(event_name, xor_c("bomb_planted"))) {
			int user_id = interfaces::engine->get_player_for_user_id(event->get_int(xor_c("userid")));

			c_csplayer* player = (c_csplayer*)interfaces::entity_list->get_entity(user_id);
			if (!player)
				return;

			const auto message = xor_str("planted by ") + player->get_name();
			g_event_logger->add_message(message, event_plant);
		}
		else if (!std::strcmp(event_name, xor_c("bomb_begindefuse"))) {
			int user_id = interfaces::engine->get_player_for_user_id(event->get_int(xor_c("userid")));

			c_csplayer* player = (c_csplayer*)interfaces::entity_list->get_entity(user_id);
			if (!player)
				return;

			const auto message = xor_str("defusing by ") + player->get_name();
			g_event_logger->add_message(message, event_plant);
		}
	}
}

void c_event_visuals::on_player_hurt(c_game_event* event) {
	const char* event_name = event->get_name();
	if (std::strcmp(event_name, xor_c("player_hurt")) || !g_ctx.local)
		return;

	int attacker = interfaces::engine->get_player_for_user_id(event->get_int(xor_c("attacker")));
	if (g_ctx.local->index() != attacker)
		return;

	int user_id = interfaces::engine->get_player_for_user_id(event->get_int(xor_c("userid")));
	c_csplayer* player = (c_csplayer*)interfaces::entity_list->get_entity(user_id);
	if (!player)
		return;

	if (player == g_ctx.local || player->team() == g_ctx.local->team())
		return;

	if (g_cfg.misc.sound != 0) {
		switch (g_cfg.misc.sound) {
		case 1:
			interfaces::surface->play_sound(xor_c("buttons/arena_switch_press_02.wav"));
			break;
		case 2:
			LI_FN(PlaySoundA).cached()(tapSound, NULL, SND_ASYNC | SND_MEMORY);
			break;
		}
	}

	if (!g_cfg.misc.hitmarker && !g_cfg.misc.damage) {
		if (!hitmarkers.empty())
			hitmarkers.clear();

		return;
	}

	vector3d origin = player->get_abs_origin();

	hitmarker_t best_impact{ };
	float best_impact_distance = -1;
	float time = g_ctx.system_time();

	for (int i = 0; i < impacts.size(); i++) {
		auto& iter = impacts[i];

		if (time > iter.impact_time + 3.f) {
			impacts.erase(impacts.begin() + i);
			continue;
		}

		float distance = iter.pos.dist_to(origin);
		if (distance < best_impact_distance || best_impact_distance == -1) {
			best_impact_distance = distance;
			best_impact = iter;
		}
	}

	if (best_impact_distance == -1)
		return;

	auto& hit = hitmarkers.emplace_back();
	hit.dmg			= event->get_int(xor_c("dmg_health"));
	hit.time		= g_ctx.system_time();
	hit.dmg_time	= g_ctx.system_time();
	hit.alpha		= 1.f;
	hit.pos			= best_impact.pos;
	hit.hp			= player->health();
}

void c_event_visuals::on_bullet_impact(c_game_event* event) {
	const char* event_name = event->get_name();
	if (std::strcmp(event_name, xor_c("bullet_impact")) || !g_ctx.local)
		return;

	int user_id = interfaces::engine->get_player_for_user_id(event->get_int(xor_c("userid")));

	c_csplayer* player = (c_csplayer*)interfaces::entity_list->get_entity(user_id);

	if (!player)
		return;

	auto x = event->get_float(xor_c("x"));
	auto y = event->get_float(xor_c("y"));
	auto z = event->get_float(xor_c("z"));

	if (player == g_ctx.local) {
		if (g_cfg.misc.impacts) {
			auto clr = g_cfg.misc.server_clr.base();
			const auto& size = g_cfg.misc.impact_size * 0.1f;
			interfaces::debug_overlay->add_box_overlay(
				vector3d(x, y, z), vector3d(-size, -size, -size),
				vector3d(size, size, size), vector3d(0, 0, 0),
				clr.r(), clr.g(), clr.b(), clr.a(), 4.f);
		}

		auto& impact = impacts.emplace_back();
		impact.pos			= vector3d(x, y, z);
		impact.impact_time	= g_ctx.system_time();
	}

	bool is_local = player == g_ctx.local;
	bool is_enemy = !is_local && player->team() != g_ctx.local->team();
	if (g_cfg.misc.tracers & 1 && is_enemy
		|| g_cfg.misc.tracers & 2 && !is_enemy && !is_local
		|| g_cfg.misc.tracers & 4 && is_local)
		bullets[player->index()].emplace_back(bullet_tracer_t{ player == g_ctx.local ? g_ctx.last_shoot_position : player->get_eye_position(), {x, y, z} });
}

void c_event_visuals::draw_hitmarkers() {
	if (!g_ctx.in_game || hitmarkers.empty()) 
		return;

	g_render->disable_aa();

	float screen_alpha = 0.f;
	for (int i = 0; i < hitmarkers.size(); ++i) {
		auto& hit = hitmarkers[i];
		if (hit.time == 0.f || hit.dmg <= 0)
			continue;

		vector2d position{ };
		if (!g_render->world_to_screen(hit.pos, position))
			continue;

		float diff = std::clamp(g_ctx.system_time() - hit.time, 0.f, 1.f);
		if (diff >= 1.f) {
			hit.alpha = std::lerp(hit.alpha, 0.f, g_ctx.animation_speed * 1.5f);
			if (hit.alpha <= 0.f) {
				hitmarkers.erase(hitmarkers.begin() + i);
				continue;
			}
		}

		if (hit.alpha > 0.f) {
			screen_alpha = hit.alpha;

			if (g_cfg.misc.hitmarker & 1) {
				auto clr = g_cfg.misc.hitmarker_clr.base();

				g_render->line(position.x - 8, position.y - 8, position.x - 3, position.y - 3, clr.new_alpha(255.f * hit.alpha));
				g_render->line(position.x + 8, position.y + 8, position.x + 3, position.y + 3, clr.new_alpha(255.f * hit.alpha));
				g_render->line(position.x - 8, position.y + 8, position.x - 3, position.y + 3, clr.new_alpha(255.f * hit.alpha));
				g_render->line(position.x + 8, position.y - 8, position.x + 3, position.y - 3, clr.new_alpha(255.f * hit.alpha));
			}

			if (g_cfg.misc.damage) {
				auto clr = g_cfg.misc.damage_clr.base();
				g_render->string(position.x, position.y - 28.f, clr.new_alpha(255.f * hit.alpha), centered_x | outline_light, g_fonts.dmg, xor_c("%d"), hit.dmg);
			}
		}
	}

	if (screen_alpha && (g_cfg.misc.hitmarker & 2)) {
		auto clr = g_cfg.misc.hitmarker_clr.base();

		auto position = vector2d(g_render->screen_size.w / 2.f, g_render->screen_size.h / 2.f);

		g_render->line(position.x - 13, position.y - 13, position.x - 5, position.y - 5, clr.new_alpha(255.f * screen_alpha));
		g_render->line(position.x + 13, position.y + 13, position.x + 5, position.y + 5, clr.new_alpha(255.f * screen_alpha));
		g_render->line(position.x - 13, position.y + 13, position.x - 5, position.y + 5, clr.new_alpha(255.f * screen_alpha));
		g_render->line(position.x + 13, position.y - 13, position.x + 5, position.y - 5, clr.new_alpha(255.f * screen_alpha));
	}

	g_render->enable_aa();
}

void c_event_visuals::on_render_start(int stage) {
	if (stage != frame_render_start)
		return;

	if (!g_ctx.local || !g_ctx.local->is_alive())
		return;

	auto& impact_list = *(c_utlvector<client_verify_t>*)((uintptr_t)g_ctx.local + 0xBA84);

	if (g_cfg.misc.impacts) {
		auto clr = g_cfg.misc.client_clr.base();

		const auto& size = g_cfg.misc.impact_size * 0.1f;
		for (auto i = impact_list.count(); i > last_impact_size; i--) {
			interfaces::debug_overlay->add_box_overlay(
				impact_list[i - 1].pos,
				vector3d(-size, -size, -size),
				vector3d(size, size, size),
				vector3d(0, 0, 0),
				clr.r(), clr.g(), clr.b(), clr.a(), 4);
		}

		impact_list.remove_all();
	}

	if (impact_list.count() != last_impact_size)
		last_impact_size = impact_list.count();

	if (g_cfg.misc.tracers) {
		for (auto i = 0; i < 65; i++) {
			auto& a = bullets[i];
			if (a.size() > 0) {
				auto player = (c_csplayer*)interfaces::entity_list->get_entity(i);
				if (player) {
					bool is_local = player == g_ctx.local;
					bool is_enemy = !is_local && player->team() != g_ctx.local->team();

					c_float_color trace_clr = c_float_color{ 255, 255, 255 };
					if (is_local)
						trace_clr = g_cfg.misc.trace_clr[2];
					else if (!is_enemy && !is_local)
						trace_clr = g_cfg.misc.trace_clr[1];
					else
						trace_clr = g_cfg.misc.trace_clr[0];

					auto base = trace_clr.base();
					switch (g_cfg.misc.tracer_type) {
					case 0:
						draw_beam(a.back().eye_position, a.back().impact_position, base);
						break;
					case 1:
						interfaces::debug_overlay->add_line_overlay(a.back().eye_position, a.back().impact_position,
							base.r(), base.g(), base.b(), false, 3.f);
						break;
					}
				}

				a.clear();
			}
		}
	}
}

void c_event_visuals::on_game_events(c_game_event* event) {
	this->on_player_hurt(event);
	this->on_bullet_impact(event);
	this->on_item_purchase(event);
	this->on_bomb_plant(event);
}

void c_event_visuals::on_directx() {
	this->draw_hitmarkers();
}
