/*
 * This file is a derivate work based on
 * https://code.google.com/p/android-vnc-viewer/source/browse/branches/antlersoft/androidVNC/src/com/antlersoft/android/bc/ScaleGestureDetector.java?r=164
 * (updated with) https://android.googlesource.com/platform/frameworks/base/+/48c7c6c/core/java/android/view/ScaleGestureDetector.java
 *
 * The modified file is (C) 2013, 2018 by Anton Persson
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

KammoGUI::ScaleGestureDetector::ScaleGestureDetector(OnScaleGestureListener *listener) : mGestureInProgress(false) {
        mListener = listener;
}

bool KammoGUI::ScaleGestureDetector::on_touch_event(const KammoGUI::MotionEvent &event) {
	const int action = event.get_action();
	const bool streamComplete = action == KammoGUI::MotionEvent::ACTION_UP || action == KammoGUI::MotionEvent::ACTION_CANCEL;

	if (action == KammoGUI::MotionEvent::ACTION_DOWN || streamComplete) {
		// Reset any scale in progress with the listener.
		// If it's an ACTION_DOWN we're beginning a new event stream.
		// This means the app probably didn't give us all the events. Shame on it.
		if (mInProgress) {
			mListener->on_scale_end(this);
			mInProgress = false;
		}
		if (streamComplete) {
			return true;
		}
	}
	const bool configChanged =
		action == KammoGUI::MotionEvent::ACTION_POINTER_UP ||
		action == KammoGUI::MotionEvent::ACTION_POINTER_DOWN;
	const bool pointerUp = action == KammoGUI::MotionEvent::ACTION_POINTER_UP;
	const int skipIndex = pointerUp ? event.get_action_index() : -1;
	// Determine focal point
	float sumX = 0, sumY = 0;
	const int count = event.get_pointer_count();
	for (int i = 0; i < count; i++) {
		if (skipIndex == i) continue;
		sumX += event.get_x(i);
		sumY += event.get_y(i);
	}
	const int div = pointerUp ? count - 1 : count;
	const float focusX = sumX / div;
	const float focusY = sumY / div;
	// Determine average deviation from focal point
	float devSumX = 0, devSumY = 0;
	for (int i = 0; i < count; i++) {
		if (skipIndex == i) continue;
		devSumX += fabsf(event.get_x(i) - focusX);
		devSumY += fabsf(event.get_y(i) - focusY);
	}
	const float devX = devSumX / div;
	const float devY = devSumY / div;
	// Span is the average distance between touch points through the focal point;
	// i.e. the diameter of the circle with a radius of the average deviation from
	// the focal point.
	const float spanX = devX * 2;
	const float spanY = devY * 2;
	const float span = sqrtf(spanX * spanX + spanY * spanY);
	// Dispatch begin/end events as needed.
	// If the configuration changes, notify the app to reset its current state by beginning
	// a fresh scale event stream.
	if (mInProgress && (span == 0 || configChanged)) {
		mListener->on_scale_end(this);
		mInProgress = false;
	}
	if (configChanged) {
		mPrevSpanX = mCurrSpanX = spanX;
		mPrevSpanY = mCurrSpanY = spanY;
		mPrevSpan = mCurrSpan = span;
	}
	if (!mInProgress && span != 0) {
		mFocusX = focusX;
		mFocusY = focusY;
		mInProgress = mListener->on_scale_begin(this);
	}
	// Handle motion; focal point and span/scale factor are changing.
	if (action == KammoGUI::MotionEvent::ACTION_MOVE) {
		mCurrSpanX = spanX;
		mCurrSpanY = spanY;
		mCurrSpan = span;
		mFocusX = focusX;
		mFocusY = focusY;
		bool updatePrev = true;
		if (mInProgress) {
			updatePrev = mListener->on_scale(this);
		}
		if (updatePrev) {
			mPrevSpanX = mCurrSpanX;
			mPrevSpanY = mCurrSpanY;
			mPrevSpan = mCurrSpan;
		}
	}
	return is_in_progress();
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
	return mCurrSpan;
}

/**
 * Return the previous distance between the two pointers forming the
 * gesture in progress.
 *
 * @return Previous distance between pointers in pixels.
 */
float KammoGUI::ScaleGestureDetector::get_previous_span() {
        return mPrevSpan;
}

/**
 * Return the scaling factor from the previous scale event to the current
 * event. This value is defined as
 * ({@link #get_current_span()} / {@link #get_previous_span()}).
 *
 * @return The current scaling factor.
 */
float KammoGUI::ScaleGestureDetector::get_scale_factor() {
	return mPrevSpan > 0 ? mCurrSpan / mPrevSpan : 1.0f;
}

/**
 * Return the time difference in milliseconds between the previous
 * accepted scaling event and the current scaling event.
 *
 * @return Time difference since the last scaling event in milliseconds.
 */
long KammoGUI::ScaleGestureDetector::get_time_delta() {
	return mCurrTime - mPrevTime;
}

/**
 * Return the event time of the current event being processed.
 *
 * @return Current event time in milliseconds.
 */
long KammoGUI::ScaleGestureDetector::get_event_time() {
	return mCurrTime;
}
