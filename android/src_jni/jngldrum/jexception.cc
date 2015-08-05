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

jException::jException(std::string m, Type t) {
	message = m;
	type = t;
}

jException::~jException() {
}

const char* jException::what() const noexcept {
	return message.c_str();
}
