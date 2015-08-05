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
import android.graphics.Canvas;
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

class CanvasHelper extends View implements GestureDetector.OnGestureListener {
	private String id;
		
	private int width, height;
	private int x, y;
	private long nativeID;
	
	private GestureDetector gestures = null;
	private android.app.Activity context = null;
	
	public CanvasHelper(
		long _nativeID,
		String parent_id,
		android.app.Activity ctx) {
		super(ctx);

		nativeID = _nativeID;
		id = parent_id;
		context = ctx;		
	}

	@Override
	public boolean onDown(android.view.MotionEvent e) {
		return true;
	}
	
	@Override
	public boolean onFling(android.view.MotionEvent e1, android.view.MotionEvent e2,
			       final float velocityX, final float velocityY) {
		final float distanceTimeFactor = 0.4f;
		final float totalDx = (distanceTimeFactor * velocityX / 2);
		final float totalDy = (distanceTimeFactor * velocityY / 2);
		
		//	view.onAnimateMove(totalDx, totalDy,
		//		   (long) (1000 * distanceTimeFactor));
		return true;
	}
	
	@Override
	public void onLongPress(android.view.MotionEvent e) {
		invalidate();
		// 3 == cvButtonHold, see kammogui.hh
		canvasEvent(nativeID, 3, x, y);
	}
	
	@Override
	public boolean onScroll(android.view.MotionEvent e1,
				android.view.MotionEvent e2,
				float distanceX, float distanceY) {
		return true;
	}
	
	@Override
	public void onShowPress(android.view.MotionEvent e) {
	}
	
	@Override
	public boolean onSingleTapUp(android.view.MotionEvent e) {
		return false;
	}
	
	@Override
	public boolean onTouchEvent(android.view.MotionEvent evt) {
		invalidate();

//			int pid = evt.getPointerId(0); // multi-touch support...

		int se = -1;
//			int x = (int)evt.getX(pid);
//			int y = (int)evt.getY(pid);

		x = (int)evt.getX();
		y = (int)evt.getY();
			
//			switch(evt.getAction(pid)) {
		switch(evt.getAction()) {
		case android.view.MotionEvent.ACTION_DOWN:
			se = 0;
			break;
		case android.view.MotionEvent.ACTION_UP:
			se = 1;
			break;
		case android.view.MotionEvent.ACTION_MOVE:
			se = 2;
			break;
		default:
			se = -1;
			break;
		}

		if(se != -1) {
			canvasEvent(nativeID, se, x, y);
//			return true;
		}
			
		return gestures.onTouchEvent(evt);
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

		// wa can't do this in the constructor, since the constructor
		// may not be the main UI thread...
		if(gestures == null)
			gestures = new GestureDetector(context, this);

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
	public native static void canvasResize(long nativeID, int w, int h);
	public native static void canvasEvent(long nativeID, int cev, int x, int y);
	public native static float canvasMeasureInches(long nativeID, boolean measureWidth);
	
	@Override
	protected void onLayout (boolean changed, int left, int top, int right, int bottom) {
		width = right - left;
		height = bottom - top;

		canvasResize(nativeID, width, height);
	}

	private boolean do_animate = false;
	
	@Override
	public void onDraw(Canvas canvas) {
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
	
