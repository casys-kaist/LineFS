# This file is part of mutrace.
#
# Copyright 2009 Lennart Poettering
#
# mutrace is free software: you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# mutrace is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with mutrace. If not, see <http://www.gnu.org/licenses/>.

EXTRA_DIST = \
	bootstrap.sh \
	GPL2 \
	LGPL3 \
	GPL3 \
	README \
	mutrace.in \
	matrace.in

#include_HEADERS = \
#	mutrace.h

lib_LTLIBRARIES = \
	libmutrace.la \
	libmatrace.la \
	libmutrace-backtrace-symbols.la

bin_SCRIPTS = \
	mutrace \
	matrace

CLEANFILES = \
	mutrace \
	matrace

libmutrace_la_SOURCES = \
	mutrace.c
libmutrace_la_LDFLAGS = \
	-avoid-version \
	-module \
	-export-dynamic \
	-shared \
	-prefer-pic
libmutrace_la_LIBADD = \
	$(PTHREAD_LIBS) \
	-lrt \
	-ldl
libmutrace_la_CFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DSONAME=\"libmutrace.so\"

libmatrace_la_SOURCES = \
	matrace.c
libmatrace_la_LDFLAGS = \
	-avoid-version \
	-module \
	-export-dynamic \
	-shared \
	-prefer-pic
libmatrace_la_LIBADD = \
	$(PTHREAD_LIBS) \
	-lrt \
	-ldl
libmatrace_la_CFLAGS = \
	$(PTHREAD_CFLAGS) \
	-DSONAME=\"libmatrace.so\"

libmutrace_backtrace_symbols_la_SOURCES = \
	backtrace-symbols.c
libmutrace_backtrace_symbols_la_LDFLAGS = \
	-avoid-version \
	-module \
	-export-dynamic \
	-shared \
	-prefer-pic
libmutrace_backtrace_symbols_la_LIBADD = \
	$(PTHREAD_LIBS) \
	-lrt \
	-ldl
libmutrace_backtrace_symbols_la_CFLAGS = \
	$(PTHREAD_CFLAGS)

mutrace: mutrace.in Makefile
	sed -e 's,@PACKAGE_STRING\@,$(PACKAGE_STRING),g' \
                -e 's,@PACKAGE_NAME\@,$(PACKAGE_NAME),g' < $< > $@
	chmod +x mutrace

matrace: matrace.in Makefile
	sed -e 's,@PACKAGE_STRING\@,$(PACKAGE_STRING),g' \
                -e 's,@PACKAGE_NAME\@,$(PACKAGE_NAME),g' < $< > $@
	chmod +x matrace

install-exec-hook:
	rm -f $(DESTDIR)$(libdir)/libmutrace.la
	rm -f $(DESTDIR)$(libdir)/libmatrace.la
	rm -f $(DESTDIR)$(libdir)/libmutrace-backtrace-symbols.la

ACLOCAL_AMFLAGS = -I m4
