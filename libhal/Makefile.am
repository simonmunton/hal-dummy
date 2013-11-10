## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(localedir)"\" \
	@DBUS_CFLAGS@

lib_LTLIBRARIES=libhal.la

libhal_la_SOURCES =                                       \
	libhal.c \
	libhal.h


libhal_la_LIBADD =  $(INTLLIBS)

libhal_la_LDFLAGS = -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

clean-local :
	rm -f *~
