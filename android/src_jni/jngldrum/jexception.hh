/*
 * jngldrum - a peer2peer networking library
 *
 * Copyright (C) 2006 by Anton Persson & Ted Bj�rling
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

#ifndef __JEXCEPTION
#define __JEXCEPTION

class jException : std::exception {
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
};

#define THROW_EXCEPTION(a,b) throw new jException(a,YExcpetion::b)

#endif
