/*
 * This file is a derivate work based on
 * https://code.google.com/p/android-vnc-viewer/source/browse/branches/antlersoft/androidVNC/src/com/antlersoft/android/bc/ScaleGestureDetector.java?r=164
 *
 * The modified file is (C) 2013 by Anton Persson
 *
 * ----------- License of Modifications ----------------
 *
 * This program is free software; you can redistribute it and/or modify it under the terms of
 * the GNU General Public License version 3; see COPYING for the complete License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program;
 * if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * ------------ Original License -------------------------
 *
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "kamogui_scale_detector.hh"
#include <math.h>

//#define __DO_KAMOFLAGE_DEBUG
#include "kamoflage_debug.hh"

/**
 * This value is the threshold ratio between our previous combined pressure
 * and the current combined pressure. We will only fire an onScale event if
 * the computed ratio between the current and previous event pressures is
 * greater than this value. When pressure decreases rapidly between events
 * the position values can often be imprecise, as it usually indicates
 * that the user is in the process of lifting a pointer off of the device.
 * Its value was tuned experimentally.
 */
#define PRESSURE_THRESHOLD 0.67f


KammoGUI::ScaleGestureDetector::ScaleGestureDetector(OnScaleGestureListener *listener) : mGestureInProgress(false) {
        mListener = listener;
        mEdgeSlop = KammoGUI::DisplayConfiguration::get_edge_slop();
}

bool KammoGUI::ScaleGestureDetector::on_touch_event(const KammoGUI::SVGCanvas::MotionEvent &event) {
        int action = event.get_action();
        bool handled = true;

        if (!mGestureInProgress) {
		handled = false;
		switch (action) {
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN: {
			// We have a new multi-finger gesture

			// as orientation can change, query the metrics in touch down
			mRightSlopEdge = KammoGUI::DisplayConfiguration::get_screen_width(KammoGUI::pixels) - mEdgeSlop;
			mBottomSlopEdge = KammoGUI::DisplayConfiguration::get_screen_height(KammoGUI::pixels) - mEdgeSlop;

			// Be paranoid in case we missed an event
			reset();

			mPrevEvent.clone(event);
			mTimeDelta = 0;

			set_context(event);

#ifdef SCALE_DETECTOR_DO_SLOPPY_CHECK 
			// Check if we have a sloppy gesture. If so, delay
			// the beginning of the gesture until we're sure that's
			// what the user wanted. Sloppy gestures can happen if the
			// edge of the user's hand is touching the screen, for example.
			float edgeSlop = mEdgeSlop;
			float rightSlop = mRightSlopEdge;
			float bottomSlop = mBottomSlopEdge;
			float x0 = event.get_raw_x();
			float y0 = event.get_raw_y();
			float x1 = get_raw_x(event, 1);
			float y1 = get_raw_y(event, 1);

			bool p0sloppy =
				(x0 < edgeSlop || y0 < edgeSlop || x0 > rightSlop || y0 > bottomSlop) ? true : false;
			bool p1sloppy =
				(x1 < edgeSlop || y1 < edgeSlop || x1 > rightSlop || y1 > bottomSlop) ? true : false;

			if (p0sloppy && p1sloppy) {
				mFocusX = -1;
				mFocusY = -1;
				mSloppyGesture = true;
			} else if (p0sloppy) {
				mFocusX = event.get_x(1);
				mFocusY = event.get_y(1);
				mSloppyGesture = true;
			} else if (p1sloppy) {
				mFocusX = event.get_x(0);
				mFocusY = event.get_y(0);
				mSloppyGesture = true;
			} else {
#else
				mSloppyGesture = false;
#endif
				handled = mGestureInProgress = mListener->on_scale_begin(this);
#ifdef SCALE_DETECTOR_DO_SLOPPY_CHECK
			}
#endif
			KAMOFLAGE_DEBUG("e: %f, re: %f, be: %f -- %s\n", mEdgeSlop, mRightSlopEdge, mBottomSlopEdge,
					mSloppyGesture ? "true" : "false");
		}
			break;
            
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
			if (mSloppyGesture) {
				// Initiate sloppy gestures if we've moved outside of the slop area.
				float edgeSlop = mEdgeSlop;
				float rightSlop = mRightSlopEdge;
				float bottomSlop = mBottomSlopEdge;
				float x0 = event.get_raw_x();
				float y0 = event.get_raw_y();
				float x1 = get_raw_x(event, 1);
				float y1 = get_raw_y(event, 1);

				bool p0sloppy = x0 < edgeSlop || y0 < edgeSlop
								      || x0 > rightSlop || y0 > bottomSlop;
				bool p1sloppy = x1 < edgeSlop || y1 < edgeSlop
								      || x1 > rightSlop || y1 > bottomSlop;
				
				if(p0sloppy && p1sloppy) {
					mFocusX = -1;
					mFocusY = -1;
				} else if (p0sloppy) {
					mFocusX = event.get_x(1);
					mFocusY = event.get_y(1);
				} else if (p1sloppy) {
					mFocusX = event.get_x(0);
					mFocusY = event.get_y(0);
				} else {
					mSloppyGesture = false;
					handled = mGestureInProgress = mListener->on_scale_begin(this);
				}
			}
			break;
                
		case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
			if (mSloppyGesture) {
				// Set focus point to the remaining finger
				int id = event.get_action_index() == 0 ? 1 : 0;
				mFocusX = event.get_x(id);
				mFocusY = event.get_y(id);
			}
			break;
		}
        } else {
		// Transform gesture in progress - attempt to handle it
		switch (action) {
                case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
			// Gesture ended
			set_context(event);

			// Set focus point to the remaining finger
			{
				int id = event.get_action_index() == 0 ? 1 : 0;
				mFocusX = event.get_x(id);
				mFocusY = event.get_y(id);
			}
			
			if (!mSloppyGesture) {
				mListener->on_scale_end(this);
			}

			reset();
			break;

                case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
			if (!mSloppyGesture) {
				mListener->on_scale_end(this);
			}

			reset();
			break;

                case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
			set_context(event);

			// Only accept the event if our relative pressure is within
			// a certain limit - this can help filter shaky data as a
			// finger is lifted.
			if (mCurrPressure / mPrevPressure > PRESSURE_THRESHOLD) {
				bool updatePrevious = mListener->on_scale(this);

				if (updatePrevious) {
					mPrevEvent.clone(event);
				}
			}
			break;
		}
        }
        return handled;
}
    
float KammoGUI::ScaleGestureDetector::get_raw_x(const KammoGUI::SVGCanvas::MotionEvent &event, int pointerIndex) {
        float offset = event.get_x() - event.get_raw_x();
        return event.get_x(pointerIndex) + offset;
}
    
float KammoGUI::ScaleGestureDetector::get_raw_y(const KammoGUI::SVGCanvas::MotionEvent &event, int pointerIndex) {
        float offset = event.get_y() - event.get_raw_y();
        return event.get_y(pointerIndex) + offset;
}

void KammoGUI::ScaleGestureDetector::set_context(const KammoGUI::SVGCanvas::MotionEvent &curr) {
        mCurrEvent.clone(curr);

        mCurrLen = -1;
        mPrevLen = -1;
        mScaleFactor = -1;

        float px0 = mPrevEvent.get_x(0);
        float py0 = mPrevEvent.get_y(0);
        float px1 = mPrevEvent.get_x(1);
        float py1 = mPrevEvent.get_y(1);
        float cx0 = curr.get_x(0);
        float cy0 = curr.get_y(0);
        float cx1 = curr.get_x(1);
        float cy1 = curr.get_y(1);

        float pvx = px1 - px0;
        float pvy = py1 - py0;
        float cvx = cx1 - cx0;
        float cvy = cy1 - cy0;
        mPrevFingerDiffX = pvx;
        mPrevFingerDiffY = pvy;
        mCurrFingerDiffX = cvx;
        mCurrFingerDiffY = cvy;

        mFocusX = cx0 + cvx * 0.5f;
        mFocusY = cy0 + cvy * 0.5f;
        mTimeDelta = curr.get_event_time() - mPrevEvent.get_event_time();
        mCurrPressure = curr.get_pressure(0) + curr.get_pressure(1);
        mPrevPressure = mPrevEvent.get_pressure(0) + mPrevEvent.get_pressure(1);
}

void KammoGUI::ScaleGestureDetector::reset() {
        mSloppyGesture = false;
        mGestureInProgress = false;
}

/**
 * Returns {@code true} if a two-finger scale gesture is in progress.
 * @return {@code true} if a scale gesture is in progress, {@code false} otherwise.
 */
bool KammoGUI::ScaleGestureDetector::is_in_progress() {
        return mGestureInProgress;
}

/**
 * Get the X coordinate of the current gesture's focal point.
 * If a gesture is in progress, the focal point is directly between
 * the two pointers forming the gesture.
 * If a gesture is ending, the focal point is the location of the
 * remaining pointer on the screen.
 * If {@link #is_in_progress()} would return false, the result of this
 * function is undefined.
 * 
 * @return X coordinate of the focal point in pixels.
 */
float KammoGUI::ScaleGestureDetector::get_focus_x() {
        return mFocusX;
}

/**
 * Get the Y coordinate of the current gesture's focal point.
 * If a gesture is in progress, the focal point is directly between
 * the two pointers forming the gesture.
 * If a gesture is ending, the focal point is the location of the
 * remaining pointer on the screen.
 * If {@link #is_in_progress()} would return false, the result of this
 * function is undefined.
 * 
 * @return Y coordinate of the focal point in pixels.
 */
float KammoGUI::ScaleGestureDetector::get_focus_y() {
        return mFocusY;
}

/**
 * Return the current distance between the two pointers forming the
 * gesture in progress.
 * 
 * @return Distance between pointers in pixels.
 */
float KammoGUI::ScaleGestureDetector::get_current_span() {
        if (mCurrLen == -1) {
		float cvx = mCurrFingerDiffX;
		float cvy = mCurrFingerDiffY;
		mCurrLen = sqrtf(cvx*cvx + cvy*cvy);
        }
        return mCurrLen;
}

/**
 * Return the previous distance between the two pointers forming the
 * gesture in progress.
 * 
 * @return Previous distance between pointers in pixels.
 */
float KammoGUI::ScaleGestureDetector::get_previous_span() {
        if (mPrevLen == -1) {
		float pvx = mPrevFingerDiffX;
		float pvy = mPrevFingerDiffY;
		mPrevLen = sqrtf(pvx*pvx + pvy*pvy);
        }
        return mPrevLen;
}

/**
 * Return the scaling factor from the previous scale event to the current
 * event. This value is defined as
 * ({@link #get_current_span()} / {@link #get_previous_span()}).
 * 
 * @return The current scaling factor.
 */
float KammoGUI::ScaleGestureDetector::get_scale_factor() {
        if (mScaleFactor == -1) {
		mScaleFactor = get_current_span() / get_previous_span();
        }
        return mScaleFactor;
}
    
/**
 * Return the time difference in milliseconds between the previous
 * accepted scaling event and the current scaling event.
 * 
 * @return Time difference since the last scaling event in milliseconds.
 */
long KammoGUI::ScaleGestureDetector::get_time_delta() {
        return mTimeDelta;
}
    
/**
 * Return the event time of the current event being processed.
 * 
 * @return Current event time in milliseconds.
 */
long KammoGUI::ScaleGestureDetector::get_event_time() {
        return mCurrEvent.get_event_time();
}
