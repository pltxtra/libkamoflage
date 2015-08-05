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

import android.util.Log;
import android.view.View;
import android.graphics.Paint;
import android.graphics.Canvas;
//import android.widget;
import java.io.InputStream;
import android.content.Context;
import java.util.IdentityHashMap;
import java.util.Vector;
import java.util.ArrayList;
import javax.xml.parsers.*;
import org.w3c.dom.*;

class SurfaceHelper extends View implements View.OnTouchListener{
	private Paint mPaint;

	private String id;
		
	private int horizo_sectors, vertic_sectors;
	private int horizo_zones, vertic_zones;

	private int width, height;
	private int hor_sec_div;
	private int ver_sec_div;
	
	public SurfaceHelper(
		String parent_id,
		Context context,
		int horizontal_sectors,
		int vertical_sectors,
		int horizontal_zones,
		int vertical_zones) {
		super(context);

		id = parent_id;
			
		horizo_sectors = horizontal_sectors;
		vertic_sectors = vertical_sectors;
		horizo_zones = horizontal_zones;
		vertic_zones = vertical_zones;

		mPaint = new Paint();
		mPaint.setColor(0xFFFFFF00);
		mPaint.setDither(true);
		mPaint.setStyle(Paint.Style.STROKE);
		mPaint.setStrokeJoin(Paint.Join.ROUND);
		mPaint.setStrokeCap(Paint.Cap.ROUND);
		mPaint.setStrokeWidth(3);

		this.setOnTouchListener(this);
	}

	private long timestamp_start;

	public boolean onTouch(View v, android.view.MotionEvent evt) {
		invalidate();

//			int pid = evt.getPointerId(0); // multi-touch support...

		int se = -1;
//			int x = (int)evt.getX(pid);
//			int y = (int)evt.getY(pid);

		int x = (int)evt.getX();
		int y = (int)evt.getY();
			
		long time_diff;
		
		time_diff = android.os.SystemClock.elapsedRealtime() - timestamp_start;

//			switch(evt.getAction(pid)) {
		switch(evt.getAction()) {
		case android.view.MotionEvent.ACTION_DOWN:
			timestamp_start = android.os.SystemClock.elapsedRealtime();
			se = 0;
			break;
		case android.view.MotionEvent.ACTION_UP:
			se = 1;
			break;
		case android.view.MotionEvent.ACTION_MOVE:
			/* because some Android devices
			 * will manage to generate a move within
			 * the first 85ms a tap will not be
			 * detected. So, this is a hack to filter
			 * away the first 85ms of move events...
			 */
			if(time_diff > 85)
				se = 2;
			break;
		default:
			se = -1;
			break;
		}

		if(se != -1) {
			x = x / hor_sec_div;
			y = y / ver_sec_div;
			if(x < 0) x = 0; if(x > (horizo_zones * horizo_sectors - 1))
				x = horizo_zones * horizo_sectors - 1;
			if(y < 0) y = 0; if(y > (vertic_zones * vertic_sectors - 1))
				y = vertic_zones * vertic_sectors - 1;

			Kamoflage.handleOnSurface(id, se, x, y);

			return true;
		}
			
		return false;
	}
		
	@Override
	protected void onLayout (boolean changed, int left, int top, int right, int bottom) {
		width = right - left;
		height = bottom - top;

		hor_sec_div = width / (horizo_sectors * horizo_zones);
		if(hor_sec_div == 0) hor_sec_div = 1; // XXX this solves a division by zero if the width is too small...
		
		ver_sec_div = height / (vertic_sectors * vertic_zones);
		if(ver_sec_div == 0) ver_sec_div = 1; // XXX this solves a division by zero if the height is too small...
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
	
	@Override
	public void onDraw(Canvas canvas) {
		int w = width / horizo_sectors;
		int h = height / vertic_sectors;
		int i, j;

		android.graphics.LinearGradient lg1 =
			new android.graphics.LinearGradient(
				0, 0, 0, height,
				0xff000000, 0xffffffff,
				android.graphics.Shader.TileMode.CLAMP);

		mPaint.setShader(lg1);
		canvas.drawPaint(mPaint);

		lg1 =
			new android.graphics.LinearGradient(
				0, 0, 0, height,
				0xffffffff, 0xff000000,
				android.graphics.Shader.TileMode.CLAMP);
		mPaint.setShader(lg1);
			
		for(j = 0; j < vertic_sectors; j++) {
			canvas.drawLine(0, h * j, width, h * j, mPaint);
		}
		for(i = 0; i < horizo_sectors; i++) {
			canvas.drawLine(w * i, 0, w * i, height, mPaint);
		}

	}
}
	
