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

namespace KammoGUI {

	class GnuVGCanvas : public Widget {
	public:
		class SVGDocument {
		private:
			GnuVGCanvas *parent;
			svg_t *svg;
			std::string file_name;

			struct State {
				static std::map<std::string, VGFont> font_table;

				VGfloat matrix[9];

				svg_color_t color;
				VGfloat opacity;

				VGbitfield paint_modes;

				VGint fill_rule;
				svg_paint_t fill_paint;
				VGfloat fill_opacity;

				svg_paint_t stroke_paint;
				VGfloat stroke_opacity;
				svg_length_t stroke_width;
				svg_stroke_line_cap_t line_cap;
				svg_stroke_line_join_t line_join;
				VGfloat miter_limit;

				std::vector<VGfloat> dash;
				VGfloat dash_phase;

				std::string font_family;
				VGfloat font_size;
				gnuVGFontStyle font_style;
				gnuVGTextAnchor text_anchor;
				VGFont active_font;
				bool font_dirty;

				VGfloat viewport_width, viewport_height;

				std::vector<VGubyte> pathSeg;
				std::vector<VGfloat> pathData;

				void init_by_copy(const State* original);
				void init_fresh();

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
			std::stack<State*> state_stack;
			std::stack<State*> state_unused;

			void set_color_and_alpha(
				VGPaint* vgpaint, State* state, const svg_color_t *color, double alpha);
			void set_gradient(
				VGPaint* vgpaint,
				State* state,
				svg_gradient_t* gradient);
			void set_paint(VGPaint* vgpaint, State* state,
				       const svg_paint_t* paint, VGfloat opacity);
			void length_to_pixel(svg_length_t *length, VGfloat* pixel);

			void stack_push();
			void stack_pop();
			void use_state_on_top();

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

		public:
			void render();
			GnuVGCanvas* get_parent();

			virtual ~SVGDocument();

			virtual void on_render() = 0;
			virtual void on_resize();
		};

	private:
		std::vector<SVGDocument *> documents;
		VGint window_width, window_height;
		VGHandle gnuVGctx = VG_INVALID_HANDLE;
		VGfloat fundamentalMatrix[9];

	public:
		inline void loadFundamentalMatrix() {
			vgLoadMatrix(fundamentalMatrix);
		}

		class GnuVGStateStackEmpty : public jException {
		public:
			GnuVGStateStackEmpty(const std::string &id) : jException(id, jException::sanity_error) {}
		};

		/*************
		 *
		 * internal android callbacks - do not use them from any application code, it won't port...
		 *
		 *************/
		void init(JNIEnv *env);
		void resize(JNIEnv *env, int width, int height);
		void step(JNIEnv *env);

	public:
		GnuVGCanvas(std::string id, jobject jobj);
		~GnuVGCanvas();
	};
};

#endif
