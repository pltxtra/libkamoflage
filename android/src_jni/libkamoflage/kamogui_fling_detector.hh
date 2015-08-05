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

#ifndef KAMOGUI_FLING_DETECTOR
#define KAMOGUI_FLING_DETECTOR

#include "kamogui.hh"

namespace KammoGUI {

	class FlingGestureDetector {
	private:
		static float fling_limit;
		static bool fling_limit_set;
		
		int fingers, max_fingers;
		int last_x, last_y;

		struct timespec before;

		float abs_speed_x, abs_speed_y;
		float speed_x, speed_y;

		void start_fling_detect(int x, int y);
		bool test_for_fling(int x, int y);
		
	public:
		FlingGestureDetector();

		// returns true if a fling was detected
		bool on_touch_event(const KammoGUI::SVGCanvas::MotionEvent &event);

		// returns the fling speed vector (pixels / second)
		void get_speed(float &speed_x, float &speed_y);

		// returns the absolute fling speed vector (pixels / second)
		void get_absolute_speed(float &abs_speed_x, float &abs_speed_y);

		// returns the maximum number of fingers involved in the fling
		int get_number_of_fingers();
	};

};

#endif
