/*
	Copyright (C) 2003 - 2022
	by David White <dave@whitevine.net>
	Part of the Battle for Wesnoth Project https://www.wesnoth.org/

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY.

	See the COPYING file for more details.
*/

/**
 * @file
 * Routines for showing the help-dialog.
 */

#define GETTEXT_DOMAIN "wesnoth-help"

#include "help/help.hpp"

#include "config.hpp"                   // for config, etc
#include "events.hpp"                   // for raise_draw_event, pump, etc
#include "font/constants.hpp"           // for relative_size
#include "preferences/game.hpp"
#include "game_config_manager.hpp"
#include "gettext.hpp"                  // for _
#include "gui/dialogs/transient_message.hpp"
#include "help/help_browser.hpp"             // for help_browser
#include "help/help_impl.hpp"                // for hidden_symbol, toplevel, etc
#include "key.hpp"                      // for CKey
#include "log.hpp"                      // for LOG_STREAM, log_domain
#include "sdl/surface.hpp"                // for surface
#include "show_dialog.hpp"              // for dialog_frame, etc
#include "terrain/terrain.hpp"                  // for terrain_type
#include "units/unit.hpp"                     // for unit
#include "units/types.hpp"               // for unit_type, unit_type_data, etc
#include "video.hpp"                    // for CVideo, resize_lock
#include "widgets/button.hpp"           // for button

#include <cassert>                     // for assert
#include <algorithm>                    // for min
#include <ostream>                      // for basic_ostream, operator<<, etc
#include <vector>                       // for vector, vector<>::iterator
#include <SDL2/SDL.h>


static lg::log_domain log_display("display");
#define WRN_DP LOG_STREAM(warn, log_display)

static lg::log_domain log_help("help");
#define ERR_HELP LOG_STREAM(err, log_help)

namespace help {
/**
 * Open a help dialog using a specified toplevel.
 *
 * This would allow for complete customization of the contents, although not in a
 * very easy way. It's used as the internal implementation of the other help*
 * functions.
 *
 *@pre The help_manager must already exist; this is different to the functions
 * declared in help.hpp, which is why this one's declaration is in the .cpp
 * file. Because this takes a section as an argument, it wouldn't make sense
 * for it to call ensure_cache_lifecycle() internally - if the help_manager
 * doesn't already exist, that would likely destroy the referenced object at
 * the point that this function exited.
 */
void show_with_toplevel(const section &toplevel, const std::string& show_topic="", int xloc=-1, int yloc=-1);


void show_unit_description(const unit &u)
{
	auto cache_lifecycle = ensure_cache_lifecycle();
	help::show_unit_description(u.type());
}

void show_terrain_description(const terrain_type &t)
{
	auto cache_lifecycle = ensure_cache_lifecycle();
	help::show_terrain_help(t.id(), t.hide_help());
}

void show_unit_description(const unit_type &t)
{
	auto cache_lifecycle = ensure_cache_lifecycle();
	std::string var_id = t.get_cfg()["variation_id"].str();
	if (var_id.empty())
		var_id = t.get_cfg()["variation_name"].str();
	bool hide_help = t.hide_help();
	bool use_variation = false;
	if (!var_id.empty()) {
		const unit_type *parent = unit_types.find(t.id());
		assert(parent);
		if (hide_help) {
			hide_help = parent->hide_help();
		} else {
			use_variation = true;
		}
	}

	if (use_variation)
		help::show_variation_help(t.id(), var_id, hide_help);
	else
		help::show_unit_help(t.id(), t.show_variations_in_help(), hide_help);
}

help_manager::help_manager(const game_config_view *cfg)
{
	assert(!game_cfg);
	assert(cfg);
	// This is a global rawpointer in the help:: namespace.
	game_cfg = cfg;
}

std::unique_ptr<help_manager> ensure_cache_lifecycle()
{
	// The internals of help_manager are that this global raw pointer is
	// non-null if and only if an instance of help_manager already exists.
	if(game_cfg)
		return nullptr;
	return std::make_unique<help_manager>(&game_config_manager::get()->game_config());
}

help_manager::~help_manager()
{
	game_cfg = nullptr;
	default_toplevel.clear();
	hidden_sections.clear();
    // These last numbers must be reset so that the content is regenerated.
    // Upon next start.
	last_num_encountered_units = -1;
	last_num_encountered_terrains = -1;
}

/**
 * Open the help browser, show topic with id show_topic.
 *
 * If show_topic is the empty string, the default topic will be shown.
 */
void show_help(const std::string& show_topic, int xloc, int yloc)
{
	auto cache_lifecycle = ensure_cache_lifecycle();
	show_with_toplevel(default_toplevel, show_topic, xloc, yloc);
}

/**
 * Open the help browser, show unit with id unit_id.
 *
 * If show_topic is the empty string, the default topic will be shown.
 */
void show_unit_help(const std::string& show_topic, bool has_variations, bool hidden, int xloc, int yloc)
{
	auto cache_lifecycle = ensure_cache_lifecycle();
	show_with_toplevel(default_toplevel,
			  hidden_symbol(hidden) + (has_variations ? ".." : "") + unit_prefix + show_topic, xloc, yloc);
}

/**
 * Open the help browser, show terrain with id terrain_id.
 *
 * If show_topic is the empty string, the default topic will be shown.
 */
void show_terrain_help(const std::string& show_topic, bool hidden, int xloc, int yloc)
{
	auto cache_lifecycle = ensure_cache_lifecycle();
	show_with_toplevel(default_toplevel, hidden_symbol(hidden) + terrain_prefix + show_topic, xloc, yloc);
}

/**
 * Open the help browser, show the variation of the unit matching.
 */
void show_variation_help(const std::string& unit, const std::string &variation, bool hidden, int xloc, int yloc)
{
	auto cache_lifecycle = ensure_cache_lifecycle();
	show_with_toplevel(default_toplevel, hidden_symbol(hidden) + variation_prefix + unit + "_" + variation, xloc, yloc);
}

/**
 * Open a help dialog using a toplevel other than the default.
 *
 * This allows for complete customization of the contents, although not in a
 * very easy way.
 */
void show_with_toplevel(const section &toplevel_sec,
			   const std::string& show_topic,
			   int xloc, int yloc)
{
	CVideo& video = CVideo::get_singleton();

	const events::event_context dialog_events_context;
	const gui::dialog_manager manager;

	SDL_Rect screen_area = video.screen_area();

	const int width  = std::min<int>(font::relative_size(1200), screen_area.w - font::relative_size(20));
	const int height = std::min<int>(font::relative_size(850), screen_area.h - font::relative_size(150));
	const int left_padding = font::relative_size(10);
	const int right_padding = font::relative_size(10);
	const int top_padding = font::relative_size(10);
	const int bot_padding = font::relative_size(10);

	// If not both locations were supplied, put the dialog in the middle
	// of the screen.
	if (yloc <= -1 || xloc <= -1) {
		xloc = screen_area.w / 2 - width / 2;
		yloc = screen_area.h / 2 - height / 2;
	}
	std::vector<gui::button*> buttons_ptr;
	gui::button close_button_(video, _("Close"));
	buttons_ptr.push_back(&close_button_);

	gui::dialog_frame f(video, _("Help"), gui::dialog_frame::default_style,
					 true, &buttons_ptr);
	f.layout(xloc, yloc, width, height);
	f.draw();

    // Find all unit_types that have not been constructed yet and fill in the information
    // needed to create the help topics
	unit_types.build_all(unit_type::HELP_INDEXED);

	if (preferences::encountered_units().size() != size_t(last_num_encountered_units) ||
	    preferences::encountered_terrains().size() != size_t(last_num_encountered_terrains) ||
	    last_debug_state != game_config::debug ||
		last_num_encountered_units < 0) {
		// More units or terrains encountered, update the contents.
		last_num_encountered_units = preferences::encountered_units().size();
		last_num_encountered_terrains = preferences::encountered_terrains().size();
		last_debug_state = game_config::debug;
		generate_contents();
	}
	try {
		help_browser hb(video, toplevel_sec);
		hb.set_location(xloc + left_padding, yloc + top_padding);
		hb.set_width(width - left_padding - right_padding);
		hb.set_height(height - top_padding - bot_padding);
		if (!show_topic.empty()) {
			hb.show_topic(show_topic);
		}
		else {
			hb.show_topic(default_show_topic);
		}
		hb.set_dirty(true);
		events::raise_draw_event();
		CKey key;
		for (;;) {
			events::pump();
			events::raise_process_event();
			f.draw();
			events::raise_draw_event();
			if (key[SDLK_ESCAPE]) {
				// Escape quits from the dialog.
				return;
			}
			for (std::vector<gui::button*>::iterator button_it = buttons_ptr.begin();
				 button_it != buttons_ptr.end(); ++button_it) {
				if ((*button_it)->pressed()) {
					// There is only one button, close.
					return;
				}
			}
			video.flip();
			CVideo::delay(10);
		}
	}
	catch (const parse_error& e) {
		ERR_HELP << _("Parse error when parsing help text:") << " " << e.message << std::endl;
#if 0
		// Displaying in the UI is disabled due to issue #2587
		std::stringstream msg;
		msg << _("Parse error when parsing help text:") << " '" << e.message << "'";
		gui2::show_transient_message("", msg.str());
#endif
	}
}

} // End namespace help.
