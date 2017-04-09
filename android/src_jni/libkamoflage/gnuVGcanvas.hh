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

#ifndef __KAMMOGUI_GNUVGCANVAS
#define __KAMMOGUI_GNUVGCANVAS

#include "kamogui.hh"

#include <VG/openvg.h>

namespace KammoGUI {

	template <typename F>
	struct FinalAction {
		FinalAction(F f) : clean_{f} {}
		~FinalAction() { if(enabled_) clean_(); }
		void disable() { enabled_ = false; };
	private:
		F clean_;
		bool enabled_{true}; };

	template <typename F>
	FinalAction<F> finally(F f) {
		return FinalAction<F>(f);
	}

	template <typename T>
	class GvgVector {
	private:
		T* __data;
		size_t total_size;
		size_t in_use;
		size_t iterate_head;

		void reallocate(size_t new_size) {
			total_size = new_size;
			T* new_data = (T*)realloc(__data, total_size * sizeof(T));
			__data = new_data;
		}
	public:
		GvgVector() : total_size(4), in_use(0) {
			__data = (T*)malloc(total_size * sizeof(T));
		}

		~GvgVector() {
			free(__data);
		}

		void push_back(T value) {
			if(total_size == in_use)
				reallocate(2 * total_size);
			__data[in_use++] = value;
		}

		void append(const T* data, size_t element_count) {
			if(in_use + element_count >= total_size)
				reallocate(2 * (in_use + element_count));
			memcpy(&__data[in_use], data, element_count * sizeof(T));
			in_use += element_count;
		}

		T* data() {
			return __data;
		}

		T* head() {
			return &__data[iterate_head];
		}

		void skip(size_t elements_to_skip) {
			iterate_head += elements_to_skip;
		}

		void clear() {
			in_use = 0;
		}

		size_t size() {
			return in_use;
		}
	};

	class VGImageAllocator {
	public:
		virtual VGImage get_fresh_bitmap() = 0;
		virtual void recycle_bitmap(VGImage vgi) = 0;
	};

	class GnuVG_feOperation {
	public:
		int depend_on_1 = -1, depend_on_2 = -1;
		int ref_count = 0, expected_ref_count = 0;
		VGImage result = VG_INVALID_HANDLE;

		svg_length_t x, y, width, height;

		void unref_result(VGImageAllocator* vgallocator);

		virtual ~GnuVG_feOperation() {}

		VGImage get_result(
			VGImageAllocator* vgallocator,
			std::vector<GnuVG_feOperation*> &fe_ops,
			VGImage sourceGraphic, VGImage backgroundImage
			) {
			if(result == VG_INVALID_HANDLE)
				result = execute(vgallocator, fe_ops,
						 sourceGraphic, backgroundImage);

			return result;
		}

		VGImage get_image(
			VGImageAllocator* vgallocator,
			std::vector<GnuVG_feOperation*> &fe_ops,
			VGImage sourceGraphic, VGImage backgroundImage,
			svg_filter_in_t in, int in_op_reference
			);
		void recycle_image(
			VGImageAllocator* vgallocator,
			std::vector<GnuVG_feOperation*> &fe_ops,
			svg_filter_in_t in, int in_op_reference);

		virtual VGImage execute(
			VGImageAllocator* vgallocator,
			std::vector<GnuVG_feOperation*> &fe_ops,
			VGImage sourceGraphic, VGImage backgroundImage
			) = 0; // returns an image containing the result

		virtual void recreate_expected_ref_count(std::vector<GnuVG_feOperation*> &fe_ops) = 0;

		GnuVG_feOperation(
			svg_length_t* _x, svg_length_t* _y,
			svg_length_t* _width, svg_length_t* _height)
			{
			x = *_x;
			y = *_y;
			width = *_width;
			height = *_height;
		}

	};

	class GnuVG_feComposite : public GnuVG_feOperation {
	public:
		feCompositeOperator_t oprt;
		svg_filter_in_t in; int in_op_reference;
		svg_filter_in_t in2; int in2_op_reference;
		double k1, k2, k3, k4;

		GnuVG_feComposite(
			svg_length_t* _x, svg_length_t* _y,
			svg_length_t* _width, svg_length_t* _height,
			feCompositeOperator_t _oprt,
			svg_filter_in_t _in, int _in_op_reference,
			svg_filter_in_t _in2, int _in2_op_reference,
			double _k1, double _k2, double _k3, double _k4
			)
			: GnuVG_feOperation(_x, _y, _width, _height)
			, oprt(_oprt)
			, in(_in), in_op_reference(_in_op_reference)
			, in2(_in2), in2_op_reference(_in2_op_reference)
			, k1(_k1), k2(_k2), k3(_k3), k4(_k4)
			{
		}

		virtual VGImage execute(
			VGImageAllocator* vgallocator,
			std::vector<GnuVG_feOperation*> &fe_ops,
			VGImage sourceGraphic, VGImage backgroundImage
			);

		virtual void recreate_expected_ref_count(std::vector<GnuVG_feOperation*> &fe_ops) {
			++expected_ref_count;

			if(in == in_Reference)
				fe_ops[in_op_reference]->recreate_expected_ref_count(fe_ops);
			if(in2 == in_Reference)
				fe_ops[in2_op_reference]->recreate_expected_ref_count(fe_ops);
		}
	};

	class GnuVG_feFlood : public GnuVG_feOperation {
	public:
		svg_filter_in_t in; int in_op_reference;
		const svg_color_t color;
		double opacity;

		VGfloat clear_color[4];

		GnuVG_feFlood(
			svg_length_t* _x, svg_length_t* _y,
			svg_length_t* _width, svg_length_t* _height,
			svg_filter_in_t _in, int _in_op_reference,
			const svg_color_t* _color, double _opacity
			)
			: GnuVG_feOperation(_x, _y, _width, _height)
			, in(_in), in_op_reference(_in_op_reference)
			, color(*_color), opacity(_opacity) {
			int r = (color.rgb >> 16) & 0xff;
			int g = (color.rgb >>  8) & 0xff;
			int b = (color.rgb >>  0) & 0xff;

			VGfloat f_r = (VGfloat)r;
			VGfloat f_g = (VGfloat)g;
			VGfloat f_b = (VGfloat)b;

			clear_color[0] = f_r / 255.0f;
			clear_color[1] = f_g / 255.0f;
			clear_color[2] = f_b / 255.0f;
			clear_color[3] = opacity;
		}

		virtual VGImage execute(
			VGImageAllocator* vgallocator,
			std::vector<GnuVG_feOperation*> &fe_ops,
			VGImage sourceGraphic, VGImage backgroundImage
			);

		virtual void recreate_expected_ref_count(std::vector<GnuVG_feOperation*> &fe_ops) {
			++expected_ref_count;

			if(in == in_Reference)
				fe_ops[in_op_reference]->recreate_expected_ref_count(fe_ops);
		}
	};

	class GnuVG_feGaussianBlur : public GnuVG_feOperation {
	public:
		svg_filter_in_t in; int in_op_reference;
		VGfloat std_dev_x, std_dev_y;

		GnuVG_feGaussianBlur(
			svg_length_t* _x, svg_length_t* _y,
			svg_length_t* _width, svg_length_t* _height,
			svg_filter_in_t _in, int _in_op_reference,
			double _std_dev_x, double _std_dev_y
			)
			: GnuVG_feOperation(_x, _y, _width, _height)
			, in(_in), in_op_reference(_in_op_reference)
			, std_dev_x(_std_dev_x)
			, std_dev_y(_std_dev_y)
			{}

		virtual VGImage execute(
			VGImageAllocator* vgallocator,
			std::vector<GnuVG_feOperation*> &fe_ops,
			VGImage sourceGraphic, VGImage backgroundImage
			);

		virtual void recreate_expected_ref_count(std::vector<GnuVG_feOperation*> &fe_ops) {
			++expected_ref_count;

			if(in == in_Reference)
				fe_ops[in_op_reference]->recreate_expected_ref_count(fe_ops);
		}
	};

	class GnuVG_feOffset : public GnuVG_feOperation {
	public:
		svg_filter_in_t in; int in_op_reference;
		VGfloat dx, dy;

		GnuVG_feOffset(
			svg_length_t* _x, svg_length_t* _y,
			svg_length_t* _width,
			svg_length_t* _height,
			svg_filter_in_t _in, int _in_op_reference,
			double _dx, double _dy
			)
			: GnuVG_feOperation(_x, _y, _width, _height)
			, in(_in), in_op_reference(_in_op_reference)
			, dx(_dx), dy(_dy)
			{}

		virtual VGImage execute(
			VGImageAllocator* vgallocator,
			std::vector<GnuVG_feOperation*> &fe_ops,
			VGImage sourceGraphic, VGImage backgroundImage
			);

		virtual void recreate_expected_ref_count(std::vector<GnuVG_feOperation*> &fe_ops) {
			++expected_ref_count;

			if(in == in_Reference)
				fe_ops[in_op_reference]->recreate_expected_ref_count(fe_ops);
		}
	};

	class GnuVGFilter {
	public:
		std::vector<GnuVG_feOperation*> fe_operations;
		GnuVG_feOperation* last_operation;

		VGImage execute(VGImageAllocator* vgallocator,
				VGImage sourceGraphic, VGImage backgroundImage);

		void clear() {
			for(auto f : fe_operations)
				delete f;
			fe_operations.clear();
			last_operation = nullptr;
		}

		~GnuVGFilter() {
			clear();
		}

		void add_operation(GnuVG_feOperation* op) {
			fe_operations.push_back(op);

			for(auto op : fe_operations)
				op->expected_ref_count = 0;

			last_operation = op;
			last_operation->recreate_expected_ref_count(fe_operations);
		}
	};

	class GnuVGCanvas : public Widget {
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

			SVGRect(double _x, double _y,
				double _width, double _height)
				: x(_x)
				, y(_y)
				, width(_width)
				, height(_height) {}

			SVGRect()
				: x(0.0), y(0.0), width(0.0), height(0.0) {}

			// if non intersecting the result will be all zeroes
			void intersect_with(const SVGRect &other);
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

			friend class GnuVGCanvas;

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

			void get_viewport(SVGRect &rect) const;
			void get_boundingbox(SVGRect &rect); // return bounding box in canvas coordinates

			void drop_element();

			void debug_dump_render_tree();
			void debug_dump_id_table();
			void debug_ref_count();

			ElementReference find_child_by_class(const std::string &class_name);

			ElementReference& operator=(ElementReference&& a);
		};

		class SVGDocument : public VGImageAllocator {
		private:
			std::map<std::string, GnuVGFilter *> filters;

			std::set<Animation *> animations;
			GnuVGCanvas *parent;
			svg_t *svg;
			std::string file_name;

			friend class ElementReference;
			friend class GnuVGCanvas;

			// called by GnuVGCanvas
			void process_active_animations();
			void process_touch_for_animations();
			int number_of_active_animations();

			struct State {
				static std::map<std::string, VGFont> font_table;

				VGfloat matrix[9];

				svg_color_t color;
				VGfloat opacity;

				VGbitfield paint_modes;

				VGImage current_bitmap, previous_bitmap;
				GnuVGFilter *current_filter;
				VGfloat bitmap_opacity;

				VGint fill_rule;
				svg_paint_t fill_paint;
				VGfloat fill_opacity;

				svg_paint_t stroke_paint;
				VGfloat stroke_opacity;
				svg_length_t stroke_width;
				svg_stroke_line_cap_t line_cap;
				svg_stroke_line_join_t line_join;
				VGfloat miter_limit;

				bool has_clip_box;
				VGfloat clip_box[4]; // x,y,w,h
				unsigned int bounding_box[4]; // left, top, right, bottom

				// current clip coordinates
				VGint final_clip_coordinates[4]; // left, top, right, bottom

				GvgVector<VGfloat> dash;
				VGfloat dash_phase;

				std::string font_family;
				VGfloat font_size;
				gnuVGFontStyle font_style;
				gnuVGTextAnchor text_anchor;
				VGFont active_font;
				bool font_dirty;

				VGfloat viewport_width, viewport_height;

				GvgVector<VGubyte> pathSeg;
				GvgVector<VGfloat> pathData;

				void init_by_copy(const State* original);
				void init_fresh();

				void update_bounding_box(unsigned int *new_bbox);

				std::string create_font_identifier(const std::string& ffamily,
								   gnuVGFontStyle fstyle);
				VGFont get_font(const std::string &identifier,
						gnuVGFontStyle fstyle);
				void configure_font();
			};

			VGfloat DPI;

			VGPaint fill;
			VGPaint stroke;
			VGPath temporary_path;

			State* state;
			std::vector<State*> state_stack;
			std::stack<State*> state_unused;
			std::deque<VGImage> bitmap_store;

			void set_color_and_alpha(
				VGPaint* vgpaint, State* state, const svg_color_t *color, double alpha);
			void set_gradient(
				VGPaint* vgpaint,
				State* state,
				svg_gradient_t* gradient);
			void set_paint(VGPaint* vgpaint, State* state,
				       const svg_paint_t* paint, VGfloat opacity);
			void regenerate_scissors();
			void length_to_pixel(svg_length_t *length, VGfloat* pixel);

			void push_current_bitmap();
			void pop_current_bitmap();

			void stack_push(VGImage offscreen_bitmap = VG_INVALID_HANDLE,
					VGfloat opacity = 1.0f);
			void stack_pop();
			void use_state_on_top();

			void fetch_gnuvg_boundingbox();

			// return 0 if bounding box is outside the visible clip
			int get_state_boundingbox(svg_bounding_box_t *bbox);

			static svg_status_t begin_group(void* closure, double opacity);
			static svg_status_t begin_element(void* closure, void *path_cache);
			static svg_status_t end_element(void* closure);
			static svg_status_t end_group(void* closure, double opacity);
			static svg_status_t move_to(void* closure, double x, double y);
			static svg_status_t line_to(void* closure, double x, double y);
			static svg_status_t curve_to(void* closure,
						     double x1, double y1,
						     double x2, double y2,
						     double x3, double y3);
			static svg_status_t quadratic_curve_to(void* closure,
							       double x1, double y1,
							       double x2, double y2);
			static svg_status_t arc_to(void* closure,
						   double	rx,
						   double	ry,
						   double	x_axis_rotation,
						   int	large_arc_flag,
						   int	sweep_flag,
						   double	x,
						   double	y);
			static svg_status_t close_path(void* closure);
			static svg_status_t free_path_cache(void* closure, void **path_cache);
			static svg_status_t set_color(void* closure, const svg_color_t *color);
			static svg_status_t set_fill_opacity(void* closure, double fill_opacity);
			static svg_status_t set_fill_paint(void* closure, const svg_paint_t *paint);
			static svg_status_t set_fill_rule(void* closure, svg_fill_rule_t fill_rule);
			static svg_status_t set_font_family(void* closure, const char *family);
			static svg_status_t set_font_size(void* closure, double size);
			static svg_status_t set_font_style(void* closure, svg_font_style_t font_style);
			static svg_status_t set_font_weight(void* closure, unsigned int font_weight);
			static svg_status_t set_opacity(void* closure, double opacity);
			static svg_status_t set_stroke_dash_array(void* closure, double *dash_array, int num_dashes);
			static svg_status_t set_stroke_dash_offset(void* closure, svg_length_t *offset_len);
			static svg_status_t set_stroke_line_cap(void* closure, svg_stroke_line_cap_t line_cap);
			static svg_status_t set_stroke_line_join(void* closure, svg_stroke_line_join_t line_join);
			static svg_status_t set_stroke_miter_limit(void* closure, double limit);
			static svg_status_t set_stroke_opacity(void* closure, double stroke_opacity);
			static svg_status_t set_stroke_paint(void* closure, const svg_paint_t *paint);
			static svg_status_t set_stroke_width(void* closure, svg_length_t *width);
			static svg_status_t set_text_anchor(void* closure, svg_text_anchor_t text_anchor);
			static svg_status_t set_filter(void* closure, const char* id) ;
			static svg_status_t begin_filter(void* closure, const char* id);
			static svg_status_t add_filter_feBlend(void* closure,
							       svg_length_t* x, svg_length_t* y,
							       svg_length_t* width, svg_length_t* height,
							       svg_filter_in_t in, int in_op_reference,
							       svg_filter_in_t in2, int in2_op_reference,
							       feBlendMode_t mode);
			static svg_status_t add_filter_feComposite(void* closure,
								   svg_length_t* x, svg_length_t* y,
								   svg_length_t* width, svg_length_t* height,
								   feCompositeOperator_t oprt,
								   svg_filter_in_t in, int in_op_reference,
								   svg_filter_in_t in2, int in2_op_reference,
								   double k1, double k2, double k3, double k4);
			static svg_status_t add_filter_feFlood(void* closure,
							       svg_length_t* x, svg_length_t* y,
							       svg_length_t* width, svg_length_t* height,
							       svg_filter_in_t in, int in_op_reference,
							       const svg_color_t* color, double opacity);
			static svg_status_t add_filter_feGaussianBlur(void* closure,
								      svg_length_t* x, svg_length_t* y,
								      svg_length_t* width, svg_length_t* height,
								      svg_filter_in_t in, int in_op_reference,
								      double std_dev_x, double std_dev_y);
			static svg_status_t add_filter_feOffset(void* closure,
								svg_length_t* x, svg_length_t* y,
								svg_length_t* width, svg_length_t* height,
								svg_filter_in_t in, int in_op_reference,
								double dx, double dy);
			static svg_status_t apply_clip_box(void* closure,
							   svg_length_t *x,
							   svg_length_t *y,
							   svg_length_t *width,
							   svg_length_t *height);
			static svg_status_t transform(void* closure,
						      double xx, double yx,
						      double xy, double yy,
						      double x0, double y0);
			static svg_status_t apply_view_box(void* closure,
							   svg_view_box_t view_box,
							   svg_length_t *width,
							   svg_length_t *height);
			static svg_status_t set_viewport_dimension(void* closure,
								   svg_length_t *width,
								   svg_length_t *height);
			static svg_status_t render_line(void* closure,
							svg_length_t *x1,
							svg_length_t *y1,
							svg_length_t *x2,
							svg_length_t *y2);
			static svg_status_t render_path(void* closure, void **path_cache);
			static svg_status_t render_ellipse(void* closure,
							   svg_length_t *cx,
							   svg_length_t *cy,
							   svg_length_t *rx,
							   svg_length_t *ry);
			static svg_status_t render_rect(void* closure,
							svg_length_t *x,
							svg_length_t *y,
							svg_length_t *width,
							svg_length_t *height,
							svg_length_t *rx,
							svg_length_t *ry);
			static svg_status_t render_text(void* closur,
							svg_length_t *x,
							svg_length_t *y,
							const char   *utf8);
			static svg_status_t render_image(void* closure,
							 unsigned char *data,
							 unsigned int	 data_width,
							 unsigned int	 data_height,
							 svg_length_t	 *x,
							 svg_length_t	 *y,
							 svg_length_t	 *width,
							 svg_length_t	 *height);
			static int get_last_bounding_box(void *closure, svg_bounding_box_t *bbox);


			svg_render_engine_t svg_render_engine;

			SVGDocument(GnuVGCanvas* parent);

		protected:
			SVGDocument(const std::string& fname, GnuVGCanvas* parent);
			SVGDocument(GnuVGCanvas* parent, const std::string& xml);

			void get_canvas_size(int &width_in_pixels, int &height_in_pixels);
			void get_canvas_size_inches(float &width_in_inches, float &height_in_inches);

			// calculate a scaling factor to fit element into a specific
			// size defined by "inches_wide" and "inches_tall"
			double fit_to_inches(const GnuVGCanvas::ElementReference *element,
					     double inches_wide, double inches_tall);

		public:
			void render();
			GnuVGCanvas* get_parent();

			virtual ~SVGDocument();

			/// start to animate a new animation
			void start_animation(Animation *new_animation);
			/// stop an animation - or if NULL pointer, all animations - now
			void stop_animation(Animation *animation_to_stop = NULL);

			virtual void on_render() = 0;
			virtual void on_resize();

			virtual float get_preferred_dimension(dimension_t dimension); // return value measured in inches

			virtual VGImage get_fresh_bitmap() override;
			virtual void recycle_bitmap(VGImage vgi) override;
			void invalidate_bitmap_store();
		};

	private:
		std::vector<SVGDocument *> documents;
		VGint window_width, window_height;
		VGfloat window_width_inches, window_height_inches;
		VGHandle gnuVGctx = VG_INVALID_HANDLE;
		VGfloat fundamentalMatrix[9];
		VGfloat bg_r, bg_g, bg_b;

		ElementReference *active_element; // if we have an active motion associated with an element
		MotionEvent m_evt; // motion event object

	public:
		inline void loadFundamentalMatrix() {
			vgLoadMatrix(fundamentalMatrix);
		}

		class GnuVGStateStackEmpty : public jException {
		public:
			GnuVGStateStackEmpty(const std::string &id) : jException(id, jException::sanity_error) {}
		};

		void set_bg_color(double r, double g, double b);

		/*************
		 *
		 * internal android callbacks - do not use them from any application code, it won't port...
		 *
		 *************/
		void init(JNIEnv *env);
		void resize(JNIEnv *env, int width, int height, float w_inches, float h_inches);
		void step(JNIEnv *env);
		void canvas_motion_event_init_event(
			JNIEnv *env, long downTime, long eventTime, int androidAction, int pointerCount, int actionIndex,
			float rawX, float rawY);
		void canvas_motion_event_init_pointer(JNIEnv *env, int index, int id, float x, float y, float pressure);
		void canvas_motion_event(JNIEnv *env);


	public:
		GnuVGCanvas(std::string id, jobject jobj);
		~GnuVGCanvas();

		class NoSuchElementException : public jException {
		public:
			NoSuchElementException(const std::string &id);
		};

		class OperationFailedException : public jException {
		public:
			OperationFailedException();
		};
	};
};

#endif
