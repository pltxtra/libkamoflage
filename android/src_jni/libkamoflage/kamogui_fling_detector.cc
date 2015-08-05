/*
 * (C) 2014 by Anton Persson
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
 */

#include "kamogui_fling_detector.hh"

//#define __DO_KAMOFLAGE_DEBUG
#include "kamoflage_debug.hh"

bool KammoGUI::FlingGestureDetector::fling_limit_set = false;
float KammoGUI::FlingGestureDetector::fling_limit = 200.0f;

KammoGUI::FlingGestureDetector::FlingGestureDetector() {
}

void KammoGUI::FlingGestureDetector::start_fling_detect(int x, int y) {
	if(!fling_limit_set) {
		float width_pixels;
		float width_inches;
		width_inches = KammoGUI::DisplayConfiguration::get_screen_width(KammoGUI::inches);
		width_pixels = KammoGUI::DisplayConfiguration::get_screen_width(KammoGUI::pixels);
		fling_limit = width_pixels / width_inches;
		fling_limit_set = true;
	}
	
	clock_gettime(CLOCK_MONOTONIC_RAW, &before);
	last_x = x;
	last_y = y;
	KAMOFLAGE_DEBUG("start: %d, %d\n", last_x, last_y);
}

static inline void calculate_speed(float pixel_difference, float time, float &speed, float &abs_speed) {
	speed = pixel_difference / time;
	abs_speed = speed < 0.0f ? (-speed) : (speed);
}

bool KammoGUI::FlingGestureDetector::test_for_fling(int x, int y) {
	float time;

	/**** calculate difference ********/
	x -= last_x;
	y -= last_y;

	/**** CALCULATE time VARIABLE *****/
	struct timespec after;
	clock_gettime(CLOCK_MONOTONIC_RAW, &after);
	
	after.tv_sec -= before.tv_sec;
	after.tv_nsec -= before.tv_nsec;
	if(after.tv_nsec < 0) {
		after.tv_sec--;
		after.tv_nsec += 1000000000;
	}
	long useconds = (after.tv_sec * 1000000 + after.tv_nsec / 1000);
	time = (float)useconds; time /= 1000000.0f; // calculate time in seconds

	/**** CALCULATE speed_ *****/
	calculate_speed((float)(x), time, speed_x, abs_speed_x);
	calculate_speed((float)(y), time, speed_y, abs_speed_y);

	KAMOFLAGE_DEBUG("detect?: limit %f\n", fling_limit);
	KAMOFLAGE_DEBUG("detect?: %d, %d - time: %f, speed_x %f, speed_y %f\n", x, y, time, speed_x, speed_y);

	if(abs_speed_x > fling_limit ||
	   abs_speed_y > fling_limit)
		return true;

	return false;	
}

bool KammoGUI::FlingGestureDetector::on_touch_event(const KammoGUI::SVGCanvas::MotionEvent &event) {
	switch(event.get_action()) {
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_DOWN:
		start_fling_detect(event.get_x(), event.get_y());
		max_fingers = fingers = 1;
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_MOVE:
		// do nothing smart
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_UP:
		// compare to initial position/time only
		return test_for_fling(event.get_x(), event.get_y());
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_DOWN:
		// increase finger count
		fingers++;
		if(fingers > max_fingers) max_fingers = fingers;
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_POINTER_UP:
		// decrease finger count
		fingers--;
		break;
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_CANCEL:
	case KammoGUI::SVGCanvas::MotionEvent::ACTION_OUTSIDE:
		// cancel detection
		break;
	}

	return false;
}

void KammoGUI::FlingGestureDetector::get_speed(float &_speed_x, float &_speed_y) {
	_speed_x = speed_x;
	_speed_y = speed_y;
}

void KammoGUI::FlingGestureDetector::get_absolute_speed(float &_abs_speed_x, float &_abs_speed_y) {
	_abs_speed_x = abs_speed_x;
	_abs_speed_y = abs_speed_y;
}

int KammoGUI::FlingGestureDetector::get_number_of_fingers() {
	return max_fingers;
}
