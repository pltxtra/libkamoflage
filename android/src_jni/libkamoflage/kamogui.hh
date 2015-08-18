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

#include <jni.h>

#include <libsvg-android/svg-android-internal.h>

extern JavaVM *__g_jvm;

void __setup_env_for_thread(JNIEnv *env);
JNIEnv *get_env_for_thread();

/**************************************
 * BEGIN EventHandler creation macros *
 **************************************/

#define KammoEventHandler_Declare(a,b) namespace a { \
class a : public  KammoGUI::EventHandler { \
	private: \
 a() : EventHandler(b) {} \
static a HandlerHolder; \
\
 public:

#define KammoEventHandler_Instance(a) };\
 a a::HandlerHolder; \
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
		jobject internal;

		std::string id;

		static std::map<std::string, KammoGUI::Widget *> widgets;

		Widget(bool this_is_ignored);

	protected:
		friend class ContainerBase;
		friend class Container;
		friend class Tabs;

		/***********************************************
		 * begin invalidation stuff (android specific) *
		 ***********************************************/
	protected:
		Widget(std::string id, jobject jobj);
		void invalidate();
		bool queued_for_invalidation;
		static std::vector<Widget *> invalidation_queue;
		static void queue_for_invalidate(Widget *wid);

	public:
		/// DO NOT CALL THIS FROM ANY APPLICATION CODE; INTERNAL!
		static void flush_invalidation_queue();
		/***********************************************
		 * end invalidation stuff (android specific) *
		 ***********************************************/

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

//		/* don't call these directly, used internally! */
//		void set_internal_reference(jobject jobj);

		static void call_on_init();
		static KammoGUI::Widget *get_widget(std::string id);
	};

	class ContainerBase : public Widget {
	protected:
		std::vector<KammoGUI::Widget *> children;

		virtual ~ContainerBase();
		ContainerBase(bool this_is_ignored);
	public:
		ContainerBase(std::string id, jobject jobj);

		virtual void clear() = 0;
		virtual void add(Widget &wid) = 0;
	};

	class Container : public ContainerBase {
	private:
	protected:
	public:
		virtual ~Container();
		Container(std::string id, jobject jobj);
		Container(bool horizontal);

		virtual void clear();
		virtual void add(Widget &wid);
	};

	class Tabs : public ContainerBase {
	protected:
		Widget *current_view;

	public:
		Tabs(std::string id, jobject jobj);

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
		Window(std::string id, jobject jobj);
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

		Label(std::string id, jobject jobj);
		Label();
	};

	class Button : public Widget {
	private:
		void create_internal_android();

	public:
		void set_title(std::string title);

		void listener();

		Button(std::string id);
		Button(std::string id, jobject jobj);
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

		Surface(std::string id, jobject jobj);
	};

	// abstract class
	class Animation {
	private:
		struct timespec before;
		float duration;
		bool is_running;

		friend class Canvas;
		friend class SVGCanvas;

		void get_now(float &progress);

		void start();
		bool has_finished();
		void new_time_tick();
		void touch_event();

	public:
		// if you want this animation to run "forever" (or until you explicitly stop it)
		// use duration = 0.0
		Animation(float duration);
		virtual ~Animation();

		void stop();

		virtual void new_frame(float progress) = 0;
		virtual void on_touch_event() = 0;
	};

	class Canvas : public Widget {
	public:
		class SVGBob;

	private:
		Animation *current_animation;

		int __width, __height;

		// internal android stuff
		jint bg_r, bg_g, bg_b;

		jobject canvas;
		JNIEnv *env; // watch out with this one...

		jclass canvas_clazz;
		jmethodID canvas_draw_bitmap;
		jmethodID canvas_constructor;

		jclass raster_clazz; // libsvg-android SvgRaster class
		jmethodID raster_createBitmap;
		jmethodID raster_matrixCreate;

		// application callback stuff
		void *__cbd;
		void (*canvas_expose_cb)(void *callback_data);
		void (*canvas_resize_cb)(void *callback_data,
					 int width, int height);
		void (*canvas_event_cb)(void *callback_data, canvasEvent_t ce,
					int x, int y);
		float (*canvas_measure_inches_cb)(void *callback_data, bool measureWidth);
		friend class SVGBob;

	public:
		// internal android callbacks
		static void canvas_expose(JNIEnv *env, void *callback_data, jobject canvas);
		static void canvas_resize(JNIEnv *env,
					  void *callback_data,
					  int width, int height);
		static void canvas_event(JNIEnv *env, void *callback_data, canvasEvent_t ce,
					 int x, int y);
		static float canvas_measure_inches(JNIEnv *env, void *callback_data, bool measureWidth);

	public:

		class SVGDefinition {
		private:
			friend class Canvas;

			svg_android_t *internal;

			std::string file_name;

			SVGDefinition();
		public:
			~SVGDefinition();
			static SVGDefinition *from_file(const std::string &fname);
			static SVGDefinition *from_string(const std::string &fname);
		};

		class SVGBob {
		private:
			friend class Canvas;

			bool dirty_flag;
			jobject bob_internal;

			SVGDefinition *svgd;
			Canvas *target;

			int bl_w, bl_h;
			int last_w, last_h;

			double blit_x_scale, blit_y_scale;

			SVGBob();

			void prepare_bob();
		public:
			SVGBob(Canvas *target, SVGDefinition *svgd);
			~SVGBob();

			void set_blit_size(int w, int h);
			void change_definition(SVGDefinition *svgd);
		};


		void start_animation(Animation *animation); // animator can be NULL, if not NULL remember that Canvas will delete the object when done...
		void stop_animation();

		void blit_SVGBob(SVGBob *svgb, int x, int y);

		void set_bg_color(double r, double g, double b);

		void set_callbacks(
			void *callback_data,
			void (*canvas_expose)(void *callback_data),
			void (*canvas_resize)(void *callback_data,
					      int width, int height),
			void (*canvas_event)(void *callback_data, canvasEvent_t ce,
					     int x, int y),
			float (*canvas_measure_inches)(void *callback_data, bool measureWidth)
			);

		Canvas(std::string id, jobject jobj);
		~Canvas();
	};

	class SVGCanvas : public Widget {
	public:
		enum dimension_t {
			HORIZONTAL_DIMENSION, VERTICAL_DIMENSION
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

		class SVGMatrix {
		public:
			SVGMatrix();

			/*****************
			 * pointInPreviousSpace = Matrix * pointInNewSpace
			 *
			 *               | a c e |   | Xn |
 			 * (Xp, Yp, 1) = | b d f | * | Yn |
                         *               | 0 0 1 |   | 1  |
			 *
			 *****************/
			double a, b, c, d, e, f;

			void init_identity();
			void translate(double x, double y);
			void scale(double x, double y);
			void rotate(double angle_in_radians);
			void multiply(const SVGMatrix &other);
		};

		class SVGRect {
		public:
			double x, y, width, height;
		};

		class SVGDocument;

		class ElementReference {
		private:
			SVGDocument *source;
			mutable svg_element_t *element;
			std::function<void(SVGDocument *source,
					   ElementReference *e,
					   const MotionEvent &event)> event_handler;
			void trigger_event_handler(const MotionEvent &event);

			friend class SVGCanvas;

			void dereference();
		public:
			ElementReference();
			/// get element reference by id
			ElementReference(SVGDocument *source, const std::string &id);
			/// get element reference by id, using the SVGDocument from the sibling element
			ElementReference(const ElementReference *sibling, const std::string &id);
			/// copy an existing ElementReference object
			ElementReference(const ElementReference *original);
			/// copy an existing ElementReference object
			ElementReference(const ElementReference &original);
			/// Explicit move constructor
			ElementReference(ElementReference &&original);
			/// get root element reference
			ElementReference(SVGDocument *source);
			~ElementReference();

			std::string get_id();
			std::string get_class();

			void get_transform(SVGMatrix &matrix_output);
			std::string get_text_content();

			void* pointer();

			void set_transform(const SVGMatrix &matrix);
			void set_xlink_href(const std::string &url);
			void set_text_content(const std::string &content);
			void set_display(const std::string &value);
			void set_style(const std::string &value);

			void set_line_coords(float x1, float y1, float x2, float y2);
			void set_rect_coords(float x, float y, float width, float height);

			void set_event_handler(std::function<void(SVGDocument *source,
								  ElementReference *e,
								  const MotionEvent &event)> event_handler);

			void add_svg_child(const std::string &svg_chunk);
			ElementReference add_element_clone(const std::string &new_id, const ElementReference &element_to_clone);

			void get_viewport(SVGRect &rect);
			void get_boundingbox(SVGRect &rect); // return bounding box in canvas coordinates

			void drop_element();

			void debug_dump_render_tree();
			void debug_dump_id_table();
			void debug_ref_count();

			ElementReference find_child_by_class(const std::string &class_name);

			ElementReference& operator=(ElementReference&& a);
		};

		class SVGDocument {
		private:
			std::set<Animation *> animations;
			SVGCanvas *parent;
			svg_android_t *svg_data;

			std::string file_name;

			friend class ElementReference;
			friend class SVGCanvas;

			// called by SVGCanvas
			void process_active_animations();
			void process_touch_for_animations();
			int number_of_active_animations();

		protected:
			SVGDocument(const std::string &fname, SVGCanvas *parent);
			SVGDocument(SVGCanvas *parent, const std::string &xml);

			void get_canvas_size(int &width_in_pixels, int &height_in_pixels);
			void get_canvas_size_inches(float &width_in_inches, float &height_in_inches);
		public:

			SVGCanvas *get_parent();

			virtual ~SVGDocument();

			/// start to animate a new animation
			void start_animation(Animation *new_animation);
			/// stop an animation - or if NULL pointer, all animations - now
			void stop_animation(Animation *animation_to_stop = NULL);

			virtual void on_render() = 0;
			virtual void on_resize();
			virtual float get_preferred_dimension(dimension_t dimension); // return value measured in inches
		};

	private:
		std::vector<SVGDocument *> documents;

		int __width, __height;
		float __width_inches, __height_inches;
		jint bg_r, bg_g, bg_b;

		ElementReference *active_element; // if we have an active motion associated with an element

		MotionEvent m_evt; // motion event object

		friend class SVGDocument;
		friend class ElementReference;

	public:
		/*************
		 *
		 * internal android callbacks - do not use them from any application code, it won't port...
		 *
		 *************/
		static void canvas_expose(JNIEnv *env, SVGCanvas *cnv, jobject canvas);
		static void canvas_resize(JNIEnv *env, SVGCanvas *cnv, int width, int height, float width_inches, float height_inches);
		static float canvas_measure_inches(JNIEnv *env, SVGCanvas *cnv, bool measureWidth);
		static void canvas_motion_event_init_event(
			JNIEnv *env, SVGCanvas *cnv, long downTime, long eventTime, int androidAction, int pointerCount, int actionIndex,
			float rawX, float rawY);
		static void canvas_motion_event_init_pointer(JNIEnv *env, SVGCanvas *cnv, int index, int id, float x, float y, float pressure);
		static void canvas_motion_event(JNIEnv *env, SVGCanvas *cnv);
	public:

		SVGCanvas(std::string id, jobject jobj);
		~SVGCanvas();

		void set_bg_color(double r, double g, double b);

	public:
		class NoSuchElementException : public jException {
		public:
			NoSuchElementException(const std::string &id);
		};

		class OperationFailedException : public jException {
		public:
			OperationFailedException();
		};

	};

	class List  : public Widget
	{
	private:
		bool is_drop_down;

		int nrcols;
		std::vector<std::string> titles;
		std::vector<int> widths;

	public:
		class iterator {
		private:
			friend class List;

			class iter_object {
			public:
				jint iter; // internal representation, for Android this is an int..
				int ref_count;
				iter_object();
			};

			iter_object *iter;
		public:
			iterator(const iterator &);
			iterator();
			~iterator();

			iterator& operator =(const iterator &i);
		};

		// PLEASE DON'T CALL THIS FROM APPLICATION CODE!
		void trigger_on_select_row_event(int row_id);

		class NoRowSelected : public jException {
		public:
			NoRowSelected();
		};


		List(std::string id, jobject jobj);

		void add_row(std::vector<std::string> data);
		//void insert_row(char *data[], int iterator);
		void remove_row(iterator &row);
		void clear(void);

		iterator get_selected();

		std::string get_value(const iterator &iter, int col);

		//void set_text(int row, int column, const char *text);
		//void select_row(int row, int column);
	};

	class Entry : public Widget {
	private:
	public:
		Entry(std::string id, jobject jobj);

		std::string get_text();
		void set_text(const std::string &txt);
	};

	class CheckButton : public Widget {
	private:
	public:
		CheckButton(std::string id, jobject jobj);

		bool get_state();
		void set_state(bool _state);
	};

	class Scale : public Widget {
	private:
	public:
		Scale(std::string id, jobject jobj);

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

	private:
		static std::vector<std::shared_ptr<Listener> > listeners;
	};

	class PollEvent : public Widget {
	private:
	public:
		PollEvent(std::string id, jobject jobj);
	};

	class UserEvent : public Widget {
	public:
		UserEvent(std::string id, jobject jobj);
	};

	class EventHandler {
		/*** STATIC BITS ***/
	private:
		static std::multimap<std::string, KammoGUI::EventHandler *> *event_handlers;

		friend class Widget;
		static std::vector<KammoGUI::EventHandler *>find_handlers(KammoGUI::Widget*);
	public:

		static void handle_on_init(KammoGUI::Widget *wid);
		static void handle_on_modify(KammoGUI::Widget *wid);
		static void handle_on_value_changed(KammoGUI::Widget *wid);
		static void handle_on_select_row(KammoGUI::Widget *wid, KammoGUI::List::iterator row);
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
		virtual void on_select_row(KammoGUI::Widget *widget, KammoGUI::List::iterator row);
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

extern std::map<pthread_t, JNIEnv *> __envm; // one env for each thread..

#define __ENV (__envm[self])
#define GET_INTERNAL_CLASS(d,I)	\
	static jclass d = NULL; \
	if(d == NULL) { \
		d = __ENV->GetObjectClass(I); \
		d = (jclass)(__ENV->NewGlobalRef((jobject)d)); \
	}

#endif
