/*
 * Jngldrum, ANDROID version
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

#ifdef __DO_JNGLDRUM_DEBUG

#ifdef ANDROID

#include <android/log.h>
#define JNGLDRUM_DEBUG_(...)       __android_log_print(ANDROID_LOG_INFO, "SATAN|NDK", __VA_ARGS__)

#else

#include <stdio.h>
#define JNGLDRUM_DEBUG_(...)       printf(__VA_ARGS__)

#endif

//#define JNGLDRUM_DEBUG(_fmt, ...)  JNGLDRUM_DEBUG_(WHERESTR _fmt, WHEREARG, __VA_ARGS__)
#define JNGLDRUM_DEBUG(_fmt, ...)  JNGLDRUM_DEBUG_(_fmt, __VA_ARGS__)

#else
// disable debugging

#define JNGLDRUM_DEBUG_(...)
#define JNGLDRUM_DEBUG(_fmt, ...)

#endif

#ifdef ANDROID

#include <android/log.h>
#define JNGLDRUM_INFORM_(...)       __android_log_print(ANDROID_LOG_INFO, "SATAN|NDK", __VA_ARGS__)
#define JNGLDRUM_INFORM(...)       __android_log_print(ANDROID_LOG_INFO, "SATAN|NDK", __VA_ARGS__)

#else

#include <stdio.h>
#define JNGLDRUM_INFORM_(...)       printf(__VA_ARGS__)
#define JNGLDRUM_INFORM(...)       printf(__VA_ARGS__)

#endif
