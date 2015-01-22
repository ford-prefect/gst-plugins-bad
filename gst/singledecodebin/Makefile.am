plugin_LTLIBRARIES = libgstsingledecodebin.la

libgstsingledecodebin_la_SOURCES = gstsingledecodebin.c

libgstsingledecodebin_la_CFLAGS = $(GST_CFLAGS)
libgstsingledecodebin_la_LIBADD = $(GST_LIBS)
libgstsingledecodebin_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstsingledecodebin_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

noinst_HEADERS = gstsingledecodebin.h

Android.mk: Makefile.am $(BUILT_SOURCES)
	androgenizer \
	-:PROJECT libgstsingledecodebin -:SHARED libgstsingledecodebin \
	 -:TAGS eng debug \
         -:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
	 -:SOURCES $(libgstsingledecodebin_la_SOURCES) \
	 -:CFLAGS $(DEFS) $(DEFAULT_INCLUDES) \
	          $(libgstsingledecodebin_la_CFLAGS) \
	 -:LDFLAGS $(libgstsingledecodebin_la_LDFLAGS) \
	           $(libgstsingledecodebin_la_LIBADD) \
	           -ldl \
	 -:PASSTHROUGH LOCAL_ARM_MODE:=arm \
		       LOCAL_MODULE_PATH:='$$(TARGET_OUT)/lib/gstreamer-0.10' \
	> $@
