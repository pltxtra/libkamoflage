/*
 * Kamoflage, ANDROID version
 * Copyright (C) 2007 by Anton Persson & Ted Björling
 * Copyright (C) 2010 by Anton Persson
 * Copyright (C) 2016 by Anton Persson
 *
 * http://www.733kru.org/
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License version 2; see COPYING for the complete License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <jni.h>
#include <android/log.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <VG/openvg.h>
#include <VG/vgext.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#include "kamogui.hh"
#include "libsvg/svgint.h"

#define __DO_KAMOFLAGE_DEBUG
#include "kamoflage_debug.hh"

/********************************************************
 *
 *           gnuVGcanvas helpers
 *
 ********************************************************/

void KammoGUI::GnuVGCanvas::set_paint(VGfloat *__rgba, const svg_paint_t* paint) {
	switch(paint->type) {
	case SVG_PAINT_TYPE_NONE:
		__rgba[3] = 0.0f;
		break;
	case SVG_PAINT_TYPE_COLOR:
		if(paint->p.color.is_current_color) {
			__rgba[0] = state->rgba[0];
			__rgba[1] = state->rgba[1];
			__rgba[2] = state->rgba[2];
			__rgba[3] = state->rgba[3];
		} else {
			VGuint c;
			VGfloat c_f;
			c = (paint->p.color.rgb & 0xff0000) >> 16;
			c_f = c;
			__rgba[0] = c_f / 255.0;

			c = (paint->p.color.rgb & 0x00ff00) >> 8;
			c_f = c;
			__rgba[1] = c_f / 255.0;

			c = (paint->p.color.rgb & 0x0000ff) >> 0;
			c_f = c;
			__rgba[2] = c_f / 255.0;
		}

		break;
	case SVG_PAINT_TYPE_GRADIENT:
		break;
	case SVG_PAINT_TYPE_PATTERN:
		break;
	}

}

void KammoGUI::GnuVGCanvas::length_to_pixel(svg_length_t *length, VGfloat* pixel) {
	VGfloat width, height;

	VGfloat lenval = (VGfloat)(length->value);

	switch (length->unit) {
	case SVG_LENGTH_UNIT_PX:
		*pixel = lenval;
		break;
	case SVG_LENGTH_UNIT_CM:
		*pixel = (lenval / 2.54) * DPI;
		break;
	case SVG_LENGTH_UNIT_MM:
		*pixel = (lenval / 25.4) * DPI;
		break;
	case SVG_LENGTH_UNIT_IN:
		*pixel = lenval * DPI;
		break;
	case SVG_LENGTH_UNIT_PT:
		*pixel = (lenval / 72.0) * DPI;
		break;
	case SVG_LENGTH_UNIT_PC:
		*pixel = (lenval / 6.0) * DPI;
		break;
	case SVG_LENGTH_UNIT_EM:
		*pixel = lenval * state->font_size;
		break;
	case SVG_LENGTH_UNIT_EX:
		*pixel = lenval * state->font_size / 2.0;
		break;
	case SVG_LENGTH_UNIT_PCT:
		width = state->viewport_width;
		height = state->viewport_height;

		if (length->orientation == SVG_LENGTH_ORIENTATION_HORIZONTAL)
			*pixel = (lenval / 100.0) * width;
		else if (length->orientation == SVG_LENGTH_ORIENTATION_VERTICAL)
			*pixel = (lenval / 100.0) * height;
		else
			*pixel = (lenval / 100.0) * sqrt(pow(width, 2) + pow(height, 2)) * sqrt(2);
		break;
	default:
		*pixel = lenval;
	}
}

/********************************************************
 *
 *           gnuVGcanvas::State
 *
 ********************************************************/

void KammoGUI::GnuVGCanvas::State::init_by_copy(const KammoGUI::GnuVGCanvas::State* original) {
	for(int i = 0; i < 9; i++) {
		matrix[i] = original->matrix[i];
	}
	for(int i = 0; i < 4; i++) {
		fill_rgba[i] = original->fill_rgba[i];
		stroke_rgba[i] = original->stroke_rgba[i];
		rgba[i] = original->rgba[i];
	}

	fill_rule = original->fill_rule;
	paint_modes = original->paint_modes;

	stroke_width.unit = original->stroke_width.unit;
	stroke_width.value = original->stroke_width.value;
	line_cap = original->line_cap;
	line_join = original->line_join;
	miter_limit = original->miter_limit;
	dash = original->dash;
	dash_phase = original->dash_phase;

	pathSeg.clear();
	pathData.clear();
}

void KammoGUI::GnuVGCanvas::State::init_fresh() {
	VGfloat identity[] = {
		1.0, 0.0, 0.0,
		0.0, 1.0, 0.0,
		0.0, 0.0, 1.0
	};
	for(int i = 0; i < 9; i++)
		matrix[i] = identity[i];

	// black
	rgba[0] = fill_rgba[0] = stroke_rgba[0] = 0;
	rgba[1] = fill_rgba[1] = stroke_rgba[1] = 0;
	rgba[2] = fill_rgba[2] = stroke_rgba[2] = 0;
	rgba[3] = fill_rgba[3] = stroke_rgba[3] = 255;

	fill_rule = VG_EVEN_ODD;
	paint_modes = 0;

	line_cap = SVG_STROKE_LINE_CAP_BUTT;
	line_join = SVG_STROKE_LINE_JOIN_MITER;
	miter_limit = 4.0f;
	stroke_width.value = 1.0f;
	stroke_width.unit = SVG_LENGTH_UNIT_PX;
	stroke_width.orientation = SVG_LENGTH_ORIENTATION_OTHER;

	dash.clear();
	dash_phase = 0.0f;

	pathSeg.clear();
	pathData.clear();
}

/********************************************************
 *
 *           gnuVGcanvas - internals
 *
 ********************************************************/

void KammoGUI::GnuVGCanvas::stack_push() {
	State* new_state;
	if(state_unused.size() == 0) {
		new_state = new State();
	} else {
		new_state = state_unused.top();
		state_unused.pop();
	}

	if(state_stack.size() != 0) {
		new_state->init_by_copy(state_stack.top());
	} else {
		new_state->init_fresh();
	}

	state_stack.push(new_state);
	state = new_state;
}

void KammoGUI::GnuVGCanvas::stack_pop() {
	if(state_stack.size() <= 1)
		throw GnuVGStateStackEmpty(get_id());

	state_unused.push(state_stack.top());
	state_stack.pop();
	state = state_stack.top();
}

void KammoGUI::GnuVGCanvas::use_state_on_top() {
	vgSeti(VG_FILL_RULE, state->fill_rule);
	vgSetParameterfv(fill, VG_PAINT_COLOR, 4, state->fill_rgba);

	vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
	vgLoadMatrix(state->matrix);

	if(state->dash.size() > 0) {
		vgSetfv(VG_STROKE_DASH_PATTERN, state->dash.size(), state->dash.data());
		vgSetf(VG_STROKE_DASH_PHASE, state->dash_phase);
	} else
		vgSetfv(VG_STROKE_DASH_PATTERN, 0, nullptr);

	vgSetParameterfv(stroke, VG_PAINT_COLOR, 4, state->stroke_rgba);
	VGfloat width;
	length_to_pixel(&state->stroke_width, &width);
	vgSetf(VG_STROKE_LINE_WIDTH, (VGfloat)width);
	vgSetf(VG_STROKE_MITER_LIMIT, state->miter_limit);
	switch (state->line_cap) {
	case SVG_STROKE_LINE_CAP_BUTT:
		vgSeti(VG_STROKE_CAP_STYLE, VG_CAP_BUTT);
		break;
	case SVG_STROKE_LINE_CAP_ROUND:
		vgSeti(VG_STROKE_CAP_STYLE, VG_CAP_ROUND);
		break;
	case SVG_STROKE_LINE_CAP_SQUARE:
		vgSeti(VG_STROKE_CAP_STYLE, VG_CAP_SQUARE);
		break;
	}
	switch (state->line_join) {
	case SVG_STROKE_LINE_JOIN_MITER:
		vgSeti(VG_STROKE_JOIN_STYLE, VG_JOIN_MITER);
		break;
	case SVG_STROKE_LINE_JOIN_ROUND:
		vgSeti(VG_STROKE_JOIN_STYLE, VG_JOIN_ROUND);
		break;
	case SVG_STROKE_LINE_JOIN_BEVEL:
		vgSeti(VG_STROKE_JOIN_STYLE, VG_JOIN_BEVEL);
		break;
	}

}

/********************************************************
 *
 *           gnuVGcanvas - libsvg backend
 *
 ********************************************************/

/* hierarchy */
svg_status_t KammoGUI::GnuVGCanvas::begin_group(void* closure, double opacity) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->stack_push();

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::begin_element(void* closure, void *path_cache) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->stack_push();

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::end_element(void* closure) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->stack_pop();

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::end_group(void* closure, double opacity) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->stack_pop();

	return SVG_STATUS_SUCCESS;
}

/* path creation */
svg_status_t KammoGUI::GnuVGCanvas::move_to(void* closure, double x, double y) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->state->pathSeg.push_back(VG_MOVE_TO_ABS);
	context->state->pathData.push_back((VGfloat)x);
	context->state->pathData.push_back((VGfloat)y);

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::line_to(void* closure, double x, double y) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->state->pathSeg.push_back(VG_LINE_TO);
	context->state->pathData.push_back((VGfloat)x);
	context->state->pathData.push_back((VGfloat)y);

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::curve_to(void* closure,
					     double x1, double y1,
					     double x2, double y2,
					     double x3, double y3) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->state->pathSeg.push_back(VG_CUBIC_TO);
	context->state->pathData.push_back((VGfloat)x1);
	context->state->pathData.push_back((VGfloat)y1);
	context->state->pathData.push_back((VGfloat)x2);
	context->state->pathData.push_back((VGfloat)y2);
	context->state->pathData.push_back((VGfloat)x3);
	context->state->pathData.push_back((VGfloat)y3);

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::quadratic_curve_to(void* closure,
						       double x1, double y1,
						       double x2, double y2) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->state->pathSeg.push_back(VG_QUAD_TO);
	context->state->pathData.push_back((VGfloat)x1);
	context->state->pathData.push_back((VGfloat)y1);
	context->state->pathData.push_back((VGfloat)x2);
	context->state->pathData.push_back((VGfloat)y2);

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::arc_to(void* closure,
					   double	rx,
					   double	ry,
					   double	x_axis_rotation,
					   int	large_arc_flag,
					   int	sweep_flag,
					   double	x,
					   double	y) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	VGubyte arc_type;
	if(large_arc_flag) {
		arc_type = sweep_flag ? VG_LCWARC_TO : VG_LCCWARC_TO;
	} else {
		arc_type = sweep_flag ? VG_SCWARC_TO : VG_SCCWARC_TO;
	}
	context->state->pathSeg.push_back(arc_type);
	context->state->pathData.push_back((VGfloat)rx);
	context->state->pathData.push_back((VGfloat)ry);
	context->state->pathData.push_back((VGfloat)x_axis_rotation);
	context->state->pathData.push_back((VGfloat)x);
	context->state->pathData.push_back((VGfloat)y);

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::close_path(void* closure) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->state->pathSeg.push_back(VG_CLOSE_PATH);

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::free_path_cache(void* closure, void **path_cache) {
	VGPath pthc = (VGPath)(*path_cache);
	if(pthc != VG_INVALID_HANDLE) {
		vgDestroyPath(pthc);
	}

	return SVG_STATUS_SUCCESS;
}

/* style */
svg_status_t KammoGUI::GnuVGCanvas::set_color(void* closure, const svg_color_t *color) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	VGuint c;
	VGfloat c_f;
	c = (color->rgb & 0xff0000) >> 16;
	c_f = c;
	context->state->rgba[0] = c_f / 255.0;

	c = (color->rgb & 0x00ff00) >> 8;
	c_f = c;
	context->state->rgba[1] = c_f / 255.0;

	c = (color->rgb & 0x0000ff) >> 0;
	c_f = c;
	context->state->rgba[2] = c_f / 255.0;

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_fill_opacity(void* closure, double fill_opacity) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->state->fill_rgba[3] = ((VGfloat)fill_opacity) * context->state->rgba[3];

	if(fill_opacity > 0.0)
		context->state->paint_modes |= VG_FILL_PATH;
	else
		context->state->paint_modes &= ~VG_FILL_PATH;

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_fill_paint(void* closure, const svg_paint_t *paint) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->set_paint(context->state->fill_rgba, paint);

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_fill_rule(void* closure, svg_fill_rule_t fill_rule) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->state->fill_rule = fill_rule == SVG_FILL_RULE_EVEN_ODD ? VG_EVEN_ODD : VG_NON_ZERO;

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_font_family(void* closure, const char *family) {
	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_font_size(void* closure, double size) {
	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_font_style(void* closure, svg_font_style_t font_style) {
	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_font_weight(void* closure, unsigned int font_weight) {
	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_opacity(void* closure, double opacity) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->state->rgba[3] = opacity;

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_stroke_dash_array(void* closure, double *dash_array, int num_dashes) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->state->dash.clear();
	if (num_dashes) {
		for(int i = 0; i < num_dashes; i++)
			context->state->dash.push_back((VGfloat)dash_array[i]);
	}

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_stroke_dash_offset(void* closure, svg_length_t *offset_len) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	VGfloat offset;
	context->length_to_pixel(offset_len, &offset);
	context->state->dash_phase = (VGfloat)offset;

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_stroke_line_cap(void* closure, svg_stroke_line_cap_t line_cap) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->state->line_cap = line_cap;

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_stroke_line_join(void* closure, svg_stroke_line_join_t line_join) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->state->line_join = line_join;

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_stroke_miter_limit(void* closure, double limit) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->state->miter_limit = (VGfloat)limit;

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_stroke_opacity(void* closure, double stroke_opacity) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->state->stroke_rgba[3] = ((VGfloat)stroke_opacity) * context->state->rgba[3];

	if(stroke_opacity > 0.0)
		context->state->paint_modes |= VG_STROKE_PATH;
	else
		context->state->paint_modes &= ~VG_STROKE_PATH;

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_stroke_paint(void* closure, const svg_paint_t *paint) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->set_paint(context->state->stroke_rgba, paint);

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_stroke_width(void* closure, svg_length_t *width) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	context->state->stroke_width = *width;

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_text_anchor(void* closure, svg_text_anchor_t text_anchor) {
	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_filter(void* closure, const char* id) {
	return SVG_STATUS_SUCCESS;
}

/* filter */
svg_status_t KammoGUI::GnuVGCanvas::begin_filter(void* closure, const char* id) {
	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::add_filter_feBlend(void* closure,
						       svg_length_t* x, svg_length_t* y,
						       svg_length_t* width, svg_length_t* height,
						       svg_filter_in_t in, int in_op_reference,
						       svg_filter_in_t in2, int in2_op_reference,
						       feBlendMode_t mode) {
	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::add_filter_feComposite(void* closure,
							   svg_length_t* x, svg_length_t* y,
							   svg_length_t* width, svg_length_t* height,
							   feCompositeOperator_t oprt,
							   svg_filter_in_t in, int in_op_reference,
							   svg_filter_in_t in2, int in2_op_reference,
							   double k1, double k2, double k3, double k4) {
	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::add_filter_feFlood(void* closure,
						       svg_length_t* x, svg_length_t* y,
						       svg_length_t* width, svg_length_t* height,
						       svg_filter_in_t in, int in_op_reference,
						       const svg_color_t* color, double opacity) {
	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::add_filter_feGaussianBlur(void* closure,
							      svg_length_t* x, svg_length_t* y,
							      svg_length_t* width, svg_length_t* height,
							      svg_filter_in_t in, int in_op_reference,
							      double std_dev_x, double std_dev_y) {
	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::add_filter_feOffset(void* closure,
							svg_length_t* x, svg_length_t* y,
							svg_length_t* width, svg_length_t* height,
							svg_filter_in_t in, int in_op_reference,
							double dx, double dy) {
	return SVG_STATUS_SUCCESS;
}


/* transform */
svg_status_t KammoGUI::GnuVGCanvas::apply_clip_box(void* closure,
						   svg_length_t *x,
						   svg_length_t *y,
						   svg_length_t *width,
						   svg_length_t *height) {
	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::transform(void* closure,
					      double xx, double yx,
					      double xy, double yy,
					      double x0, double y0) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;

	VGfloat matrix[9] = {
		(VGfloat)xx, (VGfloat)xy, (VGfloat)x0,
		(VGfloat)yx, (VGfloat)yy, (VGfloat)y0,
		       0.0f,        0.0f,        1.0f
	};

	vgLoadMatrix(context->state->matrix);
	vgMultMatrix(matrix);
	vgGetMatrix(context->state->matrix);

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::apply_view_box(void* closure,
						   svg_view_box_t view_box,
						   svg_length_t *width,
						   svg_length_t *height) {
	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::set_viewport_dimension(void* closure,
							   svg_length_t *width,
							   svg_length_t *height) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;
	context->length_to_pixel(width, &(context->state->viewport_width));
	context->length_to_pixel(height, &(context->state->viewport_height));
	return SVG_STATUS_SUCCESS;
}

/* drawing */
svg_status_t KammoGUI::GnuVGCanvas::render_line(void* closure,
						svg_length_t *x1,
						svg_length_t *y1,
						svg_length_t *x2,
						svg_length_t *y2) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;
	context->use_state_on_top();

	VGubyte cmds[] = {VG_MOVE_TO_ABS, VG_LINE_TO_ABS};
	VGfloat data[4];
	context->length_to_pixel(x1, &data[0]);
	context->length_to_pixel(y1, &data[1]);
	context->length_to_pixel(x2, &data[2]);
	context->length_to_pixel(x2, &data[3]);

	vgClearPath(context->temporary_path, VG_PATH_CAPABILITY_ALL);
	vgAppendPathData(context->temporary_path, 2, cmds, data);
	vgDrawPath(context->temporary_path, context->state->paint_modes);

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::render_path(void* closure, void **path_cache) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;
	context->use_state_on_top();

	vgClearPath(context->temporary_path, VG_PATH_CAPABILITY_ALL);
	vgAppendPathData(context->temporary_path,
			 context->state->pathSeg.size(),
			 context->state->pathSeg.data(),
			 context->state->pathData.data());
	vgDrawPath(context->temporary_path, context->state->paint_modes);

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::render_ellipse(void* closure,
						   svg_length_t *cx,
						   svg_length_t *cy,
						   svg_length_t *rx,
						   svg_length_t *ry) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;
	context->use_state_on_top();

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::render_rect(void* closure,
						svg_length_t *x,
						svg_length_t *y,
						svg_length_t *width,
						svg_length_t *height,
						svg_length_t *rx,
						svg_length_t *ry) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;
	context->use_state_on_top();

	VGubyte cmds[] = {VG_MOVE_TO_ABS,
			  VG_LINE_TO,
			  VG_LINE_TO,
			  VG_LINE_TO,
			  VG_CLOSE_PATH
	};

	VGfloat _x, _y, _w, _h;
	context->length_to_pixel(x, &_x);
	context->length_to_pixel(y, &_y);
	context->length_to_pixel(width, &_w);
	context->length_to_pixel(height, &_h);

	VGfloat data[] = {
		_x, _y,
		_w, 0.0,
		0.0, _h,
		-_w, 0.0
	};

	vgClearPath(context->temporary_path, VG_PATH_CAPABILITY_ALL);
	vgAppendPathData(context->temporary_path, 5, cmds, data);
	vgDrawPath(context->temporary_path, context->state->paint_modes);

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::render_text(void* closure,
						svg_length_t *x,
						svg_length_t *y,
						const char   *utf8) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;
	context->use_state_on_top();

	return SVG_STATUS_SUCCESS;
}

svg_status_t KammoGUI::GnuVGCanvas::render_image(void* closure,
						 unsigned char *data,
						 unsigned int	 data_width,
						 unsigned int	 data_height,
						 svg_length_t	 *x,
						 svg_length_t	 *y,
						 svg_length_t	 *width,
						 svg_length_t	 *height) {
	GnuVGCanvas* context = (GnuVGCanvas*)closure;
	context->use_state_on_top();

	return SVG_STATUS_SUCCESS;
}


/* get bounding box of last drawing, in pixels - returns 0 if bounding box is outside the visible clip, non-0 if inside the visible clip */
int KammoGUI::GnuVGCanvas::get_last_bounding_box(void *closure, svg_bounding_box_t *bbox) {
	return SVG_STATUS_SUCCESS;
}


/********************************************************
 *
 *           gnuVGcanvas - Kamoflage interface
 *
 ********************************************************/

static void printGLString(const char *name, GLenum s) {
	const char *v = (const char *) glGetString(s);
	KAMOFLAGE_DEBUG("GL %s = %s\n", name, v);
}

static void checkGlError(const char* op) {
	for (GLint error = glGetError(); error; error
		     = glGetError()) {
		KAMOFLAGE_ERROR("after %s() glError (0x%x)\n", op, error);
	}
}

KammoGUI::GnuVGCanvas::GnuVGCanvas(std::string _id, jobject jobj) :
	Widget(_id, jobj) {
	gnuVGctx = gnuVG_create_context();

	KAMOFLAGE_DEBUG("KammoGUI::GnuVGCanvas::GnuVGCanvas(%s)\n", _id.c_str());
	stack_push(); // push initial state

	stroke = vgCreatePaint();
	fill = vgCreatePaint();
	vgSetPaint(stroke, VG_STROKE_PATH);
	vgSetPaint(fill, VG_FILL_PATH);
	temporary_path = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F,
				      1,0,0,0, VG_PATH_CAPABILITY_ALL);

}

KammoGUI::GnuVGCanvas::~GnuVGCanvas() {
}

void KammoGUI::GnuVGCanvas::init(JNIEnv *env) {
	printGLString("Version", GL_VERSION);
	printGLString("Vendor", GL_VENDOR);
	printGLString("Renderer", GL_RENDERER);
	printGLString("Extensions", GL_EXTENSIONS);
}

void KammoGUI::GnuVGCanvas::resize(JNIEnv *env, int width, int height) {
	KAMOFLAGE_DEBUG("GnuVGCanvas::resize(%d, %d)", width, height);

	glViewport(0, 0, width, height);
	checkGlError("glViewport");

	gnuVG_use_context(gnuVGctx);
	gnuVG_resize(width, height);
	window_width = width;
	window_height = height;
}

void KammoGUI::GnuVGCanvas::step(JNIEnv *env) {
	gnuVG_use_context(gnuVGctx);

	VGfloat clearColor[] = {1,1,1,1};
	vgSetfv(VG_CLEAR_COLOR, 4, clearColor);
	vgClear(0, 0, window_width, window_height);
}

extern "C" {
	JNIEXPORT void Java_com_toolkits_kamoflage_gnuVGView_init
	(JNIEnv *env, jclass jcl, jlong nativeID) {
		KammoGUI::GnuVGCanvas *cnv =
			(KammoGUI::GnuVGCanvas *)nativeID;
		cnv->init(env);
	}

	JNIEXPORT void Java_com_toolkits_kamoflage_gnuVGView_resize
	(JNIEnv *env, jclass jcl, jlong nativeID, jint width, jint height) {
		KammoGUI::GnuVGCanvas *cnv =
			(KammoGUI::GnuVGCanvas *)nativeID;
		cnv->resize(env, width, height);
	}

	JNIEXPORT void Java_com_toolkits_kamoflage_gnuVGView_step
	(JNIEnv *env, jclass jcl, jlong nativeID) {
		KammoGUI::GnuVGCanvas *cnv =
			(KammoGUI::GnuVGCanvas *)nativeID;
		cnv->step(env);
	}
}