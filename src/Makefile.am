# Makefile.am - src directory
# Copyright (C) 2018 Michael L. Gran

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

guileextension_LTLIBRARIES = libguile-gi.la

libguile_gi_la_SOURCES = \
 gi_callable_info.c \
 gi_callable_info.h \
 gig_argument.c \
 gig_argument.h \
 gig_data_type.c \
 gig_data_type.h \
 gig_object.c \
 gig_object.h \
 gig_value.c \
 gig_value.h \
 gig_signal.c \
 gig_signal.h \
 gi_type_tag.c \
 gi_type_tag.h \
 gig.c \
 gig_arg_map.c \
 gig_arg_map.h \
 gig_callback.c \
 gig_callback.h \
 gig_function.c \
 gig_function.h \
 gig_function_private.h \
 gig_constant.c \
 gig_constant.h \
 gig_flag.c \
 gig_flag.h \
 gig_repository.c \
 gig_repository.h \
 gig_document.c \
 gig_type.c \
 gig_type.h \
 gig_type_private.c \
 gig_type_private.h \
 gig_util.c \
 gig_util.h

libguile_gi_la_CPPFLAGS = -DG_LOG_DOMAIN=\"GuileGI\"
if BUILDING_DLL
libguile_gi_la_CPPFLAGS += -DGIR_DLL -DGIR_DLL_EXPORTS
endif
if COVERAGE
libguile_gi_la_CPPFLAGS += -DENABLE_GCOV --coverage
endif
if MTRACE
libguile_gi_la_CPPFLAGS += -DMTRACE
endif

libguile_gi_la_CFLAGS = \
  -std=c11 \
  $(GUILE_CFLAGS) \
  $(GLIB_CFLAGS) \
  $(GOBJECT_CFLAGS) \
  $(GOBJECT_INTROSPECTION_CFLAGS) \
  $(FFI_CFLAGS)

libguile_gi_la_LDFLAGS = \
 -no-undefined \
 -version-info $(LIBGUILE_GI_INTERFACE) \
 -export-symbols-regex '^gig_' \
 $(GUILE_LDFLAGS)

if !DLL_VERSION_INFO
libguile_gi_la_LDFLAGS += -avoid-version
endif

if COVERAGE
libguile_gi_la_LDFLAGS += --coverage -Wl,--dynamic-list-data
endif

libguile_gi_la_LIBADD = \
 $(GUILE_LIBS) \
 $(GOBJECT_INTROSPECTION_LIBS) \
 $(GLIB_LIBS) \
 $(GOBJECT_LIBS) \
 $(FFI_LIBS)

################################################################

pkgguilesitedir=$(guilesitedir)/gi
pkgguileobjectdir=$(guileobjectdir)/gi

dist_guilesite_DATA = \
  gi.scm

nodist_pkgguilesite_DATA = \
  gi/config.scm

dist_pkgguilesite_DATA = \
  gi/core-generics.scm \
  gi/documentation.scm \
  gi/oop.scm \
  gi/repository.scm \
  gi/types.scm \
  gi/util.scm

pkgguileobject_DATA = \
  $(dist_guilesite_DATA:%.scm=%.go) \
  $(dist_pkgguilesite_DATA:%.scm=%.go) \
  $(nodist_pkgguilesite_DATA:%.scm=%.go)

$(pkgguileobject_DATA): libguile-gi.la

GUILEC_FLAGS = -Warity-mismatch -Wformat --load-path=$(abs_srcdir) --load-path=$(abs_builddir)

.scm.go:
	GUILE_AUTO_COMPILE=0 \
	LTDL_LIBRARY_PATH=$(abs_builddir)/.libs \
	$(GUILE_TOOLS) compile --target="$(host)" $(GUILEC_FLAGS) \
	-o "$@" "$<"

CLEANFILES = $(pkgguileobject_DATA) $(BUILT_SOURCES)

################################################################
# FLYMAKE

.PHONY: check-syntax
check-syntax:
	$(CC) -std=c11 $(GUILE_CFLAGS) $(GLIB_CFLAGS) $(GOBJECT_CFLAGS) \
	 $(GOBJECT_INTROSPECTION_CFLAGS) $(FFI_CFLAGS) \
	 -DG_LOG_DOMAIN=\"GuileGI\" $(CFLAGS) -fsyntax-only $(CHK_SOURCES)

indent:
	VERSION_CONTROL=none indent $(libguile_gi_la_SOURCES)
