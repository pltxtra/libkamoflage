/*
 * Kamoflage, ANDROID version
 * Copyright (C) 2007 by Anton Persson & Ted Björling
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

#include "kamogui.hh"

//#define __DO_KAMOFLAGE_DEBUG
#include "kamoflage_debug.hh"

/*************************
 *
 *  Animation class
 *
 *************************/

KammoGUI::Animation::Animation(float _duration) :
	duration(_duration), is_running(false) {
	KAMOFLAGE_ERROR("Animation created at %p\n", this);
}

KammoGUI::Animation::~Animation() {
	KAMOFLAGE_ERROR("Animation deleted at %p (-> %p)\n", this, this + sizeof(Animation));
}

float KammoGUI::Animation::get_now() {
	struct timespec after;
	clock_gettime(CLOCK_MONOTONIC_RAW, &after);
	after.tv_sec -= before.tv_sec;
	after.tv_nsec -= before.tv_nsec;
	if(after.tv_nsec < 0) {
		after.tv_sec--;
		after.tv_nsec += 1000000000;
	}
	long microseconds = after.tv_sec * 1000000 + after.tv_nsec / 1000;
	float progress = (float)microseconds;
	progress /= 1000000.0f; // convert to seconds

	// if duration is 0 - then progress indicates time since start
	if(duration > 0.0)
		progress = progress / duration;
	return progress;
}

void KammoGUI::Animation::start() {
	clock_gettime(CLOCK_MONOTONIC_RAW, &before);
	is_running = true;
}

bool KammoGUI::Animation::has_finished() {
	return !is_running;
}

void KammoGUI::Animation::new_time_tick() {
	if(!is_running) return;

	auto progress = get_now();
	if(duration > 0.0 && progress >= 1.0) {
		progress = 1.0;
		is_running = false;
	}
	new_frame(progress);
}

void KammoGUI::Animation::touch_event() {
	if(!is_running) return;

	on_touch_event();
}

void KammoGUI::Animation::stop() {
	is_running = false;
}
