#
# libkamoflage
# Copyright (C) 2015 by Anton Persson
#
# This program is free software; you can redistribute it and/or modify it under the terms of
# the GNU General Public License version 2; see COPYING for the complete License.
#
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
# without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with this program;
# if not, write to the
# Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := libsvgandroid
LOCAL_SRC_FILES := ../libsvgandroid/export/armeabi/libsvgandroid.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)

LOCAL_CPP_EXTENSION := .cc

LOCAL_MODULE    := kamoflage
LOCAL_CFLAGS += \
-DCONFIG_DIR=\"/\" \
-DHAVE_CONFIG_H \
-Ijni/libkamoflage/ \
-I../libsvgandroid/src_jni/ \
-I../libsvgandroid/src_jni/libsvg \
-I../libsvgandroid/prereqs/include/ \
-Wall

LOCAL_CPPFLAGS += -std=c++11

# libkamoflage stuff
LIBKAMOFLAGE_SOURCES = \
libkamoflage/kamogui.cc libkamoflage/kamogui.hh \
libkamoflage/kamogui_svg_canvas.cc \
libkamoflage/kamogui_sensorevent.cc \
libkamoflage/kamo_xml.cc libkamoflage/kamo_xml.hh \
libkamoflage/kamogui_scale_detector.cc libkamoflage/kamogui_scale_detector.hh \
libkamoflage/kamogui_fling_detector.cc libkamoflage/kamogui_fling_detector.hh

# jngldrum stuff
JNGLDRUM_SOURCES = \
jngldrum/jexception.cc jngldrum/jexception.hh \
jngldrum/jthread.cc jngldrum/jthread.hh \
jngldrum/jinformer.cc jngldrum/jinformer.hh

# kamoflage demo app
LOCAL_LDLIBS += -ldl -llog
LOCAL_SHARED_LIBRARIES := libsvgandroid
LOCAL_SRC_FILES := $(LIBKAMOFLAGE_SOURCES) $(JNGLDRUM_SOURCES)

include $(BUILD_SHARED_LIBRARY)
