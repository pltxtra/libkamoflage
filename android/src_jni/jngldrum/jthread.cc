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

#include <stdlib.h>
#include <sys/types.h>
#include <iostream>
#include <sstream>

#include <cassert>
#include <memory>
#include <stdexcept>
#include <iterator>
#include <set>
#include <vector>

#include <pthread.h>
#include <sys/prctl.h>

#include <errno.h>

#include <jni.h>

//#define DEBUG_JTHREAD_LOCKS

#include "jinformer.hh"
#include "jthread.hh"

#ifdef ANDROID
JNIEnv *get_env_for_thread();
#endif

//#define __DO_JNGLDRUM_DEBUG
#include "jdebug.hh"

jThread::Exception::Exception(std::string m, Type type) : jException(m, type) {
	/* nada */
}

#ifdef DEBUG_JTHREAD_LOCKS
jThread::WhosLock *__whoslock[] = {
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
/***/
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
/***/
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
/***/
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,

	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

jThread::WhosLock::WhosLock(jThread::Mutex *_mtx) : mtx(_mtx), owner(0), count(0) {
}

#define MAX_WHOSLOCK 400
#define TUPPLE_HASH(A) ((((uint32_t)A % 941) + 23 * (((uint32_t)A % 7) + 1)) % MAX_WHOSLOCK)

void whoslock_insert(const jThread::Mutex *mtx, jThread::WhosLock *wl) {
	uint32_t p = (uint32_t)mtx;
	int max = 7;

retry:
	if(__whoslock[TUPPLE_HASH(p)] && __whoslock[TUPPLE_HASH(p)] != wl) {
//		JNGLDRUM_DEBUG_("    -- OCCUPIED (%p) at position %d, will retry (p %d)\n", mtx, TUPPLE_HASH(p), p);
		p++;
		max--;
		if(!max) {
			JNGLDRUM_DEBUG_("MAXUR COLISSIONE\n"); exit(0);
		}
		goto retry;
	}

	JNGLDRUM_DEBUG(" insert %p at %d (tid %d, p %d)\n", wl->mtx, TUPPLE_HASH(p), gettid(), p);
	__whoslock[TUPPLE_HASH(p)] = wl;
}

jThread::WhosLock *whoslock_get(const jThread::Mutex *mtx) {
	uint32_t p = (uint32_t)mtx;
	int max = 7;

retry:
	if(__whoslock[TUPPLE_HASH(p)] && __whoslock[TUPPLE_HASH(p)]->mtx != mtx) {
		JNGLDRUM_DEBUG_("    -- NOT FOUND (%p) at position %d, might retry (p %d)\n", mtx, TUPPLE_HASH(p), p);
		p++;
		max--;
		if(!max) {
			JNGLDRUM_DEBUG_("NOT FOUND (%p) (tid %d) permanent failure\n", mtx, gettid());
			int *x = NULL;
			*x = 20;
		}
		goto retry;
	}
	JNGLDRUM_DEBUG_("  whoslock_get returning...\n");
	return __whoslock[TUPPLE_HASH(p)];
}
#endif

#ifdef SIMPLE_JTHREAD_DEBUG
jThread::Monitor *SIMPLE_JTHREAD_DEBUG_MUTX = NULL;
#endif

jThread::Mutex::Mutex(bool allow_recursion)
#ifdef DEBUG_JTHREAD_LOCKS
	: wl(this), locked_by(42), lock_count(0)
#endif
{
#ifdef DEBUG_JTHREAD_LOCKS
	{
		whoslock_insert(this, &wl);
	}
#endif

	pthread_mutexattr_init(&attr);
	if(allow_recursion) {
		// ignore error...
		(void) pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
	}

	pthread_mutex_init(&mutex, &attr);
}

jThread::Mutex::~Mutex() {
	pthread_mutex_destroy(&mutex);
	pthread_mutexattr_destroy(&attr);
}


void jThread::Mutex::lock() const {
#ifdef SIMPLE_JTHREAD_DEBUG
	if(this == SIMPLE_JTHREAD_DEBUG_MUTX) {
		JNGLDRUM_DEBUG_("Entering lock - data.. %p, %d (crnt: %d)\n", this, gettid(), locked_by);
	}
#endif
	switch(pthread_mutex_lock((pthread_mutex_t*)&mutex)) {
	case EBUSY:
		// should not be reached..
		break;
	case EDEADLK:
		// Deadlock!
		throw jException("Locking a mutex twice in the same thread would cause a deadlock!", jException::sanity_error);
	}

#ifdef SIMPLE_JTHREAD_DEBUG
//	if(this == SIMPLE_JTHREAD_DEBUG_MUTX) {
	void *ugly = (void *)this;
	Mutex *more_ugly = (Mutex *)ugly;
	more_ugly->locked_by = pthread_self();
	more_ugly->lock_count++;
//	}
#endif

#ifdef DEBUG_JTHREAD_LOCKS
	{

		pid_t tid = gettid();
		WhosLock *wl = whoslock_get(this);
		if(wl == NULL) { JNGLDRUM_DEBUG_("NO WHOSLOCK A\n"); exit(0); }

		if(wl->owner != 0 && wl->owner != tid) {
			JNGLDRUM_DEBUG("whoslock (at lock) not owned by tid: %d (%p) (owner: %d, count: %d)\n",
				       tid, this, wl->owner, wl->count);
			int *x = NULL;
			*x = 20;
		}

		wl->owner = tid;
		wl->count++;

//		JNGLDRUM_DEBUG("whoslock LOCKED BY: %d (%p) (count: %d)\n", tid, this, wl->count);
	}
#endif
#ifdef SIMPLE_JTHREAD_DEBUG
	if(this == SIMPLE_JTHREAD_DEBUG_MUTX) {
		JNGLDRUM_DEBUG_("leaving lock..\n");
	}
#endif
}

bool jThread::Mutex::trylock() const {
	int retval = 0;
	retval = pthread_mutex_trylock((pthread_mutex_t*)&mutex);
	if(retval != 0)
		return false;
	return true; // we should perhaps check for the other errors too.
}

void jThread::Mutex::unlock() const {
#ifdef SIMPLE_JTHREAD_DEBUG
	if(this == SIMPLE_JTHREAD_DEBUG_MUTX) {
		JNGLDRUM_DEBUG_("Entering unlock - data.. %p, %d (crnt: %d)\n", this, gettid(), locked_by);
		void *ugly = (void *)this;
		Mutex *more_ugly = (Mutex *)ugly;
		more_ugly->lock_count--;
	}
#endif
#ifdef DEBUG_JTHREAD_LOCKS
	{
		pid_t tid = gettid();
		WhosLock *wl = whoslock_get(this);
		if(wl == NULL) { JNGLDRUM_DEBUG_("NO WHOSLOCK B\n"); exit(0); }

		if(wl->mtx != this) {
			JNGLDRUM_DEBUG("whoslock unlocking wrong mtx tid: %d (%p vs %p)\n", tid, this, wl->mtx);
			int *x = NULL;
			*x = 20;
		}
		if(wl->owner != tid) {
			JNGLDRUM_DEBUG("whoslock not owned by tid: %d (%p) (owner: %d)\n", tid, this, wl->owner);
			int *x = NULL;
			*x = 20;
		}
		wl->count--;
//		JNGLDRUM_DEBUG("whoslock RELEASED BY: %d (%p) (count: %d)\n", tid, this, wl->count);

		if(wl->count == 0)
			wl->owner = 0;

		if(wl->count < 0) {
			JNGLDRUM_DEBUG("whoslock freed too many times by tid: %d (%p)\n", tid, this);
			int *x = NULL;
			*x = 20;
		}
	}
#endif
	pthread_mutex_unlock((pthread_mutex_t*)&mutex);
}

jThread::Condition::Condition() {
#ifndef _HAVE_PTHREAD_CONDATTR_INIT
	throw DoesNotSupportPthreadConditions();
#else
	pthread_condattr_init(&attr);
	pthread_cond_init(&cond, &attr);
#endif
}

jThread::Condition::~Condition() {
#ifndef _HAVE_PTHREAD_CONDATTR_INIT
	throw DoesNotSupportPthreadConditions();
#else
	pthread_cond_destroy(&cond);
	pthread_condattr_destroy(&attr);
#endif
}

void jThread::Condition::wait(Mutex *mtx) {
#ifdef DEBUG_JTHREAD_LOCKS
	{
		pid_t tid = gettid();
		WhosLock *wl = whoslock_get(mtx);
		if(wl == NULL) { JNGLDRUM_DEBUG_("NO WHOSLOCK (COND) B\n"); exit(0); }

		if(wl->mtx != mtx) {
			JNGLDRUM_DEBUG("whoslock unlocking (COND) wrong mtx tid: %d (%p vs %p)\n", tid, mtx, wl->mtx);
			int *x = NULL;
			*x = 20;
		}
		if(wl->owner != tid) {
			JNGLDRUM_DEBUG("whoslock (COND) not owned by tid: %d (%p) (owner: %d)\n", tid, mtx, wl->owner);
			int *x = NULL;
			*x = 20;
		}
		wl->count--;
//		JNGLDRUM_DEBUG("whoslock RELEASED BY: %d (%p) (count: %d)\n", tid, this, wl->count);

		if(wl->count == 0)
			wl->owner = 0;

		if(wl->count < 0) {
			JNGLDRUM_DEBUG("whoslock (COND) freed too many times by tid: %d (%p)\n", tid, mtx);
			int *x = NULL;
			*x = 20;
		}
	}
#endif
	pthread_cond_wait(&cond, &(mtx->mutex));
#ifdef DEBUG_JTHREAD_LOCKS
	{
		pid_t tid = gettid();
		WhosLock *wl = whoslock_get(mtx);
		if(wl == NULL) { JNGLDRUM_DEBUG_("NO WHOSLOCK A\n"); exit(0); }

		if(wl->owner != 0 && wl->owner != tid) {
			JNGLDRUM_DEBUG("whoslock (at lock) not owned by tid: %d (%p) (owner: %d, count: %d)\n",
				       tid, mtx, wl->owner, wl->count);
			int *x = NULL;
			*x = 20;
		}

		wl->owner = tid;
		wl->count++;
//		JNGLDRUM_DEBUG("whoslock LOCKED BY: %d (%p) (count: %d)\n", tid, this, wl->count);
	}
#endif
}

void jThread::Condition::signal() {
	pthread_cond_signal(&cond);
}

void jThread::Condition::broadcast() {
	pthread_cond_broadcast(&cond);
}

void jThread::EventQueue::push_event(jThread::Event *evn) {
	enter();

	eq.push(evn);
	cond.signal();

	leave();
}

jThread::Event *jThread::EventQueue::wait_for_event() {
	Event *retval = NULL;

	enter();

	if(eq.size() > 0)
		goto skip_wait;

	cond.wait(this);
 skip_wait:

	retval = eq.front();
	eq.pop();

	leave();

	return retval;
}

jThread::Event *jThread::EventQueue::poll_for_event() {
	Event *retval = NULL;

	enter();

	if(eq.size() > 0 ) {
		retval = eq.front();
		eq.pop();
	}

	leave();

	return retval;
}

jThread::Synchronizer::Synchronizer(bool allow_recursion) : Mutex(allow_recursion) {
}

jThread::Synchronizer::~Synchronizer() {
}

void jThread::Synchronizer::enter() const {
	lock();
}

void jThread::Synchronizer::leave() const {
	unlock();
}

jThread::Monitor::Monitor(bool allow_recursion) : Synchronizer(allow_recursion) {
}

jThread::Monitor::MonitorGuard::MonitorGuard(const jThread::Mutex *m) {
	monitor = m;
	monitor->lock();
}

jThread::Monitor::MonitorGuard::~MonitorGuard() {
	monitor->unlock();
}

void *jThread_internalThreadMain(void *throbj) {
	jThread *thread = (jThread *)throbj;
	prctl(PR_SET_NAME, thread->get_signum());
	try {
		JNGLDRUM_DEBUG_("jThread_internalThreadMain, calling body %p...", thread);
		thread->thread_body();
		JNGLDRUM_DEBUG_("jThread_internalThreadMain, leaving body %p...", thread);
	} catch(jException e) {
		std::ostringstream stream;
		stream << "Caught an unhandled exception, aborting. [" << e.message << "]\n";
		jInformer::inform(stream.str());
	} catch(jException *e) {
		std::ostringstream stream;
		stream << "Caught an unhandled exception, aborting. [" << e->message << "]\n";
		delete e;
		jInformer::inform(stream.str());
	} catch(...) {
		std::ostringstream stream;
		stream << "Caught an unhandled exception, this thread is aborting.\n";
		jInformer::inform(stream.str());
	}
	return 0;
}

const char* jThread::get_signum() {
	return __signum.c_str();
}

#ifdef ANDROID
#define MAX_ANDROID_THREADS 64
jThread **__android_thread = NULL;
jThread::Mutex jthread_mutex;

void jThread::java_thread_entry(int identity_hash) {
	jThread *t = NULL;

	JNGLDRUM_DEBUG_("java_thread_entry, before lock (hash = %d).", identity_hash);
	jthread_mutex.lock();
	JNGLDRUM_DEBUG_("java_thread_entry, in lock.");
	t = __android_thread[identity_hash];
	JNGLDRUM_DEBUG_("java_thread_entry, leaving lock.");
	jthread_mutex.unlock();
	JNGLDRUM_DEBUG_("java_thread_entry, after lock.");

	if(t != NULL) {
		JNGLDRUM_DEBUG_("java_thread_entry, calling internalThreadMain (signum for tid %d: %s)...",
				gettid(),
				t->__signum.c_str());
		jThread_internalThreadMain(t);
		JNGLDRUM_DEBUG_("java_thread_entry, left internalThreadMain...");
	}
}

void jThread::constructor_helper() {
	int x;

	__my_android_hash = -1;

	jthread_mutex.lock();

	if(__android_thread == NULL) {
		__android_thread = (jThread **)malloc(sizeof(jThread *) * MAX_ANDROID_THREADS);
		if(__android_thread != NULL)
			memset(__android_thread, 0, sizeof(jThread *) * MAX_ANDROID_THREADS);
	}

	if(__android_thread != NULL) {
		for(x = 0; x < MAX_ANDROID_THREADS; x++) {
			if(__android_thread[x] == NULL) {
				__android_thread[x] = this;
				__my_android_hash = x;
				JNGLDRUM_DEBUG_("jThread::constructor_helper, %p, hash %d", this, x);
				break;
			}
		}
	}
	jthread_mutex.unlock();

	if(__my_android_hash == -1) throw FailedToCreateThread();
}

#endif

jThread::jThread() : __signum("NO NAME THREAD") {
}

jThread::jThread(const std::string &identity) {
	JNGLDRUM_DEBUG("Creating jThread for %s\n", identity.c_str());
	__signum = identity;

#ifndef ANDROID
	pthread_attr_init(&pta);
#else
	constructor_helper();
#endif
}

jThread::jThread(const std::string &identity, bool joinable) {
	JNGLDRUM_DEBUG("Creating joinable jThread for %s\n", identity.c_str());
	__signum = identity;
#ifndef ANDROID
	if(joinable) {
		pthread_attr_setdetachstate(&pta, PTHREAD_CREATE_JOINABLE);
	}
#else
	constructor_helper();
#endif
}

jThread::~jThread() {
	//should deallocate pthread?
	//pthread_exit(NULL);
#ifdef ANDROID
	if(__my_android_hash != -1) {
		jthread_mutex.lock();
		if(__android_thread[__my_android_hash] == this) {
			__android_thread[__my_android_hash] = NULL;
		}
		jthread_mutex.unlock();
	}
#endif
}

void jThread::set_rr_state() {
#ifdef ANDROID
	throw UnsupportedFeature();
#else
	pthread_attr_setschedpolicy(&pta, SCHED_RR);
#endif
}

void jThread::run() {
#ifdef ANDROID
	JNIEnv *env = get_env_for_thread();

	jclass jc = env->FindClass("com/toolkits/kamoflage/Kamoflage");
	jmethodID mid = env->GetStaticMethodID(jc, "runAdditionalThread", "(I)V");

	env->CallStaticVoidMethod(jc, mid, __my_android_hash);

#else
	switch(pthread_create(&thread, &pta,
			      jThread_internalThreadMain,
			      (void *)this)) {
	case 0:
		return;
	default:
		throw Exception("Failed to create new thread!", jException::syscall_error);
	}
#endif
}


void jThread::cancel(jThread *t) {
#ifndef _HAVE_PTHREAD_CONDATTR_INIT
	throw DoesNotSupportPthreadConditions();
#else
#ifdef _HAVE_PTHREAD_CANCEL
	pthread_cancel(t->thread);
#else
	throw DoesNotSupportPthreadCancel();
#endif
#endif
}

void jThread::join(jThread *t) {
#ifndef ANDROID
	pthread_join(t->thread, NULL);
#else
	throw UnsupportedFeature();
#endif
}

void jThread::detach(jThread *t) {
#ifndef ANDROID
	pthread_detach(t->thread);
#else
	throw UnsupportedFeature();
#endif
}

void jThread_cancel_helper(void *data) {
	jCancelObject *obj = (jCancelObject *)data;
	obj->cancel();
}
/*
void jThread::cancel_handler_pop(bool e
#ifdef _USES_UGLY_LINUX_PTHREADS
				  , struct _pthread_cleanup_buffer *_buffer
#endif
	) {
#ifdef _USES_UGLY_LINUX_PTHREADS
	_pthread_cleanup_pop(
		_buffer, e ? 1 : 0);
#else
#ifdef _USES_UGLY_WIN_PTHREADS
	ptw32_pop_cleanup(e ? 1 : 0);
#else
#ifdef _USES_UGLY_CYGWIN_PTHREADS
	_pthread_cleanup_pop(e ? 1 : 0);
#else
	pthread_cleanup_pop(e ? 1 : 0);
#endif
#endif
#endif

}

void jThread::cancel_handler_push(jCancelObject *o
#ifdef _USES_UGLY_LINUX_PTHREADS
				   , struct _pthread_cleanup_buffer *_buffer
#endif
#ifdef _USES_UGLY_WIN_PTHREADS
				   , struct ptw32_cleanup_t *_cleanup
#endif
	) {
#ifdef _USES_UGLY_LINUX_PTHREADS
	_pthread_cleanup_push(_buffer, jThread_cancel_helper, o);

#else
#ifdef _USES_UGLY_WIN_PTHREADS
	ptw32_push_cleanup(_cleanup, (ptw32_cleanup_callback_t)jThread_cancel_helper, o);
#else
#ifdef _USES_UGLY_CYGWIN_PTHREADS
	__pthread_cleanup_handler __cleanup_handler = { jThread_cancel_helper, o, NULL };
	_pthread_cleanup_push( &__cleanup_handler );
#else
	pthread_cleanup_push(jThread_cancel_helper, o);
#endif
#endif
#endif
}
*/
void jThread::test_cancel() {
#ifndef _HAVE_PTHREAD_CONDATTR_INIT
	throw DoesNotSupportPthreadConditions();
#else
#ifdef _HAVE_PTHREAD_CANCEL
	pthread_testcancel();
#else
	throw DoesNotSupportPthreadCancel();
#endif
#endif
}

void jThread::set_cancel_type(jCancelType typ) {
#ifndef _HAVE_PTHREAD_CONDATTR_INIT
	throw DoesNotSupportPthreadConditions();
#else
#ifdef _HAVE_PTHREAD_CANCEL
	int oldtype, newtype, rc;
	switch(typ) {
	case async_cancel:
		newtype = PTHREAD_CANCEL_ASYNCHRONOUS;
		break;
	default:
	case normal_cancel:
		newtype = PTHREAD_CANCEL_DEFERRED;
		break;
	}
	rc = pthread_setcanceltype(newtype, &oldtype);
#else
	throw DoesNotSupportPthreadCancel();
#endif
#endif
}
