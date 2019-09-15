# Makefile.am - top level
# Copyright (C) 2018, 2019 Michael L. Gran

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

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AUTOMAKE_OPTIONS = subdir-objects
AM_DISTCHECK_CONFIGURE_FLAGS = --with-gnu-filesystem-hierarchy --enable-introspection

#################
#  Directories  #
#################

cleanpkgdatadir = $(pkgdatadir)
exampledir = $(datadir)/doc/guile-gi
emptywindowdir = $(datadir)/doc/guile-gi/empty-window
builderdir = $(datadir)/doc/guile-gi/builder
resourcesdir = $(datadir)/doc/guile-gi/resources
pkgguilesitedir = $(guilesitedir)/gi
scriptguilesitedir = $(guilesitedir)/scripts
pkgguileobjectdir = $(guileobjectdir)/gi
scriptguileobjectdir = $(guileobjectdir)/scripts

#################
#   Top Level   #
#################

# Build ChangeLog from GIT history
dist: ChangeLog README
ChangeLog:
	$(AM_V_GEN) if test -d $(top_srcdir)/.git; then \
		GIT_DIR="$(top_srcdir)/.git" git log --stat > $@; \
	fi

README: README.md
	$(LN_S) $< $@

#################
# Documentation #
#################

info_TEXINFOS = doc/guile-gi.texi

docs:
	makeinfo $(info_TEXINFOS) --css-ref=docs/document-1.0.1.css --html --output=$(abs_srcdir)/docs

-distclean-docs:
	-rm -rf docs/*.html

#################
#    Examples   #
#################

dist_example_DATA = \
  examples/browser.scm \
  examples/editor.scm

dist_emptywindow_DATA = \
  examples/empty-window/main.scm \
  examples/empty-window/empty-app.scm \
  examples/empty-window/empty-app-window.scm

dist_builder_DATA = \
  examples/builder/main.scm \
  examples/builder/builder.ui

dist_resources_DATA = \
  examples/resources/main.scm \
  examples/resources/builder.ui \
  examples/resources/test.gresource.xml

nodist_resources_DATA = \
  examples/resources/test.gresource

examples/resources/test.gresource: AM_GRESOURCE_FLAGS = --sourcedir=$(abs_srcdir)/examples/resources

.gresource.xml.gresource:
	$(GLIB_COMPILE_RESOURCES) $(AM_GRESOURCE_FLAGS) $(GRESOURCE_FLAGS) --target=$@ $<

#################
#  libguile-gi  #
#################

guileextension_LTLIBRARIES = libguile-gi.la

libguile_gi_la_c_sources = \
  src/gig_argument.c \
  src/gig_closure.c \
  src/gig_data_type.c \
  src/gig_object.c \
  src/gig_value.c \
  src/gig_signal.c \
  src/gig.c \
  src/gig_arg_map.c \
  src/gig_callback.c \
  src/gig_function.c \
  src/gig_constant.c \
  src/gig_flag.c \
  src/gig_repository.c \
  src/gig_document.c \
  src/gig_type.c \
  src/gig_type_private.c \
  src/gig_util.c

libguile_gi_la_internal_headers = \
  src/gig_argument.h \
  src/gig_closure.h \
  src/gig_data_type.h \
  src/gig_object.h \
  src/gig_value.h \
  src/gig_signal.h \
  src/gig_arg_map.h \
  src/gig_callback.h \
  src/gig_function.h \
  src/gig_function_private.h \
  src/gig_constant.h \
  src/gig_flag.h \
  src/gig_repository.h \
  src/gig_type.h \
  src/gig_type_private.h \
  src/gig_util.h

libguile_gi_la_SOURCES = \
  $(libguile_gi_la_internal_headers) \
  $(libguile_gi_la_c_sources)

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

#################
# Guile Modules #
#################

dist_guilesite_DATA = \
  module/gi.scm

nodist_pkgguilesite_DATA = \
  module/gi/config.scm

dist_pkgguilesite_DATA = \
  module/gi/core-generics.scm \
  module/gi/documentation.scm \
  module/gi/oop.scm \
  module/gi/repository.scm \
  module/gi/types.scm \
  module/gi/util.scm

dist_scriptguilesite_DATA = \
  module/scripts/gi-gtkdoc.scm

guileobject_DATA = \
  $(dist_guilesite_DATA:%.scm=%.go)

pkgguileobject_DATA = \
  $(dist_pkgguilesite_DATA:%.scm=%.go) \
  $(nodist_pkgguilesite_DATA:%.scm=%.go)

scriptguileobject_DATA = \
  $(dist_scriptguilesite_DATA:%.scm=%.go)

GUILEC_FLAGS = \
 -O2 \
 -Warity-mismatch \
 -Wformat \
 -Wmacro-use-before-definition \
 -Wunbound-variable \
 --load-path=$(abs_srcdir)/module \
 --load-path=$(abs_builddir)/module

.scm.go:
	GUILE_AUTO_COMPILE=0 \
	LTDL_LIBRARY_PATH=$(abs_builddir)/.libs \
	$(GUILE_TOOLS) compile --target="$(host)" $(GUILEC_FLAGS) \
	-o "$@" "$<"

$(guileobject_DATA) $(pkgguileobject_DATA) $(scriptguileobject_DATA): $(guileextension_LTLIBRARIES)

#################
#     Tests     #
#################

TEST_EXTENSIONS = .scm

TYPELIB_TESTS = \
  test/typelib/document.scm \
  test/typelib/to-module.scm

VERSION_INFORMATION_TESTS = \
  test/version_info/MAJOR_VERSION.scm \
  test/version_info/MINOR_VERSION.scm \
  test/version_info/MICRO_VERSION.scm \
  test/version_info/VERSION_MIN_REQUIRED.scm

BASIC_TYPE_TESTS = \
  test/basic_types/MAXINT16.scm \
  test/basic_types/MAXUINT64.scm \
  test/basic_types/GINT32_FORMAT.scm

STANDARD_MACROS_TESTS = \
  test/std_macros/DIR_SEPARATOR_S.scm \
  test/std_macros/DIR_SEPARATOR.scm \
  test/std_macros/SEARCHPATH_SEPARATOR.scm \
  test/std_macros/SEARCHPATH_SEPARATOR_S.scm

BYTE_ORDER_MACROS_TESTS = \
  test/byte_order/BIG_ENDIAN.scm \
  test/byte_order/LITTLE_ENDIAN.scm

NUMERICAL_DEFINITIONS_TESTS = \
  test/basic_types/IEEE754_FLOAT_BIAS.scm \
  test/basic_types/E.scm \
  test/basic_types/PI.scm

MAIN_EVENT_LOOP_TESTS = \
  test/main_loop/new.scm \
  test/main_loop/runs.scm \
  test/main_loop/quit.scm \
  test/main_loop/get_context.scm \
  test/main_loop/timeout.scm \
  test/main_loop/idle.scm

MEMORY_ALLOCATION_TESTS = \
  test/mem/malloc.scm \
  test/mem/try_malloc0.scm

MEMORY_SLICES_TESTS = \
  test/mem/slice_alloc.scm

IO_CHANNELS_TESTS = \
  test/io/io_channel_pipe.scm \
  test/io/io_channel_new_file.scm \
  test/io/io_channel_unichar.scm

MESSAGE_OUTPUT_TESTS = \
  test/io/log_set_handler.scm \
  test/io/log_writer_is_journald.scm

FILE_TESTS = \
  test/file/new_for_path.scm \
  test/file/get_parent.scm \
  test/file/has_parent.scm \
  test/file/get_child.scm

STRING_UTILITY_TESTS = \
  test/string/strdup.scm \
  test/string/strndup.scm \
  test/string/strnfill.scm \
  test/string/stpcpy.scm \
  test/string/strstr_len.scm \
  test/string/str_has_prefix.scm \
  test/string/str_tokenize_and_fold.scm

CHARACTER_SET_CONVERSION_TESTS = \
  test/convert/ascii.scm \
  test/convert/latin1.scm

UNICODE_MANIPULATION_TESTS = \
  test/unichar/validate_zero.scm \
  test/unichar/isalpha.scm \
  test/unichar/compose.scm \
  test/unichar/combining_class.scm \
  test/misc/unicode_canonical_ordering.scm

BASE64_ENCODING_TESTS = \
  test/misc/base64_encode.scm

DATA_CHECKSUM_TESTS = \
  test/misc/checksum.scm

SECURE_HMAC_DIGESTS_TESTS = \
  test/hmac/string.scm \
  test/hmac/data.scm \
  test/hmac/bytes.scm

DATE_AND_TIME_FUNCTIONS_TESTS = \
  test/datetime/get_monotonic_time.scm \
  test/datetime/get_real_time.scm \
  test/datetime/date_new.scm \
  test/datetime/date_copy.scm \
  test/datetime/date_clear.scm

MEMCHK_TESTS = \
  test/mem/memchk_1.scm \
  test/mem/memchk_2.scm

BYTE_ARRAY_TESTS = \
  test/byte_array/byte_array_new.scm \
  test/byte_array/byte_array_new_take.scm \
  test/byte_array/bytes_new_null_size.scm \
  test/byte_array/bytes_new_null_contents.scm

GAPPLICATION_TESTS = \
  test/gapplication/activate_order.scm \
  test/gapplication/command-line.scm \
  test/gapplication/command-line-options.scm \
  test/gapplication/make.scm \
  test/gapplication/set_resource_path.scm

GTK_ENTRY_TESTS = \
  test/gtk_entry/set_alignment.scm \
  test/gtk_entry/set_alignment_zero.scm

GTK_LAYOUT_TESTS = \
  test/gtk_layout/create_box.scm

GRILO_TESTS = \
  test/grilo/audio_simple.scm \
  test/grilo/audio_multiple_artists.scm \
  test/grilo/init_commandline.scm

OOP_TESTS = \
  test/oop/property-basics.scm \
  test/oop/guile-types.scm \
  test/oop/signal-simple.scm \
  test/oop/signal-detailed.scm \
  test/oop/signal-accumulator.scm \
  test/oop/signal-block.scm

VALUE_TESTS = \
  test/value/getset.scm \
  test/value/transform.scm \
  test/closure/simple.scm

FLAGS_TESTS = \
  test/flags/complement.scm \
  test/flags/difference.scm \
  test/flags/projection.scm \
  test/flags/union.scm

-include $(INTROSPECTION_MAKEFILE)
if HAVE_INTROSPECTION
pkglib_LTLIBRARIES = test/libeverything-1.0.la test/libmarshall-1.0.la

test/everything.c: $(GI_DATA_DIR)/tests/everything.c
	cp $< $@
test/everything.h: $(GI_DATA_DIR)/tests/everything.h
	cp $< $@
test/gitestmacros.h : $(GI_DATA_DIR)/tests/gitestmacros.h
	cp $< $@

GI_MARSHAL_SED = sed \
  -e "s/gimarshallingtests/marshall/" \
  -e "s/gi_marshalling_tests/marshall/g" \
  -e "s/GI_MARSHALLING_TESTS/MARSHALL/g" \
  -e "s/GIMarshallingTests/Marshall/g"

test/marshall.c: $(GI_DATA_DIR)/tests/gimarshallingtests.c
	$(GI_MARSHAL_SED) $< > $@
test/marshall.h: $(GI_DATA_DIR)/tests/gimarshallingtests.h
	$(GI_MARSHAL_SED) $< > $@

BUILT_SOURCES = \
  test/everything.c test/everything.h \
  test/marshall.c test/marshall.h \
  test/gitestmacros.h

INTROSPECTION_GIRS = test/Everything-1.0.gir test/Marshall-1.0.gir
INTROSPECTION_SCANNER_ARGS = \
  -I $(abs_srcdir)/test \
  -I $(abs_builddir)/test \
  --warn-all
INTROSPECTION_COMPILER_ARGS =
nodist_test_libeverything_1_0_la_SOURCES = test/everything.c test/everything.h
test_libeverything_1_0_la_CFLAGS = $(GOBJECT_CFLAGS)
test_libeverything_1_0_la_LDFLAGS =
test_libeverything_1_0_la_LIBADD = $(GOBJECT_LIBS)
if BUILDING_DLL
test_libeverything_1_0_la_LDFLAGS += -no-undefined
endif

nodist_test_libmarshall_1_0_la_SOURCES = \
  test/marshall.c test/marshall.h test/gitestmacros.h
test_libmarshall_1_0_la_CFLAGS = $(GOBJECT_CFLAGS) -I $(abs_builddir)/test
test_libmarshall_1_0_la_LDFLAGS =
test_libmarshall_1_0_la_LIBADD = $(GOBJECT_LIBS)
if BUILDING_DLL
test_libmarshall_1_0_la_LDFLAGS += -no-undefined
endif

test/Everything-1.0.gir: test/libeverything-1.0.la
test_Everything_1_0_gir_LIBS = test/libeverything-1.0.la
test_Everything_1_0_gir_INCLUDES = GObject-2.0
test_Everything_1_0_gir_FILES = test/everything.c test/everything.h

test/Marshall-1.0.gir: test/libmarshall-1.0.la
test_Marshall_1_0_gir_NAMESPACE = Marshall
test_Marshall_1_0_gir_LIBS = test/libmarshall-1.0.la
test_Marshall_1_0_gir_INCLUDES = GObject-2.0
test_Marshall_1_0_gir_FILES = test/marshall.c test/marshall.h

cleanpkgdata_DATA = \
  test/Everything-1.0.gir test/Marshall-1.0.gir \
  test/Everything-1.0.typelib test/Marshall-1.0.typelib

EVERYTHING_TESTS = \
  test/everything/nullfunc.scm \
  test/everything/const-return-gpointer.scm \
  test/everything/const-return-gintptr.scm \
  test/everything/const-return-gboolean.scm \
  test/everything/const-return-gint8.scm \
  test/everything/const-return-guint8.scm \
  test/everything/const-return-gint16.scm \
  test/everything/const-return-guint16.scm \
  test/everything/const-return-gint32.scm \
  test/everything/const-return-guint32.scm \
  test/everything/const-return-gint64.scm \
  test/everything/const-return-guint64.scm \
  test/everything/const-return-gchar.scm \
  test/everything/const-return-gshort.scm \
  test/everything/const-return-gushort.scm \
  test/everything/const-return-gint.scm \
  test/everything/const-return-guint.scm \
  test/everything/const-return-glong.scm \
  test/everything/const-return-gulong.scm \
  test/everything/const-return-gsize.scm \
  test/everything/const-return-gssize.scm \
  test/everything/const-return-gfloat.scm \
  test/everything/const-return-gdouble.scm \
  test/everything/const-return-gunichar.scm \
  test/everything/const-return-gtype.scm \
  test/everything/const-return-utf8.scm \
  test/everything/const-return-filename.scm \
  test/everything/oneparam-gpointer.scm \
  test/everything/oneparam-gintptr.scm \
  test/everything/oneparam-guintptr.scm \
  test/everything/oneparam-gboolean.scm \
  test/everything/oneparam-gint8.scm \
  test/everything/oneparam-guint8.scm \
  test/everything/oneparam-gint16.scm \
  test/everything/oneparam-guint16.scm \
  test/everything/oneparam-gint32.scm \
  test/everything/oneparam-guint32.scm \
  test/everything/oneparam-gint64.scm \
  test/everything/oneparam-guint64.scm \
  test/everything/oneparam-gchar.scm \
  test/everything/oneparam-gchar-char.scm \
  test/everything/oneparam-gshort.scm \
  test/everything/oneparam-gushort.scm \
  test/everything/oneparam-gint.scm \
  test/everything/oneparam-guint.scm \
  test/everything/oneparam-glong.scm \
  test/everything/oneparam-gulong.scm \
  test/everything/oneparam-gsize.scm \
  test/everything/oneparam-gssize.scm \
  test/everything/oneparam-gfloat.scm \
  test/everything/oneparam-gdouble.scm \
  test/everything/oneparam-gunichar.scm \
  test/everything/oneparam-gtype.scm \
  test/everything/oneparam-utf8.scm \
  test/everything/oneparam-filename.scm \
  test/everything/one-outparam-gpointer.scm \
  test/everything/one-outparam-gintptr.scm \
  test/everything/one-outparam-guintptr.scm \
  test/everything/one-outparam-gboolean.scm \
  test/everything/one-outparam-gboolean.scm \
  test/everything/one-outparam-gint8.scm \
  test/everything/one-outparam-guint8.scm \
  test/everything/one-outparam-gint16.scm \
  test/everything/one-outparam-guint16.scm \
  test/everything/one-outparam-gint32.scm \
  test/everything/one-outparam-guint32.scm \
  test/everything/one-outparam-gint64.scm \
  test/everything/one-outparam-guint64.scm \
  test/everything/one-outparam-gchar.scm \
  test/everything/one-outparam-gshort.scm \
  test/everything/one-outparam-gushort.scm \
  test/everything/one-outparam-gint.scm \
  test/everything/one-outparam-guint.scm \
  test/everything/one-outparam-glong.scm \
  test/everything/one-outparam-gulong.scm \
  test/everything/one-outparam-gsize.scm \
  test/everything/one-outparam-gssize.scm \
  test/everything/one-outparam-gfloat.scm \
  test/everything/one-outparam-gdouble.scm \
  test/everything/one-outparam-gunichar.scm \
  test/everything/one-outparam-gtype.scm \
  test/everything/one-outparam-utf8.scm \
  test/everything/one-outparam-filename.scm \
  test/everything/passthrough-one-gpointer.scm \
  test/everything/passthrough-one-gintptr.scm \
  test/everything/passthrough-one-guintptr.scm \
  test/everything/passthrough-one-utf8.scm \
  test/everything/passthrough-one-filename.scm \
  test/everything/boolean-return-true.scm \
  test/everything/boolean-return-false.scm \
  test/everything/boolean-in-true.scm \
  test/everything/boolean-in-false.scm \
  test/everything/boolean-out-true.scm \
  test/everything/boolean-out-false.scm \
  test/everything/boolean-inout-true-false.scm \
  test/everything/boolean-inout-false-true.scm \
  test/everything/intXX-out-or-return-max.scm \
  test/everything/uintXX-out-or-return-max.scm \
  test/everything/intXX-out-or-return-min.scm \
  test/everything/intXX-in-max.scm \
  test/everything/uintXX-in.scm \
  test/everything/int8-in-max-latin1.scm \
  test/everything/intXX-in-min.scm \
  test/everything/intXX-inout-max-min.scm \
  test/everything/intXX-inout-min-max.scm \
  test/everything/uintXX-inout.scm \
  test/everything/time-t-out.scm \
  test/everything/utf8-none-return.scm \
  test/everything/utf8-full-return.scm \
  test/everything/utf8-as-uint8array-in.scm \
  test/everything/utf8-none-out.scm \
  test/everything/utf8-dangling-out.scm \
  test/everything/utf8-none-inout.scm \
  test/everything/utf8-full-inout.scm \
  test/everything/init-function-null.scm \
  test/everything/init-function-zero.scm \
  test/everything/init-function-one.scm \
  test/everything/init-function-two.scm \
  test/everything/array-fixed-int-return.scm \
  test/everything/array-fixed-short-return.scm \
  test/everything/array-fixed-int-in.scm \
  test/everything/array-fixed-short-in.scm \
  test/everything/array-fixed-out.scm \
  test/everything/array-fixed-inout.scm \
  test/everything/array-return.scm \
  test/everything/array-return-etc.scm \
  test/everything/array-in.scm \
  test/everything/array-in-len-before.scm \
  test/everything/array-in-len-zero-terminated.scm \
  test/everything/array-string-in.scm \
  test/everything/array-uint8-in.scm \
  test/everything/array-uint8-in-latin1.scm \
  test/everything/array-int64-in.scm \
  test/everything/array-uint64-in.scm \
  test/everything/array-unichar-in.scm \
  test/everything/array-bool-in.scm \
  test/everything/multi-array-key-value-in.scm \
  test/everything/array-enum-in.scm \
  test/everything/array-in-guint64-len.scm \
  test/everything/array-in-guint8-len.scm \
  test/everything/array-out.scm \
  test/everything/array-out-etc.scm \
  test/everything/array-bool-out.scm \
  test/everything/array-unichar-out.scm \
  test/everything/array-inout.scm \
  test/everything/array-inout-etc.scm \
  test/everything/array-in-nonzero-nonlen.scm \
  test/everything/array-zero-terminated-return.scm \
  test/everything/array-zero-terminated-return-null.scm \
  test/everything/array-zero-terminated-return-unichar.scm \
  test/everything/array-zero-terminated-in.scm \
  test/everything/array-zero-terminated-out.scm \
  test/everything/array-zero-terminated-inout.scm \
  test/everything/array-gvariant-none-in.scm \
  test/everything/array-gvariant-container-in.scm \
  test/everything/array-gvariant-full-in.scm \
  test/everything/garray-int-none-return.scm \
  test/everything/garray-uint64-none-return.scm \
  test/everything/garray-utf8-none-return.scm \
  test/everything/garray-utf8-container-return.scm \
  test/everything/garray-utf8-full-return.scm \
  test/everything/garray-int-none-in.scm \
  test/everything/garray-uint64-none-in.scm \
  test/everything/garray-utf8-none-in.scm \
  test/everything/garray-utf8-none-out.scm \
  test/everything/garray-utf8-container-out.scm \
  test/everything/garray-utf8-full-out.scm \
  test/everything/garray-utf8-full-out-caller-allocated.scm \
  test/everything/garray-utf8-none-inout.scm \
  test/everything/garray-utf8-container-inout.scm \
  test/everything/garray-utf8-full-inout.scm \
  test/everything/garray-bool-none-in.scm \
  test/everything/garray-unichar-none-in.scm \
  test/everything/gptrarray-utf8-none-return.scm \
  test/everything/gptrarray-utf8-container-return.scm \
  test/everything/gptrarray-utf8-full-return.scm \
  test/everything/gptrarray-utf8-none-in.scm \
  test/everything/gptrarray-utf8-none-out.scm \
  test/everything/gptrarray-utf8-container-out.scm \
  test/everything/gptrarray-utf8-full-out.scm \
  test/everything/gptrarray-utf8-none-inout.scm \
  test/everything/gptrarray-utf8-container-inout.scm \
  test/everything/gptrarray-utf8-full-inout.scm \
  test/everything/bytearray-full-return.scm \
  test/everything/bytearray-none-in.scm \
  test/everything/gbytes-full-return.scm \
  test/everything/gbytes-none-in.scm \
  test/everything/gslist-int-none-return.scm \
  test/everything/gslist-utf8-none-return.scm \
  test/everything/gslist-utf8-container-return.scm \
  test/everything/gslist-utf8-full-return.scm
else
cleanpkgdata_DATA =
EVERYTHING_TESTS =
endif

TESTS = \
  $(TYPELIB_TESTS) \
  $(VERSION_INFORMATION_TESTS) \
  $(BASIC_TYPE_TESTS) \
  $(STANDARD_MACROS_TESTS) \
  $(BYTE_ORDER_MACROS_TESTS) \
  $(NUMERICAL_DEFINITIONS_TESTS) \
  $(ATOMIC_OPERATIONS_TESTS) \
  $(MESSAGE_OUTPUT_TESTS) \
  $(MAIN_EVENT_LOOP_TESTS) \
  $(MEMORY_ALLOCATION_TESTS) \
  $(MEMORY_SLICES_TESTS) \
  $(IO_CHANNELS_TESTS) \
  $(FILE_TESTS) \
  $(MEMORY_OUTPUT_TESTS) \
  $(STRING_UTILITY_TESTS) \
  $(CHARACTER_SET_CONVERSION_TESTS) \
  $(BASE64_ENCODING_TESTS) \
  $(UNICODE_MANIPULATION_TESTS) \
  $(DATA_CHECKSUM_TESTS) \
  $(SECURE_HMAC_DIGESTS_TESTS) \
  $(DATE_AND_TIME_FUNCTIONS_TESTS) \
  $(MEMCHK_TESTS) \
  $(BYTE_ARRAY_TESTS) \
  $(GAPPLICATION_TESTS) \
  $(GTK_ENTRY_TESTS) \
  $(GTK_LAYOUT_TESTS) \
  $(GRILO_TESTS) \
  $(OOP_TESTS) \
  $(VALUE_TESTS) \
  $(FLAGS_TESTS) \
  test/misc/softerror_create.scm \
  test/misc/use-typelibs.scm \
  test/misc/interfaces.scm \
  $(EVERYTHING_TESTS)

XFAIL_TESTS = \
  test/everything/one-outparam-gchar.scm \
  test/everything/garray-utf8-full-out-caller-allocated.scm

SCM_LOG_COMPILER = ${builddir}/tools/run-test

test: check

#################
#     Tools     #
#################

bin_SCRIPTS = tools/guile-gi

#################
#  Local rules  #
#################

distclean-local: -distclean-docs
html-local: docs

check-syntax:
	$(CC) -std=c11 $(GUILE_CFLAGS) $(GLIB_CFLAGS) $(GOBJECT_CFLAGS) \
	 $(GOBJECT_INTROSPECTION_CFLAGS) $(FFI_CFLAGS) \
	 -DG_LOG_DOMAIN=\"GuileGI\" $(CFLAGS) -fsyntax-only $(libguile_gi_la_c_sources)

indent:
	VERSION_CONTROL=none indent $(libguile_gi_la_SOURCES)

.PHONY: check-syntax indent test

#################
#   Variables   #
#################

noinst_DATA = \
  test/lib.scm \
  test/automake-test-lib.scm \
  test/flags/flags.scm \
  test/grilo/base.scm

CLEANFILES = \
  ChangeLog README \
  doc/guile-gi.dvi \
  examples/resources/test.gresource \
  $(guileobject_DATA) $(pkgguileobject_DATA) $(scriptguileobject_DATA) \
  tmp.txt \
  $(BUILT_SOURCES) \
  $(cleanpkgdata_DATA)

EXTRA_DIST = \
  README.md \
  build-aux/config.rpath \
  doc/example-1.scm doc/gtk-3.scm doc/fdl-1.3.texi \
  docs/document-1.0.1.css docs/README \
  examples/resources/test.gresource \
  m4/attributes.m4 \
  tools/uninstalled-env.in \
  tools/uninstalled-test-env.in \
  tools/run-guile.in \
  tools/run-test.in \
  tools/gdb-guile.in \
  tools/gdb-test.in \
  tools/guile-gi.in \
  tools/lcov.sh.in \
  tools/valgrind.sh.in \
  tools/valgrind.supp \
  $(TESTS) \
  $(noinst_DATA)

SUFFIXES = .gresource .gresource.xml
