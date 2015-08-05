/*
 * Kamoflage, ANDROID version
 * Copyright (C) 2007 by Anton Persson & Ted Bjorling
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

package com.toolkits.kamoflage;

import com.toolkits.libsvgandroid.SvgRaster;

import android.util.Log;
import android.view.View;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Canvas;
import android.graphics.Typeface;
import android.graphics.Matrix;
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

class SVGCanvasHelper extends View{
	private String id;
		
	private int width, height;
	private int x, y;
	private long nativeID;
	
	private android.app.Activity context = null;
	
	private static boolean didNotCheckHWAcc = true;
	private static boolean gfxIsHwAcc = false;
	private boolean isThisHWAcc() {
		// I have to check if the view isHardwareAccelerated()
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
					gfxIsHwAcc = true;
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
		return gfxIsHwAcc;
	}
	
	public SVGCanvasHelper(
		long _nativeID,
		String parent_id,
		android.app.Activity ctx) {
		super(ctx);

		nativeID = _nativeID;
		id = parent_id;
		context = ctx;		
	}

	@Override
	public boolean onTouchEvent(android.view.MotionEvent evt) {
		invalidate();

		canvasMotionEventInitEvent(nativeID, evt.getDownTime(), evt.getEventTime(), evt.getActionMasked(),
					   evt.getPointerCount(), evt.getActionIndex(), evt.getRawX(), evt.getRawY());

//		Log.v("KAMOFLAGE", "action index: " + evt.getActionIndex() + " (pointer id: " + evt.getPointerId() + ")");
		
		int k;
		for(k = 0; k < evt.getPointerCount(); k++) {
			canvasMotionEventInitPointer(nativeID, k, evt.getPointerId(k), evt.getX(k), evt.getY(k),
						     evt.getPressure(k));
		}
		canvasMotionEvent(nativeID);		
			
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

		float w_inch, h_inch;
		w_inch = canvasMeasureInches(nativeID, true);
		h_inch = canvasMeasureInches(nativeID, false);

		w_inch *= Kamoflage.get_w_ppi();
		h_inch *= Kamoflage.get_h_ppi();

		sw = measureSize((int)w_inch, widthMeasureSpec);
		sh = measureSize((int)h_inch, heightMeasureSpec);

		setMeasuredDimension(sw, sh);	
	}
	
	public native static void canvasExpose(long nativeID, Canvas target);
	public native static void canvasResize(long nativeID, int w, int h, float w_inches, float h_inches);
	public native static float canvasMeasureInches(long nativeID, boolean measureWidth);

	public native static void canvasMotionEventInitEvent(
		long nativeID, long downTime, long eventTime, int androidAction, int pointerCount, int actionIndex, float rawX, float rawY);
	public native static void canvasMotionEventInitPointer(long nativeID, int index, int id, float x, float y, float pressure);
	public native static void canvasMotionEvent(long nativeID);

	float abs_x, abs_y;
	@Override
	protected void onLayout (boolean changed, int left, int top, int right, int bottom) {
		width = right - left;
		height = bottom - top;
		
		int[] spos = new int[2];
		getLocationOnScreen(spos);
		abs_x = (float)spos[0];
		abs_y = (float)spos[1];

		// calculate size in inches
		float width_f = (float)width;
		float height_f = (float)height;	       
		width_f = width_f / Kamoflage.get_w_ppi();
		height_f = height_f / Kamoflage.get_h_ppi();
		
		canvasResize(nativeID, width, height, width_f, height_f);
	}

	private boolean do_animate = false;

	private static float[] vls = new float[9];
	@Override
	public void onDraw(Canvas canvas) {
		Matrix mtrx_base = canvas.getMatrix();
		mtrx_base.getValues(vls);
		SvgRaster.setCurrentScreenCanvas(canvas, vls[2], vls[5]);
		
		canvasExpose(nativeID, canvas);

		if(do_animate) invalidate();
	}

	void start_animation() {
		do_animate = true;
		invalidate();
	}

	void stop_animation() {
		do_animate = false;
	}
}
	
