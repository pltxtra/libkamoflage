/*
 * jngldrum - a peer2peer networking library
 *
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 *
 * Originally from YAFFA, moved to jngldrum.
 * YAFFA, Yet Another F****** FTP Application
 * Copyright (C) 2000-2001 by Magnus Ekdahl, Joacim Häggmark, Johan Thim, Anton Persson
 * Copyright (C) 2002-     by Anton Persson
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
 * jThread: The jngldrum thread class. Based on YThread, from YAFFA
 *
 * $Id: jthread.hh,v 1.7 2009/02/05 11:32:03 pltxtra Exp $
 *
 */

#ifndef __JTHREAD
#define __JTHREAD
#include <pthread.h>
#include <queue>
#include <stack>
#include <stdint.h>

//#define SIMPLE_JTHREAD_DEBUG

#ifdef WIN32
#define sleep(a) Sleep(a*1000)
#endif

#ifdef __CLEANUP_C
// XXX why does pthreads for windows contain such ugly solutions?!
#define _USES_UGLY_WIN_PTHREADS
#else
#ifdef pthread_cleanup_push
#ifdef __CYGWIN__
// XXX why does Cygwin often contain such ugly solutions?!
#define _USES_UGLY_CYGWIN_PTHREADS
#else
// XXX why does Linux often contain such ugly solutions?!
#define _USES_UGLY_LINUX_PTHREADS
#endif
#endif
#endif

#define _HAVE_PTHREAD_CONDATTR_INIT

#include "jexception.hh"

enum jCancelType {normal_cancel, async_cancel};

class jCancelObject {
private:
public:
	virtual ~jCancelObject() = 0;
	virtual void cancel() = 0;
};


class jThread
{
private:
#ifdef ANDROID
	int __my_android_hash;
	void constructor_helper();
#else
	pthread_t thread;
	pthread_attr_t pta;
#endif 	
	jThread();

	std::string __signum;


public:
#ifdef DEBUG_JTHREAD_LOCKS
	class Mutex;
	class WhosLock {
	public:
		Mutex *mtx;
		pid_t owner;
		int count;
		
		WhosLock(jThread::Mutex *_mtx);
	};
#endif
	
#ifdef ANDROID
	static void java_thread_entry(int identity_hash);
#endif
	
	class DoesNotSupportPthreadConditions {};
	class DoesNotSupportPthreadCancel {};

	class UnsupportedFeature {};

	class FailedToCreateThread {};
	
	class Condition;
	
	class Mutex
	{
	private:
#ifdef DEBUG_JTHREAD_LOCKS
		WhosLock wl;
#endif
		pthread_mutexattr_t attr;

		friend class Condition;
		pthread_mutex_t mutex;

	public:
#ifdef SIMPLE_JTHREAD_DEBUG
		pthread_t locked_by;
		int lock_count;
#endif
		
		Mutex(bool allow_recursion = false);
		~Mutex();

		void lock() const;
		void unlock() const;
		bool trylock() const;
	};

	class Synchronizer : public Mutex
	{
	public:				
		Synchronizer(bool allow_recursion = false);
		~Synchronizer();
		void enter() const;
		void leave() const;
	};

	class Monitor;
	class Monitor : public Synchronizer {
	public:
		Monitor(bool allow_recursion = false);
		class MonitorGuard {
		private:
			const Mutex *monitor;
		public:
			MonitorGuard(const Mutex *m);
			~MonitorGuard();
		};
	};

	/** singleton
	 * The singleton construct is a class from which only one object
	 * is created. A reference to this unique object can be created
	 * by calling the static instance() function which all Singleton
	 * classes should implement.
	 */
	class Singleton {
	public:
	};
	
	class Condition
	{
	private:
		pthread_condattr_t attr;
		pthread_cond_t cond;
	public:
		Condition();
		~Condition();

		void wait(Mutex *mtx);
		void signal();
		void broadcast();
	};

	class Event {
	public:
		virtual void dummy_event_function() = 0;
	};

	class EventQueue : public Monitor {
	private:
		std::queue<Event *>eq;
		Condition cond;
	public:
		void push_event(Event *);
		Event *wait_for_event();
		Event *poll_for_event(); // returns NULL if no events on queue
	};

	class Exception : public jException {
	public:
		Exception(std::string m, Type type);
	};
	
	jThread(const std::string &identity);
	jThread(const std::string &identity, bool joinable);
	virtual ~jThread();

	void set_rr_state();

	const char *get_signum();
	void run();
	virtual void thread_body() = 0;

	static void cancel(jThread *t);
	static void join(jThread *t);
	static void detach(jThread *t);
	
	static void set_cancel_type(jCancelType);

	static void test_cancel();
/*	
	static void cancel_handler_push(jCancelObject *
#ifdef _USES_UGLY_LINUX_PTHREADS
					, struct _pthread_cleanup_buffer *
#endif
#ifdef _USES_UGLY_WIN_PTHREADS
					, struct ptw32_cleanup_t *
#endif
		);
	static void cancel_handler_pop(bool
#ifdef _USES_UGLY_LINUX_PTHREADS				       
				       , struct _pthread_cleanup_buffer *
#endif
				       );
*/
};

#endif


