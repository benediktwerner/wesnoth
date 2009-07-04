/* $Id$ */
/*
   Copyright (C) 2008 - 2009 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 2
   or at your option any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

/**
 * @file window_builder_private.hpp
 * This file contains all classes used privately in window_builder.cpp and
 * should only be included by window_builder.cpp.
 */

#ifndef GUI_AUXILIARY_WINDOW_BUILDER_PRIVATE_HPP_INCLUDED
#define GUI_AUXILIARY_WINDOW_BUILDER_PRIVATE_HPP_INCLUDED

#include "gui/auxiliary/window_builder.hpp"

#include "config.hpp"
#include "gui/auxiliary/window_builder/control.hpp"
#include "gui/widgets/menubar.hpp"
#include "gui/widgets/scrollbar_container.hpp"

namespace gui2 {

/**
 * A temporary helper class.
 *
 * @todo refactor with the grid builder.
 */
struct tbuilder_gridcell : public tbuilder_widget
{
	tbuilder_gridcell(const config& cfg);

	/** The flags for the cell. */
	unsigned flags;

	/** The bordersize for the cell. */
	unsigned border_size;

	/** The widgets for the cell. */
	tbuilder_widget_ptr widget;

	/** We're a dummy the building is done on construction. */
	twidget* build () const { return NULL; }
};

struct tbuilder_multi_page
	: public implementation::tbuilder_control
{

private:
	tbuilder_multi_page();
public:
	tbuilder_multi_page(const config& cfg);

	twidget* build () const;

	tbuilder_grid_ptr builder;

	/**
	 * Multi page data.
	 *
	 * Contains a vector with the data to set in every cell, it's used to
	 * serialize the data in the config, so the config is no longer required.
	 */
	std::vector<string_map> data;
};

struct tbuilder_vertical_scrollbar : public implementation::tbuilder_control
{
private:
	tbuilder_vertical_scrollbar();

public:
/*WIKI
 * @page = GUIWidgetInstanceWML
 * @order = 2_vertical_scrollbar
 *
 * == Vertical scrollbar ==
 *
 * A vertical scrollbar has no special fields.
 *
 */
	tbuilder_vertical_scrollbar(const config& cfg) :
		implementation::tbuilder_control(cfg)
	{}

	twidget* build () const;
};

} // namespace gui2

#endif

