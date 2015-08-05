/*
 * Kamoflage, ANDROID version
 * Copyright (C) 2007 by Anton Persson & Ted Björling
 * Copyright (C) 2010 by Anton Persson
 * Copyright (C) 2011 by Anton Persson
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

#define WHERESTR  "[file %s, line %d]: "
#define WHEREARG  __FILE__, __LINE__

#ifdef __DO_KAMOFLAGE_DEBUG

#ifdef ANDROID

#include <android/log.h>
#define KAMOFLAGE_DEBUG_(...)       __android_log_print(ANDROID_LOG_INFO, "KAMOFLAGE_NDK", __VA_ARGS__)
#define KAMOFLAGE_DEBUG(...)       __android_log_print(ANDROID_LOG_INFO, "KAMOFLAGE_NDK", __VA_ARGS__)

#else

#include <stdio.h>
#define KAMOFLAGE_DEBUG_(...)       printf(__VA_ARGS__)
#define KAMOFLAGE_DEBUG(...)  printf(__VA_ARGS__)

#endif

#else
// disable debugging

#define KAMOFLAGE_DEBUG_(...)
#define KAMOFLAGE_DEBUG(_fmt, ...)

#endif

#ifdef ANDROID

#include <android/log.h>
#define KAMOFLAGE_ERROR(...)       __android_log_print(ANDROID_LOG_INFO, "KAMOFLAGE_NDK", __VA_ARGS__)

#else

#include <stdio.h>
#define KAMOFLAGE_ERROR(...)  printf(__VA_ARGS__)

#endif
