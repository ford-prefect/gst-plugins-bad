plugin_LTLIBRARIES = libgstpeerflix.la

libgstpeerflix_la_SOURCES =		\
	peerflixsrc.c			\
	gstpeerflixplugin.c

libgstpeerflix_la_CFLAGS = 		\
	$(GST_PLUGINS_BAD_CFLAGS) 	\
	$(GST_PLUGINS_BASE_CFLAGS) 	\
	$(GST_CFLAGS)
libgstpeerflix_la_LIBADD = 		\
	$(GST_PLUGINS_BASE_LIBS) 	\
	-lgstpbutils-$(GST_API_VERSION) \
	$(GST_LIBS)
libgstpeerflix_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS) -no-undefined
libgstpeerflix_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

# headers we need but don't want installed
noinst_HEADERS = 			\
	peerflixsrc.h
