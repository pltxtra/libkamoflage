/*
 * Kamoflage, ANDROID version
 * Copyright (C) 2007 by Anton Persson & Ted Bjorling
 * Copyright (C) 2010 by Anton Persson
 * Copyright (C) 2011 by Anton Persson
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

package com.toolkits.kamoflage;

import com.toolkits.libsvgandroid.SvgRaster;

import android.util.Log;
import android.view.View;
import android.graphics.Paint;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.PointF;
import android.util.FloatMath;
//import android.widget;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.lang.StringBuilder;
import java.io.BufferedReader;
import android.content.Context;
import java.util.IdentityHashMap;
import java.util.Vector;
import java.util.ArrayList;
import javax.xml.parsers.*;
import org.w3c.dom.*;

import android.view.GestureDetector;

import android.widget.Scroller;

class GraphNG extends View implements FGL.SimpleFGLListener, Runnable {

	final static float DEF_RADIUS = 100f;

	public interface GraphNGListener {
		boolean _handleOnSelectNode(String id, int identifier);
		boolean _handleOnDeleteNode(String id, int identifier);
		boolean _handleOnDisconnect(String id, int output_id, int input_id,
					    String output_socket, String input_socket);
		boolean _handleOnConnect(String id, int output_id, int input_id,
					 String output_socket, String input_socket);
	}
	private GraphNGListener current_listener;
	
	class GSocket {
		public GNode owner;
		public String name;

		public Vector<GSocket> connection;

		public float xpos, ypos, radius;

		public GSocket(GNode _owner, String _name) {
			connection = new Vector<GSocket>();

			xpos = ypos = (float)0.5;

			name = _name;
			owner = _owner;
		}

		public boolean is_inside(float xp, float yp) {
			if(
				(xp > xpos - radius)
				&&
				(xp < xpos + radius)
				&&
				(yp > ypos - radius)
				&&
				(yp < ypos + radius)
				) {
				return true;
			}
			return false;
		}

	}

	private class GNode {
		private Paint border_paint, body_paint, text_paint, connection_paint;		
		private Paint a_border_paint, a_body_paint; // active border and body paint

		public int node_data;

		public String title;

		public Vector<GSocket> input;
		public Vector<GSocket> output;

		public float xpos; // relative to origo at (0,0)
		public float ypos; // relative to origo

		public GNode(int _node_data, String _title,
			     Vector<String> inputs,
			     Vector<String> outputs,
			     float _start_x, float _start_y) {
			input = new Vector<GSocket>();
			output = new Vector<GSocket>();
			
			node_data = _node_data;
			title = _title;

			int k;
			for(k = 0; k < inputs.size(); k++) {
				String inp = inputs.get(k);
				GSocket s = new GSocket(this, inp);
				input.add(s);
			}
			for(k = 0; k < outputs.size(); k++) {
				String out = outputs.get(k);
				GSocket s = new GSocket(this, out);
				output.add(s);
			}

			xpos = _start_x;
			ypos = _start_y;

			border_paint = new Paint();
			border_paint.setDither(true);
			border_paint.setAntiAlias(true);
			border_paint.setColor(0xff00ffff);
			border_paint.setStyle(Paint.Style.STROKE);
			border_paint.setStrokeJoin(Paint.Join.ROUND);
			border_paint.setStrokeCap(Paint.Cap.ROUND);
			border_paint.setStrokeWidth(DEF_RADIUS * 0.05f);

			a_border_paint = new Paint();
			a_border_paint.setDither(true);
			a_border_paint.setAntiAlias(true);
			a_border_paint.setColor(0xffff00ff);
			a_border_paint.setStyle(Paint.Style.STROKE);
			a_border_paint.setStrokeJoin(Paint.Join.ROUND);
			a_border_paint.setStrokeCap(Paint.Cap.ROUND);
			a_border_paint.setStrokeWidth(DEF_RADIUS * 0.05f);

			text_paint = new Paint();
			border_paint.setAntiAlias(true);
			text_paint.setColor(0xffffffff);
			
			body_paint = new Paint();
			body_paint.setDither(true);
			body_paint.setColor(0xff348b8b);
			body_paint.setStyle(Paint.Style.FILL);
			body_paint.setStrokeJoin(Paint.Join.ROUND);
			body_paint.setStrokeCap(Paint.Cap.ROUND);
			body_paint.setStrokeWidth(2);

			a_body_paint = new Paint();
			a_body_paint.setDither(true);
			a_body_paint.setColor(0xff770077);
			a_body_paint.setStyle(Paint.Style.FILL);
			a_body_paint.setStrokeJoin(Paint.Join.ROUND);
			a_body_paint.setStrokeCap(Paint.Cap.ROUND);
			a_body_paint.setStrokeWidth(2);

			connection_paint = new Paint();
			connection_paint.setDither(true);
			connection_paint.setColor(0xff348b8b);
			connection_paint.setStyle(Paint.Style.STROKE);
			connection_paint.setStrokeJoin(Paint.Join.ROUND);
			connection_paint.setStrokeCap(Paint.Cap.ROUND);

			text_paint.setTextSize(DEF_RADIUS * 0.5f);
			connection_paint.setStrokeWidth(DEF_RADIUS * 0.05f);
		}

		public void draw_connections(Canvas canvas) {
			int i, j;
			
			for(i = 0; i < output.size(); i++) {
				GSocket s = output.get(i);
				
				for(j = 0; j < s.connection.size(); j++) {
					GSocket d = s.connection.get(j);
					
					canvas.drawLine(xpos, ypos,
							d.owner.xpos, d.owner.ypos,
							connection_paint);
				}
			}
		}
		
		public void draw_node(Canvas canvas, boolean is_active) {
			if(is_active) {
				canvas.drawCircle(xpos, ypos, DEF_RADIUS, a_body_paint);
				canvas.drawCircle(xpos, ypos, DEF_RADIUS, a_border_paint);
			} else {
				canvas.drawCircle(xpos, ypos, DEF_RADIUS, body_paint);
				canvas.drawCircle(xpos, ypos, DEF_RADIUS, border_paint);
			}
			canvas.drawText(title, xpos, ypos, text_paint);
		}

		private void draw_sockets(Canvas canvas, Vector<GSocket> socks, boolean leftSide) {
			int i;
			float angle_speed = 160.0f / socks.size();
			float rad = 3.5f * DEF_RADIUS;

			float angle = 105.0f;

			for(i = 0; i < socks.size(); i++) {
				float x, y;

				x = xpos + (leftSide ? -1.0f : 1.0f) * rad *
					FloatMath.sin((angle / 360.0f) * 2.0f * 3.1415f);
				y = ypos + rad * FloatMath.cos((angle / 360.0f) * 2.0f * 3.1415f);

				socks.get(i).xpos = x;
				socks.get(i).ypos = y;
				socks.get(i).radius = 0.5f * DEF_RADIUS;
				
				canvas.drawCircle(x, y, 0.5f * DEF_RADIUS, body_paint);
				canvas.drawCircle(x, y, 0.5f * DEF_RADIUS, border_paint);
				canvas.drawText(socks.get(i).name, x, y, text_paint);

				angle += angle_speed;
			}
		}

		public GSocket is_inside_socket(boolean is_input, float x, float y) {
			Vector<GSocket> socks = output;
			if(is_input) socks = input;
			
			int i;
			for(i = 0; i < socks.size(); i++) {
				if(socks.get(i).is_inside(x, y))
					return socks.get(i);
			}
			return null;
		}

		public void draw_output_sockets(Canvas canvas) {
			if(output.size() > 0) {
				canvas.drawText("Select output (turquoise)...",
						xpos - 2.0f * DEF_RADIUS,
						ypos + 2.0f * DEF_RADIUS, text_paint);
				canvas.drawText("... or tap machine for parameters",
						xpos - 2.0f * DEF_RADIUS,
						ypos + 2.5f * DEF_RADIUS, text_paint);
			} else {
				canvas.drawText("... tap machine again",
						xpos - 2.0f * DEF_RADIUS,
						ypos + 2.0f * DEF_RADIUS, text_paint);
				canvas.drawText("    for parameters ...",
						xpos - 2.0f * DEF_RADIUS,
						ypos + 2.5f * DEF_RADIUS, text_paint);
			}
			draw_sockets(canvas, output, false);
		}

		public void draw_input_sockets(Canvas canvas) {
			if(input.size() > 0) {
				canvas.drawText("Select target input. (turquoise)",
						xpos - 4.0f * DEF_RADIUS,
						ypos + 2.0f * DEF_RADIUS, text_paint);
			} else {
				canvas.drawText("No input sockets for this machine...",
						xpos - 4.0f * DEF_RADIUS,
						ypos + 2.0f * DEF_RADIUS, text_paint);
			}
			draw_sockets(canvas, input, true);
		}

		public boolean is_inside(float xp, float yp) {
			if(
				(xp > xpos - DEF_RADIUS)
				&&
				(xp < xpos + DEF_RADIUS)
				&&
				(yp > ypos - DEF_RADIUS)
				&&
				(yp < ypos + DEF_RADIUS)
				) {
				return true;
			}
			return false;
		}

		public float[] get_pixel_pos(Matrix tm) {
			float[] flp = new float[2]; flp[0] = xpos; flp[1] = ypos;
			tm.mapPoints(flp);
			return flp;
		}
		
		public void move_abs(float xp, float yp) {
			active_node.xpos = xp;
			active_node.ypos = yp;
		}

	}

	public float[] mapPixelToPoint(Matrix tm, float xp, float yp) {
		Matrix inv = new Matrix(); tm.invert(inv);
		float[] flp = new float[2]; flp[0] = xp; flp[1] = yp; inv.mapPoints(flp);
				
		return flp;
	}
	
	private void graph_disconnect_nodes(
		GSocket output,
		GSocket input,
		boolean call_callback) {

		boolean disconnection_is_a_success = true; // default to success in case we have no callback

		// if we should, call the event handler first
		if(call_callback) {
			disconnection_is_a_success = current_listener._handleOnDisconnect(
				id,
				output.owner.node_data, input.owner.node_data,
				output.name, input.name);
		}
		
		if(disconnection_is_a_success) {
			output.connection.remove(input);
			input.connection.remove(output);
		}
		invalidate();
	}
		
	private void graph_connect_nodes(
		GSocket output, GSocket input,
		boolean call_callback) {

		boolean connection_is_a_success = true; // default to tru in case we don't call a callback
			
		// if we should, call the native event handler
		if(call_callback) {			
			connection_is_a_success = current_listener._handleOnConnect(
				id,
				output.owner.node_data, input.owner.node_data,
				output.name, input.name);			
		}
		
		if(connection_is_a_success) {
				
			input.connection.add(output);
			output.connection.add(input);
		}
		invalidate();
	}

	private boolean sockets_already_connected(GSocket output, GSocket input) {

		if(output.connection.contains(input) &&
		   input.connection.contains(output)) {
			return true;
		}
		return false;
	}
		
	private void disconnect_specific_socket_connection(GSocket s,
							   GSocket connection) {
			
		int k;

		for(k = 0; k < s.connection.size(); k++) {
			if(s.connection.get(k) == connection) {
				s.connection.remove(k);
			}
		}
	}

	private void disconnect_all_socket_connections(GSocket s) {
		int k;

		for(k = 0; k < s.connection.size(); k++) {
			disconnect_specific_socket_connection(
				s.connection.get(k), s);
		}
		s.connection.clear();
	}

	private int get_node_index(int _node_data) {
		int i;
		for(i = 0; i < node.size(); i++) {
			if(node.get(i).node_data == _node_data) {
				return i;
			} 
		}
		return -1;
	}
	
	private GNode get_node(int _node_data) {
		int i = get_node_index(_node_data);
		if(i != -1)
			return node.get(i);
		return null;
	}

	private float last_tap_at_x = 0.0f, last_tap_at_y = 0.0f;
	public void add_node(int _node_data, String title, Vector<String> inputs, Vector<String> outputs) {
		if(get_node(_node_data) == null) {
			GNode gn = new GNode(_node_data, title, inputs, outputs,
					     last_tap_at_x,
					     last_tap_at_y);
			node.add(gn);
			invalidate();
		}
	}
	
	private void remove_node_helper(GNode node) {
		int k;
		
		// disconnect inputs
		for(k = 0; k < node.input.size(); k++) {
			disconnect_all_socket_connections(node.input.get(k));
		}
		node.input.clear();
		
		// disconnect outputs
		for(k = 0; k < node.output.size(); k++) {
			disconnect_all_socket_connections(node.output.get(k));
		}
		node.output.clear();
		
	}
		
	public void graph_remove_node(int _node_data, boolean call_callback) {
		int k = get_node_index(_node_data);
		if(k != -1) {
			// first call the native event handler, proceed if return value is true
			if(call_callback && (!(current_listener._handleOnDeleteNode(id, _node_data))))
				return; // return if native event handler returns false
			
			// ok, proceed with deletion.
			GNode n = node.get(k);
			remove_node_helper(n);
			node.remove(k);
			invalidate();
		}
	}

	public void connect_nodes(int output_node, int input_node, int output_socket, int input_socket) {
		GNode n_out = get_node(output_node);
		GNode n_in = get_node(input_node);
		GSocket output, input;

		output = n_out.output.get(output_socket);
		input = n_in.input.get(input_socket);
		
		graph_connect_nodes(
			output,
			input,
			false
			);
		invalidate();
	}
	
	public void disconnect_nodes(int output_node, int input_node, int output_socket, int input_socket) {
		GNode n_out = get_node(output_node);
		GNode n_in = get_node(input_node);
		GSocket output, input;

		output = n_out.output.get(output_socket);
		input = n_in.input.get(input_socket);

		graph_disconnect_nodes(
			output,
			input,
			false
			);
		invalidate();
	}

	public void clear() {
		while(node.size() > 0) {
			GNode n = node.get(0);
			
			graph_remove_node(n.node_data, false); 
		}
		invalidate();
	}

	public float get_x_coord(int node_data) {
		GNode gn = get_node(node_data);

		if(gn != null) return gn.xpos / (10.0f * DEF_RADIUS);
		
		return 0.0f;
	}

	public float get_y_coord(int node_data) {
		GNode gn = get_node(node_data);

		if(gn != null) return gn.ypos / (10.0f * DEF_RADIUS);
		
		return 0.0f;
	}
	
	public void set_coords(int node_data, float x, float y) {
		GNode gn = get_node(node_data);

		if(gn != null) {
			gn.xpos = x * 10.0f * DEF_RADIUS;
			gn.ypos = y * 10.0f * DEF_RADIUS;
			invalidate();
		}
	}
	
	/// This routine handles scrolling/flinging animation
	public void run() {
		boolean stillScrolling = scrl.computeScrollOffset();
			
		if(stillScrolling) {
			post(this);
		} else {
		}
		invalidate();
	}
	
	private int animTime = 300;
	private Scroller scrl = null;
	private FGL fgl;
	
	private String id;
		
	private int width = 0, height = 0;

	Vector<GNode> node;
	GNode active_node = null; // active node is one that is currently being "Pointed at"
	GNode selected_node = null; // selected node is one that is currently being operated on, but NOT pointed at by a "finger"
	GSocket source_socket = null;
	
	private android.app.Activity context = null;

	private Paint bg_paint;	
	private Paint debug_paint1;	
	private Paint debug_paint2;	
	
	public GraphNG(
		String parent_id,
		android.app.Activity ctx,
		GraphNGListener _listener) {
		super(ctx);

		current_listener = _listener;
		
		node = new Vector<GNode>();

		id = parent_id;
		context = ctx;		

		bg_paint = new Paint();
		bg_paint.setDither(true);
		bg_paint.setColor(0xff000000);
		bg_paint.setStyle(Paint.Style.FILL);
		bg_paint.setStrokeJoin(Paint.Join.ROUND);
		bg_paint.setStrokeCap(Paint.Cap.ROUND);
		bg_paint.setStrokeWidth(2);

		debug_paint1 = new Paint();
		debug_paint1.setDither(true);
		debug_paint1.setColor(0xffff0000);
		debug_paint1.setStyle(Paint.Style.FILL);
		debug_paint1.setStrokeJoin(Paint.Join.ROUND);
		debug_paint1.setStrokeCap(Paint.Cap.ROUND);
		debug_paint1.setStrokeWidth(2);
		debug_paint2 = new Paint();
		debug_paint2.setDither(true);
		debug_paint2.setColor(0xff00ff00);
		debug_paint2.setStyle(Paint.Style.FILL);
		debug_paint2.setStrokeJoin(Paint.Join.ROUND);
		debug_paint2.setStrokeCap(Paint.Cap.ROUND);
		debug_paint2.setStrokeWidth(2);

		scrl = new Scroller(ctx);
		fgl = new FGL(ctx);
		fgl.attachSimpleFGLListener(this);


	}

	private void startScrolling() {
	}

	private Matrix transformation_matrix, saved_matrix, selected_transformation_matrix;
	private PointF drag_start = new PointF();
	private PointF pinch_center = new PointF();
	private float start_xpos, start_ypos;
	public boolean onStart(int fingerIndex, float xpos, float ypos) {
		if(fingerIndex == 1) {
			saved_matrix.set(transformation_matrix);
			drag_start.set(xpos, ypos);
			
			float[] p = mapPixelToPoint(transformation_matrix, xpos, ypos);
			int i;
			for(i = 0; i < node.size(); i++) {

				if(node.get(i).is_inside(p[0], p[1])) {
					active_node = node.get(i);
					float[] px = active_node.get_pixel_pos(transformation_matrix);
					start_xpos = px[0]; start_ypos = px[1];

				} 
			}
			
		} else if(fingerIndex == 2) {
			saved_matrix.set(transformation_matrix);
			float x = drag_start.x + xpos;
			float y = drag_start.y + ypos;
			pinch_center.set(x / 2, y / 2);
 		}

		return true;
	}
	public boolean onDrag(int fingers, float x_length, float y_length) {

		if(selected_node == null) {
			if(active_node != null) {
				float[] p = mapPixelToPoint(transformation_matrix,
							    start_xpos + x_length,
							    start_ypos + y_length);
				active_node.move_abs(p[0], p[1]);
			} else {
				transformation_matrix.set(saved_matrix);
				transformation_matrix.postTranslate(x_length,
								    y_length);
			}
		}
		invalidate();
		return true;
	}
	public boolean onStop(int fingers, float x_velocity, float y_velocity) {
		active_node = null;
		return true;
	}
	public boolean onFling(int fingers, float velocity_x, float velocity_y) { Log.v("Kamoflage", "FLING");
		return false; } // ignore flings
	public boolean onPinch(int fingers, float scale, float angle) {
		if(selected_node == null) {
			transformation_matrix.set(saved_matrix);
			transformation_matrix.postScale(scale, scale, pinch_center.x, pinch_center.y);
		}
		invalidate();
		return true;
	}
	public boolean onTap(int fingers, float xpos, float ypos) {
		if(selected_node != null) {
			float[] p = mapPixelToPoint(selected_transformation_matrix, xpos, ypos);
			
			GSocket sock = selected_node.is_inside_socket(
				source_socket == null ? false : true,
				p[0], p[1]);
			
			if(sock != null && source_socket == null) {
				source_socket = sock;			
			} else if (sock != null) {
				if(sockets_already_connected(source_socket, sock)) {
					graph_disconnect_nodes(source_socket, sock, true);
				} else {
					graph_connect_nodes(source_socket, sock, true);
				}
				source_socket = null;
			} else {
				source_socket = null;

				if(selected_node.is_inside(p[0], p[1])) {
					current_listener._handleOnSelectNode(id, selected_node.node_data);
				}
			}
			selected_node = null;
		} else {
			float[] p = mapPixelToPoint(transformation_matrix, xpos, ypos);
			int i;
			for(i = 0; i < node.size(); i++) {
				
				if(node.get(i).is_inside(p[0], p[1])) {
					selected_node = node.get(i);				
				} 
			}

			// inform the listener the user tapped empty space
			if(selected_node == null) {
				last_tap_at_x = p[0];
				last_tap_at_y = p[1];
				current_listener._handleOnSelectNode(id, 0);
			}
		}
		
		invalidate();
		
		return true;
	}

	public boolean onLongpress(int fingers, float xpos, float ypos) {
		if(selected_node == null) {
			float[] p = mapPixelToPoint(transformation_matrix, xpos, ypos);
			int i;
			for(i = 0; i < node.size(); i++) {
				
				if(node.get(i).is_inside(p[0], p[1])) {
					graph_remove_node(node.get(i).node_data, true);
				} 
			}
		}
		
		invalidate();
		
		return true;
	}

	@Override
	public boolean onTouchEvent(android.view.MotionEvent evt) {
		if(!fgl.onTouchEvent(evt)) {
			// unhandled gesture
		} else {
			// FGL recognized a gesture
		}
			
		return true;
	}
		
	private int measureSize(int wanting, int mspec) {
		int size = android.view.View.MeasureSpec.getSize(mspec);
		switch(android.view.View.MeasureSpec.getMode(mspec)) {
		case android.view.View.MeasureSpec.AT_MOST:
			return size > wanting ? wanting : size;
		case android.view.View.MeasureSpec.EXACTLY:
			return size;
		case android.view.View.MeasureSpec.UNSPECIFIED:
			return wanting;
		}
		return wanting;
	}
	
	@Override
	protected void onMeasure (int widthMeasureSpec, int heightMeasureSpec) {
		int sw, sh;
		
		sw = measureSize(150, widthMeasureSpec);
		sh = measureSize(150, heightMeasureSpec);

		setMeasuredDimension(sw, sh);	
	}
	
	float abs_x, abs_y;
	float half_width, half_height, hypo;
	@Override
	protected void onLayout (boolean changed, int left, int top, int right, int bottom) {
		if(width != right - left || height != bottom - top) {
			int[] spos = new int[2];
			getLocationOnScreen(spos);
			abs_x = (float)spos[0];
			abs_y = (float)spos[1];
			
			width = right - left;
			height = bottom - top;
			
			half_width =
				(((float)(width)) / 2.0f);
			half_height =
				(((float)(height)) / 2.0f);
			hypo = FloatMath.sqrt(half_width * half_width + half_height * half_height);
			
			transformation_matrix = new Matrix();
			saved_matrix = new Matrix();
			
			transformation_matrix.postTranslate(left + half_width, top + half_height);
			transformation_matrix.postScale(
				hypo / (DEF_RADIUS * 10.0f),
				hypo / (DEF_RADIUS * 10.0f),
				half_width, half_height);

			selected_zoom_scale = (hypo / (DEF_RADIUS * 10.0f)) * 1.2f;
		}
	}

	private float selected_zoom_scale;
	
	private static boolean didNotCheckHWAcc = true;
	private static boolean shouldIDoExtraPostTranslate = false;
	@Override
	public void onDraw(Canvas canvas) {				
		canvas.drawPaint(bg_paint);

		Matrix mtrx_base = canvas.getMatrix();

		// for some reason I have to do additional translation of the coordinate space
		// if I use hardware accelleration... so, to do that I have to check if the view isHardwareAccelerated()
		// that is an API which is not available until API level 11, so to get this code to compile both when the
		// working against 11 or newer, and also older API levels, I solve it by reflection.
		if(didNotCheckHWAcc) {
			didNotCheckHWAcc = false;
			try {
				Class<?> cls = this.getClass();
				
				java.lang.reflect.Method isHardwareAcc = cls.getMethod("isHardwareAccelerated");
				Object result = isHardwareAcc.invoke(this);
				Log.v("KAMOFLAGE", "isHwAcc? " + result);
				java.lang.Boolean val = (java.lang.Boolean)result;
				if(val.booleanValue()) {
					shouldIDoExtraPostTranslate = true;
				}
			} catch(java.lang.NoSuchMethodException ignored) {
				Log.v("KAMOFLAGE", "Api isHardwareAccelerated is not available.");
				// ignore
			} catch(java.lang.IllegalAccessException ignored) {
				Log.v("KAMOFLAGE", "Api isHardwareAccelerated could not be accessed.");
				// ignore
			} catch(java.lang.reflect.InvocationTargetException ignored) {
				Log.v("KAMOFLAGE", "Api reported InvocationTargetException... ignored!");
				// ignore
			}
		}
		if(shouldIDoExtraPostTranslate) {
			mtrx_base.postTranslate(abs_x,
						abs_y);
		}
		int i;
		if(selected_node == null) {
			boolean ignore = mtrx_base.setConcat(mtrx_base, transformation_matrix);
			canvas.setMatrix(mtrx_base);

			for(i = 0; i < node.size(); i++) {
				node.get(i).draw_connections(canvas);
			}
			GNode an = null;
			if(source_socket != null) {
				an = source_socket.owner;				
			}
			for(i = 0; i < node.size(); i++) {
				GNode n = node.get(i);
				n.draw_node(canvas, n == an ? true : false);
			}
		} else {
			Matrix lm = new Matrix(); 
			lm.postTranslate(half_width,
					 half_height);
			lm.postScale(selected_zoom_scale, selected_zoom_scale,
				     half_width, half_height);

			float[] p = selected_node.get_pixel_pos(lm);

			lm.postTranslate(half_width - p[0], half_height - p[1]);
			
			if(source_socket == null) {
				lm.postTranslate(-half_width / 2.0f, 0.0f);
			} else {
				lm.postTranslate(half_width / 2.0f, 0.0f);
			}
			
			boolean ignore = mtrx_base.setConcat(mtrx_base, lm);
			selected_transformation_matrix = lm;
			canvas.setMatrix(mtrx_base);
			selected_node.draw_node(canvas, true);

			if(source_socket == null) {
				selected_node.draw_output_sockets(canvas);
			} else {
				selected_node.draw_input_sockets(canvas);
			}
		}

	}

}
