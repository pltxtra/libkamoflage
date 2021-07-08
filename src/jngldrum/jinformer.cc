/*
 * jngldrum - a peer2peer networking library
 *
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 *
 * Originally from SATAN NG, moved to jngldrum.
 * SATAN - Signal Applications To Audio Networks
 * Copyright (C) 2003 by Johan Thim & Anton Persson
 * Copyright (C) 2005 by Anton Persson & Ted Björling
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

/*
 * jInformer: An informer thread, used to display run-time
 * information, like exceptions, to the user.
 *
 */

#include <iostream>
#include "jngldrum/jinformer.hh"

jInformer::Message::~Message() {}

jInformer::Message::Message(const std::string &_message) {
	message = _message;
}

std::string jInformer::Message::get_content() {
	return message;
}

void jInformer::Message::dummy_event_function() {
}

jThread::Mutex jInformer::instance_lock;
jInformer *jInformer::internal_instance = NULL;

jInformer::jInformer() {
	instance_lock.lock();
	if(internal_instance) {
		instance_lock.unlock();
		throw jException(
			"Trying to create multiple instances of jInformer"
			", not permitted.", jException::sanity_error);
	}
	internal_instance = this;
	instance_lock.unlock();
}

#if 0
void jInformer::thread_body() {
	while(1) {
		Message *msg = (Message *)eq.wait_for_event();
		/// XXX Process event here
		
		std::cout << msg->get_content() << "\n";
		delete msg;
	}
}
#endif

void jInformer::enque_information(const std::string &information) {	
	eq.push_event(new Message(information));
}

void jInformer::inform(const std::string &information) {
	instance_lock.lock();
	
	if(internal_instance == NULL) {
		std::cout << information << "\n";

		instance_lock.unlock();
		return;
	}

	try {
		internal_instance->enque_information(information);
	} catch(...) {
		instance_lock.unlock();
		throw;
	}

	instance_lock.unlock();

}
