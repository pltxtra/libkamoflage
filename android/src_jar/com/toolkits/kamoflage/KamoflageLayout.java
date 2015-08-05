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
import android.app.Activity;
import android.view.WindowManager;
import android.view.Display;

import android.graphics.Canvas;
import android.graphics.Paint;

public class KamoflageLayout extends ViewGroup {

	final boolean DEBUG_LAYOUT = false;

	private class Contained {
		public Kamoflage.Widget kwid;
		public View vju;
		public boolean fill, expand;

		public int req_width, req_height; // requested limits
		public int rec_width, rec_height; // received limits

		public int min_w, min_h; // the minimum size allowable by the Widget

		Contained(Kamoflage.Widget wid) {
			vju = wid.getInternal();
			fill = wid.get_fill();
			expand = wid.get_expand();
			kwid = wid;
		}
	};
	
	private Vector<Contained> contained;
	private Vector<Contained> expanded;

	private Kamoflage.Widget kContainerParent; // the Kamoflage Container widget that holds this layout View
	
	private boolean horizontal;

	private int screen_w, screen_h; // properties of the full screen in pixels.

	String title;

	int measured_w, measured_h;

	boolean framed, scrollable;
	KamoflageLayoutFrame frame_view;
	
	public KamoflageLayout(Kamoflage.Widget _kContainer, String _title, Context context,
			       boolean _horizontal, boolean _framed, boolean _scrollable) {
		super(context);

		kContainerParent = _kContainer;
		
		framed = _framed;
		scrollable = _scrollable;
		
		Activity act = (Activity)context;
		if(act != null) {
			WindowManager wm = act.getWindowManager();
			Display d = wm.getDefaultDisplay();
			screen_w = d.getWidth();
			screen_h = d.getHeight();
		}

		if(framed) {
			frame_view = new KamoflageLayoutFrame(context, _title);
			super.addView(frame_view);
			frame_view.setVisibility(View.VISIBLE);
		}
		
		title = _title;
		
		contained = new Vector<Contained>();
		expanded = new Vector<Contained>();

		horizontal = _horizontal;

	}

	public void addKWidget(Kamoflage.Widget _kwid) {
		super.addView(_kwid.internal);
		_kwid.internal.setVisibility(View.VISIBLE);

		Contained cnt = new Contained(_kwid);

		if(_kwid.get_expand())
			expanded.add(cnt);
		
		contained.add(cnt);
	}

	@Override
	public void removeAllViews() {
		super.removeAllViews();

		expanded.clear();
		contained.clear();
	}

	static String tab_string = "";

	@Override
	protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
		kContainerParent.calculateMinimumSizeTopLevel();
		
		String crnt_tab = tab_string;
		tab_string = tab_string + "  ";
		
		int childWidthSpec, childHeightSpec;

		int master_w = android.view.View.MeasureSpec.getSize(widthMeasureSpec);
		int master_h = android.view.View.MeasureSpec.getSize(heightMeasureSpec);

		int offset = 0; 
		
		if(framed) {
			offset = 2 * frame_view.get_offset();
			
			frame_view.measure(widthMeasureSpec, heightMeasureSpec);		
			master_w -= 2 * offset;
			master_h -= 2 * offset;
		}
		
		int remaining_w, remaining_h;
		
		int m_mode_w = android.view.View.MeasureSpec.getMode(widthMeasureSpec);
		int m_mode_h = android.view.View.MeasureSpec.getMode(heightMeasureSpec);

		measured_w = 0;
		measured_h = 0;

		remaining_w = master_w;
		remaining_h = master_h;

		if(DEBUG_LAYOUT) Log.v("Kamoflage", crnt_tab + "onMeasure[" + title + "] - [" + android.view.View.MeasureSpec.toString(widthMeasureSpec)+ "] [" + android.view.View.MeasureSpec.toString(heightMeasureSpec)+ "]");
		
		final int c_s = contained.size();
		int min_w_acc = 0, min_h_acc = 0; // the accumualted minimum sizes
		int w_sacrifice = 0, h_sacrifice = 0; // if the accumulated sizes are larger than possible, everyone has to make a sacrifice to fit...
		for (int i = 0; i < c_s; i++) {
			Contained cnt = contained.get(i);

			IntegerPair ip = cnt.kwid.getMinimumSize();

			cnt.min_w = ip.a;
			cnt.min_h = ip.b;

			min_w_acc += cnt.min_w;
			min_h_acc += cnt.min_h;
			
			if(DEBUG_LAYOUT) Log.v("Kamoflage", crnt_tab + "...min[" +
					       cnt.kwid.get_id() +
					       "] - [" + ip.a +
					       "] [" + ip.b +
					       "]");
		}

		if(horizontal) {
			if(min_w_acc > remaining_w) {
				w_sacrifice = (min_w_acc - remaining_w) / c_s;
				
				// if not evenly distributable, make everyone sacrifice one pixel more.
				if((min_w_acc - remaining_w) % c_s > 0)
					w_sacrifice += 1;
			}
		} else {
			if(min_h_acc > remaining_h) {
				h_sacrifice = (min_h_acc - remaining_h) / c_s;
				
				// if not evenly distributable, make everyone sacrifice one pixel more.
				if((min_h_acc - remaining_h) % c_s > 0)
					h_sacrifice += 1;
			}
		}
		if(scrollable) {
			w_sacrifice = h_sacrifice = 0;
		}
		if(DEBUG_LAYOUT) Log.v("Kamoflage", crnt_tab + "SACRIFICE[" +
				       title +
				       "] - [" + w_sacrifice +
				       "] [" + h_sacrifice +
				       "]");
		
		for (int i = 0; i < c_s; i++) {
			Contained cnt = contained.get(i);
			if(horizontal) {
				childHeightSpec = heightMeasureSpec;

				childWidthSpec =
					android.view.View.MeasureSpec.makeMeasureSpec(
//						master_w / contained.size(),
						cnt.min_w - w_sacrifice,
//							remaining_w,
						android.view.View.MeasureSpec.AT_MOST);					
			} else { // vertical
				childWidthSpec = widthMeasureSpec;

				childHeightSpec =
					android.view.View.MeasureSpec.makeMeasureSpec(
//						master_h / contained.size(), 
						cnt.min_h - h_sacrifice,
//							remaining_h,
						android.view.View.MeasureSpec.AT_MOST);
			}

			android.view.View v = cnt.vju;

			v.measure(childWidthSpec, childHeightSpec);

			cnt.rec_width = cnt.req_width = v.getMeasuredWidth();
			cnt.rec_height = cnt.req_height = v.getMeasuredHeight();

			if(DEBUG_LAYOUT)
				Log.v("Kamoflage", crnt_tab + "->child:[" + Integer.toString(cnt.rec_width) + "] [" + Integer.toString(cnt.rec_height)+ "]");
			
			if(horizontal) {
				remaining_w -= cnt.req_width;
				measured_w += cnt.req_width;
				measured_h = measured_h > cnt.req_height ? measured_h : cnt.req_height;
			} else {
				remaining_h -= cnt.req_height;
				measured_h += cnt.req_height;
				measured_w = measured_w > cnt.req_width ? measured_w : cnt.req_width;
			}
			
		}

		if(scrollable) {
			// we have unspecified restrictions... (freedom!)
			if(horizontal)
				master_w = measured_w + screen_w * (expanded.size()); // allow expanded views to take up a screen each...
			else
				master_h = measured_h + screen_h * (expanded.size()); // allow expanded views to take up a screen each...
			
		} else { // we are restricted...
			if(horizontal)
				measured_w = measured_w > master_w ? master_w : measured_w;
			else
				measured_h = measured_h > master_h ? master_h : measured_h;
		}
		
		if(DEBUG_LAYOUT) Log.v("Kamoflage", crnt_tab + "onMeasure[" + title + "] - FIRST meassure[" + Integer.toString(measured_w) + "] [" + Integer.toString(measured_h)+ "]");

		if((!(m_mode_w == android.view.View.MeasureSpec.AT_MOST || m_mode_h == android.view.View.MeasureSpec.AT_MOST))) {

			remaining_w = remaining_h = 0;

			if(expanded.size() > 0)
				if(horizontal) {
					remaining_w = (master_w - measured_w) / expanded.size();
				} else { // vertical
					remaining_h = (master_h - measured_h) / expanded.size();
					if(DEBUG_LAYOUT) Log.v("Kamoflage", crnt_tab + "master_h [" + master_h + "] measured_h [" + measured_h + "] expanded.size() [" + expanded.size() + "] remaining_h [" + remaining_h + "]");
				}
			
			if(DEBUG_LAYOUT) Log.v("Kamoflage", crnt_tab + "remaining[" + title + "] - [" + Integer.toString(remaining_w) + "] [" + Integer.toString(remaining_h)+ "]");
			final int e_s = contained.size();
			for (int i = 0; i < e_s; i++) {
				Contained cnt = contained.get(i);
				if(cnt.expand) {
					cnt.rec_width += remaining_w;
					cnt.rec_height += remaining_h;
				}
				
				if(horizontal) {
					childHeightSpec =
						android.view.View.MeasureSpec.makeMeasureSpec(
							master_h,
							android.view.View.MeasureSpec.EXACTLY);
					
					childWidthSpec =
						android.view.View.MeasureSpec.makeMeasureSpec(
							cnt.rec_width,
							android.view.View.MeasureSpec.EXACTLY);
					
					
				} else { // vertical
					childWidthSpec =
						android.view.View.MeasureSpec.makeMeasureSpec(
							master_w,
							android.view.View.MeasureSpec.EXACTLY);

					childHeightSpec =
						android.view.View.MeasureSpec.makeMeasureSpec(
							cnt.rec_height,
							android.view.View.MeasureSpec.EXACTLY);
				}

				android.view.View v = cnt.vju;
				
				v.measure(childWidthSpec, childHeightSpec);			
				cnt.rec_width = cnt.req_width = v.getMeasuredWidth();
				cnt.rec_height = cnt.req_height = v.getMeasuredHeight();
				
			}
			measured_w = master_w;
			measured_h = master_h;

			if(DEBUG_LAYOUT) Log.v("Kamoflage", crnt_tab + "onMeasure[" + title + "] - SECOND meassure[" + Integer.toString(measured_w) + "] [" + Integer.toString(measured_h)+ "]");

		}

		if(framed) {
			measured_w += 2 * offset;
			measured_h += 2 * offset;
		}
		
		setMeasuredDimension(measured_w, measured_h);

		tab_string = crnt_tab;
	}
	
	@Override
	protected void onLayout(boolean changed, int l, int t, int r, int b) {
		int w = r - l;
		int h = b - t;

		int offset = 0;
		if(framed) {
			offset = 2 * frame_view.get_offset();
			
			w -= 2 * offset;
			h -= 2 * offset;
		}
		
		int c_x, c_y;

		c_x = 0; c_y = 0;

		if(framed) {
			c_x += offset;
			c_y += offset;

			frame_view.layout(0, 0, r - l, b - t);
		}

		int c_w, c_h;
		
//		if(DEBUG_LAYOUT) Log.v("Kamoflage", "onLayout[" + title + "] - area [" + Integer.toString(w) + "] [" + Integer.toString(h)+ "]");

		final int ct_s = contained.size();
		for (int i = 0; i < ct_s; i++) {
			Contained cnt = contained.get(i);
			
			View child = cnt.vju;
			child.setVisibility(View.VISIBLE);
			
			if (child.getVisibility() != GONE) {
				c_w = cnt.fill ? cnt.rec_width : cnt.req_width;
				c_h = cnt.fill ? cnt.rec_height : cnt.req_height;

//				if(DEBUG_LAYOUT) Log.v("Kamoflage", "onLayout[" + title + "] - widget posi [" + Integer.toString(c_x) + "] [" + Integer.toString(c_y)+ "]");
//				if(DEBUG_LAYOUT) Log.v("Kamoflage", "        [" + title + "] -        size [" + Integer.toString(c_w) + "] [" + Integer.toString(c_h)+ "]");
				
				child.layout(c_x, c_y, c_x + c_w, c_y + c_h);

				if(horizontal) {
					c_x += cnt.rec_width;
				} else {
					c_y += cnt.rec_height;
				}
			}
		}
	}
}
