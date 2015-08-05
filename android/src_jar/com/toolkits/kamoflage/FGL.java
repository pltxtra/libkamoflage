/*
 * the Free Gesture Library - a free software library for multitouch gestures
 * on Android.
 *
 * Copyright (C) 2011 by Anton Persson
 * http://www.733kru.org/
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

package com.toolkits.kamoflage;

import android.util.Log;
import android.view.VelocityTracker;

import android.graphics.Bitmap;
import android.graphics.Paint;
import android.graphics.Canvas;
//import android.widget;
import java.io.InputStream;
import android.content.Context;
import java.util.HashMap;
import java.util.Vector;
import java.util.ArrayList;
import javax.xml.parsers.*;
import org.w3c.dom.*;

import android.view.View;
import android.view.GestureDetector;
import android.view.ViewConfiguration;
import android.view.MotionEvent;
import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.FloatMath;

import java.lang.System;

public class FGL
{	
	private enum State {
		NONE, LONGPRESS, DRAG, PINCH
	}
	
	public interface SimpleFGLListener {
		/// New finger entered the game at
		boolean onStart(int fingerCount, float x_pos, float y_pos);

		/// A draging motion 
		boolean onDrag(int fingers, float x_length, float y_length);

		/// if the user drags gently and then lifts, the motion ends with a "stop" event
		boolean onStop(int fingers, float x_velocity, float y_velocity);

		/// in case the user flings the motion ends with a "fling" event
		boolean onFling(int fingers, float velocity_x, float velocity_y);

		/// A pinching motion
		boolean onPinch(int fingers, float scale, float angle);

		/// A tap at a point
		boolean onTap(int fingers, float xpos, float ypos);

		/// A long press on a point (single finger, in current implementation.)
		boolean onLongpress(int fingers, float xpos, float ypos);
	}

	/// at this time this library is simple and only allows one listener
	SimpleFGLListener currentListener = null;
	
	public FGL(Context context) {
		ViewConfiguration viewConfiguration = ViewConfiguration.get(context);
		scaledTouchSlopX2 = viewConfiguration.getScaledTouchSlop();
		scaledTouchSlopX2 *= scaledTouchSlopX2;
	}

	private float distance(MotionEvent event) {
		float x = event.getX(0) - event.getX(1);
		float y = event.getY(0) - event.getY(1);
		return FloatMath.sqrt(x * x + y * y);
	}

	public void attachSimpleFGLListener(SimpleFGLListener slist) {
		currentListener = slist;
	}

	int fingers = 0; // number of current fingers
	State state = State.NONE;

	// track velocity of the gesture
	VelocityTracker mVelocityTracker;

	// ignoreAdditionalFingers is set to true
	// if the time between _DOWN events is too long
	boolean ignoreAdditionalFingers = false;

	final float minFlingV = 300;
	final float maxFlingV = 1000;

	private float drag_start_x;
	private float drag_start_y;
	private long start_time;
	private float first_distance = 0.0f;
	
	private static final long tapTimeout = 
		ViewConfiguration.getTapTimeout();
	private static final long longPressTimeout = 
		ViewConfiguration.getLongPressTimeout();
	private static int scaledTouchSlopX2 = 0;

	private static final int LONGPRESS = 0xd00f;
	private Handler msg_handler = new Handler() {
			
			public void handleMessage(Message m) {
				switch (m.what) {
				case LONGPRESS:
					currentListener.onLongpress(1, drag_start_x, drag_start_y);
					state = State.NONE;
					break;
					
				default:
					Log.v("FGL", "Unhandled message - expected LONGPRESS");
					break;
				}
			}
		};
	
	boolean onTouchEvent(MotionEvent evt) {
		boolean handled = false;

		float x = (float)evt.getX();
		float y = (float)evt.getY();

		if (mVelocityTracker == null) {
			mVelocityTracker = VelocityTracker.obtain();
		}
		mVelocityTracker.addMovement(evt);

		switch (evt.getAction() & MotionEvent.ACTION_MASK) {
		case MotionEvent.ACTION_DOWN: // first touch point
			fingers = 1;
			state = State.LONGPRESS;
			drag_start_x = x;
			drag_start_y = y;
			currentListener.onStart(fingers, x, y);
			start_time = System.currentTimeMillis();

			msg_handler.removeMessages(LONGPRESS);
			msg_handler.sendEmptyMessageAtTime(LONGPRESS, 
							      evt.getDownTime() +
							      tapTimeout + longPressTimeout);

			
			break;
		case MotionEvent.ACTION_POINTER_DOWN: // additional points
			if(fingers > 0 && (!ignoreAdditionalFingers)) {
				state = State.PINCH;
				first_distance = distance(evt);
				fingers++;
				currentListener.onStart(fingers, x, y);
			}
			msg_handler.removeMessages(LONGPRESS);
			break;
		case MotionEvent.ACTION_POINTER_UP: // lifted one point, at least one left
			state = State.NONE;

			if(fingers > 0) {
				// negative fingers will prevent any other event
				// untill the user has lifted all fingers
				fingers = -fingers;

				// A fling must travel the minimum tap distance
				mVelocityTracker.computeCurrentVelocity(1000, maxFlingV);
				final float vx = mVelocityTracker.getXVelocity();
				final float vy = mVelocityTracker.getYVelocity();
				if (
					(Math.abs(vx) > minFlingV)
				    ||
					(Math.abs(vy) > minFlingV)) {
					if(currentListener != null)
						handled =
							currentListener.onFling(-fingers, vx, vy);
				} else {
					if(currentListener != null)
						handled = currentListener.onStop(-fingers,
										 x - drag_start_x,
										 y - drag_start_y);
				}
			}
			break;
		case MotionEvent.ACTION_UP:// no points left, just lifted the last one
			handled = currentListener.onStop(1,
							 x - drag_start_x,
							 y - drag_start_y);
			if(state == State.LONGPRESS) {
				msg_handler.removeMessages(LONGPRESS);
				currentListener.onTap(1, x, y);
			}
			
			fingers = 0;
			state = State.NONE;
			mVelocityTracker.recycle();
			mVelocityTracker = null;
			if(currentListener != null)
			break;
		case MotionEvent.ACTION_MOVE:
			if(state == State.DRAG || state == State.PINCH || state == State.LONGPRESS) {
				handled = currentListener.onDrag(fingers, x - drag_start_x, y - drag_start_y);
			}

			if(state == State.LONGPRESS) {
				int xd = (int) (x - drag_start_x);
				int yd = (int) (y - drag_start_y);
				int d = (xd * xd) + (yd * yd);
				if (d > scaledTouchSlopX2) {
					state = State.DRAG;
					msg_handler.removeMessages(LONGPRESS);
				}
				
			}
			
			if(state == State.PINCH) {
				float new_distance = distance(evt);
				handled = currentListener.onPinch(
					fingers,
					new_distance / first_distance, 0.0f);
			}
			break;
		}

		return handled;
	}
}
