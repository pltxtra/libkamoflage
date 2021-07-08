/*
 * Kamoflage, ANDROID version
 * Copyright (C) 2007 by Anton Persson & Ted Björling
 * Copyright (C) 2010 by Anton Persson
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

#include <string.h>
#include <typeinfo>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>

#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#include <math.h>

#include "kamogui.hh"
#include "jngldrum/jthread.hh"
#include "jngldrum/jinformer.hh"

#include <libsvg/svgint.h>

//#define __DO_KAMOFLAGE_DEBUG
#include "kamoflage_debug.hh"

/*************************
 *
 *    Exception classes
 *
 *************************/
KammoGUI::SVGCanvas::NoSuchElementException::NoSuchElementException(const std::string &id) : jException(id + " : element does not exist.", jException::sanity_error) {}
KammoGUI::SVGCanvas::OperationFailedException::OperationFailedException() : jException("SVGCanvas - operation failed.", jException::sanity_error) {}

/*************************
 *
 *   SVGMatrix class implementation
 *
 *************************/

KammoGUI::SVGCanvas::SVGMatrix::SVGMatrix() : a(1.0), b(0.0), c(0.0), d(1.0), e(0.0), f(0.0) {
}

void KammoGUI::SVGCanvas::SVGMatrix::init_identity() {
	a = 1.0; c = 0.0; e = 0.0;
	b = 0.0; d = 1.0; f = 0.0;
}


void KammoGUI::SVGCanvas::SVGMatrix::translate(double x, double y) {
	e += x;
	f += y;
}

void KammoGUI::SVGCanvas::SVGMatrix::scale(double x, double y) {
	a *= x;
	b *= y;
	c *= x;
	d *= y;
	e *= x;
	f *= y;
}

void KammoGUI::SVGCanvas::SVGMatrix::rotate(double angle_in_radians) {
	double u, v, x, y;
	u = cos(angle_in_radians); v = -sin(angle_in_radians);
	x = sin(angle_in_radians); y =  cos(angle_in_radians);

	double A, B, C, D, E, F;
	A = u * a + v * b; C = u * c + v * d; E = u * e + v * f;
	B = x * a + y * b; D = x * c + y * d; F = x * e + y * f;

	a = A; c = C; e = E;
	b = B; d = D; f = F;
}

void KammoGUI::SVGCanvas::SVGMatrix::multiply(const KammoGUI::SVGCanvas::SVGMatrix &other) {
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

void KammoGUI::SVGCanvas::SVGDocument::process_active_animations() {
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

void KammoGUI::SVGCanvas::SVGDocument::process_touch_for_animations() {
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

int KammoGUI::SVGCanvas::SVGDocument::number_of_active_animations() {
	return animations.size();
}

KammoGUI::SVGCanvas::SVGDocument::SVGDocument(const std::string &fname, KammoGUI::SVGCanvas *_parent) :
	parent(_parent), file_name(fname) {

	// link parent to us
	parent->documents.push_back(this);

	// load the SVG
	svg_data = svgAndroidCreate();
	if(svg_data == NULL)
		throw jException("Failed to allocate internal SVG structure",
				 jException::sanity_error);

	svgAndroidSetAntialiasing(svg_data, JNI_TRUE);
	svgAndroidEnablePathCache(svg_data);

//	KAMOFLAGE_DEBUG_("   trying to read SVG from %s\n", fname.c_str()); fflush(0);
	svg_status_t st = svg_parse(svg_data->svg, fname.c_str());
	if(st) {
		std::ostringstream ost;
		ost << "Failed to create internal SVG representation, code " << st << " (for file: " << fname << ")";

		(void)svgAndroidDestroy(svg_data);
		throw jException(
			ost.str(),
			//"Failed to create internal SVG representation",
				 jException::sanity_error);
	}
}

KammoGUI::SVGCanvas::SVGDocument::SVGDocument(SVGCanvas *_parent, const std::string &xml) : parent(_parent), file_name("<from string>") {
	// link parent to us
	parent->documents.push_back(this);

	// load the svg
	svg_data = svgAndroidCreate();
	if(svg_data == NULL)
		throw jException("Failed to allocate internal SVG structure",
				 jException::sanity_error);

	svgAndroidSetAntialiasing(svg_data, JNI_TRUE);
	svgAndroidEnablePathCache(svg_data);

	const char *bfr = xml.c_str();
	svg_status_t st = svg_parse_buffer(svg_data->svg, bfr, strlen(bfr));
	if(st) {
		FILE *f = fopen("/data/data/com.toolkits.kamoflage/svg_out.svg", "w");
		if(f != NULL) {
			fprintf(f, "%s", bfr);
			fclose(f);
		} else KAMOFLAGE_DEBUG_(" FAIIIIIL!\n");
		fflush(0);
		(void)svgAndroidDestroy(svg_data);
		KAMOFLAGE_DEBUG_("   svg_status_t = %d\n", st); fflush(0);
		throw jException("Failed to create internal SVG representation from text string.",
				 jException::sanity_error);
	}
}

KammoGUI::SVGCanvas::SVGDocument::~SVGDocument() {
}

void KammoGUI::SVGCanvas::SVGDocument::on_resize() {
	// default to nothing, must be overriden to perform a useful function
}

float KammoGUI::SVGCanvas::SVGDocument::get_preferred_dimension(dimension_t dimension) {
	return 0.1f;
}

KammoGUI::SVGCanvas::ElementReference::ElementReference() : source(NULL), element(NULL) {
}

KammoGUI::SVGCanvas::ElementReference::ElementReference(SVGDocument *_source, const std::string &id) {
	source = _source;

	(void) _svg_fetch_element_by_id (source->svg_data->svg, id.c_str(), &element);
	if(element) {
		(void) _svg_element_reference (element);
	} else {
		throw NoSuchElementException(id);
	}
	KAMOFLAGE_DEBUG("ElementReference::ElementReference(%p) %s -> ref count before: %d\n", element, id.c_str(), element->ref_count);
}

KammoGUI::SVGCanvas::ElementReference::ElementReference(const ElementReference *sibling, const std::string &id) {
	source = sibling->source;

	(void) _svg_fetch_element_by_id (source->svg_data->svg, id.c_str(), &element);
	if(element) {
		(void) _svg_element_reference (element);
	} else {
		throw NoSuchElementException(id);
	}
	KAMOFLAGE_DEBUG("ElementReference::ElementReference(%p) %s -> ref count before: %d\n", element, id.c_str(), element->ref_count);
}

KammoGUI::SVGCanvas::ElementReference::ElementReference(const ElementReference *original) {
	source = original->source;

	element = original->element;
	if(element) {
		(void) _svg_element_reference (element);
	} else {
		throw NoSuchElementException("id unknown.");
	}
}
KammoGUI::SVGCanvas::ElementReference::ElementReference(const ElementReference &original) : ElementReference(&original) {}

KammoGUI::SVGCanvas::ElementReference& KammoGUI::SVGCanvas::ElementReference::operator=(
	KammoGUI::SVGCanvas::ElementReference&& a) {
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

KammoGUI::SVGCanvas::ElementReference::ElementReference(ElementReference &&original)
	: source(0)
	, element(0)
{
	KAMOFLAGE_DEBUG("Move constructor.\n");
	(*this) = std::move(original); // move
}

KammoGUI::SVGCanvas::ElementReference::ElementReference(SVGDocument *_source) {
	source = _source;

	(void) _svg_fetch_element_by_id (source->svg_data->svg, NULL, &element);
	if(element) {
		(void) _svg_element_reference (element);
	} else {
		throw NoSuchElementException("<root element>");
	}
}

KammoGUI::SVGCanvas::ElementReference::~ElementReference() {
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

void KammoGUI::SVGCanvas::ElementReference::dereference() {
	if(element) {
		KAMOFLAGE_DEBUG("ElementReference::dereference() %p -> ref count before: %d\n", element, element->ref_count);
		(void) _svg_element_dereference (element);
		element = NULL;
	}
	source = NULL;
}

std::string KammoGUI::SVGCanvas::ElementReference::get_id() {
	return std::string(element->id);
}

std::string KammoGUI::SVGCanvas::ElementReference::get_class() {
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

void KammoGUI::SVGCanvas::ElementReference::get_transform(SVGMatrix &matrix) {
	svg_transform_t *transform = &(element->transform);

	matrix.a = transform->m[0][0]; matrix.b = transform->m[0][1];
	matrix.c = transform->m[1][0]; matrix.d = transform->m[1][1];
	matrix.e = transform->m[2][0]; matrix.f = transform->m[2][1];

}

void* KammoGUI::SVGCanvas::ElementReference::pointer() {
	return element;
}

void KammoGUI::SVGCanvas::ElementReference::set_transform(const SVGMatrix &matrix) {

	_svg_transform_init_matrix(&(element->transform),
				   matrix.a, matrix.b,
				   matrix.c, matrix.d,
				   matrix.e, matrix.f);
}

void KammoGUI::SVGCanvas::ElementReference::set_xlink_href(const std::string &url) {
}

std::string KammoGUI::SVGCanvas::ElementReference::get_text_content() {
	const char *content = "";
	if(element && element->type == SVG_ELEMENT_TYPE_TEXT) {
		content = _svg_text_get_content(&(element->e.text));
	}
	return content;
}

void KammoGUI::SVGCanvas::ElementReference::set_text_content(const std::string &content) {
	if(element) {
		if(element->type == SVG_ELEMENT_TYPE_TEXT) {
			_svg_text_set_content(&(element->e.text), content.c_str());
		}
	}
}

void KammoGUI::SVGCanvas::ElementReference::set_display(const std::string &value) {
	if(element) {
		_svg_element_set_display(element, value.c_str());
	}
}
void KammoGUI::SVGCanvas::ElementReference::set_style(const std::string &value) {
	if(element) {
		_svg_element_set_style(element, value.c_str());
	}
}
void KammoGUI::SVGCanvas::ElementReference::set_line_coords(float x1, float y1, float x2, float y2) {
	if(element && element->type == SVG_ELEMENT_TYPE_LINE) {
		element->e.line.x1.value = x1;
		element->e.line.y1.value = y1;
		element->e.line.x2.value = x2;
		element->e.line.y2.value = y2;
	}
}

void KammoGUI::SVGCanvas::ElementReference::set_rect_coords(float x, float y, float width, float height) {
	if(element && element->type == SVG_ELEMENT_TYPE_RECT) {
		element->e.rect.x.value = x;
		element->e.rect.y.value = y;
		element->e.rect.width.value = width;
		element->e.rect.height.value = height;
	}
}

void KammoGUI::SVGCanvas::ElementReference::set_event_handler(std::function<void(SVGDocument *source,
										 ElementReference *e,
										 const MotionEvent &event)> _event_handler) {
	event_handler = _event_handler;
	element->custom_data = this;

	KAMOFLAGE_DEBUG("  set_event_handler for internal element %p\n", element);

	svg_element_enable_events(element);
}

void KammoGUI::SVGCanvas::ElementReference::trigger_event_handler(const KammoGUI::MotionEvent &event) {
	event_handler(source, this, event);
}

void KammoGUI::SVGCanvas::ElementReference::add_svg_child(const std::string &svg_chunk) {
	const char *txt = svg_chunk.c_str();
	size_t len = svg_chunk.size();

	svg_parse_buffer_and_inject(source->svg_data->svg, element, txt, len);
}

KammoGUI::SVGCanvas::ElementReference KammoGUI::SVGCanvas::ElementReference::add_element_clone(const std::string &new_id,
							      const KammoGUI::SVGCanvas::ElementReference &element_to_clone) {
	const char *nid = NULL;
	if(new_id != "") // if the string is empty we want to use the NULL pointer instead
		nid = new_id.c_str();
	if(_svg_inject_clone(nid, element, element_to_clone.element) != SVG_STATUS_SUCCESS) {
		throw OperationFailedException();
	}
	return KammoGUI::SVGCanvas::ElementReference(&element_to_clone, new_id);
}

void KammoGUI::SVGCanvas::ElementReference::get_viewport(SVGRect &rect) const {
	svg_length_t x, y, w, h;

	_svg_element_get_viewport(element, &x, &y, &w, &h);

	rect.x = x.value;
	rect.y = y.value;
	rect.width = w.value;
	rect.height = h.value;
}

void KammoGUI::SVGCanvas::ElementReference::get_boundingbox(SVGRect &rect) {
	if(element) {
		rect.x = element->bounding_box.left;
		rect.y = element->bounding_box.top;
		rect.width = element->bounding_box.right - element->bounding_box.left;
		rect.height = element->bounding_box.bottom - element->bounding_box.top;
	}
}

void KammoGUI::SVGCanvas::ElementReference::drop_element() {
	if(element)
		(void) svg_drop_element(source->svg_data->svg, element);
	else {
		KAMOFLAGE_ERROR("Trying to call ElementReference::drop_element() with unassigned ElementReference object.\n");
	}
}

void KammoGUI::SVGCanvas::ElementReference::debug_dump_render_tree() {
	KAMOFLAGE_ERROR("---------- BEGIN debug_dump_render_tree()\n");
	if(element) {
		svg_element_debug_render_tree(element);
	}
	KAMOFLAGE_ERROR("---------- END   debug_dump_render_tree()\n");
}

void KammoGUI::SVGCanvas::ElementReference::debug_dump_id_table() {
	KAMOFLAGE_ERROR("---------- BEGIN debug_dump_id_table()\n");
	if(element) {
		svg_element_debug_id_table(element);
	}
	KAMOFLAGE_ERROR("---------- END   debug_dump_id_table()\n");
}

void KammoGUI::SVGCanvas::ElementReference::debug_ref_count() {
	if(element)
		KAMOFLAGE_ERROR("          ---------------- ref count for (%p -> %p) : %d\n", this, element, element->ref_count);
	else
		KAMOFLAGE_ERROR("          ---------------- ref count for (%p -> %p) : null pointer, no count\n", this, element);
}

KammoGUI::SVGCanvas::ElementReference KammoGUI::SVGCanvas::ElementReference::find_child_by_class(const std::string &class_name) {
	svg_element_t *new_element = NULL;

	(void) _svg_fetch_element_by_class (source->svg_data->svg, class_name.c_str(), element, &new_element);

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

void KammoGUI::SVGCanvas::SVGDocument::get_canvas_size(int &width_in_pixels, int &height_in_pixels) {
	width_in_pixels = parent->__width;
	height_in_pixels = parent->__height;
}

void KammoGUI::SVGCanvas::SVGDocument::get_canvas_size_inches(float &width_in_inches, float &height_in_inches) {
	width_in_inches = parent->__width_inches;
	height_in_inches = parent->__height_inches;
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

double KammoGUI::SVGCanvas::SVGDocument::fit_to_inches(const SVGCanvas::ElementReference *element,
						       double inches_wide, double inches_tall) {
	int canvas_w, canvas_h;
	float canvas_w_inches, canvas_h_inches;
	KammoGUI::SVGCanvas::SVGRect document_size;

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


KammoGUI::SVGCanvas* KammoGUI::SVGCanvas::SVGDocument::get_parent() {
	return parent;
}

void KammoGUI::SVGCanvas::SVGDocument::start_animation(KammoGUI::Animation *new_animation) {
	if(new_animation != NULL) {
		KAMOFLAGE_DEBUG("start_new animation: %p (%p)\n", new_animation, &animations);
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

void KammoGUI::SVGCanvas::SVGDocument::stop_animation(Animation *animation_to_stop) {
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

KammoGUI::SVGCanvas::SVGCanvas(std::string _id, jobject jobj) :
	Widget(_id, jobj), __width(128), __height(128) {
}

KammoGUI::SVGCanvas::~SVGCanvas() {
	if(documents.size() > 0) {
		for(auto document : documents) {
			if(document->svg_data)
				svgAndroidDestroy(document->svg_data);

			delete document;
		}
		documents.clear();
	}

}

void KammoGUI::SVGCanvas::set_bg_color(double r, double g, double b) {
	r *= 255;
	g *= 255;
	b *= 255;
	bg_r = (jint)r;
	bg_g = (jint)g;
	bg_b = (jint)b;
}

void KammoGUI::SVGCanvas::canvas_expose(JNIEnv *env, KammoGUI::SVGCanvas *cnv, jobject canvas) {
	{ // Fill the canvas with the selected background color

		static jclass canvas_clazz;
		if(canvas_clazz == NULL) {
			canvas_clazz = env->FindClass("android/graphics/Canvas");
			canvas_clazz = (jclass)(env->NewGlobalRef((jobject)canvas_clazz));
		}

		static jmethodID canvas_drawRGB =
			env->GetMethodID(canvas_clazz, "drawRGB", "(III)V");

		env->CallVoidMethod(canvas, canvas_drawRGB, cnv->bg_r, cnv->bg_g, cnv->bg_b);
	}

	{ // process animations

		int active_animations = 0;

		// animate and render all documents
		for(auto document : cnv->documents) {
			document->process_active_animations();
			active_animations += document->number_of_active_animations();

			document->on_render();

			svgAndroidRender(env, document->svg_data, canvas);
		}

		// if no active animations left, stop animation
		if(active_animations == 0) {
			KAMOFLAGE_DEBUG(" will STOP animation now - no animations left.\n");

			pthread_t self = pthread_self();

			GET_INTERNAL_CLASS(jc, cnv->internal);
			static jmethodID mid = __ENV->GetMethodID(
				jc,
				"stop_animation","()V");

			__ENV->CallVoidMethod(
				cnv->internal, mid
				);
		}
	}

	// invalidate
	queue_for_invalidate(cnv);
}

extern "C" {
JNIEXPORT void Java_com_toolkits_kamoflage_SVGCanvasHelper_canvasExpose
(JNIEnv *env, jclass jc, jlong nativeID, jobject canvas) {
	KammoGUI::SVGCanvas *cnv = (KammoGUI::SVGCanvas *)nativeID;
	KammoGUI::SVGCanvas::canvas_expose(env, cnv, canvas);
}
}

void KammoGUI::SVGCanvas::canvas_resize(JNIEnv *env,
					KammoGUI::SVGCanvas *cnv,
					int width, int height, float width_inches, float height_inches) {
	cnv->__width = width;
	cnv->__height = height;
	cnv->__width_inches = width_inches;
	cnv->__height_inches = height_inches;

	// call the on_resize() callback
	for(auto document : cnv->documents) {
		document->on_resize();
	}
}

extern "C" {
JNIEXPORT void Java_com_toolkits_kamoflage_SVGCanvasHelper_canvasResize
(JNIEnv *env, jclass jc, jlong nativeID, jint width, jint height, jfloat width_inches, jfloat height_inches) {
	KammoGUI::SVGCanvas *cnv = (KammoGUI::SVGCanvas *)nativeID;
	KammoGUI::SVGCanvas::canvas_resize(env, cnv, width, height, width_inches, height_inches);
}
}

float KammoGUI::SVGCanvas::canvas_measure_inches(JNIEnv *env, KammoGUI::SVGCanvas *cnv, bool measureWidth) {
	float max = -1.0f;

	// get the preferred dimension of all documents
	for(auto document : cnv->documents) {
		float preferred = document->get_preferred_dimension(measureWidth ? HORIZONTAL_DIMENSION : VERTICAL_DIMENSION);
		if(preferred > max)
			max = preferred;
	}
	// if no document is present, return default of 0.1f;
	if(max == -1.0f)
		return 0.1f;

	return max;
}

extern "C" {
JNIEXPORT jfloat Java_com_toolkits_kamoflage_SVGCanvasHelper_canvasMeasureInches
(JNIEnv *env, jclass jc, jlong nativeID, jboolean measureWidth) {
	KammoGUI::SVGCanvas *cnv =
		(KammoGUI::SVGCanvas *)nativeID;
	return KammoGUI::SVGCanvas::canvas_measure_inches(env, cnv, measureWidth == JNI_TRUE ? true : false );
}
}

void KammoGUI::SVGCanvas::canvas_motion_event_init_event(
	JNIEnv *env, SVGCanvas *cnv, long downTime, long eventTime, int androidAction, int pointerCount, int actionIndex, float rawX, float rawY) {

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

	cnv->m_evt.init(downTime, eventTime, action, pointerCount, actionIndex, rawX, rawY);
}

extern "C" {
JNIEXPORT void Java_com_toolkits_kamoflage_SVGCanvasHelper_canvasMotionEventInitEvent
(JNIEnv *env, jclass jc, jlong nativeID, jlong downTime, jlong eventTime, jint androidAction, jint pointerCount, jint actionIndex, jfloat rawX, jfloat rawY) {
	KammoGUI::SVGCanvas *cnv =
		(KammoGUI::SVGCanvas *)nativeID;
	KammoGUI::SVGCanvas::canvas_motion_event_init_event(env, cnv, downTime, eventTime, androidAction, pointerCount, actionIndex, rawX, rawY);
}
}

void KammoGUI::SVGCanvas::canvas_motion_event_init_pointer(JNIEnv *env, SVGCanvas *cnv, int index, int id, float x, float y, float pressure) {
	cnv->m_evt.init_pointer(index, id, x, y, pressure);
}

extern "C" {
JNIEXPORT void Java_com_toolkits_kamoflage_SVGCanvasHelper_canvasMotionEventInitPointer
(JNIEnv *env, jclass jc, jlong nativeID, jint index, jint id, jfloat x, jfloat y, jfloat pressure) {
	KammoGUI::SVGCanvas *cnv =
		(KammoGUI::SVGCanvas *)nativeID;
	KammoGUI::SVGCanvas::canvas_motion_event_init_pointer(env, cnv, index, id, x, y, pressure);
}
}

void KammoGUI::SVGCanvas::canvas_motion_event(JNIEnv *env, SVGCanvas *cnv) {
	KAMOFLAGE_DEBUG("    canvas_motion_event (%f, %f)\n", cnv->m_evt.get_x(), cnv->m_evt.get_y());

	for(auto document : cnv->documents) {
		document->process_touch_for_animations();
	}
	switch(cnv->m_evt.get_action()) {
	case KammoGUI::MotionEvent::ACTION_DOWN:
	{
		svg_element_t *element = NULL;

		int k = cnv->documents.size() - 1;
		for(; k >= 0 && element == NULL; k--) {
			KammoGUI::SVGCanvas::SVGDocument *document = (cnv->documents[k]);
			element = svg_event_coords_match(document->svg_data->svg, cnv->m_evt.get_x(), cnv->m_evt.get_y());
		}

		if(element) {
			KAMOFLAGE_DEBUG("   KAMOFLAGE action_down on element: %p (custom: %p)\n", element, element->custom_data);
			if(element->custom_data == NULL) {
				cnv->active_element = NULL;
				KAMOFLAGE_ERROR("KammoGUI::SVGCanvas::canvas_motion_event() - event handler is missing it's ElementReference - you probably deleted the ElementReference after calling set_event_handler(). Or maybe you used only a temporary ElementReference object.");
			} else {
				cnv->active_element = (ElementReference *)element->custom_data;
			}
		} else
			cnv->active_element = NULL;
	}
		break;
	case KammoGUI::MotionEvent::ACTION_CANCEL:
	case KammoGUI::MotionEvent::ACTION_MOVE:
	case KammoGUI::MotionEvent::ACTION_OUTSIDE:
	case KammoGUI::MotionEvent::ACTION_POINTER_DOWN:
	case KammoGUI::MotionEvent::ACTION_POINTER_UP:
	case KammoGUI::MotionEvent::ACTION_UP:
		break;
	}

	if(cnv->active_element) {
		cnv->active_element->trigger_event_handler(cnv->m_evt);

		switch(cnv->m_evt.get_action()) {
		case KammoGUI::MotionEvent::ACTION_POINTER_DOWN:
		case KammoGUI::MotionEvent::ACTION_MOVE:
		case KammoGUI::MotionEvent::ACTION_DOWN:
		case KammoGUI::MotionEvent::ACTION_OUTSIDE:
		case KammoGUI::MotionEvent::ACTION_POINTER_UP:
			break;
		case KammoGUI::MotionEvent::ACTION_CANCEL:
		case KammoGUI::MotionEvent::ACTION_UP:
			cnv->active_element = NULL;
			break;
		}
	}
}

extern "C" {
JNIEXPORT void Java_com_toolkits_kamoflage_SVGCanvasHelper_canvasMotionEvent
(JNIEnv *env, jclass jc, jlong nativeID) {
	KammoGUI::SVGCanvas *cnv =
		(KammoGUI::SVGCanvas *)nativeID;
	KammoGUI::SVGCanvas::canvas_motion_event(env, cnv);
	KammoGUI::SVGCanvas::flush_invalidation_queue();
}
}
