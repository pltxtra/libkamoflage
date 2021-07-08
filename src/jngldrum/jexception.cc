/*
 * jngldrum - a peer2peer networking library
 *
 * Copyright (C) 2006 by Anton Persson & Ted Björling
 *
 * Originally from YAFFA, moved to jngldrum.
 * YAFFA, Yet Another F****** FTP Application
 * Copyright (C) 2004 by Anton Persson
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

#include <iostream>
#include "jngldrum/jexception.hh"

#include <cxxabi.h>
#include <unwind.h>
#include <dlfcn.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <functional>

namespace {

struct BacktraceState {
    void** current;
    void** end;
};

static _Unwind_Reason_Code unwind_callback(struct _Unwind_Context* context, void* arg) {
    BacktraceState* state = static_cast<BacktraceState*>(arg);
    uintptr_t pc = _Unwind_GetIP(context);
    if (pc) {
        if (state->current == state->end) {
            return _URC_END_OF_STACK;
        } else {
            *state->current++ = reinterpret_cast<void*>(pc);
        }
    }
    return _URC_NO_REASON;
}

}

std::mutex jException::mutex;
std::atomic_bool jException::callback_enabled;
std::function<void(const std::string& backtrace)> jException::backtrace_callback;

static size_t capture_backtrace(void** buffer, size_t max) {
    BacktraceState state = {buffer, buffer + max};
    _Unwind_Backtrace(unwind_callback, &state);

    return state.current - buffer;
}

void jException::dump_backtrace(std::ostream& os, size_t max_depth) {
	void* buffer[max_depth];
	size_t count = capture_backtrace(buffer, max_depth);

	for (size_t idx = 0; idx < count; ++idx) {
		const void* addr = buffer[idx];
		const char* symbol = "";

		Dl_info info;
		if (dladdr(addr, &info) && info.dli_sname) {
			symbol = info.dli_sname;
		}

		int status = 0;
		char * buff = __cxxabiv1::__cxa_demangle(
			symbol,
			NULL, NULL, &status);

		os << "  #" << std::setw(2) << idx << ": " << addr << "  " << buff << "\n";
	}
}

std::string jException::dump_backtrace(size_t max_depth) {
	std::ostringstream oss;
	dump_backtrace(oss, 30);

	return oss.str();
}

void jException::enable_backtrace_callback(
	std::function<void(const std::string& backtrace)> callback) {
	std::lock_guard<std::mutex> lock_guard(mutex);
	backtrace_callback = callback;
	if(callback) callback_enabled = true;
	else callback_enabled = false;
}

jException::jException(std::string m, Type t) {
	message = m;
	type = t;

	if(callback_enabled) {
		std::lock_guard<std::mutex> lock_guard(mutex);
		if(backtrace_callback) {
			std::ostringstream oss;
			dump_backtrace(oss, 30);

			backtrace_callback(oss.str());
		}
	}
}

jException::~jException() {
}

const char* jException::what() const noexcept {
	return message.c_str();
}
