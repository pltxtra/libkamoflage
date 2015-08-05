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

import android.content.Context;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.view.View.MeasureSpec;
import java.util.Vector;

import android.graphics.Canvas;
import android.graphics.Paint;

public class KamoflageVSwitch extends ViewGroup {
	
	private class Contained {
		public View vju;

		public int req_width, req_height; // requested limits
		public int rec_width, rec_height; // received limits

		Contained(View v) {
			vju = v;
		}
	};
	
	private Vector<Contained> contained;

	String title;

	int measured_w, measured_h;

	int current_view;
	
	public KamoflageVSwitch(String _title, Context context) {
		super(context);

		title = _title;
		
		current_view = 0;
		
		contained = new Vector<Contained>();
	}

	public void showNext(int step) {
		contained.get(current_view).vju.setVisibility(View.INVISIBLE);

		current_view = (current_view + step) % (contained.size());
		
		contained.get(current_view).vju.setVisibility(View.VISIBLE);		
		invalidate();
		contained.get(current_view).vju.invalidate();
		requestLayout();
	}
	
	public void showNext() {
		contained.get(current_view).vju.setVisibility(View.INVISIBLE);

		current_view = (current_view + 1) % (contained.size());
		
		contained.get(current_view).vju.setVisibility(View.VISIBLE);
	}
	
	public void showPrevious() {
		contained.get(current_view).vju.setVisibility(View.INVISIBLE);

		current_view = (current_view - 1) % (contained.size());
		if(current_view < 0) current_view = contained.size() - 1;
		
		contained.get(current_view).vju.setVisibility(View.VISIBLE);
	}
	
	public void addKWidget(View vju) {
		super.addView(vju);

		contained.add(new Contained(vju));

		if(contained.size() == 1) {
			vju.setVisibility(View.VISIBLE);
		} else {
			vju.setVisibility(View.INVISIBLE);
		}
	}

	@Override
	public void removeAllViews() {
		super.removeAllViews();

		contained.clear();
	}
	
	@Override
	protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
//		Log.v("Kamoflage", "onMeasure ]" + title + "[");
		int childWidthSpec, childHeightSpec;

		int master_w = android.view.View.MeasureSpec.getSize(widthMeasureSpec);
		int master_h = android.view.View.MeasureSpec.getSize(heightMeasureSpec);

		int m_mode_w = android.view.View.MeasureSpec.getMode(widthMeasureSpec);
		int m_mode_h = android.view.View.MeasureSpec.getMode(heightMeasureSpec);

		measured_w = 0;
		measured_h = 0;
		
		final int c_s = contained.size();
//		for (int i = 0; i < c_s; i++) {
		int i = current_view; {
			childHeightSpec = heightMeasureSpec;
			childWidthSpec = widthMeasureSpec;

			Contained cnt = contained.get(i);
			android.view.View v = cnt.vju;

			v.measure(childWidthSpec, childHeightSpec);

			cnt.rec_width = cnt.req_width = v.getMeasuredWidth();
			cnt.rec_height = cnt.req_height = v.getMeasuredHeight();
			
			measured_w = measured_w > cnt.req_width ? measured_w : cnt.req_width;
			measured_h = measured_h > cnt.req_height ? measured_h : cnt.req_height;
			
		}

		// we can't use more than given...
		measured_h = measured_h > master_h ? master_h : measured_h;
		measured_w = measured_w > master_w ? master_w : measured_w;

		setMeasuredDimension(measured_w, measured_h);
	}
	
	@Override
	protected void onLayout(boolean changed, int l, int t, int r, int b) {
//		Log.v("Kamoflage", "onLayout ]" + title + "[");
		int w = r - l;
		int h = b - t;

		final int ct_s = contained.size();
//		for (int i = 0; i < ct_s; i++) {
		int i = current_view;
		{
			Contained cnt = contained.get(i);
			
			View child = cnt.vju;

			child.layout(0, 0, w, h);			
		}
	}
}
