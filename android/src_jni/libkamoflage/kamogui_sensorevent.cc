/*
 * Kamoflage, ANDROID version
 * Copyright (C) 2007 by Anton Persson & Ted Björling
 * Copyright (C) 2010 by Anton Persson
 * Copyright (C) 2015 by Anton Persson
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

#include <mutex>
#include <thread>

#define __DO_KAMOFLAGE_DEBUG
#include "kamoflage_debug.hh"

namespace KammoGUI {

	static std::mutex listeners_lock;
	static std::vector<SensorEvent::Listener*> listeners;

	void SensorEvent::register_listener(std::shared_ptr<SensorEvent::Listener> listener) {
		std::lock_guard<std::mutex> guard(listeners_lock);
		listeners.push_back(listener.get());
	}

	void SensorEvent::handle_event(float v1, float v2, float v3) {
		std::lock_guard<std::mutex> guard(listeners_lock);
		auto evt = std::make_shared<SensorEvent>();
		evt->values[0] = v1;
		evt->values[1] = v2;
		evt->values[2] = v3;

		for(auto listener : listeners) {
			listener->on_sensor_event(evt);
		}
	}

};

extern "C" {

	JNIEXPORT void JNICALL Java_com_toolkits_kamoflage_SensorHandler_handleEvent
	(JNIEnv * env, jclass jc, jfloat v1, jfloat v2 , jfloat v3) {
		KammoGUI::run_on_GUI_thread(
			[v1, v2, v3]() {
				KammoGUI::SensorEvent::handle_event(v1, v2, v3);
			});
	}

}
