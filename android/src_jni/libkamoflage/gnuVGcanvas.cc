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
#include <VG/vgu.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sstream>

#include "kamogui.hh"
#include "gnuVGcanvas.hh"
#include "libsvg/svgint.h"

//#define __DO_KAMOFLAGE_DEBUG
#include "kamoflage_debug.hh"

#define ENABLE_GNUVG_PROFILER
#include <VG/gnuVG_profiler.hh>

GNUVG_DECLARE_PROFILER_PROBE(full_frame);
GNUVG_DECLARE_PROFILER_PROBE(use_state_on_top);
GNUVG_DECLARE_PROFILER_PROBE(prepare_context);
GNUVG_DECLARE_PROFILER_PROBE(regenerate_scissors);
GNUVG_DECLARE_PROFILER_PROBE(render_tempath);
GNUVG_DECLARE_PROFILER_PROBE(render_text);
GNUVG_DECLARE_PROFILER_PROBE(render_path_cache);

namespace KammoGUI {

/*************************
 *
 *    Exception classes
 *
 *************************/
	GnuVGCanvas::NoSuchElementException::NoSuchElementException(const std::string &id)
		: jException(id + " : element does not exist.", jException::sanity_error) {}
	GnuVGCanvas::OperationFailedException::OperationFailedException()
		: jException("GnuVGCanvas - operation failed.", jException::sanity_error) {}

/*************************
 *
 *   Filter primitives implementation
 *
 *************************/

	void GnuVG_feOperation::unref_result(VGImageAllocator* vgallocator) {
		KAMOFLAGE_ERROR("feOperation::unref_result() for %p => %d\n",
				this, ref_count);
		if((--ref_count) == 0) {
			KAMOFLAGE_ERROR("feOperation for %p will recycle %p...\n",
					this, (void *)result);
			vgallocator->recycle_bitmap(result);
			result = VG_INVALID_HANDLE;
		}
	}

	VGImage GnuVG_feOperation::get_image(
		VGImageAllocator* vgallocator,
		std::vector<GnuVG_feOperation*> &fe_ops,
		VGImage sourceGraphic, VGImage backgroundImage,
		svg_filter_in_t in, int in_op_reference
		) {
		switch(in) {

		case in_SourceGraphic:
			KAMOFLAGE_ERROR("get SourceGraphic\n");
			return sourceGraphic;
		case in_BackgroundGraphic:
			KAMOFLAGE_ERROR("get BackgroundGraphic\n");
			return backgroundImage;

		case in_Reference:
			KAMOFLAGE_ERROR("get graphic by reference. %p\n",
				fe_ops[in_op_reference]);
			return fe_ops[in_op_reference]->get_result(
				vgallocator,
				fe_ops,
				sourceGraphic, backgroundImage
				);

		case in_BackgroundAlpha:
		case in_FillPaint:
		case in_StrokePaint:
		case in_SourceAlpha:
			break;
		}
		return VG_INVALID_HANDLE;
	}

	void GnuVG_feOperation::recycle_image(
		VGImageAllocator* vgallocator,
		std::vector<GnuVG_feOperation*> &fe_ops,
		svg_filter_in_t in, int in_op_reference) {
		if(in == in_Reference) {
			KAMOFLAGE_ERROR("Recycle by reference... %p\n",
				fe_ops[in_op_reference]);
				fe_ops[in_op_reference]->unref_result(vgallocator);
		}
	}

	VGImage GnuVG_feBlend::execute(
		VGImageAllocator* vgallocator,
		std::vector<GnuVG_feOperation*> &fe_ops,
		VGImage sourceGraphic, VGImage backgroundImage
		) {
		auto img_1 = get_image(vgallocator, fe_ops,
				       sourceGraphic, backgroundImage,
				       in, in_op_reference);
		auto img_2 = get_image(vgallocator, fe_ops,
				       sourceGraphic, backgroundImage,
				       in2, in2_op_reference);

		auto retval = vgallocator->get_fresh_bitmap();

		KAMOFLAGE_ERROR("feBlend::execute for %p, (%p, %p) -> %p\n",
				this,
				(void *)img_1, (void *)img_2,
				(void *)retval);
		KAMOFLAGE_ERROR("feBlend %p - %d, %d\n",
				this,
				ref_count, expected_ref_count);
		gnuvgRenderToImage(retval);

		vgSeti(VG_MATRIX_MODE,
		       VG_MATRIX_IMAGE_USER_TO_SURFACE);
		vgLoadIdentity();

		int old_blend_mode = vgGeti(VG_BLEND_MODE);

		switch(mode) {
		case feBlend_normal:
			vgSeti(VG_BLEND_MODE, VG_BLEND_SRC);
			vgDrawImage(img_2);
			vgSeti(VG_BLEND_MODE, VG_BLEND_SRC_OVER);
			vgDrawImage(img_1);
			break;
		}
			vgSeti(VG_BLEND_MODE, old_blend_mode);

		KAMOFLAGE_ERROR("Restored blend mode: %d (should be %d)\n",
				old_blend_mode, VG_BLEND_SRC_OVER);

		recycle_image(vgallocator, fe_ops,
			      in2, in2_op_reference);
		recycle_image(vgallocator, fe_ops,
			      in, in_op_reference);

		return retval;
	}

	VGImage GnuVG_feComposite::execute(
		VGImageAllocator* vgallocator,
		std::vector<GnuVG_feOperation*> &fe_ops,
		VGImage sourceGraphic, VGImage backgroundImage
		) {
		auto img_1 = get_image(vgallocator, fe_ops,
				       sourceGraphic, backgroundImage,
				       in, in_op_reference);
		auto img_2 = get_image(vgallocator, fe_ops,
				       sourceGraphic, backgroundImage,
				       in2, in2_op_reference);

		auto retval = vgallocator->get_fresh_bitmap();

		KAMOFLAGE_ERROR("feCompiste::execute for %p, (%p, %p) -> %p\n",
				this,
				(void *)img_1, (void *)img_2,
				(void *)retval);
		KAMOFLAGE_ERROR("feComposite %p - %d, %d\n",
				this,
				ref_count, expected_ref_count);
		gnuvgRenderToImage(retval);

		vgSeti(VG_MATRIX_MODE,
		       VG_MATRIX_IMAGE_USER_TO_SURFACE);
		vgLoadIdentity();

		int old_blend_mode = vgGeti(VG_BLEND_MODE);

		switch(oprt) {
		case feComposite_in:
			vgSeti(VG_BLEND_MODE, VG_BLEND_SRC);
			vgDrawImage(img_2);
			vgSeti(VG_BLEND_MODE, VG_BLEND_SRC_IN);
			vgDrawImage(img_1);
			break;

		case feComposite_out:
//			xxx implement med color conversion matrix to invert dst alpha values first
			break;


		case feComposite_over:
			vgSeti(VG_BLEND_MODE, VG_BLEND_SRC);
			vgDrawImage(img_2);
			vgSeti(VG_BLEND_MODE, VG_BLEND_SRC_OVER);
			vgDrawImage(img_1);
			break;

		case feComposite_atop:
//			tricky
			break;
		}

		vgSeti(VG_BLEND_MODE, old_blend_mode);

		KAMOFLAGE_ERROR("Restored blend mode: %d (should be %d)\n",
				old_blend_mode, VG_BLEND_SRC_OVER);

		recycle_image(vgallocator, fe_ops,
			      in2, in2_op_reference);
		recycle_image(vgallocator, fe_ops,
			      in, in_op_reference);

		return retval;
	}

	VGImage GnuVG_feFlood::execute(
		VGImageAllocator* vgallocator,
		std::vector<GnuVG_feOperation*> &fe_ops,
		VGImage sourceGraphic, VGImage backgroundImage
		) {
		auto retval = vgallocator->get_fresh_bitmap();
		KAMOFLAGE_ERROR("feFlood::execute for %p to %p\n",
				(void *)this,
				(void *)retval);

		gnuvgRenderToImage(retval);
		vgSeti(VG_MATRIX_MODE,
		       VG_MATRIX_IMAGE_USER_TO_SURFACE);
		vgLoadIdentity();

		VGint w, h;
		w = vgGetParameteri(retval, VG_IMAGE_WIDTH);
		h = vgGetParameteri(retval, VG_IMAGE_HEIGHT);

		if(color.is_current_color)
			KAMOFLAGE_ERROR("GnuVG_feFlood does not implement support for current color.");

		KAMOFLAGE_ERROR("feFlood(%d, %d) => %f, %f, %f, %f\n",
			    w, h,
			    clear_color[0],
			    clear_color[1],
			    clear_color[2],
			    clear_color[3]
			);

		vgSetfv(VG_CLEAR_COLOR, 4, clear_color);
		vgClear(0, 0, w, h);

		return retval;
	}

	VGImage GnuVG_feGaussianBlur::execute(
			VGImageAllocator* vgallocator,
			std::vector<GnuVG_feOperation*> &fe_ops,
			VGImage sourceGraphic, VGImage backgroundImage
		) {
		KAMOFLAGE_ERROR("feGaussianBlur::execute\n");
		auto img_1 = get_image(vgallocator, fe_ops,
				       sourceGraphic, backgroundImage,
				       in, in_op_reference);

		auto retval = vgallocator->get_fresh_bitmap();

		gnuvgRenderToImage(retval);
		vgSeti(VG_MATRIX_MODE,
		       VG_MATRIX_IMAGE_USER_TO_SURFACE);
		vgLoadIdentity();

		VGint w, h;
		w = vgGetParameteri(retval, VG_IMAGE_WIDTH);
		h = vgGetParameteri(retval, VG_IMAGE_HEIGHT);

		VGfloat clear_color[] = {0.0f, 0.0f, 0.0f, 0.0f};
		KAMOFLAGE_ERROR("feGaussianBlur(%d, %d)\n",
			    w, h
			);
		vgSetfv(VG_CLEAR_COLOR, 4, clear_color);
		vgClear(0, 0, w, h);

		vgGaussianBlur(VG_INVALID_HANDLE, img_1,
			       std_dev_x, std_dev_y,
			       VG_TILE_FILL);

		recycle_image(vgallocator, fe_ops,
			      in, in_op_reference);

		return retval;
	}

	VGImage GnuVG_feOffset::execute(
		VGImageAllocator* vgallocator,
		std::vector<GnuVG_feOperation*> &fe_ops,
		VGImage sourceGraphic, VGImage backgroundImage
		) {
		KAMOFLAGE_ERROR("feOffset::execute\n");
		auto img_1 = get_image(vgallocator, fe_ops,
				       sourceGraphic, backgroundImage,
				       in, in_op_reference);

		auto retval = vgallocator->get_fresh_bitmap();

		gnuvgRenderToImage(retval);
		vgSeti(VG_MATRIX_MODE,
		       VG_MATRIX_IMAGE_USER_TO_SURFACE);
		vgLoadIdentity();
		vgTranslate(dx, -dy);

		VGint w, h;
		w = vgGetParameteri(retval, VG_IMAGE_WIDTH);
		h = vgGetParameteri(retval, VG_IMAGE_HEIGHT);

		VGfloat clear_color[] = {0.0f, 0.0f, 0.0f, 0.0f};
		KAMOFLAGE_ERROR("feOffset(%d, %d)\n",
			    w, h
			);
		vgSetfv(VG_CLEAR_COLOR, 4, clear_color);
		vgClear(0, 0, w, h);

		int old_blend_mode = vgGeti(VG_BLEND_MODE);
		vgSeti(VG_BLEND_MODE, VG_BLEND_SRC);
		vgDrawImage(img_1);
		vgSeti(VG_BLEND_MODE, old_blend_mode);

		recycle_image(vgallocator, fe_ops,
			      in, in_op_reference);

		return retval;
	}

	VGImage GnuVGFilter::execute(VGImageAllocator* vgallocator,
				     VGImage sourceGraphic, VGImage backgroundImage) {
			// never forget to return to previous render target
			auto old_target = gnuvgGetRenderTarget();
			auto cleanup = finally(
				[old_target] {
					KAMOFLAGE_ERROR("Render filter to old target: %p\n",
						    (void *)old_target);
					gnuvgRenderToImage(old_target);
				}
				);

			// OK, let's get down to business
			if(last_operation) {
				for(auto op : fe_operations)
					op->ref_count = op->expected_ref_count;
				auto result = last_operation->get_result(
					vgallocator,
					fe_operations,
					sourceGraphic, backgroundImage
					);
				last_operation->result = VG_INVALID_HANDLE;
				KAMOFLAGE_ERROR("Result from last_operation %p is %p\n",
						last_operation, (void *)result);
				return result;
			}
			return VG_INVALID_HANDLE;
		}


/*************************
 *
 *   MotionEvent implementation
 *
 *************************/
	void GnuVGCanvas::MotionEvent::init(long _downTime,
					    long _eventTime,
					    motionEvent_t _action,
					    int _pointerCount, int _actionIndex,
					    float rawX, float rawY) {
		down_time = _downTime;
		event_time = _eventTime;
		action = _action;
		pointer_count = _pointerCount > 16 ? 16 : _pointerCount;
		action_index = _actionIndex;
		raw_x = rawX;
		raw_y = rawY;
	}
	void GnuVGCanvas::MotionEvent::clone(const MotionEvent &source) {
		init(source.get_down_time(),
		     source.get_event_time(),
		     source.get_action(),
		     source.get_pointer_count(),
		     source.get_action_index(),
		     source.get_raw_x(),
		     source.get_raw_y());

		int k;
		for(k = 0; k < source.get_pointer_count(); k++) {
			init_pointer(k, source.get_pointer_id(k), source.get_x(k), source.get_y(k), source.get_pressure(k));
		}
	}

	void GnuVGCanvas::MotionEvent::init_pointer(int index, int id, float x, float y, float pressure)  {
		if(index >= pointer_count) return;
		pointer_id[index] = id;
		pointer_x[index] = x;
		pointer_y[index] = y;
		pointer_pressure[index] = pressure;
	}

	long GnuVGCanvas::MotionEvent::get_down_time() const {
		return down_time;
	}

	long GnuVGCanvas::MotionEvent::get_event_time() const {
		return event_time;
	}

	GnuVGCanvas::MotionEvent::motionEvent_t GnuVGCanvas::MotionEvent::get_action() const {
		return action;
	}

	int GnuVGCanvas::MotionEvent::get_action_index() const {
		return action_index;
	}

	float GnuVGCanvas::MotionEvent::get_x() const {
		return pointer_x[action_index];
	}

	float GnuVGCanvas::MotionEvent::get_y() const {
		return pointer_y[action_index];
	}

	float GnuVGCanvas::MotionEvent::get_pressure() const {
		return pointer_pressure[action_index];
	}

	float GnuVGCanvas::MotionEvent::get_x(int index) const {
		if(index >= pointer_count) return 0.0f;
		return pointer_x[index];
	}

	float GnuVGCanvas::MotionEvent::get_y(int index) const {
		if(index >= pointer_count) return 0.0f;
		return pointer_y[index];
	}

	float GnuVGCanvas::MotionEvent::get_pressure(int index) const {
		if(index >= pointer_count) return 0.0f;
		return pointer_pressure[index];
	}

	float GnuVGCanvas::MotionEvent::get_raw_x() const {
		return raw_x;
	}

	float GnuVGCanvas::MotionEvent::get_raw_y() const {
		return raw_y;
	}

	int GnuVGCanvas::MotionEvent::get_pointer_count() const {
		return pointer_count;
	}

	int GnuVGCanvas::MotionEvent::get_pointer_id(int index) const {
		if(index >= pointer_count) return 0;
		return pointer_id[index];
	}

/*************************
 *
 *   SVGMatrix class implementation
 *
 *************************/

	GnuVGCanvas::SVGMatrix::SVGMatrix() : a(1.0), b(0.0), c(0.0), d(1.0), e(0.0), f(0.0) {
	}

	void GnuVGCanvas::SVGMatrix::init_identity() {
		a = 1.0; c = 0.0; e = 0.0;
		b = 0.0; d = 1.0; f = 0.0;
	}


	void GnuVGCanvas::SVGMatrix::translate(double x, double y) {
		e += x;
		f += y;
	}

	void GnuVGCanvas::SVGMatrix::scale(double x, double y) {
		a *= x;
		b *= y;
		c *= x;
		d *= y;
		e *= x;
		f *= y;
	}

	void GnuVGCanvas::SVGMatrix::rotate(double angle_in_radians) {
		double u, v, x, y;
		u = cos(angle_in_radians); v = -sin(angle_in_radians);
		x = sin(angle_in_radians); y =  cos(angle_in_radians);

		double A, B, C, D, E, F;
		A = u * a + v * b; C = u * c + v * d; E = u * e + v * f;
		B = x * a + y * b; D = x * c + y * d; F = x * e + y * f;

		a = A; c = C; e = E;
		b = B; d = D; f = F;
	}

	void GnuVGCanvas::SVGMatrix::multiply(const GnuVGCanvas::SVGMatrix &other) {
		double t1, t2, t3, t4, t5, t6;

		t1 = a * other.a + c * other.b;
		t2 = b * other.a + d * other.b;

		t3 = a * other.c + c * other.d;
		t4 = b * other.c + d * other.d;

		t5 = a * other.e + c * other.f + e;
		t6 = b * other.e + d * other.f + f;

		a = t1;
		b = t2;
		c = t3;
		d = t4;
		e = t5;
		f = t6;
	}

/********************************************************
 *
 *           gnuVGcanvas::SVGRect
 *
 ********************************************************/

	// return if they intersected
	void GnuVGCanvas::SVGRect::intersect_with(const SVGRect &other) {
		// our coordinates
		auto a_x1 = x;
		auto a_x2 = x + width;
		auto a_y1 = y;
		auto a_y2 = y + height;

		// other coordinates
		auto b_x1 = other.x;
		auto b_x2 = other.x + other.width;
		auto b_y1 = other.y;
		auto b_y2 = other.y + other.height;

		// check if non-intersecting
		if(b_x1 >= a_x2 || b_x2 <= a_x1 ||
		   b_y1 >= a_y2 || b_y2 <= a_y1) {
			x = y = width = height = 0.0;
			return;
		}

		// compute intersection
		auto f_x1 = a_x1 > b_x1 ? a_x1 : b_x1;
		auto f_x2 = a_x2 > b_x2 ? b_x2 : a_x2;
		auto f_y1 = a_y1 > b_y1 ? a_y1 : b_y1;
		auto f_y2 = a_y2 > b_y2 ? b_y2 : a_y2;

		x = f_x1; width = f_x2 - f_x1;
		y = f_y1; height = f_y2 - f_y1;
	}

/********************************************************
 *
 *           gnuVGcanvas::ElementReference
 *
 ********************************************************/

	GnuVGCanvas::ElementReference::ElementReference() : source(NULL), element(NULL) {
	}

	GnuVGCanvas::ElementReference::ElementReference(SVGDocument *_source, const std::string &id) {
		source = _source;

		(void) _svg_fetch_element_by_id (source->svg, id.c_str(), &element);
		if(element) {
			(void) _svg_element_reference (element);
		} else {
			throw NoSuchElementException(id);
		}
		KAMOFLAGE_DEBUG("ElementReference::ElementReference(%p) %s -> ref count before: %d\n", element, id.c_str(), element->ref_count);
	}

	GnuVGCanvas::ElementReference::ElementReference(const ElementReference *sibling, const std::string &id) {
		source = sibling->source;

		(void) _svg_fetch_element_by_id (source->svg, id.c_str(), &element);
		if(element) {
			(void) _svg_element_reference (element);
		} else {
			throw NoSuchElementException(id);
		}
		KAMOFLAGE_DEBUG("ElementReference::ElementReference(%p) %s -> ref count before: %d\n", element, id.c_str(), element->ref_count);
	}

	GnuVGCanvas::ElementReference::ElementReference(const ElementReference *original) {
		source = original->source;

		element = original->element;
		if(element) {
			(void) _svg_element_reference (element);
		} else {
			throw NoSuchElementException("id unknown.");
		}
	}
	GnuVGCanvas::ElementReference::ElementReference(const ElementReference &original) : ElementReference(&original) {}

	GnuVGCanvas::ElementReference& GnuVGCanvas::ElementReference::operator=(
		GnuVGCanvas::ElementReference&& a) {
		// dereference previous content
		dereference();

		// create new content
		source = std::move(a.source);
		element = std::move(a.element);
		event_handler = std::move(a.event_handler);
		if(element) {
			element->custom_data = this;
			a.element = NULL;
		}

		KAMOFLAGE_DEBUG("ElementReference move operator = : element pointer %p\n", element);

		return *this;
	}

	GnuVGCanvas::ElementReference::ElementReference(ElementReference &&original)
		: source(0)
		, element(0)
	{
		KAMOFLAGE_DEBUG("Move constructor.\n");
		(*this) = std::move(original); // move
	}

	GnuVGCanvas::ElementReference::ElementReference(SVGDocument *_source) {
		source = _source;

		(void) _svg_fetch_element_by_id (source->svg, NULL, &element);
		if(element) {
			(void) _svg_element_reference (element);
		} else {
			throw NoSuchElementException("<root element>");
		}
	}

	GnuVGCanvas::ElementReference::~ElementReference() {
		KAMOFLAGE_DEBUG("ElementReference::~ElementReference() - %p\n", element);
		if(element) {
			if(element->custom_data == this) {
				// the developer has enabled the event_handler for this ElementReference object
				// now the developer is deleting the ElementReference
				// if the element is not deleted the custom_data might be referenced. To make
				// sure a non-valid pointer is not used, we set it to zero here.
				// in the case the Element is left and an event is triggered, we can issue a proper
				// error message for it by checking that this pointer is NULL
				element->custom_data = NULL;
			}
		}
		dereference();
	}

	void GnuVGCanvas::ElementReference::dereference() {
		if(element) {
			KAMOFLAGE_DEBUG("ElementReference::dereference() %p -> ref count before: %d\n", element, element->ref_count);
			(void) _svg_element_dereference (element);
			element = NULL;
		}
		source = NULL;
	}

	std::string GnuVGCanvas::ElementReference::get_id() {
		return std::string(element->id);
	}

	std::string GnuVGCanvas::ElementReference::get_class() {
		std::string class_string = "";
		if(element->classes) {
			int k = 0;
			while(element->classes[k]) {
				if(k)
					class_string = class_string + ":";
				class_string = class_string + element->classes[k];
			}
		}
		return class_string;
	}

	void GnuVGCanvas::ElementReference::get_transform(SVGMatrix &matrix) {
		svg_transform_t *transform = &(element->transform);

		matrix.a = transform->m[0][0]; matrix.b = transform->m[0][1];
		matrix.c = transform->m[1][0]; matrix.d = transform->m[1][1];
		matrix.e = transform->m[2][0]; matrix.f = transform->m[2][1];

	}

	void* GnuVGCanvas::ElementReference::pointer() {
		return element;
	}

	void GnuVGCanvas::ElementReference::set_transform(const SVGMatrix &matrix) {

		_svg_transform_init_matrix(&(element->transform),
					   matrix.a, matrix.b,
					   matrix.c, matrix.d,
					   matrix.e, matrix.f);
	}

	void GnuVGCanvas::ElementReference::set_xlink_href(const std::string &url) {
	}

	std::string GnuVGCanvas::ElementReference::get_text_content() {
		const char *content = "";
		if(element && element->type == SVG_ELEMENT_TYPE_TEXT) {
			content = _svg_text_get_content(&(element->e.text));
		}
		return content;
	}

	void GnuVGCanvas::ElementReference::set_text_content(const std::string &content) {
		if(element) {
			if(element->type == SVG_ELEMENT_TYPE_TEXT) {
				_svg_text_set_content(&(element->e.text), content.c_str());
			}
		}
	}

	void GnuVGCanvas::ElementReference::set_display(const std::string &value) {
		if(element) {
			_svg_element_set_display(element, value.c_str());
		}
	}
	void GnuVGCanvas::ElementReference::set_style(const std::string &value) {
		if(element) {
			_svg_element_set_style(element, value.c_str());
		}
	}
	void GnuVGCanvas::ElementReference::set_line_coords(float x1, float y1, float x2, float y2) {
		if(element && element->type == SVG_ELEMENT_TYPE_LINE) {
			element->e.line.x1.value = x1;
			element->e.line.y1.value = y1;
			element->e.line.x2.value = x2;
			element->e.line.y2.value = y2;
		}
	}

	void GnuVGCanvas::ElementReference::set_rect_coords(float x, float y, float width, float height) {
		if(element && element->type == SVG_ELEMENT_TYPE_RECT) {
			element->e.rect.x.value = x;
			element->e.rect.y.value = y;
			element->e.rect.width.value = width;
			element->e.rect.height.value = height;
		}
	}

	void GnuVGCanvas::ElementReference::set_event_handler(std::function<void(SVGDocument *source,
										 ElementReference *e,
										 const MotionEvent &event)> _event_handler) {
		event_handler = _event_handler;
		element->custom_data = this;

		KAMOFLAGE_DEBUG("  set_event_handler for internal element %p\n", element);

		svg_element_enable_events(element);
	}

	void GnuVGCanvas::ElementReference::trigger_event_handler(const GnuVGCanvas::MotionEvent &event) {
		event_handler(source, this, event);
	}

	void GnuVGCanvas::ElementReference::add_svg_child(const std::string &svg_chunk) {
		const char *txt = svg_chunk.c_str();
		size_t len = svg_chunk.size();

		svg_parse_buffer_and_inject(source->svg, element, txt, len);
	}

	GnuVGCanvas::ElementReference GnuVGCanvas::ElementReference::add_element_clone(const std::string &new_id,
										       const GnuVGCanvas::ElementReference &element_to_clone) {
		const char *nid = NULL;
		if(new_id != "") // if the string is empty we want to use the NULL pointer instead
			nid = new_id.c_str();
		if(_svg_inject_clone(nid, element, element_to_clone.element) != SVG_STATUS_SUCCESS) {
			throw OperationFailedException();
		}
		return GnuVGCanvas::ElementReference(&element_to_clone, new_id);
	}

	void GnuVGCanvas::ElementReference::get_viewport(SVGRect &rect) const {
		_svg_element_get_viewport(element,
					  &rect.x,
					  &rect.y,
					  &rect.width,
					  &rect.height);
	}

	void GnuVGCanvas::ElementReference::get_boundingbox(SVGRect &rect) {
		if(element) {
			rect.x = element->bounding_box.left;
			rect.y = element->bounding_box.top;
			rect.width = element->bounding_box.right - element->bounding_box.left;
			rect.height = element->bounding_box.bottom - element->bounding_box.top;
		}
	}

	void GnuVGCanvas::ElementReference::drop_element() {
		if(element)
			(void) svg_drop_element(source->svg, element);
		else {
			KAMOFLAGE_ERROR("Trying to call ElementReference::drop_element() with unassigned ElementReference object.\n");
		}
	}

	void GnuVGCanvas::ElementReference::debug_dump_render_tree() {
		KAMOFLAGE_ERROR("---------- BEGIN debug_dump_render_tree()\n");
		if(element) {
			svg_element_debug_render_tree(element);
		}
		KAMOFLAGE_ERROR("---------- END   debug_dump_render_tree()\n");
	}

	void GnuVGCanvas::ElementReference::debug_dump_id_table() {
		KAMOFLAGE_ERROR("---------- BEGIN debug_dump_id_table()\n");
		if(element) {
			svg_element_debug_id_table(element);
		}
		KAMOFLAGE_ERROR("---------- END   debug_dump_id_table()\n");
	}

	void GnuVGCanvas::ElementReference::debug_ref_count() {
		if(element)
			KAMOFLAGE_ERROR("          ---------------- ref count for (%p -> %p) : %d\n", this, element, element->ref_count);
		else
			KAMOFLAGE_ERROR("          ---------------- ref count for (%p -> %p) : null pointer, no count\n", this, element);
	}

	GnuVGCanvas::ElementReference GnuVGCanvas::ElementReference::find_child_by_class(const std::string &class_name) {
		svg_element_t *new_element = NULL;

		(void) _svg_fetch_element_by_class (source->svg, class_name.c_str(), element, &new_element);

		if(!new_element) {
			throw NoSuchElementException(std::string("class ") + class_name);
		}

		ElementReference destination;

		destination.source = source;
		destination.element = new_element;
		(void) _svg_element_reference (new_element);

		KAMOFLAGE_DEBUG("  found element of class %s -> element %p\n", class_name.c_str(), new_element);

		return destination;
	}

/********************************************************
 *
 *           gnuVGcanvas::SVGDocument
 *
 ********************************************************/

	void GnuVGCanvas::SVGDocument::get_canvas_size(int &width_in_pixels, int &height_in_pixels) {
		width_in_pixels = parent->window_width;
		height_in_pixels = parent->window_height;
	}

	void GnuVGCanvas::SVGDocument::get_canvas_size_inches(float &width_in_inches, float &height_in_inches) {
		width_in_inches = parent->window_width_inches;
		height_in_inches = parent->window_height_inches;
	}

	static inline double __fit_to_inches(double width_inches, double height_inches,
					     double width_pixels, double height_pixels,
					     double fit_width_inches, double fit_height_inches,
					     double source_width_pixels, double source_height_pixels) {
		// calculate the size of a finger in pixels
		double pxl_per_inch_w = width_pixels / width_inches;
		double pxl_per_inch_h = height_pixels / height_inches;

		// force scaling to fit into predefined "fingers"-area
		if(width_inches > fit_width_inches) width_inches = fit_width_inches;
		if(height_inches > fit_height_inches) height_inches = fit_height_inches;

		// calculate scaling factor
		double target_w = (double)width_inches  * pxl_per_inch_w;
		double target_h = (double)height_inches  * pxl_per_inch_h;

		double scale_w = target_w / (double)source_width_pixels;
		double scale_h = target_h / (double)source_height_pixels;

		return scale_w < scale_h ? scale_w : scale_h;
	}

	double GnuVGCanvas::SVGDocument::fit_to_inches(const GnuVGCanvas::ElementReference *element,
						       double inches_wide, double inches_tall) {
		int canvas_w, canvas_h;
		float canvas_w_inches, canvas_h_inches;
		GnuVGCanvas::SVGRect document_size;

		element->get_viewport(document_size);
		get_canvas_size(canvas_w, canvas_h);
		get_canvas_size_inches(canvas_w_inches, canvas_h_inches);

		KAMOFLAGE_DEBUG("canvas(pxls):    %d, %d\n", canvas_w, canvas_h);
		KAMOFLAGE_DEBUG("canvas(inch):    %f, %f\n",
				canvas_w_inches, canvas_h_inches);
		KAMOFLAGE_DEBUG("docume(pxls)     %f, %f\n",
				document_size.width,
				document_size.height);
		KAMOFLAGE_DEBUG("fit ar(pxls)     %f, %f\n",
				inches_wide,
				inches_tall);

		return __fit_to_inches(
			(double)canvas_w_inches, (double)canvas_h_inches,
			(double)canvas_w, (double)canvas_h,
			inches_wide, inches_tall,
			document_size.width, document_size.height);
	}

	void GnuVGCanvas::SVGDocument::process_active_animations() {
		KAMOFLAGE_DEBUG("process animations: (%p) %p\n", this, &animations);
		std::set<Animation *>::iterator k = animations.begin();
		while(k != animations.end()) {
			(*k)->new_time_tick();
			if((*k)->has_finished()) {
				delete (*k);
				k = animations.erase(k);
			} else {
				k++;
			}
		}
	}

	void GnuVGCanvas::SVGDocument::process_touch_for_animations() {
		std::set<Animation *>::iterator k = animations.begin();
		while(k != animations.end()) {
			(*k)->touch_event();
			if((*k)->has_finished()) {
				delete (*k);
				k = animations.erase(k);
			} else {
				k++;
			}
		}
	}

	int GnuVGCanvas::SVGDocument::number_of_active_animations() {
		return animations.size();
	}

	void GnuVGCanvas::SVGDocument::start_animation(KammoGUI::Animation *new_animation) {
		if(new_animation != NULL) {
			KAMOFLAGE_ERROR("start_new animation: %p (%p)\n", new_animation, &animations);
			animations.insert(new_animation);
			new_animation->start();
		}

		parent->queue_for_invalidate(parent);

		pthread_t self = pthread_self();

		GET_INTERNAL_CLASS(jc, parent->internal);
		static jmethodID mid = __ENV->GetMethodID(
			jc,
			"start_animation","()V");

		__ENV->CallVoidMethod(
			parent->internal, mid
			);
	}

	void GnuVGCanvas::SVGDocument::stop_animation(Animation *animation_to_stop) {
		if(animation_to_stop) {
			std::set<Animation *>::iterator k = animations.find(animation_to_stop);
			if(k != animations.end()) {
				if(!((*k)->has_finished())) {
					(*k)->new_frame(1.0); // inform the animation that we've finished
				}

				delete (*k);
				animations.erase(k);
			}
		} else {
			for(auto animation : animations) {
				if(!(animation->has_finished())) {
					animation->new_frame(1.0); // inform the animation that we've finished
				}
				delete animation;
			}
			animations.clear();
		}
	}

	float GnuVGCanvas::SVGDocument::get_preferred_dimension(dimension_t dimension) {
		return 0.1f;
	}

	GnuVGCanvas::SVGDocument::SVGDocument(GnuVGCanvas* _parent)
		: parent(_parent)
	{
		gnuvgUseContext(parent->gnuVGctx);

		stack_push(); // push initial state

		stroke = vgCreatePaint();
		fill = vgCreatePaint();
		vgSetPaint(stroke, VG_STROKE_PATH);
		vgSetPaint(fill, VG_FILL_PATH);
		temporary_path = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F,
					      1,0,0,0, VG_PATH_CAPABILITY_ALL);

		// link parent to us
		parent->documents.push_back(this);

		// load the svg
		if(svg_create (&(svg))) {
			throw jException("Failed to allocate internal SVG structure",
					 jException::sanity_error);
		}

		svg_enable_path_cache(svg);

		// initialize engine

		/* hierarchy */
		svg_render_engine.begin_group = begin_group;
		svg_render_engine.begin_element = begin_element;
		svg_render_engine.end_element = end_element;
		svg_render_engine.end_group = end_group;
		/* path creation */
		svg_render_engine.move_to = move_to;
		svg_render_engine.line_to = line_to;
		svg_render_engine.curve_to = curve_to;
		svg_render_engine.quadratic_curve_to = quadratic_curve_to;
		svg_render_engine.arc_to = arc_to;
		svg_render_engine.close_path = close_path;
		svg_render_engine.free_path_cache = free_path_cache;
		/* style */
		svg_render_engine.set_color = set_color;
		svg_render_engine.set_fill_opacity = set_fill_opacity;
		svg_render_engine.set_fill_paint = set_fill_paint;
		svg_render_engine.set_fill_rule = set_fill_rule;
		svg_render_engine.set_font_family = set_font_family;
		svg_render_engine.set_font_size = set_font_size;
		svg_render_engine.set_font_style = set_font_style;
		svg_render_engine.set_font_weight = set_font_weight;
		svg_render_engine.set_opacity = set_opacity;
		svg_render_engine.set_stroke_dash_array = set_stroke_dash_array;
		svg_render_engine.set_stroke_dash_offset = set_stroke_dash_offset;
		svg_render_engine.set_stroke_line_cap = set_stroke_line_cap;
		svg_render_engine.set_stroke_line_join = set_stroke_line_join;
		svg_render_engine.set_stroke_miter_limit = set_stroke_miter_limit;
		svg_render_engine.set_stroke_opacity = set_stroke_opacity;
		svg_render_engine.set_stroke_paint = set_stroke_paint;
		svg_render_engine.set_stroke_width = set_stroke_width;
		svg_render_engine.set_text_anchor = set_text_anchor;
		svg_render_engine.set_filter = set_filter;
		/* filter */
		svg_render_engine.begin_filter = begin_filter;
		svg_render_engine.add_filter_feBlend = add_filter_feBlend;
		svg_render_engine.add_filter_feComposite = add_filter_feComposite;
		svg_render_engine.add_filter_feFlood = add_filter_feFlood;
		svg_render_engine.add_filter_feGaussianBlur = add_filter_feGaussianBlur;
		svg_render_engine.add_filter_feOffset = add_filter_feOffset;
		/* transform */
		svg_render_engine.apply_clip_box = apply_clip_box;
		svg_render_engine.transform = transform;
		svg_render_engine.apply_view_box = apply_view_box;
		svg_render_engine.set_viewport_dimension = set_viewport_dimension;

		/* drawing */
		svg_render_engine.render_line = render_line;
		svg_render_engine.render_path = render_path;
		svg_render_engine.render_ellipse = render_ellipse;
		svg_render_engine.render_rect = render_rect;
		svg_render_engine.render_text = render_text;
		svg_render_engine.render_image = render_image;

		/* get bounding box of last drawing, in pixels */
		svg_render_engine.get_last_bounding_box = get_last_bounding_box;
	}

	GnuVGCanvas::SVGDocument::SVGDocument(const std::string& fname, GnuVGCanvas* _parent)
		: SVGDocument(_parent)
	{
		(void) svg_parse(svg, fname.c_str());
	}

	GnuVGCanvas::SVGDocument::SVGDocument(GnuVGCanvas* _parent, const std::string& xml)
		: SVGDocument(_parent)
	{
		const char *bfr = xml.c_str();
		(void) svg_parse_buffer(svg, bfr, strlen(bfr));
	}

	GnuVGCanvas::SVGDocument::~SVGDocument() {}

	GnuVGCanvas* GnuVGCanvas::SVGDocument::get_parent() {
		return parent;
	}

	void GnuVGCanvas::SVGDocument::on_resize() {
	}

	void GnuVGCanvas::SVGDocument::render() {
		(void) svg_render(svg, &svg_render_engine, this);
	}

/********************************************************
 *
 *           gnuVGcanvas::SVGDocument helpers
 *
 ********************************************************/

	void GnuVGCanvas::SVGDocument::length_to_pixel(svg_length_t *length, VGfloat* pixel) {
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
 *           gnuVGcanvas::SVGDocument::State
 *
 ********************************************************/

	void GnuVGCanvas::SVGDocument::State::init_by_copy(const GnuVGCanvas::SVGDocument::State* original) {
		for(int i = 0; i < 9; ++i) {
			matrix[i] = original->matrix[i];
		}
		color = original->color;
		opacity = original->opacity;
		paint_modes = original->paint_modes;

		fill_paint = original->fill_paint;
		fill_opacity = original->fill_opacity;
		fill_rule = original->fill_rule;

		stroke_paint = original->stroke_paint;
		stroke_opacity = original->stroke_opacity;
		stroke_width.unit = original->stroke_width.unit;
		stroke_width.value = original->stroke_width.value;
		line_cap = original->line_cap;
		line_join = original->line_join;
		miter_limit = original->miter_limit;

		has_clip_box = false; // we don't copy the clip box

		dash = original->dash;
		dash_phase = original->dash_phase;

		font_family = original->font_family;
		font_size = original->font_size;
		font_style = original->font_style;
		text_anchor = original->text_anchor;
		font_dirty = true;
		active_font = VG_INVALID_HANDLE;

		// bounding box is always cleared, even for a copy
		bounding_box[0] = ~0;
		bounding_box[1] = ~0;
		bounding_box[2] = 0;
		bounding_box[3] = 0;

		pathSeg.clear();
		pathData.clear();

		// current and previous start out as equal
		current_bitmap = previous_bitmap = original->current_bitmap;

		current_filter = nullptr;
	}

	void GnuVGCanvas::SVGDocument::State::init_fresh() {
		VGfloat identity[] = {
			1.0, 0.0, 0.0,
			0.0, 1.0, 0.0,
			0.0, 0.0, 1.0
		};
		for(int i = 0; i < 9; i++)
			matrix[i] = identity[i];

		// black is the default
		color.rgb = 0x000000;
		color.is_current_color = 0;
		opacity = 1.0f;
		paint_modes = VG_FILL_PATH;

		fill_paint.type = SVG_PAINT_TYPE_COLOR;
		fill_paint.p.color.rgb = 0x000000;
		fill_paint.p.color.is_current_color = 0;
		fill_rule = VG_EVEN_ODD;
		fill_opacity = 1.0f;

		stroke_paint.type = SVG_PAINT_TYPE_NONE;
		stroke_paint.p.color.rgb = 0x000000;
		stroke_paint.p.color.is_current_color = 0;
		stroke_opacity = 0.0f;
		line_cap = SVG_STROKE_LINE_CAP_BUTT;
		line_join = SVG_STROKE_LINE_JOIN_MITER;
		miter_limit = 4.0f;
		stroke_width.value = 1.0f;
		stroke_width.unit = SVG_LENGTH_UNIT_PX;
		stroke_width.orientation = SVG_LENGTH_ORIENTATION_OTHER;

		has_clip_box = false;

		dash.clear();
		dash_phase = 0.0f;

		font_family = "";
		font_size = 12.0;
		font_style = gnuVG_NORMAL;
		text_anchor = gnuVG_ANCHOR_START;
		font_dirty = true;
		active_font = VG_INVALID_HANDLE;

		bounding_box[0] = ~0;
		bounding_box[1] = ~0;
		bounding_box[2] = 0;
		bounding_box[3] = 0;

		pathSeg.clear();
		pathData.clear();

		current_bitmap = previous_bitmap = VG_INVALID_HANDLE;
		current_filter = nullptr;
	}

	void GnuVGCanvas::SVGDocument::State::update_bounding_box(unsigned int *new_bbox) {
		if(bounding_box[0] > new_bbox[0])
			bounding_box[0] = new_bbox[0];
		if(bounding_box[1] > new_bbox[1])
			bounding_box[1] = new_bbox[1];
		if(bounding_box[2] < new_bbox[2])
			bounding_box[2] = new_bbox[2];
		if(bounding_box[3] < new_bbox[3])
			bounding_box[3] = new_bbox[3];
	}

	std::string GnuVGCanvas::SVGDocument::State::create_font_identifier(
		const std::string& ffamily, gnuVGFontStyle fstyle) {
		std::ostringstream os ;
		os << fstyle << ":" << ffamily;
		return os.str();
	}

	std::map<std::string, VGFont> GnuVGCanvas::SVGDocument::State::font_table;
	VGFont GnuVGCanvas::SVGDocument::State::get_font(const std::string &ffamily,
							 gnuVGFontStyle fstyle) {
		auto font_identifier = create_font_identifier(ffamily, fstyle);

		VGFont return_font = VG_INVALID_HANDLE;
		auto font_i = font_table.find(font_identifier);
		if(font_i == font_table.end()) {
			KAMOFLAGE_DEBUG("Will try and load font: %s\n", ffamily.c_str());
			return_font = gnuvgLoadFont(ffamily.c_str(), fstyle);
			if(return_font != VG_INVALID_HANDLE) {
				KAMOFLAGE_DEBUG("           ---- font load SUCCESS! %p\n", return_font);
				font_table[font_identifier] = return_font;
			} else {
				KAMOFLAGE_DEBUG("           ---- font load failed...\n");
			}
		} else {
			return_font = (*font_i).second;
		}
		return return_font;
	}

	void GnuVGCanvas::SVGDocument::State::configure_font() {
		if(!font_dirty) return;
		font_dirty = false;
		if(font_family == "") {
			active_font = VG_INVALID_HANDLE;
			return;
		}

		active_font = get_font(font_family, font_style);
	}

/********************************************************
 *
 *           gnuVGcanvas::SVGDocument::State - internals
 *
 ********************************************************/

	void GnuVGCanvas::SVGDocument::push_current_bitmap() {
		if(state->current_bitmap != state->previous_bitmap) {
			gnuvgRenderToImage(state->current_bitmap);

			int w, h;
			get_canvas_size(w, h);

			VGfloat clearColor[] = {0,0, 0,0, 0.0, 0.0};
			vgSetfv(VG_CLEAR_COLOR, 4, clearColor);
			vgClear(0, 0, w, h);

			KAMOFLAGE_ERROR("   cleared new render target: %p\n",
					(void *)state->current_bitmap);
		}
	}

	void GnuVGCanvas::SVGDocument::pop_current_bitmap() {
		if(state->current_bitmap != state->previous_bitmap) {
			gnuvgRenderToImage(state->previous_bitmap);
			KAMOFLAGE_ERROR("   reverted to previous render target: %p\n",
					(void *)state->previous_bitmap);
		}
	}

	VGImage GnuVGCanvas::SVGDocument::get_fresh_bitmap() {
		VGImage allocated;

		if(bitmap_store.size() == 0) {
			int w, h;

			get_canvas_size(w, h);

			allocated = vgCreateImage(VG_sRGBA_8888,
						  w, h,
						  VG_IMAGE_QUALITY_BETTER);
		} else {
			allocated = bitmap_store.front();
			bitmap_store.pop_front();
		}

		return allocated;
	}

	void GnuVGCanvas::SVGDocument::recycle_bitmap(VGImage vgi) {
		bitmap_store.push_back(vgi);
	}

	void GnuVGCanvas::SVGDocument::invalidate_bitmap_store() {
		for(auto vgi : bitmap_store)
			vgDestroyImage(vgi);

		bitmap_store.clear();
	}

	void GnuVGCanvas::SVGDocument::stack_push(VGImage offscreen_bitmap,
						  VGfloat opacity) {
		State* new_state;
		if(state_unused.size() == 0) {
			new_state = new State();
		} else {
			new_state = state_unused.top();
			state_unused.pop();
		}

		if(state_stack.size() != 0) {
			new_state->init_by_copy(state_stack.back());
		} else {
			new_state->init_fresh();
		}

		state_stack.push_back(new_state);
		state = new_state;

		if(offscreen_bitmap != VG_INVALID_HANDLE)
			state->current_bitmap = offscreen_bitmap;
		state->opacity = opacity;

		push_current_bitmap();
	}

	void GnuVGCanvas::SVGDocument::stack_pop() {
		pop_current_bitmap();

		auto current_bitmap = state->current_bitmap;
		auto previous_bitmap = state->previous_bitmap;
		auto previous_filter = state->current_filter;

		if(state_stack.size() <= 1)
			throw GnuVGStateStackEmpty(parent->get_id());

		auto old_state = state_stack.back();

		state_stack.pop_back();
		state = state_stack.back();

		if(previous_bitmap != current_bitmap) {
			KAMOFLAGE_ERROR(" will render %p into %p.\n",
					(void *)current_bitmap, (void *)previous_bitmap);

			if(previous_filter) {
				KAMOFLAGE_ERROR(" will render filter...\n");
				auto filtered_bitmap = previous_filter->execute(
					this,
					current_bitmap,
					VG_INVALID_HANDLE
					);
				KAMOFLAGE_ERROR("  filter generated: %p\n",
						(void *)filtered_bitmap);
				if(filtered_bitmap != VG_INVALID_HANDLE) {
					recycle_bitmap(current_bitmap);
					current_bitmap = filtered_bitmap;
				}
			}

			VGfloat old_values[8];
			VGfloat new_values[] = {
				1.0, 1.0, 1.0, old_state->opacity,
				0.0, 0.0, 0.0, 0.0
			};
			VGint old_color_transform;

			// remember previos color transformation state
			old_color_transform = vgGeti(VG_COLOR_TRANSFORM);
			vgGetfv(VG_COLOR_TRANSFORM_VALUES, 8, old_values);

			// set proper color transformation
			vgSeti(VG_COLOR_TRANSFORM, VG_TRUE);
			vgSetfv(VG_COLOR_TRANSFORM_VALUES, 8, new_values);

			// do actual rendering
			vgSeti(VG_MATRIX_MODE,
			       VG_MATRIX_IMAGE_USER_TO_SURFACE);
			vgLoadIdentity();
			vgDrawImage(current_bitmap);

			// restore previous color transformation state
			vgSeti(VG_COLOR_TRANSFORM, old_color_transform);
			vgSetfv(VG_COLOR_TRANSFORM_VALUES, 8, old_values);

			recycle_bitmap(current_bitmap);
		}

		state->update_bounding_box(old_state->bounding_box);

		state_unused.push(old_state);
	}

	void GnuVGCanvas::SVGDocument::set_color_and_alpha(
		VGPaint* vgpaint, State* state,
		const svg_color_t *color, double alpha) {
		if(color->is_current_color)
			color = &state->color;

		VGfloat rgba[4];
		VGuint c;
		VGfloat c_f;

		c = (color->rgb & 0xff0000) >> 16;
		c_f = c;
		rgba[0] = c_f / 255.0;

		c = (color->rgb & 0x00ff00) >> 8;
		c_f = c;
		rgba[1] = c_f / 255.0;

		c = (color->rgb & 0x0000ff) >> 0;
		c_f = c;
		rgba[2] = c_f / 255.0;

		rgba[3] = alpha;

		vgSetParameterfv(*vgpaint, VG_PAINT_COLOR, 4, rgba);
		vgSetParameteri(*vgpaint, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
	}

	void GnuVGCanvas::SVGDocument::set_gradient(VGPaint* vgpaint,
						    State* state,
						    svg_gradient_t* gradient) {
		VGfloat gradientMatrix[] = {
			1.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 1.0f
		};

		/* apply bounding box - if applicable */
		if(gradient->units == SVG_GRADIENT_UNITS_BBOX) {
			KAMOFLAGE_ERROR("Trying to use BBOX - will exit.\n");
			// we will exit here - this code doesn't work
			// we need to get the non-transformed
			// bounding box of the path that we will
			// render - not the on-surface bounding
			// box of the last rendered path.
			exit(0);

			VGfloat sp_ep[4];
			gnuvgGetBoundingBox(sp_ep);

			VGfloat w = sp_ep[2] - sp_ep[0];
			VGfloat h = sp_ep[3] - sp_ep[1];

			gradientMatrix[2] = sp_ep[0];
			gradientMatrix[5] = sp_ep[2];

			gradientMatrix[0] = w;
			gradientMatrix[4] = h;
		}

		/* set stops */
		VGfloat stop_data[5 * gradient->num_stops];
		int stop_index, stop_offset;

		for(stop_index = 0, stop_offset = 0;
		    stop_index < gradient->num_stops;
		    ++stop_index, stop_offset += 5) {
			svg_gradient_stop_t *stop = &gradient->stops[stop_index];
			int cdat_i;
			VGfloat cdat_f;

			// offset
			stop_data[stop_offset + 0] = stop->offset;

			// R
			cdat_i = (stop->color.rgb & 0xff0000) >> 16;
			cdat_f = (VGfloat)cdat_i;
			stop_data[stop_offset + 1] = cdat_f / 255.0f;

			// G
			cdat_i = (stop->color.rgb & 0x00ff00) >>  8;
			cdat_f = (VGfloat)cdat_i;
			stop_data[stop_offset + 2] = cdat_f / 255.0f;

			// B
			cdat_i = (stop->color.rgb & 0x0000ff) >> 0;
			cdat_f = (VGfloat)cdat_i;
			stop_data[stop_offset + 3] = cdat_f / 255.0f;

			// A
			stop_data[stop_offset + 4] = stop->opacity;
		}

		vgSetParameterfv(*vgpaint, VG_PAINT_COLOR_RAMP_STOPS,
				 5 * gradient->num_stops,
				 stop_data);

		/* set spread mode */
		switch (gradient->spread) {
		case SVG_GRADIENT_SPREAD_REPEAT:
			vgSetParameteri(*vgpaint,
					VG_PAINT_COLOR_RAMP_SPREAD_MODE,
					VG_COLOR_RAMP_SPREAD_REPEAT);
			break;
		case SVG_GRADIENT_SPREAD_REFLECT:
			vgSetParameteri(*vgpaint,
					VG_PAINT_COLOR_RAMP_SPREAD_MODE,
					VG_COLOR_RAMP_SPREAD_REFLECT);
			break;
		default:
			vgSetParameteri(*vgpaint,
					VG_PAINT_COLOR_RAMP_SPREAD_MODE,
					VG_COLOR_RAMP_SPREAD_PAD);
			break;
		}

		/* select linear or radial gradient */
		switch (gradient->type) {
		case SVG_GRADIENT_LINEAR:
		{
			VGfloat lingrad[4];

			length_to_pixel(&gradient->u.linear.x1, &lingrad[0]);
			length_to_pixel(&gradient->u.linear.y1, &lingrad[1]);
			length_to_pixel(&gradient->u.linear.x2, &lingrad[2]);
			length_to_pixel(&gradient->u.linear.y2, &lingrad[3]);

			vgSetParameteri(*vgpaint, VG_PAINT_TYPE, VG_PAINT_TYPE_LINEAR_GRADIENT);
			vgSetParameterfv(*vgpaint, VG_PAINT_LINEAR_GRADIENT, 4, lingrad);

		}
		break;
		case SVG_GRADIENT_RADIAL:
		{
			VGfloat radgrad[5];

			length_to_pixel(&gradient->u.radial.cx, &radgrad[0]);
			length_to_pixel(&gradient->u.radial.cy, &radgrad[1]);
			length_to_pixel(&gradient->u.radial.fx, &radgrad[2]);
			length_to_pixel(&gradient->u.radial.fy, &radgrad[3]);
			length_to_pixel(&gradient->u.radial.r,  &radgrad[4]);

			vgSetParameteri(*vgpaint, VG_PAINT_TYPE, VG_PAINT_TYPE_RADIAL_GRADIENT);
			vgSetParameterfv(*vgpaint, VG_PAINT_RADIAL_GRADIENT, 5, radgrad);
		} break;
		}

		if(vgpaint == &fill)
			vgSeti(VG_MATRIX_MODE, VG_MATRIX_FILL_PAINT_TO_USER);
		else
			vgSeti(VG_MATRIX_MODE, VG_MATRIX_STROKE_PAINT_TO_USER);

		vgLoadMatrix(gradientMatrix);

		gradientMatrix[0] = gradient->transform[0];
		gradientMatrix[1] = gradient->transform[1];
		gradientMatrix[2] = 0.0f;
		gradientMatrix[3] = gradient->transform[2];
		gradientMatrix[4] = gradient->transform[3];
		gradientMatrix[5] = 0.0f;
		gradientMatrix[6] = gradient->transform[4];
		gradientMatrix[7] = gradient->transform[5];
		gradientMatrix[8] = 1.0f;

		vgMultMatrix(gradientMatrix);
	}

	void GnuVGCanvas::SVGDocument::set_paint(
		VGPaint* vgpaint, State* state,
		const svg_paint_t* paint, VGfloat opacity) {

		opacity *= state->opacity;

		switch(paint->type) {
		case SVG_PAINT_TYPE_NONE: /* we would not be called if this is actually set */
			break;
		case SVG_PAINT_TYPE_COLOR:
			set_color_and_alpha(vgpaint, state, &paint->p.color, opacity);
			break;
		case SVG_PAINT_TYPE_GRADIENT:
			set_gradient(vgpaint, state, paint->p.gradient);
			break;
		case SVG_PAINT_TYPE_PATTERN:
			break;
		}

	}

	void GnuVGCanvas::SVGDocument::regenerate_scissors() {
		GNUVG_APPLY_PROFILER_GUARD(regenerate_scissors);

		SVGRect final_clip(0, 0,
				   parent->window_width,
				   parent->window_height);

		parent->loadFundamentalMatrix();
		bool do_clipping = false;
		for(auto s : state_stack) {
			VGfloat m[9];

			vgGetMatrix(m);

			VGfloat x1, y1, x2, y2, w, h;

			x1 = s->clip_box[0];
			y1 = s->clip_box[1];
			w = s->clip_box[2];
			h = s->clip_box[3];
			x2 = x1 + w; y2 = y1 + h;

			VGfloat rx1, ry1, rx2, ry2;

			rx1 = x1 * m[0] + y1 * m[3] + m[6];
			ry1 = x1 * m[1] + y1 * m[4] + m[7];

			rx2 = x2 * m[0] + y2 * m[3] + m[6];
			ry2 = x2 * m[1] + y2 * m[4] + m[7];

			SVGRect current_clip(
				rx1,
				ry1,
				rx2 - rx1,
				ry2 - ry1);
			parent->loadFundamentalMatrix();
			vgMultMatrix(s->matrix);

			// if the current state doesn't
			// set a clip box - we don't touch
			// the mask - just proceed to next
			if(s->has_clip_box) {
				do_clipping = true;
				final_clip.intersect_with(current_clip);
			}
		}

		vgSeti(VG_SCISSORING, do_clipping ? VG_TRUE : VG_FALSE);

		state->final_clip_coordinates[0] = (VGint)final_clip.x;
		state->final_clip_coordinates[1] = (VGint)final_clip.y;
		state->final_clip_coordinates[2] = (VGint)final_clip.width;
		state->final_clip_coordinates[3] = (VGint)final_clip.height;

		vgSetiv(VG_SCISSOR_RECTS, 4, state->final_clip_coordinates);
	}

	void GnuVGCanvas::SVGDocument::use_state_on_top() {
		GNUVG_APPLY_PROFILER_GUARD(use_state_on_top);
		regenerate_scissors();

		if(state->fill_paint.type) {
			state->paint_modes |= VG_FILL_PATH;
			set_paint(&fill, state, &state->fill_paint, state->fill_opacity);
			vgSeti(VG_FILL_RULE, state->fill_rule);
		} else
			state->paint_modes &= ~VG_FILL_PATH;

		if(state->stroke_paint.type) {
			state->paint_modes |= VG_STROKE_PATH;
			set_paint(&stroke, state, &state->stroke_paint, state->stroke_opacity);
		} else
			state->paint_modes &= ~VG_STROKE_PATH;

		vgSeti(VG_MATRIX_MODE, VG_MATRIX_GLYPH_USER_TO_SURFACE);
		parent->loadFundamentalMatrix();
		vgMultMatrix(state->matrix);

		vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);
		parent->loadFundamentalMatrix();
		vgMultMatrix(state->matrix);

		if(state->dash.size() > 0) {
			vgSetfv(VG_STROKE_DASH_PATTERN, state->dash.size(), state->dash.data());
			vgSetf(VG_STROKE_DASH_PHASE, state->dash_phase);
		} else
			vgSetfv(VG_STROKE_DASH_PATTERN, 0, nullptr);

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

		state->configure_font();
	}

/********************************************************
 *
 *           gnuVGcanvas::SVGDocument - libsvg backend
 *
 ********************************************************/

/* hierarchy */
	svg_status_t GnuVGCanvas::SVGDocument::begin_group(void* closure, double opacity) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		VGImage current_bitmap = VG_INVALID_HANDLE;
		if(opacity != 1.0) {
			current_bitmap = context->get_fresh_bitmap();
		}

		context->stack_push(current_bitmap, opacity);
		KAMOFLAGE_ERROR(" --- begin group called. (state is %p)\n", context->state);

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::begin_element(void* closure, void *path_cache) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->stack_push();

		KAMOFLAGE_ERROR(" --- begin element called. (state is %p)\n", context->state);
		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::end_element(void* closure) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		KAMOFLAGE_ERROR(" --- end element called. (state is %p)\n", context->state);
		context->stack_pop();

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::end_group(void* closure, double opacity) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		KAMOFLAGE_ERROR(" --- end group called. (state is %p)\n", context->state);
		context->stack_pop();

		return SVG_STATUS_SUCCESS;
	}

/* path creation */
	svg_status_t GnuVGCanvas::SVGDocument::move_to(void* closure, double x, double y) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->pathSeg.push_back(VG_MOVE_TO_ABS);
		context->state->pathData.push_back((VGfloat)x);
		context->state->pathData.push_back((VGfloat)y);

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::line_to(void* closure, double x, double y) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->pathSeg.push_back(VG_LINE_TO);
		context->state->pathData.push_back((VGfloat)x);
		context->state->pathData.push_back((VGfloat)y);

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::curve_to(void* closure,
							double x1, double y1,
							double x2, double y2,
							double x3, double y3) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->pathSeg.push_back(VG_CUBIC_TO);
		context->state->pathData.push_back((VGfloat)x1);
		context->state->pathData.push_back((VGfloat)y1);
		context->state->pathData.push_back((VGfloat)x2);
		context->state->pathData.push_back((VGfloat)y2);
		context->state->pathData.push_back((VGfloat)x3);
		context->state->pathData.push_back((VGfloat)y3);

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::quadratic_curve_to(void* closure,
								  double x1, double y1,
								  double x2, double y2) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->pathSeg.push_back(VG_QUAD_TO);
		context->state->pathData.push_back((VGfloat)x1);
		context->state->pathData.push_back((VGfloat)y1);
		context->state->pathData.push_back((VGfloat)x2);
		context->state->pathData.push_back((VGfloat)y2);

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::arc_to(void* closure,
						      double	rx,
						      double	ry,
						      double	x_axis_rotation,
						      int	large_arc_flag,
						      int	sweep_flag,
						      double	x,
						      double	y) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		VGubyte arc_type;
		sweep_flag = !sweep_flag;
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

	svg_status_t GnuVGCanvas::SVGDocument::close_path(void* closure) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->pathSeg.push_back(VG_CLOSE_PATH);

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::free_path_cache(void* closure, void **path_cache) {
		VGPath pthc = (VGPath)(*path_cache);
		if(pthc != VG_INVALID_HANDLE) {
			vgDestroyPath(pthc);
		}

		return SVG_STATUS_SUCCESS;
	}

/* style */
	svg_status_t GnuVGCanvas::SVGDocument::set_color(void* closure, const svg_color_t *color) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->color = *color;

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_fill_opacity(void* closure, double fill_opacity) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->fill_opacity = (VGfloat)fill_opacity;

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_fill_paint(void* closure, const svg_paint_t *paint) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->fill_paint = *paint;

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_fill_rule(void* closure, svg_fill_rule_t fill_rule) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->fill_rule = fill_rule == SVG_FILL_RULE_EVEN_ODD ? VG_EVEN_ODD : VG_NON_ZERO;

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_font_family(void* closure, const char *family) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->font_family = family;
		context->state->font_dirty = true;

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_font_size(void* closure, double size) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->font_size = size;
		context->state->font_dirty = true;

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_font_style(void* closure, svg_font_style_t font_style) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		// gnUVG does not distinguish between italic and oblique.. Bad gnuVG.. BAD gnuVG!
		switch(font_style) {
		case SVG_FONT_STYLE_ITALIC:
		case SVG_FONT_STYLE_OBLIQUE:
			context->state->font_style |= gnuVG_ITALIC;
			break;
		default:
			context->state->font_style &= ~gnuVG_ITALIC;
			break;
		}
		context->state->font_dirty = true;

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_font_weight(void* closure, unsigned int font_weight) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		if(font_weight >= 700)
			context->state->font_style |=  gnuVG_BOLD;
		else
			context->state->font_style &= ~gnuVG_BOLD;

		context->state->font_dirty = true;

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_opacity(void* closure, double opacity) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->opacity = opacity;

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_stroke_dash_array(void* closure, double *dash_array, int num_dashes) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->dash.clear();
		if (num_dashes) {
			for(int i = 0; i < num_dashes; i++)
				context->state->dash.push_back((VGfloat)dash_array[i]);
		}

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_stroke_dash_offset(void* closure, svg_length_t *offset_len) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		VGfloat offset;
		context->length_to_pixel(offset_len, &offset);
		context->state->dash_phase = (VGfloat)offset;

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_stroke_line_cap(void* closure, svg_stroke_line_cap_t line_cap) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->line_cap = line_cap;

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_stroke_line_join(void* closure, svg_stroke_line_join_t line_join) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->line_join = line_join;

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_stroke_miter_limit(void* closure, double limit) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->miter_limit = (VGfloat)limit;

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_stroke_opacity(void* closure, double stroke_opacity) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->stroke_opacity = (VGfloat)stroke_opacity;

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_stroke_paint(void* closure, const svg_paint_t *paint) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->stroke_paint = *paint;

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_stroke_width(void* closure, svg_length_t *width) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		context->state->stroke_width = *width;

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_text_anchor(void* closure, svg_text_anchor_t text_anchor) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		switch(context->state->text_anchor) {
		default:
		case SVG_TEXT_ANCHOR_START:
			context->state->text_anchor = gnuVG_ANCHOR_START;
			break;
		case SVG_TEXT_ANCHOR_MIDDLE:
			context->state->text_anchor = gnuVG_ANCHOR_MIDDLE;
			break;
		case SVG_TEXT_ANCHOR_END:
			context->state->text_anchor = gnuVG_ANCHOR_END;
			break;
		}

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_filter(void* closure, const char* id) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		auto found = context->filters.find(id);
		if(found != context->filters.end()) {
			context->state->current_filter = (*found).second;
			KAMOFLAGE_ERROR(" --- set filter called.\n");

			if(context->state->current_bitmap == context->state->previous_bitmap) {
				context->state->current_bitmap = context->get_fresh_bitmap();
				context->push_current_bitmap();
			}
		} else
			context->state->current_filter = nullptr;

		return SVG_STATUS_SUCCESS;
	}

/* filter */
	svg_status_t GnuVGCanvas::SVGDocument::begin_filter(void* closure, const char* id) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		GnuVGFilter *filter_object = nullptr;

		auto found = context->filters.find(id);
		if(found != context->filters.end()) {
			filter_object = (*found).second;
			filter_object->clear();
		} else {
			filter_object = new GnuVGFilter();
			context->filters[id] = filter_object;
		}

		context->state->current_filter = filter_object;

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::add_filter_feBlend(
		void* closure,
		svg_length_t* x, svg_length_t* y,
		svg_length_t* width, svg_length_t* height,
		svg_filter_in_t in, int in_op_reference,
		svg_filter_in_t in2, int in2_op_reference,
		feBlendMode_t mode)
	{
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;
		if(auto cf = context->state->current_filter) {
			auto op = new GnuVG_feBlend(
				x, y,
				width, height,
				in, in_op_reference,
				in2, in2_op_reference,
				mode);
			cf->add_operation(op);
		}

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::add_filter_feComposite(
		void* closure,
		svg_length_t* x, svg_length_t* y,
		svg_length_t* width, svg_length_t* height,
		feCompositeOperator_t oprt,
		svg_filter_in_t in, int in_op_reference,
		svg_filter_in_t in2, int in2_op_reference,
		double k1, double k2, double k3, double k4)
	{
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;
		if(auto cf = context->state->current_filter) {
			auto op = new GnuVG_feComposite(
				x, y,
				width, height,
				oprt,
				in, in_op_reference,
				in2, in2_op_reference,
				k1, k2, k3, k4);
			cf->add_operation(op);
		}

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::add_filter_feFlood(
		void* closure,
		svg_length_t* x, svg_length_t* y,
		svg_length_t* width, svg_length_t* height,
		svg_filter_in_t in, int in_op_reference,
		const svg_color_t* color, double opacity)
	{
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;
		if(auto cf = context->state->current_filter) {
			auto op = new GnuVG_feFlood(
				x, y,
				width, height,
				in, in_op_reference,
				color, opacity);
			cf->add_operation(op);
		}

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::add_filter_feGaussianBlur(
		void* closure,
		svg_length_t* x, svg_length_t* y,
		svg_length_t* width, svg_length_t* height,
		svg_filter_in_t in, int in_op_reference,
		double std_dev_x, double std_dev_y)
	{
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;
		if(auto cf = context->state->current_filter) {
			auto op = new GnuVG_feGaussianBlur(
				x, y,
				width, height,
				in, in_op_reference,
				std_dev_x, std_dev_y);
			cf->add_operation(op);
		}

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::add_filter_feOffset(
		void* closure,
		svg_length_t* x, svg_length_t* y,
		svg_length_t* width,
		svg_length_t* height,
		svg_filter_in_t in, int in_op_reference,
		double dx, double dy)
	{
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;
		if(auto cf = context->state->current_filter) {
			auto op = new GnuVG_feOffset(
				x, y,
				width, height,
				in, in_op_reference,
				dx, dy);
			cf->add_operation(op);
		}

		return SVG_STATUS_SUCCESS;
	}


/* transform */
	svg_status_t GnuVGCanvas::SVGDocument::apply_clip_box(void* closure,
							      svg_length_t *x_l,
							      svg_length_t *y_l,
							      svg_length_t *width_l,
							      svg_length_t *height_l) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;
		auto state = context->state;
		VGfloat x, y, width, height;

		context->length_to_pixel(x_l, &x);
		context->length_to_pixel(y_l, &y);
		context->length_to_pixel(width_l, &width);
		context->length_to_pixel(height_l, &height);

		state->has_clip_box = true;
		state->clip_box[0] = x;
		state->clip_box[1] = y;
		state->clip_box[2] = width;
		state->clip_box[3] = height;

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::transform(void* closure,
							 double xx, double yx,
							 double xy, double yy,
							 double x0, double y0) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		VGfloat matrix[9] = {
			(VGfloat)xx, (VGfloat)yx,        0.0f,
			(VGfloat)xy, (VGfloat)yy,        0.0f,
			(VGfloat)x0, (VGfloat)y0,        1.0f
		};

		vgLoadMatrix(context->state->matrix);
		vgMultMatrix(matrix);
		vgGetMatrix(context->state->matrix);

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::apply_view_box(void* closure,
							      svg_view_box_t view_box,
							      svg_length_t *width,
							      svg_length_t *height) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;

		VGfloat vpar, svgar;
		VGfloat logic_width, logic_height;
		VGfloat logic_x, logic_y;
		VGfloat phys_width, phys_height;

		context->length_to_pixel(width, &phys_width);
		context->length_to_pixel(height, &phys_height);

		vpar = view_box.box.width / view_box.box.height;
		svgar = phys_width / phys_height;
		logic_x = view_box.box.x;
		logic_y = view_box.box.y;
		logic_width = view_box.box.width;
		logic_height = view_box.box.height;

		vgLoadMatrix(context->state->matrix);

		if (view_box.aspect_ratio == SVG_PRESERVE_ASPECT_RATIO_NONE)
		{
			vgScale(phys_width / logic_width,
				phys_height / logic_height);
			vgTranslate(-logic_x, -logic_y);
		}
		else if ((vpar < svgar && view_box.meet_or_slice == SVG_MEET_OR_SLICE_MEET) ||
			 (vpar >= svgar && view_box.meet_or_slice == SVG_MEET_OR_SLICE_SLICE))
		{
			vgScale(phys_height / logic_height, phys_height / logic_height);

			if (view_box.aspect_ratio == SVG_PRESERVE_ASPECT_RATIO_XMINYMIN ||
			    view_box.aspect_ratio == SVG_PRESERVE_ASPECT_RATIO_XMINYMID ||
			    view_box.aspect_ratio == SVG_PRESERVE_ASPECT_RATIO_XMINYMAX)
				vgTranslate(-logic_x, -logic_y);
			else if(view_box.aspect_ratio == SVG_PRESERVE_ASPECT_RATIO_XMIDYMIN ||
				view_box.aspect_ratio == SVG_PRESERVE_ASPECT_RATIO_XMIDYMID ||
				view_box.aspect_ratio == SVG_PRESERVE_ASPECT_RATIO_XMIDYMAX)
				vgTranslate(-logic_x -
					    (logic_width - phys_width * logic_height / phys_height) / 2,
					    -logic_y);
			else
				vgTranslate(-logic_x - (logic_width - phys_width * logic_height / phys_height),
					    -logic_y);
		}
		else
		{
			vgScale(phys_width / logic_width, phys_width / logic_width);

			if (view_box.aspect_ratio == SVG_PRESERVE_ASPECT_RATIO_XMINYMIN ||
			    view_box.aspect_ratio == SVG_PRESERVE_ASPECT_RATIO_XMIDYMIN ||
			    view_box.aspect_ratio == SVG_PRESERVE_ASPECT_RATIO_XMAXYMIN)
				vgTranslate(-logic_x, -logic_y);
			else if(view_box.aspect_ratio == SVG_PRESERVE_ASPECT_RATIO_XMINYMID ||
				view_box.aspect_ratio == SVG_PRESERVE_ASPECT_RATIO_XMIDYMID ||
				view_box.aspect_ratio == SVG_PRESERVE_ASPECT_RATIO_XMAXYMID)
				vgTranslate(-logic_x,
					    -logic_y -
					    (logic_height - phys_height * logic_width / phys_width) / 2);
			else
				vgTranslate(-logic_x,
					    -logic_y - (logic_height - phys_height * logic_width / phys_width));
		}

		vgGetMatrix(context->state->matrix);
		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::set_viewport_dimension(void* closure,
								      svg_length_t *width,
								      svg_length_t *height) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;
		context->length_to_pixel(width, &(context->state->viewport_width));
		context->length_to_pixel(height, &(context->state->viewport_height));
		return SVG_STATUS_SUCCESS;
	}

/* drawing */
	svg_status_t GnuVGCanvas::SVGDocument::render_line(void* closure,
							   svg_length_t *x1,
							   svg_length_t *y1,
							   svg_length_t *x2,
							   svg_length_t *y2) {
		gnuvgResetBoundingBox();

		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;
		context->use_state_on_top();

		VGfloat _x0, _y0, _x1, _y1;
		context->length_to_pixel(x1, &_x0);
		context->length_to_pixel(y1, &_y0);
		context->length_to_pixel(x2, &_x1);
		context->length_to_pixel(x2, &_y1);

		GNUVG_APPLY_PROFILER_GUARD(render_tempath);
		vgClearPath(context->temporary_path, VG_PATH_CAPABILITY_ALL);
		vguLine(context->temporary_path, _x0, _y0, _x1, _y1);
		vgDrawPath(context->temporary_path, context->state->paint_modes);

		context->fetch_gnuvg_boundingbox();

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::render_path(void* closure, void **path_cache) {
		gnuvgResetBoundingBox();

		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;
		context->use_state_on_top();

		GNUVG_APPLY_PROFILER_GUARD(render_path_cache);

		VGPath this_path;
		if((*path_cache) == NULL) {
			KAMOFLAGE_ERROR("Creating new OpenVG path object...\n");
			this_path = vgCreatePath(VG_PATH_FORMAT_STANDARD, VG_PATH_DATATYPE_F,
						 1,0,0,0, VG_PATH_CAPABILITY_ALL);

			vgAppendPathData(this_path,
					 context->state->pathSeg.size(),
					 context->state->pathSeg.data(),
					 context->state->pathData.data());

			*path_cache = (void*)this_path;
		} else
			this_path = (VGPath)(*path_cache);

		vgDrawPath(this_path, context->state->paint_modes);

		context->fetch_gnuvg_boundingbox();

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::render_ellipse(void* closure,
							      svg_length_t *cx,
							      svg_length_t *cy,
							      svg_length_t *rx,
							      svg_length_t *ry) {
		gnuvgResetBoundingBox();

		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;
		context->use_state_on_top();

		VGfloat _cx, _cy, _rx, _ry;
		context->length_to_pixel(cx, &_cx);
		context->length_to_pixel(cy, &_cy);
		context->length_to_pixel(rx, &_rx);
		context->length_to_pixel(rx, &_ry);

		static const VGubyte segments[4] = {VG_MOVE_TO | VG_ABSOLUTE,
						    VG_SCCWARC_TO | VG_ABSOLUTE,
						    VG_SCCWARC_TO | VG_ABSOLUTE,
						    VG_CLOSE_PATH};
		const VGfloat data[12] = {_cx + _rx, _cy,
					  _rx, _ry, 0, _cx - _rx, _cy,
					  _rx, _ry, 0, _cx + _rx, _cy};

		GNUVG_APPLY_PROFILER_GUARD(render_tempath);
		vgClearPath(context->temporary_path, VG_PATH_CAPABILITY_ALL);
		vgAppendPathData(context->temporary_path, 4, segments, data);
		vgDrawPath(context->temporary_path, context->state->paint_modes);

		context->fetch_gnuvg_boundingbox();

		KAMOFLAGE_ERROR("Rendered an ellipse.\n");

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::render_rect(void* closure,
							   svg_length_t *x,
							   svg_length_t *y,
							   svg_length_t *width,
							   svg_length_t *height,
							   svg_length_t *rx,
							   svg_length_t *ry) {
		gnuvgResetBoundingBox();

		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;
		context->use_state_on_top();

		VGfloat _x, _y, _w, _h, _rx, _ry;
		context->length_to_pixel(x, &_x);
		context->length_to_pixel(y, &_y);
		context->length_to_pixel(width, &_w);
		context->length_to_pixel(height, &_h);
		context->length_to_pixel(rx, &_rx);
		context->length_to_pixel(ry, &_ry);

		GNUVG_APPLY_PROFILER_GUARD(render_tempath);
		vgClearPath(context->temporary_path, VG_PATH_CAPABILITY_ALL);

		if(_rx == 0.0f && _ry == 0.0f) {
			static const VGubyte cmds[] = {VG_MOVE_TO_ABS,
						       VG_LINE_TO_REL,
						       VG_LINE_TO_REL,
						       VG_LINE_TO_REL,
						       VG_CLOSE_PATH
			};

			VGfloat data[] = {
				_x, _y,
				_w, 0.0,
				0.0, _h,
				-_w, 0.0
			};

			vgAppendPathData(context->temporary_path, 5, cmds, data);
		} else {
			vguRoundRect(context->temporary_path,
				     _x, _y, _w, _h, _rx, _ry);
		}
		vgDrawPath(context->temporary_path, context->state->paint_modes);

		context->fetch_gnuvg_boundingbox();

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::render_text(void* closure,
							   svg_length_t *x,
							   svg_length_t *y,
							   const char   *utf8) {
		gnuvgResetBoundingBox();

		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;
		context->use_state_on_top();

		GNUVG_APPLY_PROFILER_GUARD(render_text);

		VGfloat _x, _y;
		context->length_to_pixel(x, &_x);
		context->length_to_pixel(y, &_y);

		vgSeti(VG_MATRIX_MODE, VG_MATRIX_GLYPH_USER_TO_SURFACE);

		VGfloat mtrx[9];

		vgGetMatrix(mtrx);
		vgTranslate(_x, _y);

		gnuvgRenderText(context->state->active_font,
				context->state->font_size,
				context->state->text_anchor,
				utf8, 0.0f, 0.0f);

		vgLoadMatrix(mtrx);
		vgSeti(VG_MATRIX_MODE, VG_MATRIX_PATH_USER_TO_SURFACE);

		context->fetch_gnuvg_boundingbox();

		return SVG_STATUS_SUCCESS;
	}

	svg_status_t GnuVGCanvas::SVGDocument::render_image(void* closure,
							    unsigned char *data,
							    unsigned int	 data_width,
							    unsigned int	 data_height,
							    svg_length_t	 *x,
							    svg_length_t	 *y,
							    svg_length_t	 *width,
							    svg_length_t	 *height) {
		gnuvgResetBoundingBox();

		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;
		context->use_state_on_top();

		context->fetch_gnuvg_boundingbox();

		return SVG_STATUS_SUCCESS;
	}

	void GnuVGCanvas::SVGDocument::fetch_gnuvg_boundingbox() {
		VGfloat sp[4];
		unsigned int bbx[4];

		/* convert from gnuVG float to unsigned integers */
		gnuvgGetBoundingBox(sp);
		for(int k = 0; k < 4; k++)
			bbx[k] = (unsigned int)sp[k];

		state->update_bounding_box(bbx);
	}

	int GnuVGCanvas::SVGDocument::get_state_boundingbox(svg_bounding_box_t *bbox) {
		auto left = (VGint)state->bounding_box[0];
		auto top = (VGint)state->bounding_box[1];
		auto right = (VGint)state->bounding_box[2];
		auto bottom = (VGint)state->bounding_box[3];

		/* check if outside the clip */
		if(left > state->final_clip_coordinates[2])
			return 0;
		if(top > state->final_clip_coordinates[3])
			return 0;
		if(right < state->final_clip_coordinates[0])
			return 0;
		if(bottom < state->final_clip_coordinates[1])
			return 0;

		/* if not - limit by clip */
		if(left < state->final_clip_coordinates[0])
			left = state->final_clip_coordinates[0];
		if(top < state->final_clip_coordinates[1])
			top = state->final_clip_coordinates[1];
		if(right > state->final_clip_coordinates[2])
			right = state->final_clip_coordinates[2];
		if(bottom > state->final_clip_coordinates[3])
			bottom = state->final_clip_coordinates[3];

		bbox->left = (unsigned int)left; bbox->top = (unsigned int)top;
		bbox->right = (unsigned int)right; bbox->bottom = (unsigned int)bottom;

		return 1;
	}


/* get bounding box of last drawing, in pixels - returns 0 if bounding box is outside the visible clip, non-0 if inside the visible clip */
	int GnuVGCanvas::SVGDocument::get_last_bounding_box(void *closure, svg_bounding_box_t *bbox) {
		GnuVGCanvas::SVGDocument* context = (GnuVGCanvas::SVGDocument*)closure;
		return context->get_state_boundingbox(bbox);
	}


/********************************************************
 *
 *           gnuVGcanvas - Kamoflage interface
 *
 ********************************************************/

	static void printGLString(const char *name, GLenum s) {
		KAMOFLAGE_ERROR("GL %s = %s\n", name, glGetString(s));
	}

	static void checkGlError(const char* op) {
		for (GLint error = glGetError();
		     error;
		     error = glGetError()) {
			KAMOFLAGE_ERROR("after %s() glError (0x%x)\n", op, error);
		}
	}

	GnuVGCanvas::GnuVGCanvas(std::string _id, jobject jobj) :
		Widget(_id, jobj) {
		gnuVGctx = gnuvgCreateContext();

		KAMOFLAGE_DEBUG("GnuVGCanvas::GnuVGCanvas(%s)\n", _id.c_str());
	}

	GnuVGCanvas::~GnuVGCanvas() {
	}

	void GnuVGCanvas::init(JNIEnv *env) {
		printGLString("Version", GL_VERSION);
		printGLString("Vendor", GL_VENDOR);
		printGLString("Renderer", GL_RENDERER);
		printGLString("Extensions", GL_EXTENSIONS);
	}

	void GnuVGCanvas::resize(JNIEnv *env, int width, int height,
				 float width_inches, float height_inches) {
		KAMOFLAGE_DEBUG("GnuVGCanvas::resize(%d, %d)", width, height);

		glViewport(0, 0, width, height);
		checkGlError("glViewport");

		gnuvgUseContext(gnuVGctx);
		gnuvgResize(width, height);
		window_width = width;
		window_height = height;
		window_width_inches = width_inches;
		window_height_inches = height_inches;

		vgLoadIdentity();
		vgTranslate(0.0f, window_height);
		vgScale(1.0, -1.0);
		vgGetMatrix(fundamentalMatrix);
		vgLoadIdentity();

		for(auto doc : documents)
			doc->invalidate_bitmap_store();
	}

	void GnuVGCanvas::set_bg_color(double r, double g, double b) {
		bg_r = (VGfloat)r;
		bg_g = (VGfloat)g;
		bg_b = (VGfloat)b;
	}

	void GnuVGCanvas::step(JNIEnv *env) {
		usleep(1000000);
		KAMOFLAGE_ERROR("\n\n\n::step() new frame\n");

		GNUVG_APPLY_PROFILER_GUARD(full_frame);
		{
			GNUVG_APPLY_PROFILER_GUARD(prepare_context);

			gnuvgUseContext(gnuVGctx);
			VGfloat clearColor[] = {bg_r, bg_g, bg_b, 1.0};
			vgSetfv(VG_CLEAR_COLOR, 4, clearColor);
			vgClear(0, 0, window_width, window_height);
		}
		int active_animations = 0;
		for(auto document : documents) {
			gnuvgRenderToImage(VG_INVALID_HANDLE);

			document->process_active_animations();
			active_animations += document->number_of_active_animations();
			document->on_render();
			document->render();
		}

#if 0
		if(active_animations == 0) {
			KAMOFLAGE_ERROR(" will STOP animation now - no animations left.\n");

			pthread_t self = pthread_self();

			GET_INTERNAL_CLASS(jc, internal);
			static jmethodID mid = __ENV->GetMethodID(
				jc,
				"stop_animation","()V");

			__ENV->CallVoidMethod(
				internal, mid
				);
		}
#endif
	}

	void GnuVGCanvas::canvas_motion_event_init_event(
		JNIEnv *env, long downTime, long eventTime,
		int androidAction, int pointerCount, int actionIndex, float rawX, float rawY) {

		MotionEvent::motionEvent_t action = MotionEvent::ACTION_CANCEL;

		switch(androidAction) {
		case 0x00000000:
			action = MotionEvent::ACTION_DOWN;
			break;
		case 0x00000002:
			action = MotionEvent::ACTION_MOVE;
			break;
		case 0x00000004:
			action = MotionEvent::ACTION_OUTSIDE;
			break;
		case 0x00000005:
			action = MotionEvent::ACTION_POINTER_DOWN;
			break;
		case 0x00000006:
			action = MotionEvent::ACTION_POINTER_UP;
			break;
		case 0x00000001:
			action = MotionEvent::ACTION_UP;
			break;
		}

		m_evt.init(downTime, eventTime, action, pointerCount, actionIndex, rawX, rawY);
	}

	void GnuVGCanvas::canvas_motion_event_init_pointer(JNIEnv *env,
							   int index, int id,
							   float x, float y, float pressure) {
		m_evt.init_pointer(index, id, x, y, pressure);
	}

	void GnuVGCanvas::canvas_motion_event(JNIEnv *env) {
		KAMOFLAGE_DEBUG("    canvas_motion_event (%f, %f)\n", m_evt.get_x(), m_evt.get_y());

		for(auto document : documents) {
			document->process_touch_for_animations();
		}
		switch(m_evt.get_action()) {
		case GnuVGCanvas::MotionEvent::ACTION_DOWN:
		{
			svg_element_t *element = NULL;

			KAMOFLAGE_DEBUG("Will scan docs..\n");
			int k = documents.size() - 1;
			for(; k >= 0 && element == NULL; k--) {
				GnuVGCanvas::SVGDocument *document = (documents[k]);
				element = svg_event_coords_match(document->svg,
								 m_evt.get_x(), m_evt.get_y());
				KAMOFLAGE_DEBUG("  element = %p\n", element);
			}

			if(element) {
				KAMOFLAGE_DEBUG("   KAMOFLAGE action_down on element: %p (custom: %p)\n", element, element->custom_data);
				if(element->custom_data == NULL) {
					active_element = NULL;
					KAMOFLAGE_ERROR("GnuVGCanvas::canvas_motion_event() - event handler is missing it's ElementReference - you probably deleted the ElementReference after calling set_event_handler(). Or maybe you used only a temporary ElementReference object.");
				} else {
					active_element = (ElementReference *)element->custom_data;
				}
			} else
				active_element = NULL;
		}
		break;
		case GnuVGCanvas::MotionEvent::ACTION_CANCEL:
		case GnuVGCanvas::MotionEvent::ACTION_MOVE:
		case GnuVGCanvas::MotionEvent::ACTION_OUTSIDE:
		case GnuVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
		case GnuVGCanvas::MotionEvent::ACTION_POINTER_UP:
		case GnuVGCanvas::MotionEvent::ACTION_UP:
			break;
		}

		if(active_element) {
			active_element->trigger_event_handler(m_evt);

			switch(m_evt.get_action()) {
			case GnuVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
			case GnuVGCanvas::MotionEvent::ACTION_MOVE:
			case GnuVGCanvas::MotionEvent::ACTION_DOWN:
			case GnuVGCanvas::MotionEvent::ACTION_OUTSIDE:
			case GnuVGCanvas::MotionEvent::ACTION_POINTER_UP:
				break;
			case GnuVGCanvas::MotionEvent::ACTION_CANCEL:
			case GnuVGCanvas::MotionEvent::ACTION_UP:
				active_element = NULL;
				break;
			}
		}
	}

};

extern "C" {
	JNIEXPORT void Java_com_toolkits_kamoflage_gnuVGView_init
	(JNIEnv *env, jclass jcl, jlong nativeID) {
		KammoGUI::GnuVGCanvas *cnv =
			(KammoGUI::GnuVGCanvas *)nativeID;
		cnv->init(env);
	}

	JNIEXPORT void Java_com_toolkits_kamoflage_gnuVGView_resize
	(JNIEnv *env, jclass jcl, jlong nativeID, jint width, jint height,
	 jfloat width_inches, jfloat height_inches) {
		KammoGUI::GnuVGCanvas *cnv =
			(KammoGUI::GnuVGCanvas *)nativeID;
		cnv->resize(env, width, height, width_inches, height_inches);
	}

	static void print_timing() {
		static struct timespec last_time;
		struct timespec this_time, time_difference;

		(void) clock_gettime(CLOCK_MONOTONIC_RAW, &this_time);

		time_difference.tv_sec = this_time.tv_sec - last_time.tv_sec;
		time_difference.tv_nsec = this_time.tv_nsec - last_time.tv_nsec;
		if(time_difference.tv_nsec < 0)
			time_difference.tv_sec--;

		if(time_difference.tv_sec >= 1) {
			last_time = this_time;

			double full_frame_time = GNUVG_GET_PROFILER_TIME(full_frame);
			double full_frame_count = GNUVG_GET_PROFILER_COUNT(full_frame);
			KAMOFLAGE_ERROR("\n");

			KAMOFLAGE_ERROR("------------\n");
			KAMOFLAGE_ERROR("Time/Frame: %f\n",
					full_frame_time / full_frame_count);

			FOR_ALL_GNUVG_PROFILE_PROBES(
				KAMOFLAGE_ERROR("%f (%5d): %s\n",
						gvgp_time / full_frame_time,
						gvgp_count,
						gvgp_name)
				);

			KAMOFLAGE_ERROR("Triangles rendered: %d\n",
					GNUVG_READ_PROFILER_COUNTER(render_elements) / 3);

			KAMOFLAGE_ERROR("\n");
		} else
			(void) GNUVG_READ_PROFILER_COUNTER(render_elements);
	}

	JNIEXPORT void Java_com_toolkits_kamoflage_gnuVGView_step
	(JNIEnv *env, jclass jcl, jlong nativeID) {
		KammoGUI::GnuVGCanvas *cnv =
			(KammoGUI::GnuVGCanvas *)nativeID;
		cnv->step(env);

		print_timing();
	}

	JNIEXPORT void Java_com_toolkits_kamoflage_gnuVGView_canvasMotionEventInitEvent
	(JNIEnv *env, jclass jc, jlong nativeID, jlong downTime, jlong eventTime, jint androidAction, jint pointerCount, jint actionIndex, jfloat rawX, jfloat rawY) {
		KammoGUI::GnuVGCanvas *cnv =
			(KammoGUI::GnuVGCanvas *)nativeID;
		cnv->canvas_motion_event_init_event(env, downTime, eventTime, androidAction, pointerCount, actionIndex, rawX, rawY);
	}

	JNIEXPORT void Java_com_toolkits_kamoflage_gnuVGView_canvasMotionEventInitPointer
	(JNIEnv *env, jclass jc, jlong nativeID, jint index, jint id, jfloat x, jfloat y, jfloat pressure) {
		KammoGUI::GnuVGCanvas *cnv =
			(KammoGUI::GnuVGCanvas *)nativeID;
		cnv->canvas_motion_event_init_pointer(env, index, id, x, y, pressure);
	}

	JNIEXPORT void Java_com_toolkits_kamoflage_gnuVGView_canvasMotionEvent
	(JNIEnv *env, jclass jc, jlong nativeID) {
		KammoGUI::GnuVGCanvas *cnv =
			(KammoGUI::GnuVGCanvas *)nativeID;
		cnv->canvas_motion_event(env);
		KammoGUI::GnuVGCanvas::flush_invalidation_queue();
	}

}
