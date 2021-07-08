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
#include <unistd.h>

#include <math.h>

#include "kamogui.hh"
#include "gnuVGcanvas.hh"

#include "jngldrum/jthread.hh"
#include "jngldrum/jinformer.hh"

#include <libsvg/svgint.h>

//#define __DO_KAMOFLAGE_DEBUG
#include "kamoflage_debug.hh"

bool __is_ui_thread() {
	// these are created to optimize over time for repeated calls
	// to the __is_ui_thread() method
	static pthread_t uiThreadId = 0;
	static std::set<pthread_t> nonUiThreadId;
	static jThread::Monitor mtr;

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

	return false;
}

/************* Event ***************/
std::multimap<std::string, KammoGUI::EventHandler *> KammoGUI::EventHandler::event_handlers;

KammoGUI::EventHandler::~EventHandler() {
}

KammoGUI::EventHandler::EventHandler(std::string ids) {
	KAMOFLAGE_ERROR("EventHandler::EventHandler()\n");
	printf("EventHandler(%s)\n", ids.c_str());
//	std::cout << "new event class declared for widgets with ids: " << ids << "\n"; fflush(0);
	this->id = id;
//	return;

	int k = -1;
	std::string id;

	do {
		if(k != -1)
			ids.erase(k, ids.size());
		k = ids.rfind(":", ids.size());
		id = ids.substr(k + 1, ids.size());
		event_handlers.insert(
			std::pair<std::string, KammoGUI::EventHandler *>(id,this));
	} while(k != -1);
}

std::vector<KammoGUI::EventHandler *>KammoGUI::EventHandler::find_handlers(
	KammoGUI::Widget *wid) {

	std::vector<KammoGUI::EventHandler *> result;

	static std::multimap<std::string, KammoGUI::EventHandler *>::iterator evhi;

	for(evhi = event_handlers.find(wid->id);
	    (evhi != event_handlers.end()) && ((*evhi).first == wid->id);
	    evhi++) {
		result.push_back((*evhi).second);
	}
	return result;
}

void KammoGUI::EventHandler::handle_on_init(KammoGUI::Widget *wid) {
	for(auto k : wid->evh) {
		try {
			k->on_init(wid);
		} catch(jException& e) {
			KAMOFLAGE_ERROR("on_init() caught an exception: %s\n", e.message.c_str());
			exit(1);
		}
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
KammoGUI::PollEvent::PollEvent(std::string _id) : Widget(_id) {
}

/************* UserEvent **************/
KammoGUI::UserEvent::UserEvent(std::string _id) : Widget(_id) {
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

KammoGUI::Widget::Widget(bool this_is_ignored) {
	PROTECTOR[0] = 'P';
	PROTECTOR[1] = 'R';
	PROTECTOR[2] = 'O';
	PROTECTOR[3] = 'T';
	PROTECTOR[4] = 'E';
	PROTECTOR[5] = 'C';
	PROTECTOR[6] = 'T';
	PROTECTOR[7] = 'E';
	PROTECTOR[8] = 'D';
	char uniq_id_buffer[128];
	snprintf(uniq_id_buffer, 128, "__INTERNAL_UNIQ_PTR_%p", this);

	id = std::string(uniq_id_buffer);

	register_id();
}

void KammoGUI::Widget::show_now() {
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
}

void KammoGUI::Widget::set_fill(bool fl) {
}

int KammoGUI::Widget::get_border_width() {
	// XXX not implemented right
	return 0;
}

void KammoGUI::Widget::redraw() {
}

void KammoGUI::Widget::attach_event_handler(EventHandler *_evh) {
	evh.push_back(_evh);
}

void KammoGUI::Widget::inject_key_event(bool is_press, char chr) {
}

KammoGUI::Widget::Widget(std::string _id) : id(_id) {
	KAMOFLAGE_ERROR("Widget::Widget()\n");
	PROTECTOR[0] = 'P';
	PROTECTOR[1] = 'R';
	PROTECTOR[2] = 'O';
	PROTECTOR[3] = 'T';
	PROTECTOR[4] = 'E';
	PROTECTOR[5] = 'C';
	PROTECTOR[6] = 'T';
	PROTECTOR[7] = 'E';
	PROTECTOR[8] = 'D';
	register_id();
}

KammoGUI::Widget::~Widget() {
	KAMOFLAGE_DEBUG_("ERASING WIDGET %s\n", id.c_str()); fflush(0);

	std::map<std::string, KammoGUI::Widget *>::iterator k = widgets.find(id);
	if(k != widgets.end()) widgets.erase(k);
}

std::map<std::string, KammoGUI::Widget *> KammoGUI::Widget::widgets;
void KammoGUI::Widget::call_on_init() {
	/* run init handlers */
	std::map<std::string, KammoGUI::Widget *>::iterator k;
	for(k  = Widget::widgets.begin();
	    k != Widget::widgets.end();
	    k++) {
		printf("::call_on_init()...\n");
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

KammoGUI::Surface::Surface(std::string _id) : Widget(_id), cbdat(NULL), surface_event_cb(NULL) {
}

/************* Entry **************/

KammoGUI::Entry::Entry(std::string _id) : Widget(_id) {
}

std::string KammoGUI::Entry::get_text() {
	return "";
}

void KammoGUI::Entry::set_text(const std::string &txt) {
}

/************* Scale **************/
KammoGUI::Scale::Scale(std::string _id) : Widget(_id) {
}

KammoGUI::Scale::Scale(bool _horizontal, double _min,
		       double _max, double _inc) : Widget(true) {
}

double KammoGUI::Scale::get_value(void) {
	return 0.0;
}

void KammoGUI::Scale::set_value(double val) {
}

/************* CheckButton **************/

KammoGUI::CheckButton::CheckButton(std::string _id) : Widget(_id) {
}

bool KammoGUI::CheckButton::get_state() {
	return false;
}

void KammoGUI::CheckButton::set_state(bool _state) {
}

/************* Label **************/
KammoGUI::Label::Label(std::string _id) : Widget(_id) {
}

KammoGUI::Label::Label() : Widget(true) {
}

void KammoGUI::Label::set_title(std::string title) {
}


/************* Button **************/
void KammoGUI::Button::create_internal_android() {
}

KammoGUI::Button::Button(std::string _id) : Widget(_id) {
}

KammoGUI::Button::Button() : Widget(true) {
}

void KammoGUI::Button::listener() {
	EventHandler::handle_on_click(this);
}

void KammoGUI::Button::set_title(std::string title) {
}

/************* ContainerBase **************/
KammoGUI::ContainerBase::ContainerBase(std::string _id) : Widget(_id) {
}

KammoGUI::ContainerBase::ContainerBase(bool this_is_ignored) : Widget(this_is_ignored) {
}

KammoGUI::ContainerBase::~ContainerBase() {
}

/************* Container **************/

void KammoGUI::Container::add(Widget &wid) {
	children.push_back(&wid);
}

void KammoGUI::Container::clear() {
	std::vector<Widget *>::iterator it;
	for (it=children.begin(); it!=children.end(); it++) {
		delete (*it);
	}
	children.clear();
}

KammoGUI::Container::Container(std::string _id) : ContainerBase(_id) {
}

KammoGUI::Container::Container(bool horizontal) : ContainerBase(true) {
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

KammoGUI::Tabs::Tabs(std::string _id) : ContainerBase(_id), current_view(NULL) {
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

KammoGUI::Window::Window(std::string _id) : Container(_id) {
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
	}

	static void hide_busy_dialog(void *d) {
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

				} catch(jException& j) {
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
}

void KammoGUI::external_uri(const std::string &input_uri) {
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

pid_t gettid(void);
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
		while(1) {
			GuiThreadJob *gtj = (GuiThreadJob *)internal_queue->wait_for_event();
			KAMOFLAGE_DEBUG("RunOnUiThread: received gtj %p\n", gtj);
			if(gtj == NULL)
				throw gtjObject_is_null_inRunOnUiThread();

			on_ui_thread_queue->push_event(gtj);
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
	}
}

void KammoGUI::start() {
	Widget::call_on_init();
}

void KammoGUI::get_widget(KammoGUI::Widget **target, std::string id) {
	if((*target) != NULL) return; // optimization
	(*target) = Widget::get_widget(id);
}
