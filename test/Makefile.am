# Makefile.am - test directory
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

################################################################
# Test driver

.PHONY: test
test: check

AM_TESTS_ENVIRONMENT = \
  GUILE_AUTO_COMPILE=0; \
  export GUILE_AUTO_COMPILE; \
  GI_TYPELIB_PATH='$(abs_builddir)'; \
  export GI_TYPELIB_PATH;

AM_CFLAGS = $(GUILE_CFLAGS) $(GOBJECT_INTROSPECTION_CFLAGS)

TEST_EXTENSIONS = .scm

noinst_DATA = lib.scm \
  automake-test-lib.scm \
  flags/flags.scm \
  grilo/base.scm

TYPELIB_TESTS = \
 typelib/document.scm \
 typelib/to-module.scm

VERSION_INFORMATION_TESTS = \
 version_info/MAJOR_VERSION.scm \
 version_info/MINOR_VERSION.scm \
 version_info/MICRO_VERSION.scm \
 version_info/VERSION_MIN_REQUIRED.scm

BASIC_TYPE_TESTS = \
 basic_types/MAXINT16.scm \
 basic_types/MAXUINT64.scm \
 basic_types/GINT32_FORMAT.scm

STANDARD_MACROS_TESTS = \
 std_macros/DIR_SEPARATOR_S.scm \
 std_macros/DIR_SEPARATOR.scm \
 std_macros/SEARCHPATH_SEPARATOR.scm \
 std_macros/SEARCHPATH_SEPARATOR_S.scm

BYTE_ORDER_MACROS_TESTS = \
 byte_order/BIG_ENDIAN.scm \
 byte_order/LITTLE_ENDIAN.scm

NUMERICAL_DEFINITIONS_TESTS = \
 basic_types/IEEE754_FLOAT_BIAS.scm \
 basic_types/E.scm \
 basic_types/PI.scm

MAIN_EVENT_LOOP_TESTS = \
 main_loop/new.scm \
 main_loop/runs.scm \
 main_loop/quit.scm \
 main_loop/get_context.scm \
 main_loop/timeout.scm \
 main_loop/idle.scm

MEMORY_ALLOCATION_TESTS = \
 mem/malloc.scm \
 mem/try_malloc0.scm

MEMORY_SLICES_TESTS = \
 mem/slice_alloc.scm

IO_CHANNELS_TESTS = \
 io/io_channel_pipe.scm \
 io/io_channel_new_file.scm \
 io/io_channel_unichar.scm

MESSAGE_OUTPUT_TESTS = \
 io/log_set_handler.scm \
 io/log_writer_is_journald.scm

FILE_TESTS = \
 file/new_for_path.scm \
 file/get_parent.scm \
 file/has_parent.scm \
 file/get_child.scm

STRING_UTILITY_TESTS = \
 string/strdup.scm \
 string/strndup.scm \
 string/strnfill.scm \
 string/stpcpy.scm \
 string/strstr_len.scm \
 string/str_has_prefix.scm \
 string/str_tokenize_and_fold.scm

CHARACTER_SET_CONVERSION_TESTS = \
 convert/ascii.scm \
 convert/latin1.scm

UNICODE_MANIPULATION_TESTS = \
 unichar/validate_zero.scm \
 unichar/isalpha.scm \
 unichar/compose.scm \
 unichar/combining_class.scm \
 misc/unicode_canonical_ordering.scm

BASE64_ENCODING_TESTS = \
 misc/base64_encode.scm

DATA_CHECKSUM_TESTS = \
 misc/checksum.scm

SECURE_HMAC_DIGESTS_TESTS = \
 hmac/string.scm \
 hmac/data.scm \
 hmac/bytes.scm

DATE_AND_TIME_FUNCTIONS_TESTS = \
 datetime/get_monotonic_time.scm \
 datetime/get_real_time.scm \
 datetime/date_new.scm \
 datetime/date_copy.scm \
 datetime/date_clear.scm

MEMCHK_TESTS = \
 mem/memchk_1.scm \
 mem/memchk_2.scm

BYTE_ARRAY_TESTS = \
 byte_array/byte_array_new.scm \
 byte_array/byte_array_new_take.scm \
 byte_array/bytes_new_null_size.scm \
 byte_array/bytes_new_null_contents.scm

GAPPLICATION_TESTS = \
 gapplication/activate_order.scm \
 gapplication/command-line.scm \
 gapplication/command-line-options.scm \
 gapplication/make.scm \
 gapplication/set_resource_path.scm

FUZZY_MATCH_TESTS = \
 misc/fuzzy_match.scm

GTK_ENTRY_TESTS = \
 gtk_entry/set_alignment.scm \
 gtk_entry/set_alignment_zero.scm

GTK_LAYOUT_TESTS = \
 gtk_layout/create_box.scm

GRILO_TESTS = \
 grilo/audio_simple.scm \
 grilo/audio_multiple_artists.scm \
 grilo/init_commandline.scm

OOP_TESTS = \
 oop/property-basics.scm \
 oop/guile-types.scm \
 oop/signal-simple.scm \
 oop/signal-detailed.scm \
 oop/signal-accumulator.scm \
 oop/signal-block.scm

FLAGS_TESTS = \
 flags/complement.scm \
 flags/difference.scm \
 flags/projection.scm \
 flags/union.scm

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
 $(FUZZY_MATCH_TESTS) \
 $(GTK_ENTRY_TESTS) \
 $(GTK_LAYOUT_TESTS) \
 $(GRILO_TESTS) \
 $(OOP_TESTS) \
 $(FLAGS_TESTS) \
 misc/softerror_create.scm \
 misc/use-typelibs.scm \
 misc/interfaces.scm

XFAIL_TESTS =

if HAVE_INTROSPECTION

SCM_LOG_COMPILER = \
  ${top_builddir}/libtool \
  --dlopen=${top_builddir}/src/libguile-gi.la \
  --dlopen=${top_builddir}/test/libeverything-1.0.la \
  --dlopen=${top_builddir}/test/libmarshall-1.0.la \
  --mode=execute $(GUILE)

else

SCM_LOG_COMPILER = \
  ${top_builddir}/libtool \
  --dlopen=${top_builddir}/src/libguile-gi.la \
  --mode=execute $(GUILE)

endif

AM_SCM_LOG_FLAGS = --no-auto-compile -L $(abs_top_builddir)/src -L $(abs_top_srcdir)/src -L $(abs_top_srcdir) -s

EXTRA_DIST = \
  $(TESTS) \
  $(noinst_DATA)

CLEANFILES = tmp.txt

################
# Testing libraries

-include $(INTROSPECTION_MAKEFILE)
if HAVE_INTROSPECTION
pkglib_LTLIBRARIES = libeverything-1.0.la libmarshall-1.0.la

BUILT_SOURCES = everything.c everything.h gitestmacros.h
CLEANFILES += $(BUILT_SOURCES)

everything.c: $(GI_DATA_DIR)/tests/everything.c
	cp $< $@
everything.h: $(GI_DATA_DIR)/tests/everything.h
	cp $< $@
gitestmacros.h : $(GI_DATA_DIR)/tests/gitestmacros.h
	cp $< $@

INTROSPECTION_GIRS = Everything-1.0.gir Marshall-1.0.gir
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(abs_srcdir) --warn-all
INTROSPECTION_COMPILER_ARGS = --includedir=$(abs_srcdir)
nodist_libeverything_1_0_la_SOURCES = everything.c everything.h
libeverything_1_0_la_CFLAGS = $(GOBJECT_CFLAGS)
libeverything_1_0_la_LIBADD = $(GOBJECT_LIBS)

libmarshall_1_0_la_SOURCES = \
  marshall.c marshall.h
nodist_libmarshall_1_0_la_SOURCES = \
  gitestmacros.h
libmarshall_1_0_la_CFLAGS = $(GOBJECT_CFLAGS)
libmarshall_1_0_la_LIBADD = $(GOBJECT_LIBS)

Everything-1.0.gir: libeverything-1.0.la
Everything_1_0_gir_LIBS = libeverything-1.0.la
Everything_1_0_gir_INCLUDES = GObject-2.0
Everything_1_0_gir_FILES = everything.c everything.h

Marshall-1.0.gir: libmarshall-1.0.la
Marshall_1_0_gir_NAMESPACE = Marshall
Marshall_1_0_gir_LIBS = libmarshall-1.0.la
Marshall_1_0_gir_INCLUDES = GObject-2.0
Marshall_1_0_gir_FILES = marshall.c marshall.h

pkgdata_DATA = \
  Everything-1.0.gir Marshall-1.0.gir \
  Everything-1.0.typelib Marshall-1.0.typelib

CLEANFILES += \
 Everything-1.0.gir Everything-1.0.typelib \
 Marshall-1.0.gir Marshall-1.0.typelib

EVERYTHING_TESTS = \
 everything/a21.scm \
 everything/callback_a21.scm \
 everything/nullfunc.scm \
 everything/const-return-gpointer.scm \
 everything/const-return-gintptr.scm \
 everything/const-return-gboolean.scm \
 everything/const-return-gint8.scm \
 everything/const-return-guint8.scm \
 everything/const-return-gint16.scm \
 everything/const-return-guint16.scm \
 everything/const-return-gint32.scm \
 everything/const-return-guint32.scm \
 everything/const-return-gint64.scm \
 everything/const-return-guint64.scm \
 everything/const-return-gchar.scm \
 everything/const-return-gshort.scm \
 everything/const-return-gushort.scm \
 everything/const-return-gint.scm \
 everything/const-return-guint.scm \
 everything/const-return-glong.scm \
 everything/const-return-gulong.scm \
 everything/const-return-gsize.scm \
 everything/const-return-gssize.scm \
 everything/const-return-gfloat.scm \
 everything/const-return-gdouble.scm \
 everything/const-return-gunichar.scm \
 everything/const-return-gtype.scm \
 everything/const-return-utf8.scm \
 everything/const-return-filename.scm \
 everything/oneparam-gpointer.scm \
 everything/oneparam-gintptr.scm \
 everything/oneparam-guintptr.scm \
 everything/oneparam-gboolean.scm \
 everything/oneparam-gint8.scm \
 everything/oneparam-guint8.scm \
 everything/oneparam-gint16.scm \
 everything/oneparam-guint16.scm \
 everything/oneparam-gint32.scm \
 everything/oneparam-guint32.scm \
 everything/oneparam-gint64.scm \
 everything/oneparam-guint64.scm \
 everything/oneparam-gchar.scm \
 everything/oneparam-gchar-char.scm \
 everything/oneparam-gshort.scm \
 everything/oneparam-gushort.scm \
 everything/oneparam-gint.scm \
 everything/oneparam-guint.scm \
 everything/oneparam-glong.scm \
 everything/oneparam-gulong.scm \
 everything/oneparam-gsize.scm \
 everything/oneparam-gssize.scm \
 everything/oneparam-gfloat.scm \
 everything/oneparam-gdouble.scm \
 everything/oneparam-gunichar.scm \
 everything/oneparam-gtype.scm \
 everything/oneparam-utf8.scm \
 everything/oneparam-filename.scm \
 everything/one-outparam-gpointer.scm \
 everything/one-outparam-gintptr.scm \
 everything/one-outparam-guintptr.scm \
 everything/one-outparam-gboolean.scm \
 everything/one-outparam-gboolean.scm \
 everything/one-outparam-gint8.scm \
 everything/one-outparam-guint8.scm \
 everything/one-outparam-gint16.scm \
 everything/one-outparam-guint16.scm \
 everything/one-outparam-gint32.scm \
 everything/one-outparam-guint32.scm \
 everything/one-outparam-gint64.scm \
 everything/one-outparam-guint64.scm \
 everything/one-outparam-gchar.scm \
 everything/one-outparam-gshort.scm \
 everything/one-outparam-gushort.scm \
 everything/one-outparam-gint.scm \
 everything/one-outparam-guint.scm \
 everything/one-outparam-glong.scm \
 everything/one-outparam-gulong.scm \
 everything/one-outparam-gsize.scm \
 everything/one-outparam-gssize.scm \
 everything/one-outparam-gfloat.scm \
 everything/one-outparam-gdouble.scm \
 everything/one-outparam-gunichar.scm \
 everything/one-outparam-gtype.scm \
 everything/one-outparam-utf8.scm \
 everything/one-outparam-filename.scm \
 everything/passthrough-one-gpointer.scm \
 everything/passthrough-one-gintptr.scm \
 everything/passthrough-one-guintptr.scm \
 everything/passthrough-one-utf8.scm \
 everything/passthrough-one-filename.scm \
 everything/boolean-return-true.scm \
 everything/boolean-return-false.scm \
 everything/boolean-in-true.scm \
 everything/boolean-in-false.scm \
 everything/boolean-out-true.scm \
 everything/boolean-out-false.scm \
 everything/boolean-inout-true-false.scm \
 everything/boolean-inout-false-true.scm \
 everything/intXX-out-or-return-max.scm \
 everything/uintXX-out-or-return-max.scm \
 everything/intXX-out-or-return-min.scm \
 everything/intXX-in-max.scm \
 everything/int8-in-max-latin1.scm \
 everything/intXX-in-min.scm \
 everything/intXX-inout-max-min.scm \
 everything/intXX-inout-min-max.scm \
 everything/time-t-out.scm \
 everything/utf8-none-return.scm \
 everything/utf8-full-return.scm \
 everything/utf8-as-uint8array-in.scm \
 everything/utf8-none-out.scm \
 everything/utf8-dangling-out.scm \
 everything/utf8-none-inout.scm \
 everything/utf8-full-inout.scm \
 everything/init-function-null.scm \
 everything/init-function-zero.scm \
 everything/init-function-one.scm \
 everything/init-function-two.scm \
 everything/array-fixed-int-return.scm \
 everything/array-fixed-short-return.scm \
 everything/array-fixed-int-in.scm \
 everything/array-fixed-short-in.scm \
 everything/array-fixed-out.scm \
 everything/array-fixed-inout.scm \
 everything/array-return.scm \
 everything/array-return-etc.scm \
 everything/array-in.scm \
 everything/array-in-len-before.scm \
 everything/array-in-len-zero-terminated.scm \
 everything/array-string-in.scm \
 everything/array-uint8-in.scm \
 everything/array-uint8-in-latin1.scm \
 everything/array-int64-in.scm \
 everything/array-uint64-in.scm \
 everything/array-unichar-in.scm \
 everything/array-bool-in.scm \
 everything/array-enum-in.scm \
 everything/array-in-guint64-len.scm \
 everything/array-in-guint8-len.scm \
 everything/array-out.scm \
 everything/array-out-etc.scm \
 everything/array-bool-out.scm \
 everything/array-unichar-out.scm \
 everything/array-inout.scm \
 everything/array-inout-etc.scm \
 everything/array-in-nonzero-nonlen.scm \
 everything/array-zero-terminated-return.scm \
 everything/array-zero-terminated-return-null.scm \
 everything/array-zero-terminated-return-unichar.scm \
 everything/array-zero-terminated-in.scm \
 everything/array-zero-terminated-out.scm \
 everything/array-zero-terminated-inout.scm \
 everything/gslist-int-none-return.scm \
 everything/gslist-utf8-none-return.scm \
 everything/gslist-utf8-container-return.scm \
 everything/gslist-utf8-full-return.scm


TESTS += \
 $(EVERYTHING_TESTS)

XFAIL_TESTS += \
 everything/one-outparam-gchar.scm

endif
