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
import android.graphics.Rect;
//import android.widget;
import java.io.InputStream;
import android.content.Context;
import java.util.IdentityHashMap;
import java.util.Vector;
import java.util.ArrayList;
import javax.xml.parsers.*;
import org.w3c.dom.*;

class KamoflageLayoutFrame extends View {

	private Paint
	light_paint, dark_paint;

	int width, height;
	
	private String title;
		
	public KamoflageLayoutFrame (
		Context context, String _title) {
		super(context);

		title = _title;
		
		light_paint = new Paint();
		light_paint.setDither(true);
		light_paint.setColor(0xFFFFFFFF);
		light_paint.setStyle(Paint.Style.STROKE);
		
		dark_paint = new Paint();
		dark_paint.setDither(true);
		dark_paint.setColor(0xff000000);
		dark_paint.setStyle(Paint.Style.FILL);

	}

	@Override
	protected void onLayout (boolean changed, int left, int top, int right, int bottom) {
		width = right - left;
		height = bottom - top;
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

		sw = measureSize(100, widthMeasureSpec);
		sh = measureSize(100, heightMeasureSpec);

		setMeasuredDimension(sw, sh);	
	}

	public int get_offset(Rect tbounds) {
		light_paint.getTextBounds(title, 0, title.length(), tbounds);

		int offset = (tbounds.bottom - tbounds.top) / 2 + 3;

		return offset;
	}
	
	public int get_offset() {
		Rect tbounds = new Rect();

		int offset = get_offset(tbounds);

		return offset;
	}
	
	@Override
	public void onDraw(Canvas canvas) {
		super.onDraw(canvas);

		Rect tbounds = new Rect();
		int offset = get_offset(tbounds);
		
		canvas.drawPaint(dark_paint);

		canvas.drawLine(offset, offset, 2 * offset, offset, light_paint);
		
		canvas.drawText(title, 0, title.length(),
				3 * offset, -tbounds.top + 1, light_paint);
		
		canvas.drawLine(tbounds.right + 4 * offset,
				offset, width - offset, offset, light_paint);
		
		canvas.drawLine(
			width - offset, offset, width - offset, height - offset, light_paint);
		canvas.drawLine(
			width - offset, height - offset, offset, height - offset, light_paint);
		canvas.drawLine(
			offset, offset, offset, height - offset, light_paint);
	}
}
	
