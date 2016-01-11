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

/* $Id*/

#include <string>
#include <exception>
#include <atomic>
#include <mutex>

#ifndef __JEXCEPTION
#define __JEXCEPTION

class jException : std::exception {
private:
	static std::mutex mutex;
	static std::atomic_bool callback_enabled;
	static std::function<void(const std::string& backtrace)> backtrace_callback;
public:

	enum Type {
		io_error = 0,
		null_pointer = 1,
		syscall_error = 2,
		sanity_error = 3,
		program_error = 4,

		NO_ERROR
	};

	std::string message;
	Type type;

	/// Create a new exception.
	jException(std::string message, Type type);
	virtual ~jException();

	virtual const char* what() const noexcept;

	static void dump_backtrace(std::ostream& os, size_t max_depth);
	static std::string dump_backtrace(size_t max_depth);
	static void enable_backtrace_callback(
		std::function<void(const std::string& stacktrace)> callback);

};

#define THROW_EXCEPTION(a,b) throw new jException(a,YExcpetion::b)

#endif
