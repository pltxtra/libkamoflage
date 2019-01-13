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
#include <cxxabi.h>

#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#include <math.h>

#include "kamogui.hh"
#include "gnuVGcanvas.hh"

#include "jngldrum/jthread.hh"
#include "jngldrum/jinformer.hh"

#include "libsvg/svgint.h"

//#define __DO_KAMOFLAGE_DEBUG
#include "kamoflage_debug.hh"

JavaVM *__g_jvm = NULL;
std::map<pthread_t, JNIEnv *> __envm; // one env for each thread..

void __setup_env_for_thread(JNIEnv *env) {
	pthread_t self = pthread_self();
	if(__envm.find(self) == __envm.end()) __envm[self] = env;

}

JNIEnv *get_env_for_thread() {
	pthread_t self = pthread_self();
	if(__envm.find(self) == __envm.end()) {
#if 0
		KAMOFLAGE_DEBUG_("DID NOT FIND JNI ENV FOR THIS THREAD! %ld --- will try and register\n", self);

		JNIEnv *env = NULL;

		__g_jvm->AttachCurrentThread(&env, NULL);

		KAMOFLAGE_DEBUG_("                                         --- env pointer %p\n", env);

		__setup_env_for_thread(env);

		return env;

#else
		KAMOFLAGE_DEBUG_("DID NOT FIND JNI ENV FOR THIS THREAD! %ld\n", self);
		throw jException(
			"JNIEnv not found for calling thread!", jException::sanity_error);
#endif
	}
	return __envm[self];
}


bool __is_ui_thread() {
	// these are created to optimize over time for repeated calls
	// to the __is_ui_thread() method
	static pthread_t uiThreadId = 0;
	static std::set<pthread_t> nonUiThreadId;
	static jThread::Monitor mtr;

	JNIEnv *env = NULL;

	try {
		env = get_env_for_thread();
	} catch(...) { /* ignore */ }

	// if we don't have a stored env we can't be the UI thread..
	if(env == NULL) return false;

	// this does not have to be protected since uiThreadId
	// can only be changed by the UI thread
	// and if this is read at the same time the result
	// will be only that one final call to isUiThread might
	// slip through. This is not critical, so we prefer lock less since it's faster.
	if(uiThreadId != 0) {
		return (uiThreadId == pthread_self()) ? true : false;
	}

	{ // Access to the nonUiThreadId set has to be mutex protected since it is a complex structure
		jThread::Monitor::MonitorGuard g(&mtr);
		if(nonUiThreadId.find(pthread_self()) != nonUiThreadId.end())
			return false;
	}

	env->PushLocalFrame(32);
	jclass jc = NULL;
	jc = env->FindClass("com/toolkits/kamoflage/Kamoflage");

	if(jc) {
		jmethodID mid = env->GetStaticMethodID(jc, "isUIThread", "()Z");
		if(mid) {
			jboolean rval = env->CallStaticBooleanMethod(jc, mid);
			env->PopLocalFrame(NULL);

			if(rval == JNI_TRUE) {
				uiThreadId = pthread_self();
				return true;
			}

			{
				jThread::Monitor::MonitorGuard g(&mtr);
				nonUiThreadId.insert(pthread_self());
			}
			return false;
		}
	}
	env->PopLocalFrame(NULL);
	return false;
}

/************* Event ***************/
std::multimap<std::string, KammoGUI::EventHandler *> *KammoGUI::EventHandler::event_handlers = NULL;

KammoGUI::EventHandler::~EventHandler() {
}

KammoGUI::EventHandler::EventHandler(std::string ids) {
//	std::cout << "new event class declared for widgets with ids: " << ids << "\n"; fflush(0);
	this->id = id;
//	return;

	/* Since the OpenMoko version of GCC/G++/STL does not initiate static
	 * std::map<> objects (i.e. non "new"-allocated ones..)
	 * I had to resort to this...
	 */
	if(event_handlers == NULL)
		event_handlers = new std::multimap<std::string, KammoGUI::EventHandler *>();

	int k = -1;
	std::string id;

	do {
		if(k != -1)
			ids.erase(k, ids.size());
		k = ids.rfind(":", ids.size());
		id = ids.substr(k + 1, ids.size());
		(*event_handlers).insert(
			std::pair<std::string, KammoGUI::EventHandler *>(id,this));
	} while(k != -1);
}

std::vector<KammoGUI::EventHandler *>KammoGUI::EventHandler::find_handlers(
	KammoGUI::Widget *wid) {

	std::vector<KammoGUI::EventHandler *> result;

	static std::multimap<std::string, KammoGUI::EventHandler *>::iterator evhi;

	for(evhi  = event_handlers->find(wid->id);
	    (evhi != event_handlers->end()) && ((*evhi).first == wid->id);
	    evhi++) {
		result.push_back((*evhi).second);
	}
	return result;
}

void KammoGUI::EventHandler::handle_on_init(KammoGUI::Widget *wid) {
	pthread_t self = pthread_self();

	for(auto k : wid->evh) {
		__ENV->PushLocalFrame(32);
		try {
			k->on_init(wid);
		} catch(jException e) {
			KAMOFLAGE_ERROR("on_init() caught an exception: %s\n", e.message.c_str());
			exit(1);
		}
		__ENV->PopLocalFrame(NULL);
	}
}

void KammoGUI::EventHandler::on_init(KammoGUI::Widget *widget) {
}

void KammoGUI::EventHandler::handle_on_modify(KammoGUI::Widget *wid) {
	for(auto k : wid->evh)
		k->on_modify(wid);
}

void KammoGUI::EventHandler::on_modify(KammoGUI::Widget *widget) {
}

void KammoGUI::EventHandler::handle_on_value_changed(KammoGUI::Widget *wid) {
	for(auto k : wid->evh)
		k->on_value_changed(wid);
}

void KammoGUI::EventHandler::on_value_changed(KammoGUI::Widget *widget) {
}

void KammoGUI::EventHandler::handle_on_select_row(KammoGUI::Widget *wid, KammoGUI::List::iterator row) {
	for(auto k : wid->evh)
		k->on_select_row(wid, row);
}

void KammoGUI::EventHandler::on_select_row(
	KammoGUI::Widget *widget, KammoGUI::List::iterator row) {
}

void KammoGUI::EventHandler::handle_on_click(KammoGUI::Widget *wid) {
	for(auto k : wid->evh)
		k->on_click(wid);
}

void KammoGUI::EventHandler::on_click(KammoGUI::Widget *widget) {
}

void KammoGUI::EventHandler::handle_on_double_click(KammoGUI::Widget *wid) {
	for(auto k : wid->evh)
		k->on_double_click(wid);
}

void KammoGUI::EventHandler::on_double_click(KammoGUI::Widget *widget) {
}

void KammoGUI::EventHandler::handle_on_destroy(KammoGUI::Widget *wid) {
	for(auto k : wid->evh)
		k->on_destroy(wid);
}

void KammoGUI::EventHandler::on_destroy(KammoGUI::Widget *widget) {
}

bool KammoGUI::EventHandler::handle_on_delete(KammoGUI::Widget *wid) {
	bool retval = true;
	for(auto k : wid->evh)
		if(!(k->on_delete(wid))) retval = false;
	return retval;
}

bool KammoGUI::EventHandler::on_delete(KammoGUI::Widget *widget) {
	return true;
}

void KammoGUI::EventHandler::handle_on_poll(KammoGUI::Widget *wid) {
	for(auto k : wid->evh)
		k->on_poll(wid);
}

void KammoGUI::EventHandler::on_poll(KammoGUI::Widget *widget) {
}

void KammoGUI::EventHandler::trigger_user_event(KammoGUI::UserEvent *ue, std::map<std::string, void *> data) {
	run_on_GUI_thread([ue, data]() {
			if(ue == NULL)
				throw jException("Can't trigger user event at address zero. (NULL pointer exception.)",
						 jException::sanity_error);
			KammoGUI::Widget *wid = (KammoGUI::Widget *)ue;

			// notify Java runtime that we are triggering a user event
			pthread_t self = pthread_self();
			// OK, prepare to call....
			GET_INTERNAL_CLASS(jc, wid->internal);
			static jmethodID mid = __ENV->GetMethodID(jc,
								  "trigger_user_event","()V");
			// OK, finally time to call the trigger_user_event() java method.. PUH!
			__ENV->CallVoidMethod(
				wid->internal, mid // object and method reference
				);

			// OK, do the real event stuff
			for(auto k : wid->evh)
				k->on_user_event(ue, data);
		}
		);
}

void KammoGUI::EventHandler::trigger_user_event(KammoGUI::UserEvent *ue) {
	std::map<std::string, void *> empty;

	trigger_user_event(ue, empty);
}

void KammoGUI::EventHandler::on_user_event(KammoGUI::UserEvent *ev, std::map<std::string, void *> data) {
}

std::string KammoGUI::EventHandler::get_id() {
	return id;
}

/************* PollEvent **************/
KammoGUI::PollEvent::PollEvent(std::string _id, jobject jobj) : Widget(_id, jobj) {
}

/************* UserEvent **************/
KammoGUI::UserEvent::UserEvent(std::string _id, jobject jobj) : Widget(_id, jobj) {
}

/************* Widget **************/

void KammoGUI::Widget::register_id() {
	if(widgets.find(id) != widgets.end()) {
		std::string msg = std::string("The ID is already in use. [") + id + "]";
		id = "____FAILED____";
		throw jException(msg, jException::sanity_error);
	} else
		widgets[id] = this;

	evh = KammoGUI::EventHandler::find_handlers(this);
}

/*** BEGIN ANDROID SPECIFIC INVALIDATION STUFF ***/
void KammoGUI::Widget::invalidate() {
	pthread_t self = pthread_self();
	// OK, call the java method now
	GET_INTERNAL_CLASS(jc,internal);
	static jmethodID mid = __ENV->GetMethodID(
		jc,
		"invalidate_view","()V");

	// OK, finally time to call the invalidate_view java method.. PUH!
	__ENV->CallVoidMethod(
		internal, mid // object and method reference
		);
}

std::vector<KammoGUI::Widget *> KammoGUI::Widget::invalidation_queue;

void KammoGUI::Widget::queue_for_invalidate(Widget *wid) {
	if(wid->queued_for_invalidation) return;

	invalidation_queue.push_back(wid);
}

void KammoGUI::Widget::flush_invalidation_queue() {
	for(auto k : invalidation_queue) {
		k->queued_for_invalidation = false;
		k->invalidate();
	}

	invalidation_queue.clear();
}

/*** END ANDROID SPECIFIC INVALIDATION STUFF ***/

KammoGUI::Widget::Widget(bool this_is_ignored) : queued_for_invalidation(false) {
	char uniq_id_buffer[128];
	snprintf(uniq_id_buffer, 128, "__INTERNAL_UNIQ_PTR_%p", this);

	id = std::string(uniq_id_buffer);

	register_id();
}

void KammoGUI::Widget::show_now() {
	pthread_t self = pthread_self();
	// OK, call the java method now
	GET_INTERNAL_CLASS(jc,internal);
	static jmethodID mid = __ENV->GetMethodID(
		jc,
		"show_now","()V");

	// OK, finally time to call the show_now java method.. PUH!
	__ENV->CallVoidMethod(
		internal, mid // object and method reference
		);
}

std::string KammoGUI::Widget::get_id() {
	return id;
}

std::string KammoGUI::Widget::get_title() {
	// XXX not implemented right
	return "<not implemented>";
}

bool KammoGUI::Widget::get_expand() {
	// XXX not implemented right
	return false;
}

bool KammoGUI::Widget::get_fill() {
	// XXX not implemented right
	return false;
}

void KammoGUI::Widget::set_expand(bool xp) {
	pthread_t self = pthread_self();
	// OK, call the java method now
	GET_INTERNAL_CLASS(jc,internal);
	static jmethodID mid = __ENV->GetMethodID(
		jc,
		"set_expand","(Z)V");

	// OK, finally time to call the show_now java method.. PUH!
	__ENV->CallVoidMethod(
		internal, mid, // object and method reference
		xp
		);
}

void KammoGUI::Widget::set_fill(bool fl) {
	pthread_t self = pthread_self();
	// OK, call the java method now
	GET_INTERNAL_CLASS(jc,internal);
	static jmethodID mid = __ENV->GetMethodID(
		jc,
		"set_fill","(Z)V");

	// OK, finally time to call the show_now java method.. PUH!
	__ENV->CallVoidMethod(
		internal, mid, // object and method reference
		fl
		);
}

int KammoGUI::Widget::get_border_width() {
	// XXX not implemented right
	return 0;
}

void KammoGUI::Widget::redraw() {
	invalidate();
}

void KammoGUI::Widget::attach_event_handler(EventHandler *_evh) {
	evh.push_back(_evh);
}

void KammoGUI::Widget::inject_key_event(bool is_press, char chr) {
}

KammoGUI::Widget::Widget(std::string _id, jobject _jobj) : internal(_jobj), id(_id), queued_for_invalidation(false) {
	register_id();
}

KammoGUI::Widget::Widget(std::string _id) : id(_id), queued_for_invalidation(false) {
	register_id();
}

KammoGUI::Widget::~Widget() {
	KAMOFLAGE_DEBUG_("ERASING WIDGET %s\n", id.c_str()); fflush(0);

	pthread_t self = pthread_self();

	static jclass jc = NULL;
	if(jc == NULL) {
		jc = __ENV->FindClass("com/toolkits/kamoflage/Kamoflage$Widget");
		jc = (jclass)(__ENV->NewGlobalRef((jobject)jc));
	}
	static jmethodID mid = __ENV->GetMethodID(jc, "deregister_self", "()V");
	__ENV->CallVoidMethod(
		internal, mid
		);
	KAMOFLAGE_DEBUG_("JAVA erasure done, proceeding native.\n"); fflush(0);

	std::map<std::string, KammoGUI::Widget *>::iterator k = widgets.find(id);
	if(k != widgets.end()) widgets.erase(k);

	// make sure the Java object can be garbage collected...
	__ENV->DeleteGlobalRef(internal);
}

std::map<std::string, KammoGUI::Widget *> KammoGUI::Widget::widgets;
void KammoGUI::Widget::call_on_init() {
	/* run init handlers */
	std::map<std::string, KammoGUI::Widget *>::iterator k;
	for(k  = Widget::widgets.begin();
	    k != Widget::widgets.end();
	    k++) {
		EventHandler::handle_on_init((*k).second);
	}
}

KammoGUI::Widget *KammoGUI::Widget::get_widget(std::string id) {
	std::map<std::string, KammoGUI::Widget *>::iterator k;
	k = widgets.find(id);
	if(k != widgets.end()) return (*k).second;

	KAMOFLAGE_DEBUG_("  Could not find widget %s\n", id.c_str()); fflush(0);
	throw jException(
		std::string("No such widget found. [") + id + "]",
		jException::sanity_error);

}

/************ Surface ***********/

void KammoGUI::Surface::trigger_surface_event(KammoGUI::surfaceEvent_t se, int x, int y) {
	KAMOFLAGE_DEBUG_("  trying to TRIGGER SURFACE\n"); fflush(0);
	if(surface_event_cb != NULL)
		surface_event_cb(cbdat, se, x, y);
}

void KammoGUI::Surface::set_callbacks(
			void *callback_data,
			void (*surface_event)(void *callback_data, KammoGUI::surfaceEvent_t se,
					      int x, int y)
	) {
	cbdat = callback_data;
	surface_event_cb = surface_event;
}

KammoGUI::Surface::Surface(std::string _id, jobject jobj) : Widget(_id, jobj), cbdat(NULL), surface_event_cb(NULL) {
}

/************ Canvas ***********/

KammoGUI::Canvas::SVGDefinition::SVGDefinition() : internal(NULL) {
}

KammoGUI::Canvas::SVGDefinition::~SVGDefinition() {
	if(internal != NULL)
		svgAndroidDestroy(internal);
}

KammoGUI::Canvas::SVGDefinition *KammoGUI::Canvas::SVGDefinition::from_file(const std::string &fname) {
	KammoGUI::Canvas::SVGDefinition *result = new SVGDefinition();
	result->file_name = fname;
	result->internal = svgAndroidCreate();
	svgAndroidSetAntialiasing(result->internal, JNI_TRUE);

	if(result->internal == NULL)
		throw jException("Failed to allocate internal SVG structure",
				 jException::sanity_error);
//	KAMOFLAGE_DEBUG_("   trying to read SVG from %s\n", fname.c_str()); fflush(0);
	svg_status_t st = svg_parse(result->internal->svg, fname.c_str());
	if(st) {
		std::ostringstream ost;
		ost << "Failed to create internal SVG representation, code " << st << " (for file: " << fname << ")";

		(void)svgAndroidDestroy(result->internal);
		throw jException(
			ost.str(),
			//"Failed to create internal SVG representation",
				 jException::sanity_error);
	}

	return result;
}

KammoGUI::Canvas::SVGDefinition *KammoGUI::Canvas::SVGDefinition::from_string(
	const std::string &xml) {

	KammoGUI::Canvas::SVGDefinition *result = new SVGDefinition();
	result->file_name = "<from string>";
	result->internal = svgAndroidCreate();

	const char *bfr = xml.c_str();
	svg_status_t st = svg_parse_buffer(result->internal->svg, bfr, strlen(bfr));
	if(st) {
		FILE *f = fopen("/data/data/com.toolkits.kamoflage/svg_out.svg", "w");
		if(f != NULL) {
			fprintf(f, "%s", bfr);
			fclose(f);
		} else KAMOFLAGE_DEBUG_(" FAIIIIIL!\n");
		fflush(0);
		(void)svgAndroidDestroy(result->internal);
		KAMOFLAGE_DEBUG_("   svg_status_t = %d\n", st); fflush(0);
		throw jException("Failed to create internal SVG representation",
				 jException::sanity_error);
	}

	return result;
}

KammoGUI::Canvas::SVGBob::SVGBob() {
	throw jException("Operation not allowed.", jException::sanity_error);
}

KammoGUI::Canvas::SVGBob::SVGBob(Canvas *_trgt, SVGDefinition *_svgd) :
	dirty_flag(true), bob_internal(NULL), svgd(_svgd), target(_trgt), bl_w(-1), bl_h(-1), last_w(-1), last_h(-1) {

//	cairo_matrix_init_identity(&transformation);
}

KammoGUI::Canvas::SVGBob::~SVGBob() {
 	if(bob_internal) {
		target->env->DeleteGlobalRef(bob_internal);
		bob_internal = NULL;
	}
}

void KammoGUI::Canvas::SVGBob::prepare_bob() {
	if(!dirty_flag) return;
	if(bl_w == -1 || bl_h == -1) return;

	if(target->canvas == NULL) return;

	if(last_w == bl_w && last_h == bl_h) return;

	if(svgd == NULL || svgd->internal == NULL || svgd->internal->svg == NULL) {
		KAMOFLAGE_DEBUG_("Warning: prepare_bob called for SVGBob without a proper definition."
		       " Possibly there was a failure previously when trying to parse the SVG data.\n");
		fflush(0);
		return;
	}

 	if(bob_internal) {
		target->env->DeleteGlobalRef(bob_internal);
		bob_internal = NULL;
	}

	last_w = bl_w; last_h = bl_h;
	dirty_flag = false;

	double raster_width, raster_height;

	svg_length_t width, height;
	double width_P, height_P; // pixel values

	svg_get_size(svgd->internal->svg, &width, &height);

	if(width.unit == SVG_LENGTH_UNIT_PCT ||
	   height.unit == SVG_LENGTH_UNIT_PCT) {
		width_P = bl_w;
		height_P = bl_h;

		width_P *= width.value / 100.0;
		height_P *= height.value / 100.0;

		raster_width = (int)width_P;
		raster_height = (int)height_P;
	} else {
		_svg_android_length_to_pixel(svgd->internal, &width, &width_P);
		_svg_android_length_to_pixel(svgd->internal, &height, &height_P);

		double x = ((double)bl_w) / width_P;
		double y = ((double)bl_h) / height_P;

		double scale = x > y ? x : y;

		raster_width = width_P * scale;
		raster_height = height_P * scale;
	}

//	std::cout << " **** Prepareing bob for ]" << svgd->file_name << "[\n";
//	KAMOFLAGE_DEBUG_("  Acquired size from internal: %f, %f\n", width_P, height_P); fflush(0);
//	KAMOFLAGE_DEBUG_("  Blit size                  : %d, %d\n", bl_w, bl_h); fflush(0);
//	KAMOFLAGE_DEBUG_("  Calculated raster size     : %d, %d\n", raster_width, raster_height); fflush(0);

	// libsvg-android always keeps aspect ratio to the smalles scale, that's why
	// we make sure we scale to the BIGGEST so we won't lose info..
	bob_internal = target->env->CallStaticObjectMethod(target->raster_clazz,
							   target->raster_createBitmap,
							   (int)raster_width, (int)raster_height
		);
	if(bob_internal == NULL) {
		KAMOFLAGE_DEBUG_("Failure in kamogui; Bob was not created.. We miss him.. :-(\n");
		exit(-1);
	}
	bob_internal =
		target->env->NewGlobalRef(bob_internal);

	if(bob_internal == NULL) {
		KAMOFLAGE_DEBUG_("Failure in kamogui; Bob could not be referenced.. :-(\n");
		exit(-1);
	}
//	KAMOFLAGE_DEBUG_("  Bob was created!\n"); fflush(0);

	jobject bobs_canvas = target->env->NewObject(
		target->canvas_clazz, target->canvas_constructor, bob_internal);

	if(bobs_canvas == NULL) {
		KAMOFLAGE_DEBUG_("Failure in kamogui; Could not create canvas for Bob, he want's it bad.\n");
		exit(-1);
	}

//	KAMOFLAGE_DEBUG_("  Bob's canvas was created!\n"); fflush(0);

//	KAMOFLAGE_DEBUG_("  dbg: %p, %p, %p\n",
//	       target->env, svgd->internal, bobs_canvas); fflush(0);
	svgAndroidRenderToArea(
		target->env,
		svgd->internal, bobs_canvas, 0, 0, raster_width, raster_height);

//	KAMOFLAGE_DEBUG_("Pre DeleteLocalRef\n");
//	target->env->DeleteLocalRef(bobs_canvas);

//	KAMOFLAGE_DEBUG_("  Bob was rendered to canvas!\n"); fflush(0);

	// calculate blit matrix values (used when the bitmap is drawn to the canvas..)
	blit_x_scale = bl_w / raster_width;
	blit_y_scale = bl_h / raster_height;
}

void KammoGUI::Canvas::SVGBob::set_blit_size(int w, int h) {
	if(w == bl_w && h == bl_h) return;

	bl_w = w;
	bl_h = h;

	dirty_flag = true;
}

void KammoGUI::Canvas::SVGBob::change_definition(SVGDefinition *_svgd) {
	svgd = _svgd;

	dirty_flag = true;
}

void KammoGUI::Canvas::start_animation(KammoGUI::Animation *new_animation) {
	current_animation = new_animation;

	if(current_animation) current_animation->start();

	queue_for_invalidate(this);

	pthread_t self = pthread_self();

	GET_INTERNAL_CLASS(jc, internal);
	static jmethodID mid = __ENV->GetMethodID(
		jc,
		"start_animation","()V");

	__ENV->CallVoidMethod(
		internal, mid
		);
}

void KammoGUI::Canvas::stop_animation() {
	if(current_animation) delete current_animation;
	current_animation = NULL;

	pthread_t self = pthread_self();

	GET_INTERNAL_CLASS(jc, internal);
	static jmethodID mid = __ENV->GetMethodID(
		jc,
		"stop_animation","()V");

	__ENV->CallVoidMethod(
		internal, mid
		);
}

void KammoGUI::Canvas::blit_SVGBob(SVGBob *svgb, int x, int y) {
	if(svgb->dirty_flag) svgb->prepare_bob();
	if(svgb->bob_internal == NULL) return;
	queue_for_invalidate(this);

	jobject mtrx;

	svg_android_t *e = svgb->svgd->internal;
	mtrx =
		e->env->CallStaticObjectMethod(
			svgb->target->raster_clazz,
			svgb->target->raster_matrixCreate,
			svgb->blit_x_scale, 0.0, 0.0, svgb->blit_y_scale, (double)x, (double)y);
	e->env->CallVoidMethod(svgb->target->canvas,
			       svgb->target->canvas_draw_bitmap,
			       svgb->bob_internal, mtrx, NULL);
	e->env->DeleteLocalRef(mtrx);
}

void KammoGUI::Canvas::set_bg_color(double r, double g, double b) {
	r *= 255;
	g *= 255;
	b *= 255;
	bg_r = (jint)r;
	bg_g = (jint)g;
	bg_b = (jint)b;
}

void KammoGUI::Canvas::canvas_expose(JNIEnv *env, void *callback_data, jobject canvas) {
	KammoGUI::Canvas *cnv =
		(KammoGUI::Canvas *)callback_data;

	cnv->env = env;
	cnv->canvas = canvas;

	if(cnv->canvas_clazz == NULL) {
		cnv->canvas_clazz = env->FindClass("android/graphics/Canvas");
		cnv->canvas_clazz = (jclass)(env->NewGlobalRef((jobject)cnv->canvas_clazz));
	}

	static jmethodID canvas_drawRGB =
		env->GetMethodID(cnv->canvas_clazz, "drawRGB", "(III)V");

	env->CallVoidMethod(canvas, canvas_drawRGB, cnv->bg_r, cnv->bg_g, cnv->bg_b);

	cnv->canvas_constructor = env->GetMethodID(
		cnv->canvas_clazz, "<init>", "(Landroid/graphics/Bitmap;)V");
	cnv->canvas_draw_bitmap = env->GetMethodID(
		cnv->canvas_clazz, "drawBitmap",
		"(Landroid/graphics/Bitmap;Landroid/graphics/Matrix;Landroid/graphics/Paint;)V");

	// prepare raster
	if(cnv->raster_clazz == NULL) {
		cnv->raster_clazz = env->FindClass("com/toolkits/libsvgandroid/SvgRaster");
		cnv->raster_clazz = (jclass)(env->NewGlobalRef((jobject)cnv->raster_clazz));
	}

	cnv->raster_createBitmap = env->GetStaticMethodID(
		cnv->raster_clazz, "createBitmap",
		"(II)Landroid/graphics/Bitmap;"
		);

	cnv->raster_matrixCreate = env->GetStaticMethodID(
		cnv->raster_clazz, "matrixCreate",
		"(FFFFFF)Landroid/graphics/Matrix;"
		);


//	KAMOFLAGE_DEBUG_("*******   ENTER EXPOSE (%p, %p)!!!! **********", cnv, cnv->canvas);
	if(cnv->current_animation) {
//		KAMOFLAGE_DEBUG("   new tick..\n");
		cnv->current_animation->new_time_tick();
		if(cnv->current_animation->has_finished()) {
//			KAMOFLAGE_DEBUG("   animation finished..\n");
			cnv->stop_animation();
		}
	}

	if(cnv->canvas_expose_cb != NULL) {
		cnv->canvas_expose_cb(cnv->__cbd);
	}
//	KAMOFLAGE_DEBUG_("*******   EXIT EXPOSE!!!! **********");
}

extern "C" {
JNIEXPORT void Java_com_toolkits_kamoflage_CanvasHelper_canvasExpose
(JNIEnv *env, jclass jc, jlong nativeID, jobject canvas) {
	KammoGUI::Canvas *cnv = (KammoGUI::Canvas *)nativeID;

	KammoGUI::Canvas::canvas_expose(env, cnv, canvas);
}
}

void KammoGUI::Canvas::canvas_resize(JNIEnv *env,
				     void *callback_data,
				     int width, int height) {
	KammoGUI::Canvas *cnv =
		(KammoGUI::Canvas *)callback_data;
	cnv->env = env;
	cnv->__width = width;
	cnv->__height = height;
	if(cnv->canvas_resize_cb != NULL) {
		cnv->canvas_resize_cb(cnv->__cbd, width, height);
	}
}

extern "C" {
JNIEXPORT void Java_com_toolkits_kamoflage_CanvasHelper_canvasResize
(JNIEnv *env, jclass jc, jlong nativeID, jint width, jint height) {
	KammoGUI::Canvas *cnv = (KammoGUI::Canvas *)nativeID;
	KammoGUI::Canvas::canvas_resize(env, cnv, width, height);
}
}

void KammoGUI::Canvas::canvas_event(JNIEnv *env, void *callback_data,
				    KammoGUI::canvasEvent_t ce, int x, int y) {
	KammoGUI::Canvas *cnv =
		(KammoGUI::Canvas *)callback_data;
	if(cnv->current_animation) {
		cnv->current_animation->touch_event();
		if(cnv->current_animation->has_finished()) {
			cnv->stop_animation();
		}
	}

	if(cnv->canvas_event_cb != NULL) {
		cnv->canvas_event_cb(cnv->__cbd, ce, x, y);
	}
}

extern "C" {
JNIEXPORT void Java_com_toolkits_kamoflage_CanvasHelper_canvasEvent
(JNIEnv *env, jclass jc, jlong nativeID, jint cev, jint x, jint y) {
	KammoGUI::Canvas *cnv =
		(KammoGUI::Canvas *)nativeID;
	KammoGUI::Canvas::canvas_event(env, cnv, (KammoGUI::canvasEvent_t)cev, x, y);
	KammoGUI::Canvas::flush_invalidation_queue();
}
}

float KammoGUI::Canvas::canvas_measure_inches(JNIEnv *env, void *callback_data, bool measureWidth) {
	KammoGUI::Canvas *cnv =
		(KammoGUI::Canvas *)callback_data;
	if(cnv->canvas_measure_inches_cb != NULL) {
		return cnv->canvas_measure_inches_cb(cnv->__cbd, measureWidth);
	}
	return 0.0f;
}

extern "C" {
JNIEXPORT jfloat Java_com_toolkits_kamoflage_CanvasHelper_canvasMeasureInches
(JNIEnv *env, jclass jc, jlong nativeID, jboolean measureWidth) {
	KammoGUI::Canvas *cnv =
		(KammoGUI::Canvas *)nativeID;
	return KammoGUI::Canvas::canvas_measure_inches(env, cnv, measureWidth == JNI_TRUE ? true : false );
}
}

void KammoGUI::Canvas::set_callbacks(
	void *callback_data,
	void (*_canvas_expose_cb)(void *callback_data),
	void (*_canvas_resize_cb)(void *callback_data,
			      int width, int height),
	void (*_canvas_event_cb)(void *callback_data, KammoGUI::canvasEvent_t ce,
			     int x, int y),
	float (*_canvas_measure_inches_cb)(void *callback_data, bool measureWidth)
	) {
	__cbd = callback_data;
	canvas_expose_cb = _canvas_expose_cb;
	canvas_resize_cb = _canvas_resize_cb;
	canvas_event_cb = _canvas_event_cb;
	canvas_measure_inches_cb = _canvas_measure_inches_cb;
	return;
}

KammoGUI::Canvas::Canvas(std::string _id, jobject jobj) :
	Widget(_id, jobj), current_animation(NULL), __width(128), __height(128), canvas_clazz(NULL), raster_clazz(NULL), __cbd(NULL), canvas_expose_cb(NULL), canvas_resize_cb(NULL), canvas_event_cb(NULL), canvas_measure_inches_cb(NULL) {
}

KammoGUI::Canvas::~Canvas() {
	pthread_t self = pthread_self();
	if(canvas_clazz != NULL) {
		__ENV->DeleteGlobalRef(canvas_clazz);
	}
	if(raster_clazz != NULL) {
		__ENV->DeleteGlobalRef(raster_clazz);
	}
}

/************* List ***************/
KammoGUI::List::NoRowSelected::NoRowSelected()  : jException("No row is selected", jException::sanity_error) {
}

KammoGUI::List::iterator::iter_object::iter_object() : iter(-1), ref_count(1) {
}

KammoGUI::List::iterator::iterator(const KammoGUI::List::iterator &old) : iter(NULL) {
	*this = old;
}

KammoGUI::List::iterator::iterator() : iter(NULL) {
	iter = new iter_object();
}

KammoGUI::List::iterator::~iterator() {
	iter->ref_count--;

	if(!(iter->ref_count)) {
		delete iter;
		iter = NULL;
	}
}

KammoGUI::List::iterator& KammoGUI::List::iterator::operator =(const KammoGUI::List::iterator &i) {
	if(iter != NULL) {
		iter->ref_count--;
		if(!(iter->ref_count)) {
			delete iter;
			iter = NULL;
		}
	}

	iter = i.iter;
	iter->ref_count++;

	return *this;
}

KammoGUI::List::List(std::string _id, jobject jobj) : Widget(_id, jobj) {
}

void KammoGUI::List::trigger_on_select_row_event(jint rowid) {
	iterator iter;

	iter.iter->iter = rowid;

	KammoGUI::EventHandler::handle_on_select_row(this, (iterator)iter);
}


void KammoGUI::List::add_row(std::vector<std::string> data) {
	rows.push_back(data);

	pthread_t self = pthread_self();

	__ENV->PushLocalFrame(32);

	GET_INTERNAL_CLASS(jc,internal);
	static jmethodID add_row = __ENV->GetMethodID(jc, "add_row", "(Ljava/util/Vector;)V");

	// Get vector object class
	static jclass vec_class = NULL;
	if(vec_class == NULL) {
		vec_class = __ENV->FindClass("java/util/Vector");
		vec_class = (jclass)(__ENV->NewGlobalRef((jobject)vec_class));
	}
	// Get vector constructor
	static jmethodID vec_new = __ENV->GetMethodID( vec_class, "<init>","()V");
	// Get "addElement" method id of clase vector
	static jmethodID vec_add = __ENV->GetMethodID( vec_class, "addElement", "(Ljava/lang/Object;)V");
	std::vector<std::string>::iterator k;

	// create vector object
	jobject vect = __ENV->NewObject( vec_class, vec_new);

	for(k = data.begin(); k != data.end(); k++) {
		__ENV->CallVoidMethod(vect, vec_add, __ENV->NewStringUTF((*k).c_str()) );
	}

	// add the vector to the list
	__ENV->CallVoidMethod(internal, add_row, vect);

	__ENV->PopLocalFrame(NULL);
}

void KammoGUI::List::remove_row(KammoGUI::List::iterator &iter) {
	int kount = 0;
	for(auto k = rows.begin(); k != rows.end(); k++, kount++) {
		if(kount == iter.iter->iter) {
			rows.erase(k);
			break;
		}
	}

	pthread_t self = pthread_self();
	GET_INTERNAL_CLASS(jc,internal);
	static jmethodID mid = __ENV->GetMethodID(jc, "remove_row", "(I)V");

	KAMOFLAGE_DEBUG_("Calling List.remove_row()...\n"); fflush(0);

	__ENV->CallVoidMethod(internal, mid, iter.iter->iter);
	KAMOFLAGE_DEBUG_(" --- List.remove_row() called!!\n"); fflush(0);
}

void KammoGUI::List::clear(void) {
	rows.clear();

	pthread_t self = pthread_self();
	GET_INTERNAL_CLASS(jc,internal);
	static jmethodID clear = __ENV->GetMethodID(jc, "clear", "()V");

	__ENV->CallVoidMethod(internal, clear);
}

KammoGUI::List::iterator KammoGUI::List::get_selected() {
	pthread_t self = pthread_self();
	GET_INTERNAL_CLASS(jc,internal);
	static jmethodID mid = __ENV->GetMethodID(jc, "get_selected", "()I");

	KAMOFLAGE_DEBUG_("Calling List.get_selected()...\n"); fflush(0);

	jint selected = (jint)__ENV->CallIntMethod(internal, mid);
	KAMOFLAGE_DEBUG_(" --- List.get_selected() called (returned: %d)!!\n", selected); fflush(0);

	if(__ENV->ExceptionOccurred()) {
		__ENV->ExceptionDescribe();
	}

	if(selected == -1) throw NoRowSelected();

	iterator iter;
	iter.iter->iter = selected;

	return iter;
}

std::string KammoGUI::List::get_value(const KammoGUI::List::iterator &iter, int col) {
#if 0
	pthread_t self = pthread_self();

	GET_INTERNAL_CLASS(jc,internal);
	static jmethodID mid = __ENV->GetMethodID(jc, "get_value", "(II)Ljava/lang/String;");

	KAMOFLAGE_ERROR("Calling List.get_value(%d, %d)...\n", iter.iter->iter, col); fflush(0);

	jstring jstr = (jstring)__ENV->CallObjectMethod(internal, mid, iter.iter->iter, col);
	KAMOFLAGE_ERROR(" --- List.get_value() called (%p)!!\n", jstr); fflush(0);

	if(__ENV->ExceptionOccurred()) {
		__ENV->ExceptionDescribe();
		return "";
	}
	const char *str = __ENV->GetStringUTFChars(jstr, NULL);
	KAMOFLAGE_ERROR(" --- List.get_value() converted -- %p -> %s!!\n", str, str ? str : "<NULL>"); fflush(0);
	std::string retval = std::string(str);
	__ENV->ReleaseStringUTFChars(jstr, str);
	KAMOFLAGE_ERROR(" ---       (%p) ]%s[\n", retval.c_str(), retval.c_str()); fflush(0);

	return retval;
#else
	auto r = iter.iter->iter;
	if(r >= 0 && r < (int)rows.size()) {
		if(col >= 0 && col < (int)rows[r].size())
			return rows[r][col];
	}
	return "";
#endif
}

/************* Entry **************/

KammoGUI::Entry::Entry(std::string _id, jobject jobj) : Widget(_id, jobj) {
}

std::string KammoGUI::Entry::get_text() {
	pthread_t self = pthread_self();
	GET_INTERNAL_CLASS(jc,internal);
	static jmethodID mid = __ENV->GetMethodID(jc, "get_text", "()Ljava/lang/String;");

	KAMOFLAGE_DEBUG_("Calling Entry.get_text()...\n"); fflush(0);

	jstring jstr = (jstring)__ENV->CallObjectMethod(internal, mid);
	KAMOFLAGE_DEBUG_(" --- Entry.get_text() called (%p)!!\n", jstr); fflush(0);

	const char *str = __ENV->GetStringUTFChars(jstr, NULL);
	KAMOFLAGE_DEBUG_(" --- Entry.get_text() converted!!\n"); fflush(0);
	std::string retval = str;
	__ENV->ReleaseStringUTFChars(jstr, str);

	return retval;
}

void KammoGUI::Entry::set_text(const std::string &txt) {
	pthread_t self = pthread_self();
	GET_INTERNAL_CLASS(jc,internal);
	static jmethodID mid = __ENV->GetMethodID(jc, "set_text", "(Ljava/lang/String;)V");
	__ENV->CallVoidMethod(internal, mid,
			      __ENV->NewStringUTF(txt.c_str())
			      );
}

/************* Scale **************/
KammoGUI::Scale::Scale(std::string _id, jobject jobj) : Widget(_id, jobj) {
}

KammoGUI::Scale::Scale(bool _horizontal, double _min,
		       double _max, double _inc) : Widget(true) {

	pthread_t self = pthread_self();
	static jclass jc = NULL;
	if(jc == NULL) {
		jc = __ENV->FindClass("com/toolkits/kamoflage/Kamoflage$Scale");
		jc = (jclass)(__ENV->NewGlobalRef((jobject)jc));
	}
	static jmethodID mid = __ENV->GetMethodID(jc, "<init>", "(Ljava/lang/String;ZDDD)V");

	jstring str_id = __ENV->NewStringUTF(id.c_str());

	jobject local = __ENV->NewObject(
		jc, mid,
		str_id,
		_horizontal ? JNI_TRUE : JNI_FALSE,
		_min, _max, _inc);
	internal = __ENV->NewGlobalRef(local);

//	__ENV->DeleteLocalRef(jc);
	__ENV->DeleteLocalRef(str_id);
	__ENV->DeleteLocalRef(local);
}

double KammoGUI::Scale::get_value(void) {
	pthread_t self = pthread_self();
	static jclass jc = NULL;

	if(jc == NULL) {
		jc = __ENV->GetObjectClass(internal);
		jc = (jclass)(__ENV->NewGlobalRef((jobject)jc));
	}
	static jmethodID mid = NULL;
	if(mid == NULL) {
		mid = __ENV->GetMethodID(jc, "get_value", "()D");
	}

	jdouble val = __ENV->CallDoubleMethod(internal, mid);

	return (double)val;
}

void KammoGUI::Scale::set_value(double val) {
	pthread_t self = pthread_self();
	GET_INTERNAL_CLASS(jc,internal);
	static jmethodID mid = __ENV->GetMethodID(jc, "set_value", "(D)V");
	__ENV->CallVoidMethod(internal, mid, val);
}

/************* CheckButton **************/

KammoGUI::CheckButton::CheckButton(std::string _id, jobject jobj) : Widget(_id, jobj) {
}

bool KammoGUI::CheckButton::get_state() {
	pthread_t self = pthread_self();
	GET_INTERNAL_CLASS(jc,internal);
	static jmethodID mid = __ENV->GetMethodID(jc, "get_state", "()Z");

	KAMOFLAGE_DEBUG_("Calling CheckButton.get_state()...\n"); fflush(0);

	jboolean jbl = __ENV->CallBooleanMethod(internal, mid);
	KAMOFLAGE_DEBUG_(" --- CheckButton.get_state() called (%d)!!\n", jbl); fflush(0);

	return jbl == JNI_TRUE ? true : false;
}

void KammoGUI::CheckButton::set_state(bool _state) {
	pthread_t self = pthread_self();
	GET_INTERNAL_CLASS(jc,internal);
	static jmethodID mid = __ENV->GetMethodID(jc, "set_state", "(Z)V");
	__ENV->CallVoidMethod(internal, mid,
			      _state ? JNI_TRUE : JNI_FALSE
			      );
}

/************* Label **************/
KammoGUI::Label::Label(std::string _id, jobject jobj) : Widget(_id, jobj) {
}

KammoGUI::Label::Label() : Widget(true) {
	pthread_t self = pthread_self();
	static jclass jc = NULL;
	if(jc == NULL) {
		jc = __ENV->FindClass("com/toolkits/kamoflage/Kamoflage$Label");
		jc = (jclass)(__ENV->NewGlobalRef((jobject)jc));
	}
	static jmethodID mid = __ENV->GetMethodID(jc, "<init>", "(Ljava/lang/String;)V");

	jstring str_id = __ENV->NewStringUTF(id.c_str());

	jobject local = __ENV->NewObject(
		jc, mid,
		str_id);
	internal = __ENV->NewGlobalRef(local);

	__ENV->DeleteLocalRef(str_id);
	__ENV->DeleteLocalRef(local);

}

void KammoGUI::Label::set_title(std::string title) {
	pthread_t self = pthread_self();
	GET_INTERNAL_CLASS(jc,internal);
	static jmethodID mid = __ENV->GetMethodID(jc, "set_title", "(Ljava/lang/String;)V");

	jstring str_id = __ENV->NewStringUTF(title.c_str());

	__ENV->CallVoidMethod(internal, mid, str_id);

	__ENV->DeleteLocalRef(str_id);
}


/************* Button **************/
KammoGUI::Button::Button(std::string _id, jobject jobj) : Widget(_id, jobj) {
}

void KammoGUI::Button::create_internal_android() {
	pthread_t self = pthread_self();
	static jclass jc = NULL;
	if(jc == NULL) {
		jc = __ENV->FindClass("com/toolkits/kamoflage/Kamoflage$Button");
		jc = (jclass)(__ENV->NewGlobalRef((jobject)jc));
	}
	static jmethodID mid = __ENV->GetMethodID(jc, "<init>", "(Ljava/lang/String;)V");

	jstring str_id = __ENV->NewStringUTF(id.c_str());

	jobject local = __ENV->NewObject(
		jc, mid,
		str_id);
	internal = __ENV->NewGlobalRef(local);

	__ENV->DeleteLocalRef(str_id);
	__ENV->DeleteLocalRef(local);
}

KammoGUI::Button::Button(std::string _id) : Widget(_id) {
	create_internal_android();
}

KammoGUI::Button::Button() : Widget(true) {
	create_internal_android();
}

void KammoGUI::Button::listener() {
	EventHandler::handle_on_click(this);
}

void KammoGUI::Button::set_title(std::string title) {
	pthread_t self = pthread_self();
	GET_INTERNAL_CLASS(jc,internal);
	static jmethodID mid = __ENV->GetMethodID(jc, "set_title", "(Ljava/lang/String;)V");

	jstring str_id = __ENV->NewStringUTF(title.c_str());

	__ENV->CallVoidMethod(internal, mid, str_id);
	__ENV->DeleteLocalRef(str_id);
}

/************* ContainerBase **************/
KammoGUI::ContainerBase::ContainerBase(std::string _id, jobject jobj) : Widget(_id, jobj) {
}

KammoGUI::ContainerBase::ContainerBase(bool this_is_ignored) : Widget(this_is_ignored) {
}

KammoGUI::ContainerBase::~ContainerBase() {
}

/************* Container **************/

void KammoGUI::Container::add(Widget &wid) {
	children.push_back(&wid);

	pthread_t self = pthread_self();
	// OK, call the java method now
	GET_INTERNAL_CLASS(jc,internal);
	static jmethodID mid = __ENV->GetMethodID(
		jc,
		"add", "(Lcom/toolkits/kamoflage/Kamoflage$Widget;)V"
		);

	// OK, finally time to call the set_coords java method.. PUH!
	__ENV->CallVoidMethod(
		internal, mid, // object and method reference

		// then arguments...
		wid.internal

		);
}

void KammoGUI::Container::clear() {
	std::vector<Widget *>::iterator it;
	for (it=children.begin(); it!=children.end(); it++) {
		delete (*it);
	}
	children.clear();

	pthread_t self = pthread_self();
	// OK, call the java method now
	GET_INTERNAL_CLASS(jc,internal);
	static jmethodID mid = __ENV->GetMethodID(
		jc,
		"clear", "()V"
		);

	// OK, finally time to call the set_coords java method.. PUH!
	__ENV->CallVoidMethod(
		internal, mid // object and method reference
		);
}

KammoGUI::Container::Container(std::string _id, jobject jobj) : ContainerBase(_id, jobj) {
}

KammoGUI::Container::Container(bool horizontal) : ContainerBase(true) {
	pthread_t self = pthread_self();
	static jclass jc = NULL;
	if(jc == NULL) {
		jc = __ENV->FindClass("com/toolkits/kamoflage/Kamoflage$Container");
		jc = (jclass)(__ENV->NewGlobalRef((jobject)jc));
	}
	static jmethodID mid = __ENV->GetMethodID(jc, "<init>", "(Ljava/lang/String;Z)V");

	jstring str_id = __ENV->NewStringUTF(id.c_str());

	jobject local = __ENV->NewObject(
		jc, mid,
		str_id,
		horizontal ? JNI_TRUE : JNI_FALSE);
	internal = __ENV->NewGlobalRef(local);

	__ENV->DeleteLocalRef(str_id);
	__ENV->DeleteLocalRef(local);
}

KammoGUI::Container::~Container() {
	clear();
}

/************* Tabs **************/

void KammoGUI::Tabs::clear() {
	std::cout << "Not supported\n";
}

void KammoGUI::Tabs::add(Widget &wid) {
}

void KammoGUI::Tabs::set_current_view(Widget *w) {
	current_view = w;
}

KammoGUI::Widget *KammoGUI::Tabs::get_current_view() {
	return current_view;
}

KammoGUI::Tabs::Tabs(std::string _id, jobject jobj) : ContainerBase(_id, jobj), current_view(NULL) {
}

/************* Window **************/

void KammoGUI::Window::set_title(const char *title)
{
}

void KammoGUI::Window::set_defsize(int w, int h)
{
}

void KammoGUI::Window::set_fullscreen(bool fsc) {
}

void KammoGUI::Window::clear() {
	std::cout << "Not supported\n";
}

void KammoGUI::Window::add(Widget &wid) {
}

KammoGUI::Window::~Window() {
}

KammoGUI::Window::Window(std::string _id, jobject jobj) : Container(_id, jobj) {
}

void KammoGUI::Window::destroy_event() {
	EventHandler::handle_on_destroy(this);
}

bool KammoGUI::Window::delete_event() {
	return EventHandler::handle_on_delete(this);
}

/********************************************************
 *
 *   DisplayConfiguration
 *
 ********************************************************/
// in inches
float KammoGUI::DisplayConfiguration::width_inch = 3.0f;
float KammoGUI::DisplayConfiguration::height_inch = 4.0f;
float KammoGUI::DisplayConfiguration::resolution_h = 120.0f;
float KammoGUI::DisplayConfiguration::resolution_v = 120.0f;
int KammoGUI::DisplayConfiguration::edge_slop = 10;

float KammoGUI::DisplayConfiguration::inches_to_unit(KammoGUI::geometricUnit_t geo, float in_inches, bool horizontal) {
	float return_value = in_inches; // default
	switch(geo) {
	case KammoGUI::centimeters:
		return_value = in_inches * 2.54;
		break;
	case KammoGUI::inches:
		// do nothing
		break;
	case KammoGUI::pixels:
		return_value *= horizontal ? resolution_h : resolution_v;
		break;
	}

	return return_value;
}

float KammoGUI::DisplayConfiguration::get_screen_width(KammoGUI::geometricUnit_t geo) {
	return inches_to_unit(geo, width_inch, true);
}

float KammoGUI::DisplayConfiguration::get_screen_height(KammoGUI::geometricUnit_t geo) {
	return inches_to_unit(geo, height_inch, false);
}

int KammoGUI::DisplayConfiguration::get_edge_slop() {
	return edge_slop;
}

void KammoGUI::DisplayConfiguration::init(float _width_inch, float _height_inch,
					  float _resolution_h, float _resolution_v,
					  int _edge_slop) {
	width_inch = _width_inch;
	height_inch = _height_inch;
	resolution_h = _resolution_h;
	resolution_v = _resolution_v;
	edge_slop = _edge_slop;
}

/********************************************************
 *
 *   Dialog Interface
 *
 ********************************************************/

// the busy thread is a continuous thread
// waiting for orders to run specific "Busy Functions", i.e.
// background type work which must not interrupt the flow
// or hang the GUI.
class BusyThread : public jThread {
private:
	class Trigger : public jThread::Event {
	public:
		virtual ~Trigger() {}

		std::string title, message;
		void (*busy_function)(void *data);
		void (*cancelable_busy_function)(void *data, KammoGUI::CancelIndicator &cid);
		void *data;

		virtual void dummy_event_function() {}
	};

	class BTCancelIndicator : public KammoGUI::CancelIndicator, public jThread::Monitor {
	private:
		bool should_be_cancelled;
	public:
		BTCancelIndicator() : should_be_cancelled(false) {}

		virtual bool is_cancelled() {
			MonitorGuard g(this);

			return should_be_cancelled;
		}

		void cancel_now() {
			MonitorGuard g(this);

			should_be_cancelled = true;
		}

		void reset() {
			MonitorGuard g(this);

			should_be_cancelled = false;
		}
	};

	jThread::EventQueue *eq;

	static BTCancelIndicator cid;

public:
	static void set_cid() {
		cid.cancel_now();
	}

	BusyThread() : jThread("BusyThread") {
		eq = new EventQueue();
	}

	void trigger_busy_function(
		std::string title,
		std::string message,
		void (*_busy_function)(void *data), void *_data) {
		Trigger *trg = new Trigger();
		trg->busy_function = _busy_function;
		trg->cancelable_busy_function = NULL;
		trg->data = _data;
		trg->title = title;
		trg->message = message;

		eq->push_event(trg);
	}

	void trigger_cancelable_busy_function(
		std::string title,
		std::string message,
		void (*_cancelable_busy_function)(void *data, KammoGUI::CancelIndicator &cid), void *_data) {
		Trigger *trg = new Trigger();
		trg->busy_function = NULL;
		trg->cancelable_busy_function = _cancelable_busy_function;
		trg->data = _data;
		trg->title = title;
		trg->message = message;

		eq->push_event(trg);
	}

	static void show_busy_dialog(void *d) {
		Trigger *trg = (Trigger *)d;

		// show busy dialog
		JNIEnv *env = get_env_for_thread();
		jclass jc = env->FindClass("com/toolkits/kamoflage/Kamoflage");
		jmethodID mid = env->GetStaticMethodID(jc, "showKamoflageDialog",
						       "(IILjava/lang/String;Ljava/lang/String;)V");

		jstring t = env->NewStringUTF(trg->title.c_str());
		jstring m = env->NewStringUTF(trg->message.c_str());

		if(trg != NULL) {
			if(trg->busy_function) {
				env->CallStaticVoidMethod(jc, mid, KammoGUI::busy_dialog, 0, t, m);
			} else if(trg->cancelable_busy_function) {
				env->CallStaticVoidMethod(jc, mid, KammoGUI::cancelable_busy_dialog, 0, t, m);
			}
		}

		env->DeleteLocalRef(t);
		env->DeleteLocalRef(m);
	}

	static void hide_busy_dialog(void *d) {
		Trigger *trg = (Trigger *)d;

		// hide busy dialog
		JNIEnv *env = get_env_for_thread();
		jclass jc = env->FindClass("com/toolkits/kamoflage/Kamoflage");
		jmethodID mid = env->GetStaticMethodID(jc, "hideKamoflageDialog", "(I)V");

		if(trg != NULL) {
			if(trg->busy_function) {
				env->CallStaticVoidMethod(jc, mid, KammoGUI::busy_dialog);
			} else if(trg->cancelable_busy_function) {
				env->CallStaticVoidMethod(jc, mid, KammoGUI::cancelable_busy_dialog);
			}
		}
	}

	virtual void thread_body() {
		while(1) {
			// wait for new busy function
			Trigger *trg = (Trigger *)eq->wait_for_event();
			cid.reset();

			if(trg != NULL) {

				const char *title = trg->title.c_str();

				KammoGUI::run_on_GUI_thread_synchronized(show_busy_dialog, trg);

				// call the busy function
				try {
					if(trg->busy_function) {
						trg->busy_function(trg->data);
					} else if(trg->cancelable_busy_function) {
						trg->cancelable_busy_function(trg->data, cid);
					}

				} catch(jException j) {
					KAMOFLAGE_ERROR("BusyThread::thread_body() - jException caught (%s) - %s\n", title, j.message.c_str());
					jInformer::inform(j.message);
				} catch(std::exception &e) {
					KAMOFLAGE_ERROR("BusyThread::thread_body() - std::exception caught (%s)- %s\n", title, e.what());
					jInformer::inform(e.what());
				} catch(...) {
					jInformer::inform("Unknown exception caught.");
				}

				KammoGUI::run_on_GUI_thread_synchronized(hide_busy_dialog, trg);

				delete trg;
			}
		}
	}
};

BusyThread::BTCancelIndicator BusyThread::cid;

BusyThread *__kamoflage_busy_thread = NULL;
void KammoGUI::do_busy_work(
	const std::string &btitle,
	const std::string &bmessage,
	void (*busy_function)(void *data), void *data) {
	if(__kamoflage_busy_thread)
		__kamoflage_busy_thread->trigger_busy_function(btitle, bmessage, busy_function, data);
}

void KammoGUI::do_cancelable_busy_work(
	const std::string &btitle,
	const std::string &bmessage,
	void (*busy_function)(void *data, CancelIndicator &cid), void *data) {
	if(__kamoflage_busy_thread)
		__kamoflage_busy_thread->trigger_cancelable_busy_function(btitle, bmessage, busy_function, data);
}

void KammoGUI::display_notification(const std::string &title, const std::string &message) {
	pthread_t self = pthread_self();
	jclass jc = __ENV->FindClass("com/toolkits/kamoflage/Kamoflage");
	jmethodID mid = __ENV->GetStaticMethodID(jc, "showKamoflageDialog",
						 "(IILjava/lang/String;Ljava/lang/String;)V");

	jstring t = __ENV->NewStringUTF(title.c_str());
	jstring m = __ENV->NewStringUTF(message.c_str());

	__ENV->CallStaticVoidMethod(jc, mid, KammoGUI::notification_dialog, 0, t, m);

	__ENV->DeleteLocalRef(t);
	__ENV->DeleteLocalRef(m);
}

void KammoGUI::external_uri(const std::string &input_uri) {
	pthread_t self = pthread_self();
	jclass jc = __ENV->FindClass("com/toolkits/kamoflage/Kamoflage");
	jmethodID mid = __ENV->GetStaticMethodID(jc, "showExternalURI",
						 "(Ljava/lang/String;)V");

	jstring uri = __ENV->NewStringUTF(input_uri.c_str());

	__ENV->CallStaticVoidMethod(jc, mid, uri);

	__ENV->DeleteLocalRef(uri);
}

class GuiThreadJob : public jThread::Event {
private:
	GuiThreadJob() {}
public:
	virtual ~GuiThreadJob() {}

	bool do_synch;
	GuiThreadJob(bool _do_synch) : do_synch(_do_synch) {}

	void (*gui_thread_function)(void *data);
	void *data;
	std::function<void()> func;

	jThread::EventQueue synch_queue;

	virtual void dummy_event_function() {}


};

jThread::EventQueue *on_ui_thread_queue = NULL;

pid_t kammogui_ui_thread = 0;

// This should NOT be a member of the KammoGUI namespace, and should
// not be in the kamogui.hh header file
void KammoGUI__run_on_GUI_thread_JNI_CALLBACK() {
	GuiThreadJob *gtj = NULL;

	kammogui_ui_thread = gettid();

	try {
		while((gtj = ((GuiThreadJob *)on_ui_thread_queue->poll_for_event())) != NULL) {

			// call the function
			if(gtj) {
				KAMOFLAGE_DEBUG("gtj to process: %p\n", gtj);
				if(gtj->gui_thread_function)
					gtj->gui_thread_function(gtj->data);
				else
					gtj->func();

				// push us back, the other direction...
				if(gtj->do_synch) {
					KAMOFLAGE_DEBUG_("run on gui - synch, will trigger.\n");
					gtj->synch_queue.push_event(gtj);
					KAMOFLAGE_DEBUG_("run on gui - synch, triggered.\n");
				} else {
//			KAMOFLAGE_DEBUG_("run on gui - non synch, will delete gtj.\n");
					delete gtj;
//			KAMOFLAGE_DEBUG_("run on gui - non synch, deleted.\n");
				}
			} else {
				KAMOFLAGE_DEBUG_("wow, run on gui got a null object.\n");
			}
		}
	} catch(...) {
		int status = 0;
		char * buff = __cxxabiv1::__cxa_demangle(
			__cxxabiv1::__cxa_current_exception_type()->name(),
			NULL, NULL, &status);
		KAMOFLAGE_ERROR("KammoGUI__run_on_GUI_thread_JNI_CALLBACK() caught an unknown exception: %s\n", buff);
		throw;
	}
}

// the RunOnUiThread thread is a continuous thread
// waiting for orders to run specific functions in the UI context.
// basically it will wait on a queue for an event, put it on another queue, then call the JNI function executeEventOnUI
// wich will poll the secondary queue and execute the job.
// the reason we have two queues is that we want to be able to call this construct
// from a thread which does not have a JNI environment context (some threads in Android do not have that...)
// so, unfortunately this makes it a bit ugly..
class RunOnUiThread : public jThread {
private:
	static jThread::EventQueue *internal_queue;
	static RunOnUiThread *internal_thread;

	static void spin_for_queue() {
		while(internal_queue == NULL)
			usleep(1000);
	}

	static void queue_event(GuiThreadJob *gtj, bool do_synch) {
		if(!internal_queue) // if queue is not yet created, wait for it...
			spin_for_queue();
		internal_queue->push_event(gtj);

		if(do_synch) {
			GuiThreadJob *gtj2 = (GuiThreadJob *)gtj->synch_queue.wait_for_event();

			if(gtj2 != gtj)
				throw jException(
					"Synchronization error, KammoGUI::run_on_GUI_thread. This is a serious bug...",
					jException::sanity_error);

			delete gtj2;
		}
	}

public:
	class gtjObject_is_null_inRunOnUiThread {};
	class envObject_is_null_inRunOnUiThread {};
	class jclass_is_null_inRunOnUiThread {};

	static void prepare_RunOnUiThread() {
		internal_queue = new EventQueue();
		on_ui_thread_queue = new jThread::EventQueue();

		internal_thread = new RunOnUiThread();
		internal_thread->run();
	}

	RunOnUiThread() : jThread("RunOnUiThread") { }

	virtual void thread_body() {
		JNIEnv *env = NULL;
		env = get_env_for_thread();
		if(env == NULL)
			throw envObject_is_null_inRunOnUiThread();

		jclass jc = NULL;
		jc = env->FindClass("com/toolkits/kamoflage/Kamoflage");

		if(jc == NULL) {
			throw jclass_is_null_inRunOnUiThread();
		}

		jmethodID mid = env->GetStaticMethodID(jc, "executeEventOnUI", "()V");

		while(1) {
			GuiThreadJob *gtj = (GuiThreadJob *)internal_queue->wait_for_event();
			KAMOFLAGE_DEBUG("RunOnUiThread: received gtj %p\n", gtj);
			if(gtj == NULL)
				throw gtjObject_is_null_inRunOnUiThread();

			on_ui_thread_queue->push_event(gtj);
			{

				env->PushLocalFrame(32);

				env->CallStaticVoidMethod(jc, mid);

				env->PopLocalFrame(NULL);
			}
		}
	}

	static void run_on_GUI_thread(void (*on_ui_func)(void *data), void *data, bool synchronized) {
		if(__is_ui_thread()) {
			KAMOFLAGE_DEBUG_("run_on_GUI_thread() already in that thread, run directly...\n");

			// if this is the ui thread, just run it directly... it's always synchronized.
			on_ui_func(data);

			return;
		}

		GuiThreadJob *gtj = new GuiThreadJob(synchronized);
		KAMOFLAGE_DEBUG_("run_on_GUI_thread() - gtj %p\n", gtj);
		{
			gtj->gui_thread_function = on_ui_func;
			gtj->data = data;
		}

		queue_event(gtj, synchronized);
	}

	static void run_on_GUI_thread(std::function<void()> func, bool synchronized) {
		if(__is_ui_thread()) {
			KAMOFLAGE_DEBUG_("run_on_GUI_thread() already in that thread, run directly...\n");

			// if this is the ui thread, just run it directly... it's always synchronized.
			func();

			return;
		}

		GuiThreadJob *gtj = new GuiThreadJob(synchronized);
		KAMOFLAGE_DEBUG_("run_on_GUI_thread() - gtj %p\n", gtj);
		{
			gtj->gui_thread_function = NULL;
			gtj->data = NULL;
			gtj->func = func;
		}

		queue_event(gtj, synchronized);
	}
};
jThread::EventQueue *RunOnUiThread::internal_queue = NULL;
RunOnUiThread *RunOnUiThread::internal_thread = NULL;

void KammoGUI::run_on_GUI_thread(void (*on_ui_func)(void *data), void *data) {
	RunOnUiThread::run_on_GUI_thread(on_ui_func, data, false);
}

void KammoGUI::run_on_GUI_thread_synchronized(void (*on_ui_func)(void *data), void *data) {
	RunOnUiThread::run_on_GUI_thread(on_ui_func, data, true);
}

void KammoGUI::run_on_GUI_thread(std::function<void()> func, bool sync) {
	RunOnUiThread::run_on_GUI_thread(func, sync);
}

class YesNoEvent {
public:
	void (*yes_function)(void *data);
	void *yes_data;
	void (*no_function)(void *data);
	void *no_data;
};

std::set<YesNoEvent *> yes_no_queue;
jThread::Mutex yes_no_queue_mutex;

// This should NOT be a member of the KammoGUI namespace, and should
// not be in the kamogui.hh header file
// this will be called from Android when the user press yes or no
void KammoGUI__yes_no_dialog_JNI_CALLBACK(void *ptr, bool yes) {

	std::set<YesNoEvent *>::iterator k;
	yes_no_queue_mutex.lock();
	k = yes_no_queue.find((YesNoEvent *)ptr);

	if(k != yes_no_queue.end()) {
		YesNoEvent *yne = (*k);
		yes_no_queue.erase(k);
		yes_no_queue_mutex.unlock();

		// call the function
		if(yes) {
			if(yne->yes_function != NULL) yne->yes_function(yne->yes_data);
		} else {
			if(yne->no_function != NULL) yne->no_function(yne->no_data);
		}

		delete yne;
	} else {
		yes_no_queue_mutex.unlock();
	}
}

void KammoGUI::ask_yes_no(const std::string &title,
			  const std::string &message,
			  void (*ya_function)(void *data), void *ya_data,
			  void (*no_function)(void *data), void *no_data
	) {

	YesNoEvent *yne = new YesNoEvent();
	{

		yne->yes_function = ya_function;
		yne->no_function = no_function;
		yne->yes_data = ya_data;
		yne->no_data = no_data;

		yes_no_queue_mutex.lock();
		yes_no_queue.insert(yne);
		yes_no_queue_mutex.unlock();
	}

	{
		pthread_t self = pthread_self();
		jclass jc = __ENV->FindClass("com/toolkits/kamoflage/Kamoflage");
		jmethodID mid = __ENV->GetStaticMethodID(jc, "showKamoflageDialog",
							 "(IILjava/lang/String;Ljava/lang/String;)V");

		jstring t = __ENV->NewStringUTF(title.c_str());
		jstring m = __ENV->NewStringUTF(message.c_str());

		__ENV->CallStaticVoidMethod(jc, mid, KammoGUI::yesno_dialog, (jint)yne, t, m);

		__ENV->DeleteLocalRef(t);
		__ENV->DeleteLocalRef(m);
	}
}

void KammoGUI::start() {
}

void KammoGUI::get_widget(KammoGUI::Widget **target, std::string id) {
	if((*target) != NULL) return; // optimization
	(*target) = Widget::get_widget(id);
}

/********************************************** JNI INTERFACE ***********************************/
/********************************************** JNI INTERFACE ***********************************/
/********************************************** JNI INTERFACE ***********************************/
/********************************************** JNI INTERFACE ***********************************/
/********************************************** JNI INTERFACE ***********************************/

extern "C" {

	JNIEXPORT jint JNICALL JNI_OnLoad (JavaVM * vm, void * reserved) {
		__g_jvm = vm;
		return JNI_VERSION_1_6;
	}

	JNIEXPORT jstring JNICALL Java_com_toolkits_kamoflage_Kamoflage_stringFromJNI
	(JNIEnv * env, jclass jc) {
		__setup_env_for_thread(env);

		fflush(0);
		return (*env).NewStringUTF("Rasta!");
	}

	JNIEXPORT void JNICALL Java_com_toolkits_kamoflage_Kamoflage_yesNoDialogCallback
	(JNIEnv * env, jclass jc, jboolean yes, jint ptr_stored_in_int) {
		__setup_env_for_thread(env);

		KammoGUI__yes_no_dialog_JNI_CALLBACK(
			(void *)ptr_stored_in_int,
			yes == JNI_TRUE ? true : false
			);
	}

	JNIEXPORT void JNICALL Java_com_toolkits_kamoflage_Kamoflage_onCancelableBusyWorkCallback
	(JNIEnv * env, jclass jc) {
		__setup_env_for_thread(env);

		BusyThread::set_cid();
		KammoGUI::Canvas::flush_invalidation_queue();

	}

	JNIEXPORT void JNICALL Java_com_toolkits_kamoflage_Kamoflage_onUiThreadCallback
	(JNIEnv * env, jclass jc) {
		__setup_env_for_thread(env);

		KammoGUI__run_on_GUI_thread_JNI_CALLBACK();
		// in case any canvas changes where made, make sure we flush the invalidation queue
		KammoGUI::Canvas::flush_invalidation_queue();

	}

	JNIEXPORT void JNICALL Java_com_toolkits_kamoflage_Kamoflage_extraThreadEntry
	(JNIEnv * env, jclass jc, jint identity_hash) {
		__setup_env_for_thread(env);

		jThread::java_thread_entry(identity_hash);

	}

	JNIEXPORT void JNICALL Java_com_toolkits_kamoflage_Kamoflage_setDisplayConfiguration
	(JNIEnv * env, jclass jc,
	 jfloat widthInInches, jfloat heightInInches,
	 jfloat horizontalResolution, jfloat verticalResolution,
	 int edgeSlop) {
		KammoGUI::DisplayConfiguration::init(widthInInches, heightInInches, horizontalResolution, verticalResolution, edgeSlop);
	}

	JNIEXPORT void JNICALL Java_com_toolkits_kamoflage_Kamoflage_handleOnSurface
	(JNIEnv * env, jclass jc, jstring j_id, int evt, int x, int y) {
		__setup_env_for_thread(env);
		const char *id = (*env).GetStringUTFChars(j_id, NULL);

		KammoGUI::Widget *w = NULL;
		KammoGUI::get_widget(&w, id);

		(*env).ReleaseStringUTFChars(j_id, id);

		if(w != NULL) {
			KammoGUI::Surface *srf = (KammoGUI::Surface *)w;
			srf->trigger_surface_event((KammoGUI::surfaceEvent_t)evt,
						   x, y);
		}

		KammoGUI::Canvas::flush_invalidation_queue();
	}

	JNIEXPORT void JNICALL Java_com_toolkits_kamoflage_Kamoflage_handleOnModify
	(JNIEnv * env, jclass jc, jstring j_id) {
		__setup_env_for_thread(env);
		const char *id = (*env).GetStringUTFChars(j_id, NULL);

		KammoGUI::Widget *w = NULL;
		KammoGUI::get_widget(&w, id);

		KAMOFLAGE_DEBUG_("Calling handle_on_modify for %s\n", id); fflush(0);

		(*env).ReleaseStringUTFChars(j_id, id);

		if(w != NULL)
			KammoGUI::EventHandler::handle_on_modify(w);
		KAMOFLAGE_DEBUG_("Finished handle_on_modify for %s\n", id); fflush(0);


		KammoGUI::Canvas::flush_invalidation_queue();
	}

	JNIEXPORT void JNICALL Java_com_toolkits_kamoflage_Kamoflage_handleOnValueChanged
	(JNIEnv * env, jclass jc, jstring j_id) {
		__setup_env_for_thread(env);

		const char *id = (*env).GetStringUTFChars(j_id, NULL);

		KammoGUI::Widget *w = NULL;
		KammoGUI::get_widget(&w, id);

		(*env).ReleaseStringUTFChars(j_id, id);

		if(w != NULL)
			KammoGUI::EventHandler::handle_on_value_changed(w);

		KammoGUI::Canvas::flush_invalidation_queue();
	}

	JNIEXPORT void JNICALL Java_com_toolkits_kamoflage_Kamoflage_handleOnSelectRow
	(JNIEnv * env, jclass jc, jstring j_id, jint row_id) {
		__setup_env_for_thread(env);

		const char *id = (*env).GetStringUTFChars(j_id, NULL);

		KammoGUI::Widget *w = NULL;
		KammoGUI::get_widget(&w, id);

		(*env).ReleaseStringUTFChars(j_id, id);

		if(w != NULL) {
			KammoGUI::List *lst = (KammoGUI::List *)w;
			lst->trigger_on_select_row_event(row_id);
		}

		KammoGUI::Canvas::flush_invalidation_queue();
	}

	JNIEXPORT void JNICALL Java_com_toolkits_kamoflage_Kamoflage_handleOnClick
	(JNIEnv * env, jclass jc, jstring j_id) {
		__setup_env_for_thread(env);
		const char *id = (*env).GetStringUTFChars(j_id, NULL);

		KammoGUI::Widget *w = NULL;
		KammoGUI::get_widget(&w, id);

		(*env).ReleaseStringUTFChars(j_id, id);

		if(w != NULL)
			KammoGUI::EventHandler::handle_on_click(w);

		KammoGUI::Canvas::flush_invalidation_queue();
	}

	JNIEXPORT void JNICALL Java_com_toolkits_kamoflage_Kamoflage_handleOnDoubleClick
	(JNIEnv * env, jclass jc, jstring j_id) {
		__setup_env_for_thread(env);
		const char *id = (*env).GetStringUTFChars(j_id, NULL);


		KammoGUI::Widget *w = NULL;
		KammoGUI::get_widget(&w, id);

		(*env).ReleaseStringUTFChars(j_id, id);

		if(w != NULL)
			KammoGUI::EventHandler::handle_on_double_click(w);

		KammoGUI::Canvas::flush_invalidation_queue();
	}

	JNIEXPORT void JNICALL Java_com_toolkits_kamoflage_Kamoflage_handleOnPoll
	(JNIEnv * env, jclass jc, jstring j_id) {
		__setup_env_for_thread(env);
		const char *id = (*env).GetStringUTFChars(j_id, NULL);

		KammoGUI::Widget *w = NULL;
		KammoGUI::get_widget(&w, id);

		(*env).ReleaseStringUTFChars(j_id, id);

		if(w != NULL)
			KammoGUI::EventHandler::handle_on_poll(w);

		KammoGUI::Canvas::flush_invalidation_queue();
	}

	JNIEXPORT void JNICALL Java_com_toolkits_kamoflage_Kamoflage_tabViewChanged
	(JNIEnv * env, jclass jc, jstring j_tab_id, jstring j_view_id) {
		__setup_env_for_thread(env);
		const char *tab_id = (*env).GetStringUTFChars(j_tab_id, NULL);
		const char *view_id = (*env).GetStringUTFChars(j_view_id, NULL);

		KammoGUI::Widget *w_tab = NULL;
		KammoGUI::get_widget(&w_tab, tab_id);
		KammoGUI::Widget *w_view = NULL;
		KammoGUI::get_widget(&w_view, view_id);

		(*env).ReleaseStringUTFChars(j_tab_id, tab_id);
		(*env).ReleaseStringUTFChars(j_view_id, view_id);

		if(w_tab != NULL && w_view != NULL) {
			((KammoGUI::Tabs *)w_tab)->set_current_view(w_view);
		}

		KammoGUI::Canvas::flush_invalidation_queue();
	}

	JNIEXPORT void JNICALL Java_com_toolkits_kamoflage_Kamoflage_runMainNativeThread
	(JNIEnv * env, jclass jc) {
		__setup_env_for_thread(env);

		__kamoflage_busy_thread = new BusyThread();
		__kamoflage_busy_thread->run();

		RunOnUiThread::prepare_RunOnUiThread();

		KammoGUI::Widget::call_on_init();
	}

#define Kamoflage_addWIDGET(N,A) \
	JNIEXPORT jlong JNICALL N \
	(JNIEnv * env, jclass jc, jstring j_id, jobject wobj) { \
		__setup_env_for_thread(env); \
		const char *id = (*env).GetStringUTFChars(j_id, NULL); \
		KammoGUI::A *wid = new KammoGUI::A(std::string(id), (*env).NewGlobalRef(wobj));	\
		if(wid->get_id() != id) \
			delete wid; \
 		(*env).ReleaseStringUTFChars(j_id, id);  \
		return (jlong)wid; \
	}

	Kamoflage_addWIDGET(Java_com_toolkits_kamoflage_Kamoflage_addCheckButton,CheckButton);
	Kamoflage_addWIDGET(Java_com_toolkits_kamoflage_Kamoflage_addLabel,Label);
	Kamoflage_addWIDGET(Java_com_toolkits_kamoflage_Kamoflage_addButton,Button);
	Kamoflage_addWIDGET(Java_com_toolkits_kamoflage_Kamoflage_addCanvas,Canvas);
	Kamoflage_addWIDGET(Java_com_toolkits_kamoflage_Kamoflage_addSVGCanvas,SVGCanvas);
	Kamoflage_addWIDGET(Java_com_toolkits_kamoflage_Kamoflage_addGnuVGCanvas,GnuVGCanvas);
	Kamoflage_addWIDGET(Java_com_toolkits_kamoflage_Kamoflage_addSurface,Surface);
	Kamoflage_addWIDGET(Java_com_toolkits_kamoflage_Kamoflage_addList,List);
	Kamoflage_addWIDGET(Java_com_toolkits_kamoflage_Kamoflage_addEntry,Entry);
	Kamoflage_addWIDGET(Java_com_toolkits_kamoflage_Kamoflage_addScale,Scale);
	Kamoflage_addWIDGET(Java_com_toolkits_kamoflage_Kamoflage_addContainer,Container);
	Kamoflage_addWIDGET(Java_com_toolkits_kamoflage_Kamoflage_addTabs,Tabs);
	Kamoflage_addWIDGET(Java_com_toolkits_kamoflage_Kamoflage_addWindow,Window);
	Kamoflage_addWIDGET(Java_com_toolkits_kamoflage_Kamoflage_addPollEvent,PollEvent);
	Kamoflage_addWIDGET(Java_com_toolkits_kamoflage_Kamoflage_addUserEvent,UserEvent);

}
