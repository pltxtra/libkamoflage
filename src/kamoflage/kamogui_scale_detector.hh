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

#ifndef KAMOGUI_SCALE_DETECTOR
#define KAMOGUI_SCALE_DETECTOR

#include "kamogui.hh"

namespace KammoGUI {

/**
 * Detects transformation gestures involving more than one pointer ("multitouch")
 * using the supplied {@link MotionEvent}s. The {@link OnScaleGestureListener}
 * callback will notify users when a particular gesture event has occurred.
 * This class should only be used with {@link MotionEvent}s reported via touch.
 *
 * To use this class:
 * <ul>
 *  <li>Create an instance of the {@code ScaleGestureDetector} for your
 *      {@link View}
 *  <li>In the {@link View#onTouchEvent(MotionEvent)} method ensure you call
 *          {@link #onTouchEvent(MotionEvent)}. The methods defined in your
 *          callback will be executed when the events occur.
 * </ul>
 */
	class ScaleGestureDetector {
	public:
		/**
		 * The listener for receiving notifications when gestures occur.
		 * If you want to listen for all the different gestures then implement
		 * this interface. If you only want to listen for a subset it might
		 * be easier to extend {@link SimpleOnScaleGestureListener}.
		 *
		 * An application will receive events in the following order:
		 * <ul>
		 *  <li>One {@link OnScaleGestureListener#onScaleBegin(ScaleGestureDetector)}
		 *  <li>Zero or more {@link OnScaleGestureListener#onScale(ScaleGestureDetector)}
		 *  <li>One {@link OnScaleGestureListener#onScaleEnd(ScaleGestureDetector)}
		 * </ul>
		 */
		class OnScaleGestureListener {
		public:
			/**
			 * Responds to scaling events for a gesture in progress.
			 * Reported by pointer motion.
			 *
			 * @param detector The detector reporting the event - use this to
			 *          retrieve extended info about event state.
			 * @return Whether or not the detector should consider this event
			 *          as handled. If an event was not handled, the detector
			 *          will continue to accumulate movement until an event is
			 *          handled. This can be useful if an application, for example,
			 *          only wants to update scaling factors if the change is
			 *          greater than 0.01.
			 */
			virtual bool on_scale(ScaleGestureDetector *detector) = 0;

			/**
			 * Responds to the beginning of a scaling gesture. Reported by
			 * new pointers going down.
			 *
			 * @param detector The detector reporting the event - use this to
			 *          retrieve extended info about event state.
			 * @return Whether or not the detector should continue recognizing
			 *          this gesture. For example, if a gesture is beginning
			 *          with a focal point outside of a region where it makes
			 *          sense, onScaleBegin() may return false to ignore the
			 *          rest of the gesture.
			 */
			virtual bool on_scale_begin(ScaleGestureDetector *detector) = 0;

			/**
			 * Responds to the end of a scale gesture. Reported by existing
			 * pointers going up.
			 *
			 * Once a scale has ended, {@link ScaleGestureDetector#get_focus_x()}
			 * and {@link ScaleGestureDetector#get_focus_y()} will return the location
			 * of the pointer remaining on the screen.
			 *
			 * @param detector The detector reporting the event - use this to
			 *          retrieve extended info about event state.
			 */
			virtual void on_scale_end(ScaleGestureDetector *detector) = 0;
		};

	private:
		OnScaleGestureListener *mListener;
		bool mGestureInProgress;

		KammoGUI::MotionEvent mPrevEvent;
		KammoGUI::MotionEvent mCurrEvent;

		float mFocusX;
		float mFocusY;
		float mCurrSpan;
		float mPrevSpan;
		float mCurrSpanX;
		float mCurrSpanY;
		float mPrevSpanX;
		float mPrevSpanY;
		long mCurrTime;
		long mPrevTime;
		bool mInProgress;

	public:
		ScaleGestureDetector(OnScaleGestureListener *listener);

		/**
		 * Accepts MotionEvents and dispatches events to a {@link OnScaleGestureListener}
		 * when appropriate.
		 *
		 * <p>Applications should pass a complete and consistent event stream to this method.
		 * A complete and consistent event stream involves all MotionEvents from the initial
		 * ACTION_DOWN to the final ACTION_UP or ACTION_CANCEL.</p>
		 *
		 * @param event The event to process
		 * @return true if the event was processed and the detector wants to receive the
		 *         rest of the MotionEvents in this event stream.
		 */
		bool on_touch_event(const KammoGUI::MotionEvent &event);

		/**
		 * Returns {@code true} if a two-finger scale gesture is in progress.
		 * @return {@code true} if a scale gesture is in progress, {@code false} otherwise.
		 */
		bool is_in_progress();

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
		float get_focus_x();

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
		float get_focus_y();

		/**
		 * Return the current distance between the two pointers forming the
		 * gesture in progress.
		 *
		 * @return Distance between pointers in pixels.
		 */
		float get_current_span();

		/**
		 * Return the average X distance between each of the pointers forming the
		 * gesture in progress through the focal point.
		 *
		 * @return Distance between pointers in pixels.
		 */
		float get_current_span_x() {
			return mCurrSpanX;
		}

		/**
		 * Return the average Y distance between each of the pointers forming the
		 * gesture in progress through the focal point.
		 *
		 * @return Distance between pointers in pixels.
		 */
		float get_current_span_y() {
			return mCurrSpanY;
		}

		/**
		 * Return the previous distance between the two pointers forming the
		 * gesture in progress.
		 *
		 * @return Previous distance between pointers in pixels.
		 */
		float get_previous_span();

		/**
		 * Return the previous average X distance between each of the pointers forming the
		 * gesture in progress through the focal point.
		 *
		 * @return Previous distance between pointers in pixels.
		 */
		float get_previous_span_x() {
			return mPrevSpanX;
		}

		/**
		 * Return the previous average Y distance between each of the pointers forming the
		 * gesture in progress through the focal point.
		 *
		 * @return Previous distance between pointers in pixels.
		 */
		float get_previous_span_y() {
			return mPrevSpanY;
		}

		/**
		 * Return the scaling factor from the previous scale event to the current
		 * event. This value is defined as
		 * ({@link #get_current_span()} / {@link #get_previous_span()}).
		 *
		 * @return The current scaling factor.
		 */
		float get_scale_factor();

		/**
		 * Return the time difference in milliseconds between the previous
		 * accepted scaling event and the current scaling event.
		 *
		 * @return Time difference since the last scaling event in milliseconds.
		 */
		long get_time_delta();

		/**
		 * Return the event time of the current event being processed.
		 *
		 * @return Current event time in milliseconds.
		 */
		long get_event_time();
	};

};

#endif
