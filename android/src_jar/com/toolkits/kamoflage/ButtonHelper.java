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
import android.widget.Button;
import java.io.InputStream;
import android.content.Context;
import java.util.IdentityHashMap;
import java.util.Vector;
import java.util.ArrayList;
import javax.xml.parsers.*;
import org.w3c.dom.*;

class ButtonHelper extends Button {
	final boolean DEBUG_LAYOUT = false;

	ButtonHelper(android.app.Activity context) {
		super(context);
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
		/*
		int sw, sh;

		sw = measureSize(150, widthMeasureSpec);
		sh = measureSize(150, heightMeasureSpec);

		setMeasuredDimension(sw, sh);
		*/
		super.onMeasure(widthMeasureSpec, heightMeasureSpec);
		
		if(DEBUG_LAYOUT)
			Log.v("Kamoflage", "BUTTON MEASURE:[" + Integer.toString(getMeasuredWidth()) + "] [" + Integer.toString(getMeasuredHeight())+ "]");
	}
	

}