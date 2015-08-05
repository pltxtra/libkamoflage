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
 * $Id: jinformer.hh,v 1.4 2006/03/26 11:11:03 pltxtra Exp $
 *
 */

#ifndef __JINFORMER
#define __JINFORMER

#include "jexception.hh"
#include "jthread.hh"
#include <string>

class jInformer : public jThread::Singleton {
protected:
	class Message : public jThread::Event {
	private:
		std::string message;
	public:
		virtual ~Message();
		Message(const std::string &message);
		std::string get_content();
		
		virtual void dummy_event_function();
	};

	jThread::EventQueue eq;
	
private:
	static jThread::Mutex instance_lock;
	static jInformer *internal_instance;

	/// Used to enque an informative string for the user to see
	void enque_information(const std::string &information);

protected:
	jInformer();	
	static jInformer *instance();
	
public:
	/**
	 * Inform the user about something, anything.
	 */
	static void inform(const std::string &information);
};

#endif
