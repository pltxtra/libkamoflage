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

/************************
 *
 * Missing stuff:
 *
 ************************
 *
 * --- Not needed for Satan:
 *
 * Tabs add/clear functionality
 * Window add/clear functionality
 *
 ************************/

#ifndef __KAMMOGUI
#define __KAMMOGUI

#include <string>
#include <map>
#include <vector>
#include <stack>
#include <set>
#include <memory>
#include <functional>

#include "jngldrum/jexception.hh"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

/**************************************
 * BEGIN EventHandler creation macros *
 **************************************/

#define KammoEventHandler_Declare(a,b) namespace a {			\
					       class __handler__ : public  KammoGUI::EventHandler { \
                                               public: \
					       __handler__() : EventHandler(b) { printf("Tjooofa\n"); } \
					       void callee() { printf("Kurfo\n"); }

#define KammoEventHandler_Instance(a)          };		\
					       __handler__ HandlerHolder;	\
                                                                                \
					       void __caller__() { \
					       HandlerHolder.callee(); \
					       } \
					       }

/**************************************
 * END EventHandler creation macros *
 **************************************/

namespace KammoGUI {

	// ANDROID: These MUST have EXACTLY the same values like in Kamoflage.java !!!
	typedef enum _KamoflageDialogType {
		busy_dialog = 10,
		notification_dialog = 20,
		yesno_dialog = 30,
		cancelable_busy_dialog = 40
	} dialogType_t;

	typedef enum _SurfaceEvent {
		sfButtonPress = 0,
		sfButtonRelease = 1,
		sfMotion = 2
	} surfaceEvent_t;

	// cvButtonHold is mapped to touch&hold events on android
	// on other devices (mouse/trackpad operated) it might
	// be mapped to a double click.
	typedef enum _CanvasEvent {
		cvButtonPress = 0,
		cvButtonRelease = 1,
		cvMotion = 2,
		cvButtonHold = 3
	} canvasEvent_t;

	typedef enum _GeometricUnit {
		inches = 0,
		centimeters = 1,
		pixels = 2
	} geometricUnit_t;

	class KamoflageFactory;
	class EventHandler;

	class ContainerBase;
	class Container;
	class Tabs;

	class Widget {
	private:
		friend class EventHandler;
		friend class KamoflageFactory;

		std::vector<EventHandler *>evh;

		void register_id();

	protected:
		char PROTECTOR[9];

		std::string id;

		static std::map<std::string, KammoGUI::Widget *> widgets;

		Widget(bool this_is_ignored);

	protected:
		friend class ContainerBase;
		friend class Container;
		friend class Tabs;

	public:
		Widget(std::string id);

		virtual ~Widget();

		// show_now() tells Kamoflage to show this object to the user now. This might require multiple operations by Kamoflage, such as shifting tabs, scrolling through lists and so forth. (please note, this is not fully implemented yet... Currently it will only work on tabs directly controlling this object... no chain reactions..)
		void show_now();

		std::string get_id();
		std::string get_title();
		bool get_expand();
		bool get_fill();
		void set_expand(bool exp);
		void set_fill(bool fill);
		int get_border_width();

		// might be used in some to trigger a redraw of this widget
		void redraw();

		void attach_event_handler(EventHandler *evh);

		virtual void inject_key_event(bool is_press, char chr);

		static void call_on_init();
		static KammoGUI::Widget *get_widget(std::string id);
	};

	class ContainerBase : public Widget {
	protected:
		std::vector<KammoGUI::Widget *> children;

		virtual ~ContainerBase();
		ContainerBase(bool this_is_ignored);
	public:
		ContainerBase(std::string id);

		virtual void clear() = 0;
		virtual void add(Widget &wid) = 0;
	};

	class Container : public ContainerBase {
	private:
	protected:
	public:
		virtual ~Container();
		Container(std::string id);
		Container(bool horizontal);

		virtual void clear();
		virtual void add(Widget &wid);
	};

	class Tabs : public ContainerBase {
	protected:
		Widget *current_view;

	public:
		Tabs(std::string id);

		virtual void clear();
		virtual void add(Widget &wid);

		Widget *get_current_view();

		// don't call this directly (only intended for internal use)
		void set_current_view(Widget *current);
	};

	class Window : public Container {
	private:
	protected:

	public:
		Window(std::string id);
		virtual ~Window();

		void set_title(const char *title);
		void set_defsize(int w, int h);

		void set_fullscreen(bool fsc);

		virtual bool delete_event();
		virtual void destroy_event();

		virtual void clear();
		virtual void add(Widget &wid);
	};

	class Label : public Widget {
	public:
		void set_title(std::string txt);

		Label(std::string id);
		Label();
	};

	class Button : public Widget {
	private:
		void create_internal_android();

	public:
		void set_title(std::string title);

		void listener();

		Button(std::string id);
		Button();
	};

	class Surface : public Widget {
	private:
		void *cbdat;
		void (*surface_event_cb)(void *callback_data, surfaceEvent_t se,
					 int x, int y);
	public:
		void trigger_surface_event(surfaceEvent_t se, int x, int y);

		void set_callbacks(
			void *callback_data,
			void (*surface_event)(void *callback_data, surfaceEvent_t se,
					      int x, int y)
			);

		Surface(std::string id);
	};

	// abstract class
	class Animation {
	private:
		struct timespec before;
		float duration;
		bool is_running;

		friend class Canvas;
		friend class SVGCanvas;

	public:
		float get_now();

		void start();
		bool has_finished();
		void new_time_tick();
		void touch_event();

		// if you want this animation to run "forever" (or until you explicitly stop it)
		// use duration = 0.0
		Animation(float duration);
		virtual ~Animation();

		void stop();

		virtual void new_frame(float progress) = 0;
		virtual void on_touch_event() = 0;
	};

	class SimpleAnimation : public Animation {
	private:
		std::function<void(float progress)> callback;
	public:
		SimpleAnimation(float duration, std::function<void(float progress)> _callback)
			: Animation(duration)
			, callback(_callback) {}
		virtual void new_frame(float progress) override {
			callback(progress);
		}
		virtual void on_touch_event() override { /* ignored */ }
	};

	// class MotionEvent is modelled after the MotionEvent
	// class available in Android and is semi-compatible
	class MotionEvent {
	public:
		enum motionEvent_t {
			ACTION_CANCEL,
			ACTION_DOWN,
			ACTION_MOVE,
			ACTION_OUTSIDE,
			ACTION_POINTER_DOWN,
			ACTION_POINTER_UP,
			ACTION_UP
		};
	private:
		long down_time, event_time;
		motionEvent_t action;
		int action_index; // which index is responsible for the action
		int pointer_count; // maximum 16
		int pointer_id[16];
		float pointer_x[16];
		float pointer_y[16];
		float pointer_pressure[16];
		float raw_x, raw_y;

	public:
		void init(long downTime, long eventTime, motionEvent_t action, int pointerCount, int actionIndex, float rawX, float rawY);
		void clone(const MotionEvent &source);

		// each pointer will have the same id between different motion events, from the time they are POINTER_DOWN
		// to the time they are POINTER_UP. However, because pointers may come and go (go DOWN or UP) the index
		// between different events may change. (in one event you might have three pointers, while only two of them
		// are left in the following..)
		void init_pointer(int index, int id, float x, float y, float pressure);

		long get_down_time() const;
		long get_event_time()  const;

		motionEvent_t get_action() const;
		int get_action_index() const;

		float get_x() const;
		float get_y() const;
		float get_pressure() const;
		float get_x(int index) const;
		float get_y(int index) const;
		float get_pressure(int index) const;

		float get_raw_x() const;
		float get_raw_y() const;

		int get_pointer_count() const;
		int get_pointer_id(int index) const;
	};

	class Entry : public Widget {
	private:
	public:
		Entry(std::string id);

		std::string get_text();
		void set_text(const std::string &txt);
	};

	class CheckButton : public Widget {
	private:
	public:
		CheckButton(std::string id);

		bool get_state();
		void set_state(bool _state);
	};

	class Scale : public Widget {
	private:
	public:
		Scale(std::string id);

		Scale(bool _horizontal, double _min,
		      double _max, double _inc);

		void set_value(double val);
		double get_value(void);
	};

	class SensorEvent {
	public:
		enum Type {
			Accelerometer
		};

		Type type;
		float values[3];

		class Listener {
		public:
			virtual void on_sensor_event(std::shared_ptr<SensorEvent> event) = 0;
		};

		static void register_listener(std::shared_ptr<Listener> listener);
		static void handle_event(float v1, float v2, float v3);
	};

	class PollEvent : public Widget {
	private:
	public:
		PollEvent(std::string id);
	};

	class UserEvent : public Widget {
	public:
		UserEvent(std::string id);
	};

	class EventHandler {
		/*** STATIC BITS ***/
	private:
		static std::multimap<std::string, KammoGUI::EventHandler *> event_handlers;

		friend class Widget;
		static std::vector<KammoGUI::EventHandler *>find_handlers(KammoGUI::Widget*);
	public:

		static void handle_on_init(KammoGUI::Widget *wid);
		static void handle_on_modify(KammoGUI::Widget *wid);
		static void handle_on_value_changed(KammoGUI::Widget *wid);
		static void handle_on_click(KammoGUI::Widget *wid);
		static void handle_on_double_click(KammoGUI::Widget *wid);
		static void handle_on_destroy(KammoGUI::Widget *wid);
		static bool handle_on_delete(KammoGUI::Widget *wid);
		static void handle_on_poll(KammoGUI::Widget *wid);

		static void trigger_user_event(KammoGUI::UserEvent *event, std::map<std::string, void *> data);
		static void trigger_user_event(KammoGUI::UserEvent *event);

		/*** DYNAMIC BITS ***/
	private:
		std::string id;

	public:
		EventHandler(std::string id);
		virtual ~EventHandler();

		virtual void on_init(KammoGUI::Widget *widget);
		virtual void on_modify(KammoGUI::Widget *widget);
		virtual void on_value_changed(KammoGUI::Widget *widget);
		virtual void on_click(KammoGUI::Widget *widget);
		virtual void on_double_click(KammoGUI::Widget *widget);
		virtual void on_destroy(KammoGUI::Widget *widget);
		virtual bool on_delete(KammoGUI::Widget *widget);
		virtual void on_poll(KammoGUI::Widget *wid);
		virtual void on_user_event(KammoGUI::UserEvent *event, std::map<std::string, void *> data);

		std::string get_id();
	};

	class DisplayConfiguration {
	private:
		static float width_inch, height_inch; // in inches
		static float resolution_h, resolution_v; // pixels / inch (horizontal, vertical)
		static int edge_slop;

		static float inches_to_unit(KammoGUI::geometricUnit_t geo, float in_inches, bool horizontal);
	public:
		static float get_screen_width(geometricUnit_t geo);
		static float get_screen_height(geometricUnit_t geo);
		static int get_edge_slop(); // in pixels

		static void init(float width_inch, float height_inch,
				 float resolution_h, float resolution_v,
				 int edge_slop);
	};

	class CancelIndicator {
	public:
		virtual bool is_cancelled() = 0;
	};

	/// this will run the busy_function in a background thread
	/// while Kamoflage displays a unmuteable Busy dialog to the user.
	/// When the busy_work function returns the dialog will disappear.
	/// NOTE! No UI calls should be made from the busy work function
	/// since it might cause a dead lock! Unless you use run_on_GUI_thread()
	void do_busy_work(const std::string &title,
			  const std::string &message,
			  void (*busy_function)(void *data), void *data);
	void do_cancelable_busy_work(const std::string &title,
				     const std::string &message,
				     void (*busy_function)(void *data, CancelIndicator &cid), void *data);

	/// run this callback in the UI thread, usefull for triggering
	/// stuff from a busy. This runs ASYNCHRONOUSLY
	void run_on_GUI_thread(void (*on_ui_func)(void *data), void *data);

	/// run this callback in the UI thread, usefull for triggering
	/// stuff from a busy. This runs SYNCHRONOUSLY with calling thread, i.e. it won't return until
	/// the on_ui_func returns
	void run_on_GUI_thread_synchronized(void (*on_ui_func)(void *data), void *data);

	void run_on_GUI_thread(std::function<void()> func, bool synchronized = false);

	void display_notification(const std::string &title,
				  const std::string &message);

	void ask_yes_no(const std::string &title,
			const std::string &message,
			void (*ya_function)(void *data), void *ya_data,
			void (*no_function)(void *data), void *no_data
		);

	void external_uri(const std::string &uri);

	void start();

	void get_widget(KammoGUI::Widget **target, std::string id);
};

#endif
