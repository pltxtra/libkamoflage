lib_LTLIBRARIES = libkamoflage.la

libkamoflage_la_CPPFLAGS = -std=c++11 $(gnuVG_CFLAGS) $(libsvgandroid_CFLAGS) -Wall -Werror -Wfatal-errors

libkamoflage_la_SOURCES = \
jngldrum/jdebug.hh \
jngldrum/jexception.cc \
jngldrum/jexception.hh \
jngldrum/jinformer.cc \
jngldrum/jinformer.hh \
jngldrum/jthread.cc \
jngldrum/jthread.hh \
kamoflage/gnuVGcanvas.cc \
kamoflage/gnuVGcanvas.hh \
kamoflage/kamoflage_debug.hh \
kamoflage/kamogui.cc \
kamoflage/kamogui.hh \
kamoflage/kamogui_animation.cc \
kamoflage/kamogui_fling_detector.cc \
kamoflage/kamogui_fling_detector.hh \
kamoflage/kamogui_scale_detector.cc \
kamoflage/kamogui_scale_detector.hh \
kamoflage/kamogui_sensorevent.cc \
kamoflage/kamo_xml.cc \
kamoflage/kamo_xml.hh

libkamoflage_la_LDFLAGS =  -L${prefix}/lib $(gnuVG_LIBS) $(libsvgandroid_LIBS)

nobase_include_HEADERS = \
jngldrum/jdebug.hh \
jngldrum/jexception.hh \
jngldrum/jinformer.hh \
jngldrum/jthread.hh \
kamoflage/gnuVGcanvas.hh \
kamoflage/kamoflage_debug.hh \
kamoflage/kamogui_fling_detector.hh \
kamoflage/kamogui.hh \
kamoflage/kamogui_scale_detector.hh \
kamoflage/kamo_xml.hh
