// Copyright (C) 2018, 2019 Michael L. Gran

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <girepository.h>
#include <glib.h>
#include <libguile.h>
#include <math.h>
#include <stdint.h>
#include "gig_argument.h"
#include "gig_object.h"
#include "gi_type_tag.h"
#include "gig_callback.h"
#include "gig_type.h"

#ifndef FLT_MAX
#define FLT_MAX 3.402823466e+38F
#endif

#if GIG_DEBUG_TRANSFERS
#define TRACE_C2S() g_debug("[C2S] On line %d while handing %s of %s.", __LINE__, gig_type_meta_describe(meta), subr)
#define TRACE_S2C() g_debug("[S2C] On line %d while handing %s of %s.", __LINE__, gig_type_meta_describe(meta), subr)
#else
#define TRACE_C2S()
#define TRACE_S2C()
#endif


#define SURPRISING \
    do { \
    g_warning("Unusual argument type '%s' %s:%d", gig_type_meta_describe(meta), __FILE__, __LINE__); \
    } while(FALSE)

#define UNHANDLED                               \
    do { \
    g_error("Unhandled argument type '%s' %s:%d", gig_type_meta_describe(meta), __FILE__, __LINE__); \
    } while(FALSE)

static gpointer later_free(GPtrArray *must_free, GigTypeMeta *meta, gpointer ptr);
static void scm_to_c_char(S2C_ARG_DECL);
static void scm_to_c_boolean(S2C_ARG_DECL);
static void scm_to_c_integer(S2C_ARG_DECL);
static void scm_to_c_enum(S2C_ARG_DECL);
static void scm_to_c_real(S2C_ARG_DECL);
static void scm_to_c_boxed(S2C_ARG_DECL);
static void scm_to_c_object(S2C_ARG_DECL);
static void scm_to_c_variant(S2C_ARG_DECL);
static void scm_to_c_interface(S2C_ARG_DECL);
static void scm_to_c_string(S2C_ARG_DECL);
static void scm_to_c_pointer(S2C_ARG_DECL);
static void scm_to_c_native_array(S2C_ARG_DECL);
static void scm_to_c_native_unichar_array(S2C_ARG_DECL);
static void scm_to_c_native_boolean_array(S2C_ARG_DECL);
static void scm_to_c_native_immediate_array(S2C_ARG_DECL);
static void scm_to_c_native_string_array(S2C_ARG_DECL);
static void scm_to_c_native_interface_array(S2C_ARG_DECL);
static void scm_to_c_garray(S2C_ARG_DECL);
static void scm_to_c_byte_array(S2C_ARG_DECL);
static void c_char_to_scm(C2S_ARG_DECL);
static void c_boolean_to_scm(C2S_ARG_DECL);
static void c_integer_to_scm(C2S_ARG_DECL);
static void c_enum_to_scm(C2S_ARG_DECL);
static void c_real_to_scm(C2S_ARG_DECL);
static void c_boxed_to_scm(C2S_ARG_DECL);
static void c_object_to_scm(C2S_ARG_DECL);
static void c_variant_to_scm(C2S_ARG_DECL);
static void c_param_to_scm(C2S_ARG_DECL);
static void c_interface_to_scm(C2S_ARG_DECL);
static void c_string_to_scm(C2S_ARG_DECL);
static void c_pointer_to_scm(C2S_ARG_DECL);
static void c_array_to_scm(C2S_ARG_DECL);
static void c_byte_array_to_scm(C2S_ARG_DECL);
static void c_native_array_to_scm(C2S_ARG_DECL);
static void c_garray_to_scm(C2S_ARG_DECL);
static void c_gptrarray_to_scm(C2S_ARG_DECL);
static void c_list_to_scm(C2S_ARG_DECL);

// Use this to register allocated data to be freed after use.
static gpointer
later_free(GPtrArray *must_free, GigTypeMeta *meta, gpointer ptr)
{
    if ((must_free != NULL) && !meta->is_transfer_ownership)
        g_ptr_array_insert(must_free, 0, ptr);
    return ptr;
}

#define LATER_FREE(_ptr) later_free(must_free, meta, _ptr)

static gsize
array_length(GigTypeMeta *meta, GIArgument *arg)
{
    if (meta->gtype == G_TYPE_ZERO_TERMINATED_CARRAY) {
        gpointer array = arg->v_pointer;
        if (array == NULL)
            return 0;

        gsize length = 0;

        if (G_TYPE_FUNDAMENTAL(meta->params[0].gtype) == G_TYPE_STRING) {
            gchar **ptr = array;
            while (ptr[length] != NULL)
                length++;
            return length;
        }

        switch (meta->params[0].item_size) {
        case 0:
            g_assert_not_reached();
        case 1:
            return strlen(array);
        case 2:
        {
            gint16 *ptr = array;
            while (*ptr++ != 0)
                length++;
            return length;
        }
        case 4:
        {
            gint32 *ptr = array;
            while (*ptr++ != 0)
                length++;
            return length;
        }
        case 8:
        {
            gint64 *ptr = array;
            while (*ptr++ != 0)
                length++;
            return length;
        }
        default:
        {
            gchar *ptr = array;
            gboolean non_null;
            length = -1;
            do {
                length++;
                non_null = FALSE;
                for (gsize i = 0; i <= meta->params[0].item_size; i++)
                    if (ptr + i != 0) {
                        non_null = TRUE;
                        break;
                    }
                ptr += meta->params[0].item_size;
            } while (non_null);

            return length;
        }
        }
    }
    else if (meta->gtype == G_TYPE_FIXED_SIZE_CARRAY)
        return meta->length;

    return GIG_ARRAY_SIZE_UNKNOWN;
}

//////////////////////////////////////////////////////////
// CONVERTING SCM OBJECTS TO GIARGUMENTS
//////////////////////////////////////////////////////////

// This is the main entry point of the conversion of SCM objects to
// GIArguments.
void
gig_argument_scm_to_c(S2C_ARG_DECL)
{
    TRACE_S2C();
    if (size)
        *size = 0;

    GType fundamental_type = G_TYPE_FUNDAMENTAL(meta->gtype);
    switch (fundamental_type) {
    case G_TYPE_INVALID:
        UNHANDLED;
        break;
    case G_TYPE_NONE:
        SURPRISING;
        arg->v_pointer = NULL;
        break;
    case G_TYPE_INTERFACE:
        scm_to_c_interface(S2C_ARGS);
        break;
    case G_TYPE_CHAR:
    case G_TYPE_UCHAR:
        scm_to_c_char(S2C_ARGS);
        break;
    case G_TYPE_BOOLEAN:
        scm_to_c_boolean(S2C_ARGS);
        break;
    case G_TYPE_INT:
    case G_TYPE_UINT:
    case G_TYPE_LONG:
    case G_TYPE_ULONG:
    case G_TYPE_INT64:
    case G_TYPE_UINT64:
        scm_to_c_integer(S2C_ARGS);
        break;
    case G_TYPE_ENUM:
    case G_TYPE_FLAGS:
        scm_to_c_enum(S2C_ARGS);
        break;
    case G_TYPE_FLOAT:
    case G_TYPE_DOUBLE:
        scm_to_c_real(S2C_ARGS);
        break;
    case G_TYPE_STRING:
        scm_to_c_string(S2C_ARGS);
        break;
    case G_TYPE_POINTER:
        scm_to_c_pointer(S2C_ARGS);
        break;
    case G_TYPE_BOXED:
        scm_to_c_boxed(S2C_ARGS);
        break;
    case G_TYPE_PARAM:
        //scm_to_c_param(S2C_ARGS);
        UNHANDLED;
        break;
    case G_TYPE_OBJECT:
        scm_to_c_object(S2C_ARGS);
        break;
    case G_TYPE_VARIANT:
        scm_to_c_variant(S2C_ARGS);
        break;
    default:
        UNHANDLED;
        break;
    }
}

static void
scm_to_c_char(S2C_ARG_DECL)
{
    TRACE_S2C();
    GType t = meta->gtype;
    if (t == G_TYPE_CHAR) {
        if (SCM_CHARP(object)) {
            if (SCM_CHAR(object) > 255)
                scm_out_of_range(subr, object);
            else
                arg->v_int8 = (guint8)SCM_CHAR(object);
        }
        else if (!scm_is_signed_integer(object, INT8_MIN, INT8_MAX))
            scm_wrong_type_arg_msg(subr, argpos, object, "int8");
        else
            arg->v_int8 = scm_to_int8(object);
    }
    else if (t == G_TYPE_UCHAR) {
        if (SCM_CHARP(object)) {
            if (SCM_CHAR(object) > 255)
                scm_out_of_range(subr, object);
            else
                arg->v_uint8 = (guint8)SCM_CHAR(object);
        }
        else if (!scm_is_unsigned_integer(object, 0, UINT8_MAX))
            scm_wrong_type_arg_msg(subr, argpos, object, "uint8");
        else
            arg->v_uint8 = scm_to_uint8(object);
    }
}

static void
scm_to_c_boolean(S2C_ARG_DECL)
{
    TRACE_S2C();
    arg->v_boolean = scm_is_true(object);
}

static void
scm_to_c_integer(S2C_ARG_DECL)
{
    TRACE_S2C();
    GType t = meta->gtype;
    if (t == G_TYPE_INT16) {
        if (!scm_is_signed_integer(object, INT16_MIN, INT16_MAX))
            scm_wrong_type_arg_msg(subr, argpos, object, "int16");
        arg->v_int16 = scm_to_int16(object);
    }
    else if (t == G_TYPE_UINT16) {
        if (!scm_is_unsigned_integer(object, 0, UINT16_MAX))
            scm_wrong_type_arg_msg(subr, argpos, object, "uint16");
        arg->v_uint16 = scm_to_uint16(object);
    }
    else if (t == G_TYPE_INT32) {
        if (!scm_is_signed_integer(object, INT32_MIN, INT32_MAX))
            scm_wrong_type_arg_msg(subr, argpos, object, "int32");
        arg->v_int32 = scm_to_int32(object);
    }
    else if (t == G_TYPE_UINT32) {
        if (!scm_is_unsigned_integer(object, 0, UINT32_MAX))
            scm_wrong_type_arg_msg(subr, argpos, object, "uint32");
        arg->v_uint32 = scm_to_uint32(object);
    }
    else if (t == G_TYPE_INT64) {
        if (!scm_is_signed_integer(object, INT64_MIN, INT64_MAX))
            scm_wrong_type_arg_msg(subr, argpos, object, "int64");
        arg->v_int64 = scm_to_int64(object);
    }
    else if (t == G_TYPE_UINT64) {
        if (!scm_is_unsigned_integer(object, 0, UINT64_MAX))
            scm_wrong_type_arg_msg(subr, argpos, object, "uint64");
        arg->v_uint64 = scm_to_uint64(object);
    }
    else if (t == G_TYPE_UNICHAR) {
        if (SCM_CHARP(object))
            arg->v_uint32 = SCM_CHAR(object);
        else if (scm_is_unsigned_integer(object, 0, SCM_CODEPOINT_MAX))
            arg->v_uint32 = scm_to_uint32(object);
        else {
            scm_wrong_type_arg_msg(subr, argpos, object, "char");
        }
    }
    else
        UNHANDLED;
}

static void
scm_to_c_real(S2C_ARG_DECL)
{
    TRACE_S2C();
    GType t = meta->gtype;

    if (t == G_TYPE_FLOAT) {
        if (!scm_is_real(object))
            scm_wrong_type_arg_msg(subr, argpos, object, "float32");
        gdouble dtmp = scm_to_double(object);
        if (dtmp > FLT_MAX || dtmp < -FLT_MAX)
            scm_out_of_range(subr, object);
        arg->v_float = (float)dtmp;
    }
    else if (t == G_TYPE_DOUBLE) {
        if (!scm_is_real(object))
            scm_wrong_type_arg_msg(subr, argpos, object, "float64");
        arg->v_double = scm_to_double(object);
    }
    else
        UNHANDLED;
}

static void
scm_to_c_enum(S2C_ARG_DECL)
{
    TRACE_S2C();
    GType fundamental_type = G_TYPE_FUNDAMENTAL(meta->gtype);
    if (fundamental_type == G_TYPE_ENUM)
        arg->v_int = scm_to_int(object);
    else if (fundamental_type == G_TYPE_FLAGS)
        arg->v_uint = scm_to_uint(object);
    else
        UNHANDLED;
}

static void
scm_to_c_pointer(S2C_ARG_DECL)
{
    TRACE_S2C();
    if (scm_is_false(object) && meta->is_nullable)
        arg->v_pointer = NULL;
    else if (meta->gtype == G_TYPE_CALLBACK) {
        if (scm_is_true(scm_procedure_p(object))) {
            arg->v_pointer = gig_callback_get_ptr(meta->callable_info, object);
            g_assert(arg->v_pointer != NULL);
        }
        else
            scm_wrong_type_arg_msg(subr, argpos, object, "a procedure");
    }
    else if (meta->gtype == G_TYPE_GTYPE) {
        if (!scm_is_unsigned_integer(object, 0, UINT64_MAX))
            scm_wrong_type_arg_msg(subr, argpos, object, "GType integer");
        arg->v_size = scm_to_size_t(object);
    }
    else if (SCM_POINTER_P(object))
        arg->v_pointer = scm_to_pointer(object);
    else if (scm_is_bytevector(object))
        arg->v_pointer = SCM_BYTEVECTOR_CONTENTS(object);
    else
        UNHANDLED;
}

static void
scm_to_c_boxed(S2C_ARG_DECL)
{
    TRACE_S2C();
    GType t = meta->gtype;
    if (meta->is_nullable && scm_is_false(object)) {
        arg->v_pointer = NULL;
    }
    else if (t == G_TYPE_LENGTH_CARRAY
             || t == G_TYPE_ZERO_TERMINATED_CARRAY || t == G_TYPE_FIXED_SIZE_CARRAY)
        scm_to_c_native_array(S2C_ARGS);
    else if (t == G_TYPE_ARRAY)
        scm_to_c_garray(S2C_ARGS);
    else if (t == G_TYPE_BYTE_ARRAY)
        scm_to_c_byte_array(S2C_ARGS);
    else
        arg->v_pointer = gig_type_peek_object(object);
}

static void
scm_to_c_object(S2C_ARG_DECL)
{
    TRACE_S2C();
    if (meta->is_nullable && scm_is_false(object)) {
        arg->v_pointer = NULL;
    }
    else
        arg->v_pointer = gig_type_peek_object(object);
}

static void
scm_to_c_variant(S2C_ARG_DECL)
{
    TRACE_S2C();
    arg->v_pointer = gig_type_peek_object(object);
}

static void
scm_to_c_interface(S2C_ARG_DECL)
{
    TRACE_S2C();
    if (meta->is_nullable && scm_is_false(object)) {
        arg->v_pointer = NULL;
        return;
    }
    else
        UNHANDLED;
}

static void
scm_to_c_string(S2C_ARG_DECL)
{
    TRACE_S2C();
    if (meta->is_nullable && scm_is_false(object)) {
        arg->v_pointer = NULL;
        return;
    }

    // Here we convert an input string into an argument.  The input
    // string is either UTF8 or locale encoded.
    if (scm_is_bytevector(object)) {
        // Some methods expect passed-in strings to be overwriteable,
        // like g_date_strftime(char *s, ...) expects 's' to be a
        // place to store an output.  Since Glib strings and Guile
        // strings have no encoding in common, we can use
        // bytevectors...
        if (!meta->is_transfer_ownership) {
            // But when we're using bytevectors as a possibly writable
            // location, they do need to be null terminated.
            gboolean terminated = FALSE;
            for (gsize i = 0; i < SCM_BYTEVECTOR_LENGTH(object); i++)
                if (SCM_BYTEVECTOR_CONTENTS(object)[i] == 0)
                    terminated = TRUE;
            if (!terminated)
                scm_wrong_type_arg_msg(subr, argpos, object, "null-terminated bytevector");
            arg->v_string = (char *)SCM_BYTEVECTOR_CONTENTS(object);
        }
        else
            // But when we're copying the contents of the string, the
            // null termination can be enforced here.
            arg->v_string = g_strndup((const gchar *)SCM_BYTEVECTOR_CONTENTS(object),
                                      SCM_BYTEVECTOR_LENGTH(object));
    }
    else if (scm_is_string(object)) {
        if (meta->gtype == G_TYPE_LOCALE_STRING)
            arg->v_string = scm_to_locale_string(object);
        else
            arg->v_string = scm_to_utf8_string(object);
        LATER_FREE(arg->v_string);
    }
    else
        scm_wrong_type_arg_msg(subr, argpos, object, "string or bytevector");
}


static void
scm_to_c_native_array(S2C_ARG_DECL)
{
    TRACE_S2C();
    GType item_type = meta->params[0].gtype;
    GType fundamental_item_type = G_TYPE_FUNDAMENTAL(item_type);

    if (item_type == G_TYPE_BOOLEAN)
        scm_to_c_native_boolean_array(S2C_ARGS);
    else if (item_type == G_TYPE_UNICHAR)
        scm_to_c_native_unichar_array(S2C_ARGS);
    else if (item_type == G_TYPE_CHAR
             || item_type == G_TYPE_UCHAR
             || item_type == G_TYPE_INT16
             || item_type == G_TYPE_UINT16
             || item_type == G_TYPE_INT32
             || item_type == G_TYPE_UINT32
             || item_type == G_TYPE_INT64
             || item_type == G_TYPE_UINT64
             || item_type == G_TYPE_FLOAT
             || item_type == G_TYPE_DOUBLE
             || fundamental_item_type == G_TYPE_ENUM || fundamental_item_type == G_TYPE_FLAGS)
        scm_to_c_native_immediate_array(S2C_ARGS);
    else if (item_type == G_TYPE_STRING || item_type == G_TYPE_LOCALE_STRING)
        scm_to_c_native_string_array(S2C_ARGS);
    else if (item_type == G_TYPE_VARIANT)
        scm_to_c_native_interface_array(S2C_ARGS);
    else
        UNHANDLED;
}

static void
scm_to_c_native_boolean_array(S2C_ARG_DECL)
{
    TRACE_S2C();
    // For booleans, we expect a vector of booleans
    if (!scm_is_vector(object))
        scm_wrong_type_arg_msg(subr, argpos, object, "vector of booleans");
    *size = scm_c_vector_length(object);
    if (meta->gtype == G_TYPE_ZERO_TERMINATED_CARRAY) {
        arg->v_pointer = malloc(sizeof(gboolean) * (*size + 1));
        ((gboolean *)arg->v_pointer)[*size] = 0;
        LATER_FREE(arg->v_pointer);
    }
    else {
        arg->v_pointer = malloc(sizeof(gboolean) * *size);
        LATER_FREE(arg->v_pointer);
    }
    for (gsize i = 0; i < *size; i++)
        ((gboolean *)(arg->v_pointer))[i] = (gboolean)scm_is_true(scm_c_vector_ref(object, i));
}

static void
scm_to_c_native_unichar_array(S2C_ARG_DECL)
{
    TRACE_S2C();
    // When asked to convert an SCM to an array of unichars,
    // we expect that SCM to be a string.
    if (!scm_is_string(object))
        scm_wrong_type_arg_msg(subr, argpos, object, "string");
    *size = scm_c_string_length(object);
    if (meta->gtype == G_TYPE_ZERO_TERMINATED_CARRAY) {
        arg->v_pointer = malloc(sizeof(gunichar) * (*size + 1));
        ((gunichar *)arg->v_pointer)[*size] = 0;
        LATER_FREE(arg->v_pointer);
    }
    else {
        arg->v_pointer = malloc(sizeof(gunichar) * *size);
        LATER_FREE(arg->v_pointer);
    }
    for (gsize i = 0; i < *size; i++)
        ((gunichar *)(arg->v_pointer))[i] = (gunichar)SCM_CHAR(scm_c_string_ref(object, i));
}

static void
scm_to_c_native_immediate_array(S2C_ARG_DECL)
{
    TRACE_S2C();
#define FUNC_NAME "%object->c-native-immediate-array-arg"
    // IMMEDIATE TYPES.  It seems only double, and 8 and 32-bit
    // integer arrays are ever used. Sometimes deep copy.  Sometimes
    // zero terminated.

    if (G_TYPE_FUNDAMENTAL(meta->params[0].gtype) == G_TYPE_ENUM
        || G_TYPE_FUNDAMENTAL(meta->params[0].gtype) == G_TYPE_FLAGS)
        // FIXME: figure out where this should have been filled in upstream
        meta->params[0].item_size = sizeof(int);

    g_assert_cmpint(meta->params[0].item_size, !=, 0);

    if (scm_is_bytevector(object)) {
        *size = SCM_BYTEVECTOR_LENGTH(object) / meta->params[0].item_size;
        if (!meta->is_transfer_ownership) {
            if (meta->gtype != G_TYPE_ZERO_TERMINATED_CARRAY) {
                // The fast path
                arg->v_pointer = SCM_BYTEVECTOR_CONTENTS(object);
            }
            else {
                gsize len = SCM_BYTEVECTOR_LENGTH(object);
                // Adding null terminator element.
                arg->v_pointer = g_malloc0(len + meta->params[0].item_size);
                LATER_FREE(arg->v_pointer);
                memcpy(arg->v_pointer, SCM_BYTEVECTOR_CONTENTS(object), len);
            }
        }
        else if (meta->is_transfer_ownership) {
            if (meta->gtype != G_TYPE_ZERO_TERMINATED_CARRAY) {
                arg->v_pointer = g_memdup(SCM_BYTEVECTOR_CONTENTS(object),
                                          SCM_BYTEVECTOR_LENGTH(object));
            }
            else {
                gsize len = SCM_BYTEVECTOR_LENGTH(object);
                // Note, null terminated here.
                arg->v_pointer = g_malloc0(len + meta->params[0].item_size);
                memcpy(arg->v_pointer, SCM_BYTEVECTOR_CONTENTS(object), len);
            }
        }
    }
    else
        scm_wrong_type_arg_msg(subr, argpos, object, "bytevector");
#undef FUNC_NAME
}

static void
scm_to_c_byte_array(S2C_ARG_DECL)
{
    TRACE_S2C();
    if (scm_is_bytevector(object)) {
        gpointer contents = SCM_BYTEVECTOR_CONTENTS(object);
        gsize len = SCM_BYTEVECTOR_LENGTH(object);
        *size = len;
        if (meta->is_transfer_ownership == GI_TRANSFER_EVERYTHING)
            arg->v_pointer = g_byte_array_new_take(contents, len);
        else
            arg->v_pointer = g_byte_array_new_take(g_memdup(contents, len), len);
    }
    else
        scm_wrong_type_arg_msg(subr, argpos, object, "bytevector");
}

static void
scm_to_c_garray(S2C_ARG_DECL)
{
    TRACE_S2C();
    GIArgument _arg;
    GigTypeMeta _meta = *meta;
    _meta.gtype = G_TYPE_LENGTH_CARRAY;
    gig_argument_scm_to_c(subr, argpos, &_meta, object, NULL, &_arg, size);
    arg->v_pointer = g_new0(GArray, 1);
    ((GArray *)(arg->v_pointer))->len = *size;
    ((GArray *)(arg->v_pointer))->data = _arg.v_pointer;
}

static void
scm_to_c_native_interface_array(S2C_ARG_DECL)
{
    TRACE_S2C();
#define FUNC_NAME "%object->c-native-interface-array-arg"
    GType item_type = meta->params[0].gtype;
    GType fundamental_item_type = G_TYPE_FUNDAMENTAL(item_type);
    if (fundamental_item_type == G_TYPE_OBJECT
        || fundamental_item_type == G_TYPE_INTERFACE || fundamental_item_type == G_TYPE_VARIANT) {
        // If we are a Struct or Object, we need to look up
        // our actual GType.
        if (!scm_is_vector(object))
            scm_wrong_type_arg_msg(subr, argpos, object, "vector of objects");
        *size = scm_c_vector_length(object);
        if (meta->is_ptr) {
            if (meta->gtype == G_TYPE_ZERO_TERMINATED_CARRAY) {
                arg->v_pointer = malloc(sizeof(gpointer) * (*size + 1));
                ((gpointer *)arg->v_pointer)[*size] = 0;
            }
            else {
                arg->v_pointer = malloc(sizeof(gpointer) * *size);
            }
            for (gsize i = 0; i < *size; i++) {
                gpointer p = gig_type_peek_object(scm_c_vector_ref(object, i));
                if (meta->is_transfer_ownership) {
                    if (fundamental_item_type == G_TYPE_BOXED) {
                        ((gpointer *)(arg->v_pointer))[i] = g_memdup(p, meta->params[0].item_size);
                        // ((gpointer *)(arg->v_pointer))[i] = p;
                    }
                    else {
                        ((gpointer *)(arg->v_pointer))[i] = p;
                        g_object_ref(p);
                    }
                }
                else
                    ((gpointer *)(arg->v_pointer))[i] = p;
            }
        }
        else {
            if (meta->gtype == G_TYPE_ZERO_TERMINATED_CARRAY)
                arg->v_pointer = g_malloc0(meta->item_size * (*size + 1));
            else
                arg->v_pointer = malloc(sizeof(gpointer) * *size);
            for (gsize i = 0; i < *size; i++) {
                gpointer p = gig_type_peek_object(scm_c_vector_ref(object, i));
                if (meta->is_transfer_ownership)
                    memcpy((char *)(arg->v_pointer) + i * meta->item_size,
                           g_memdup(p, meta->item_size), meta->item_size);
                else
                    memcpy((char *)(arg->v_pointer) + i * meta->item_size, p, meta->item_size);

            }
        }
    }
    else {
        // Everything else is unhandled.
        UNHANDLED;
    }
#undef FUNC_NAME
}



static void
scm_to_c_native_string_array(S2C_ARG_DECL)
{
    TRACE_S2C();
    // This is a C array of string pointers, e.g. char **

    // We're adding a NULL termination to the list of strings
    // regardless of the value of array_is_zero_terminated, because it
    // does no harm.
    if (scm_is_vector(object)) {
        scm_t_array_handle handle;
        gsize len;
        gssize inc;
        const SCM *elt;

        elt = scm_vector_elements(object, &handle, &len, &inc);
        *size = len;
        gchar **strv = g_new0(gchar *, len + 1);
        LATER_FREE(strv);

        for (gsize i = 0; i < len; i++, elt += inc) {
            if (meta->params[0].gtype == G_TYPE_LOCALE_STRING)
                strv[i] = scm_to_locale_string(*elt);
            else
                strv[i] = scm_to_utf8_string(*elt);
            LATER_FREE(strv[i]);
        }
        strv[len] = NULL;
        arg->v_pointer = strv;

        scm_array_handle_release(&handle);
    }
    else if (scm_is_true(scm_list_p(object))) {
        gsize len = scm_to_size_t(scm_length(object));
        *size = len;
        gchar **strv = g_new0(gchar *, len + 1);
        LATER_FREE(strv);
        SCM iter = object;
        for (gsize i = 0; i < len; i++) {
            SCM elt = scm_car(iter);
            if (meta->params[0].gtype == G_TYPE_LOCALE_STRING)
                strv[i] = scm_to_locale_string(elt);
            else
                strv[i] = scm_to_utf8_string(elt);
            iter = scm_cdr(iter);
            LATER_FREE(strv[i]);
        }
        strv[len] = NULL;
        arg->v_pointer = strv;
    }
    else
        scm_wrong_type_arg_msg(subr, argpos, object, "list or vector of strings");
}

//////////////////////////////////////////////////////////
// CONVERTING GIARGUMENTS TO SCM OBJECTS
//////////////////////////////////////////////////////////


void
gig_argument_c_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();

    GType fundamental_type = G_TYPE_FUNDAMENTAL(meta->gtype);
    TRACE_C2S();
    switch (fundamental_type) {
    case G_TYPE_NONE:
        *object = SCM_UNSPECIFIED;
        break;
    case G_TYPE_INTERFACE:
        c_interface_to_scm(C2S_ARGS);
        break;
    case G_TYPE_CHAR:
    case G_TYPE_UCHAR:
        c_char_to_scm(C2S_ARGS);
        break;
    case G_TYPE_BOOLEAN:
        c_boolean_to_scm(C2S_ARGS);
        break;
    case G_TYPE_INT:
    case G_TYPE_UINT:
    case G_TYPE_LONG:
    case G_TYPE_ULONG:
    case G_TYPE_INT64:
    case G_TYPE_UINT64:
        c_integer_to_scm(C2S_ARGS);
        break;
    case G_TYPE_ENUM:
    case G_TYPE_FLAGS:
        c_enum_to_scm(C2S_ARGS);
        break;
    case G_TYPE_FLOAT:
    case G_TYPE_DOUBLE:
        c_real_to_scm(C2S_ARGS);
        break;
    case G_TYPE_STRING:
        c_string_to_scm(C2S_ARGS);
        break;
    case G_TYPE_POINTER:
        c_pointer_to_scm(C2S_ARGS);
        break;
    case G_TYPE_BOXED:
        c_boxed_to_scm(C2S_ARGS);
        break;
    case G_TYPE_PARAM:
        c_param_to_scm(C2S_ARGS);
        break;
    case G_TYPE_OBJECT:
        c_object_to_scm(C2S_ARGS);
        break;
    case G_TYPE_VARIANT:
        c_variant_to_scm(C2S_ARGS);
        break;
    default:
        UNHANDLED;
        break;
    }
}

static void
c_char_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();
    if (meta->gtype == G_TYPE_CHAR)
        *object = scm_from_int8(arg->v_int8);
    else if (meta->gtype == G_TYPE_UCHAR)
        *object = scm_from_uint8(arg->v_uint8);
}

static void
c_boolean_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();
    *object = scm_from_bool(arg->v_boolean);
}

static void
c_integer_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();
    if (meta->gtype == G_TYPE_INT16)
        *object = scm_from_int16(arg->v_int16);
    else if (meta->gtype == G_TYPE_UINT16)
        *object = scm_from_uint16(arg->v_uint16);
    else if (meta->gtype == G_TYPE_INT32)
        *object = scm_from_int32(arg->v_int32);
    else if (meta->gtype == G_TYPE_UINT32)
        *object = scm_from_uint32(arg->v_uint32);
    else if (meta->gtype == G_TYPE_INT64)
        *object = scm_from_int64(arg->v_int64);
    else if (meta->gtype == G_TYPE_UINT64)
        *object = scm_from_uint64(arg->v_uint64);
    else if (meta->gtype == G_TYPE_UNICHAR)
        *object = SCM_MAKE_CHAR(arg->v_uint32);
    else
        UNHANDLED;
}

static void
c_real_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();
    if (meta->gtype == G_TYPE_FLOAT)
        *object = scm_from_double((double)arg->v_float);
    else if (meta->gtype == G_TYPE_DOUBLE)
        *object = scm_from_double(arg->v_double);
    else
        UNHANDLED;
}

static void
c_boxed_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();
    if (meta->gtype == G_TYPE_LENGTH_CARRAY) {
        meta->length = size;
        c_native_array_to_scm(C2S_ARGS);
    }
    else if (meta->gtype == G_TYPE_FIXED_SIZE_CARRAY ||
             meta->gtype == G_TYPE_ZERO_TERMINATED_CARRAY)
        c_native_array_to_scm(C2S_ARGS);
    else if (meta->gtype == G_TYPE_BYTE_ARRAY)
        c_byte_array_to_scm(C2S_ARGS);
    else if (meta->gtype == G_TYPE_ARRAY)
        c_garray_to_scm(C2S_ARGS);
    else if (meta->gtype == G_TYPE_PTR_ARRAY)
        c_gptrarray_to_scm(C2S_ARGS);
    else if (meta->gtype == G_TYPE_LIST || meta->gtype == G_TYPE_SLIST)
        c_list_to_scm(C2S_ARGS);
    else
        *object =
            gig_type_transfer_object(meta->gtype, arg->v_pointer, meta->is_transfer_ownership);
}

static void
c_interface_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();
    *object = gig_type_transfer_object(meta->gtype, arg->v_pointer, meta->is_transfer_ownership);
}

static void
c_object_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();
    *object = gig_type_transfer_object(meta->gtype, arg->v_pointer, meta->is_transfer_ownership);
}

static void
c_variant_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();
    *object = gig_type_transfer_object(meta->gtype, arg->v_pointer, meta->is_transfer_ownership);
}

static void
c_param_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();
    *object = gig_type_transfer_object(meta->gtype, arg->v_pointer, meta->is_transfer_ownership);
}

static void
c_enum_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();
    *object = scm_from_uint32(arg->v_uint32);
}

static void
c_string_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();
    // We can't transfer strings directly, since GObject and Guile use
    // different internal encodings.  So for GI_TRANSFER_EVERYTHGING,
    // we just free.
    if (meta->gtype == G_TYPE_STRING || meta->gtype == G_TYPE_LOCALE_STRING) {
        if (!arg->v_string)
            *object = scm_c_make_string(0, SCM_MAKE_CHAR(0));
        else {
            if (meta->gtype == G_TYPE_STRING) {
                if (size != GIG_ARRAY_SIZE_UNKNOWN)
                    *object = scm_from_utf8_stringn(arg->v_string, size);
                else
                    *object = scm_from_utf8_string(arg->v_string);
            }
            else {
                if (size != GIG_ARRAY_SIZE_UNKNOWN)
                    *object = scm_from_locale_stringn(arg->v_string, size);
                else
                    *object = scm_from_locale_string(arg->v_string);
            }
            if (meta->is_transfer_ownership) {
                g_free(arg->v_string);
                arg->v_string = NULL;
            }
        }
    }
    else
        UNHANDLED;
}

static void
c_array_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();
    if (meta->gtype == G_TYPE_LENGTH_CARRAY)
        meta->length = size;
    if (meta->gtype == G_TYPE_LENGTH_CARRAY
        || meta->gtype == G_TYPE_FIXED_SIZE_CARRAY || meta->gtype == G_TYPE_ZERO_TERMINATED_CARRAY)
        c_native_array_to_scm(C2S_ARGS);
    else if (meta->gtype == G_TYPE_PTR_ARRAY)
        c_gptrarray_to_scm(C2S_ARGS);
    else
        UNHANDLED;
}

static void
c_native_array_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();
    gsize length = array_length(meta, arg);
    if (length == GIG_ARRAY_SIZE_UNKNOWN) {
        length = size;
    }
    if (length == 0 && meta->is_nullable) {
        *object = SCM_BOOL_F;
        return;
    }

#define TRANSFER(_type,_short_type)                                     \
    do {                                                                \
        gsize sz;                                                       \
        if (!g_size_checked_mul(&sz, length, meta->params[0].item_size) || sz == G_MAXSIZE) \
            scm_misc_error(subr, "Array size overflow", SCM_EOL);               \
        if (sz == 0) \
            *object = scm_make_ ## _short_type ## vector (scm_from_int(0), scm_from_int(0)); \
        else if (meta->is_transfer_ownership) \
            *object = scm_take_ ## _short_type ## vector((_type *)(arg->v_pointer), length); \
        else                                                            \
            *object = scm_take_ ## _short_type ## vector((_type *)g_memdup(arg->v_pointer, sz), length); \
    } while(0)

    GType item_type = meta->params[0].gtype;
    if (item_type == G_TYPE_NONE && meta->params[0].item_size > 0) {
        *object = scm_c_make_vector(length, SCM_BOOL_F);
        scm_t_array_handle handle;
        size_t len;
        ssize_t inc;
        SCM *elt;
        elt = scm_vector_writable_elements(*object, &handle, &len, &inc);
        for (gsize k = 0; k < len; k++, elt += inc) {
            *elt = scm_c_make_bytevector(meta->params[0].item_size);
            memcpy(SCM_BYTEVECTOR_CONTENTS(*elt),
                   arg->v_pointer + k * meta->params[0].item_size, meta->params[0].item_size);
        }
        scm_array_handle_release(&handle);
        if (meta->is_transfer_ownership) {
            free(arg->v_pointer);
            arg->v_pointer = 0;
        }
    }
    else if (item_type == G_TYPE_CHAR)
        TRANSFER(gint8, s8);
    else if (item_type == G_TYPE_UCHAR)
        TRANSFER(guint8, u8);
    else if (item_type == G_TYPE_INT16)
        TRANSFER(gint16, s16);
    else if (item_type == G_TYPE_UINT16)
        TRANSFER(guint16, u16);
    else if (item_type == G_TYPE_INT32)
        TRANSFER(gint32, s32);
    else if (item_type == G_TYPE_UINT32)
        TRANSFER(guint32, u32);
    else if (item_type == G_TYPE_INT64)
        TRANSFER(gint64, s64);
    else if (item_type == G_TYPE_UINT64)
        TRANSFER(guint64, u64);
    else if (item_type == G_TYPE_FLOAT)
        TRANSFER(gfloat, f32);
    else if (item_type == G_TYPE_DOUBLE)
        TRANSFER(gdouble, f64);
    else if (item_type == G_TYPE_GTYPE)
        UNHANDLED;
    else if (item_type == G_TYPE_BOOLEAN) {
        *object = scm_c_make_vector(length, SCM_BOOL_F);
        scm_t_array_handle handle;
        size_t len;
        ssize_t inc;
        SCM *elt;
        elt = scm_vector_writable_elements(*object, &handle, &len, &inc);
        for (gsize k = 0; k < len; k++, elt += inc)
            *elt = ((gboolean *)(arg->v_pointer))[k] ? SCM_BOOL_T : SCM_BOOL_F;
        scm_array_handle_release(&handle);
        if (meta->is_transfer_ownership) {
            free(arg->v_pointer);
            arg->v_pointer = 0;
        }
    }
    else if (item_type == G_TYPE_UNICHAR) {
        *object = scm_c_make_string(length, SCM_MAKE_CHAR(0));
        for (gsize k = 0; k < length; k++)
            scm_c_string_set_x(*object, k, SCM_MAKE_CHAR(((gunichar *)(arg->v_pointer))[k]));
        if (meta->is_transfer_ownership) {
            free(arg->v_pointer);
            arg->v_pointer = 0;
        }
    }
    else if (item_type == G_TYPE_VARIANT) {
        *object = scm_c_make_vector(length, SCM_BOOL_F);
        scm_t_array_handle handle;
        size_t len;
        ssize_t inc;
        SCM *elt;
        elt = scm_vector_writable_elements(*object, &handle, &len, &inc);
        g_assert_nonnull(arg->v_pointer);

        GIArgument _arg;
        _arg.v_pointer = arg->v_pointer;

        for (gsize k = 0; k < len; k++, elt += inc, _arg.v_pointer += meta->item_size)
            gig_argument_c_to_scm(subr, argpos, &meta->params[0], &_arg, elt, -1);

        scm_array_handle_release(&handle);
        if (meta->is_transfer_ownership) {
            free(arg->v_pointer);
            arg->v_pointer = 0;
        }
    }
    else if (item_type == G_TYPE_STRING || item_type == G_TYPE_LOCALE_STRING) {
        *object = scm_c_make_vector(length, SCM_BOOL_F);
        scm_t_array_handle handle;
        gsize len;
        gssize inc;
        SCM *elt;

        elt = scm_vector_writable_elements(*object, &handle, &len, &inc);
        g_assert(len == length);

        for (gsize i = 0; i < length; i++, elt += inc) {
            gchar *str = ((gchar **)(arg->v_pointer))[i];
            if (str) {
                if (item_type == G_TYPE_STRING)
                    *elt = scm_from_utf8_string(str);
                else
                    *elt = scm_from_locale_string(str);
            }
            if (meta->is_transfer_ownership) {
                free(((gchar **)(arg->v_pointer))[i]);
                ((gchar **)(arg->v_pointer))[i] = NULL;
            }
        }
        if (meta->is_transfer_ownership || meta->is_transfer_container) {
            free(arg->v_pointer);
            arg->v_pointer = NULL;
        }
        scm_array_handle_release(&handle);
    }
    else
        UNHANDLED;
    g_assert(!SCM_UNBNDP(*object));
#undef TRANSFER
}

static void
c_byte_array_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();
    GByteArray *byte_array = arg->v_pointer;
    *object = scm_c_make_bytevector(byte_array->len);
    memcpy(SCM_BYTEVECTOR_CONTENTS(*object), byte_array->data, byte_array->len);
    if (meta->is_transfer_ownership)
        g_byte_array_free(byte_array, TRUE);
    else
        g_byte_array_free(byte_array, FALSE);
}

static void
c_garray_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();
    GigTypeMeta _meta = *meta;
    GIArgument _arg;
    GArray *array = arg->v_pointer;
    _arg.v_pointer = array->data;
    _meta.gtype = G_TYPE_FIXED_SIZE_CARRAY;
    _meta.length = array->len;

    size = array->len;
    c_array_to_scm(subr, argpos, &_meta, &_arg, object, size);
}

static void
c_gptrarray_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();
    GigTypeMeta _meta = *meta;
    GIArgument _arg;
    GPtrArray *array = arg->v_pointer;
    _arg.v_pointer = array->pdata;
    _meta.gtype = G_TYPE_FIXED_SIZE_CARRAY;
    _meta.length = array->len;
    size = array->len;
    c_array_to_scm(subr, argpos, &_meta, &_arg, object, size);
}

static void
c_list_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();
    // Actual conversion
    gpointer list = arg->v_pointer, data;
    GList *_list = NULL;
    GSList *_slist = NULL;
    gsize length;

    // Step 1: allocate
    if (meta->gtype == G_TYPE_LIST) {
        _list = list;
        length = g_list_length(_list);
    }
    else if (meta->gtype == G_TYPE_SLIST) {
        _slist = list;
        length = g_slist_length(_slist);
    }
    else
        g_assert_not_reached();

    *object = scm_make_list(scm_from_size_t(length), SCM_UNDEFINED);

    SCM out_iter = *object;

    // Step 2: iterate
    while (list != NULL) {
        if (meta->gtype == G_TYPE_LIST) {
            data = &_list->data;
            list = _list = _list->next;
        }
        else {
            data = &_slist->data;
            list = _slist = _slist->next;
        }

        if (!meta->params[0].is_ptr) {
            GType item_type = meta->params[0].gtype;
            if (item_type == G_TYPE_CHAR)
                scm_set_car_x(out_iter, scm_from_int8(*(gint8 *)data));
            else if (item_type == G_TYPE_INT16)
                scm_set_car_x(out_iter, scm_from_int16(*(gint16 *)data));
            else if (item_type == G_TYPE_INT32)
                scm_set_car_x(out_iter, scm_from_int32(*(gint32 *)data));
            else if (item_type == G_TYPE_INT64)
                scm_set_car_x(out_iter, scm_from_int64(*(gint64 *)data));
            else if (item_type == G_TYPE_UCHAR)
                scm_set_car_x(out_iter, scm_from_uint8(*(guint8 *)data));
            else if (item_type == G_TYPE_UINT16)
                scm_set_car_x(out_iter, scm_from_uint16(*(guint16 *)data));
            else if (item_type == G_TYPE_UINT32)
                scm_set_car_x(out_iter, scm_from_uint32(*(guint32 *)data));
            else if (item_type == G_TYPE_UINT64)
                scm_set_car_x(out_iter, scm_from_uint64(*(guint64 *)data));
            else if (item_type == G_TYPE_FLOAT)
                scm_set_car_x(out_iter, scm_from_double(*(float *)data));
            else if (item_type == G_TYPE_DOUBLE)
                scm_set_car_x(out_iter, scm_from_double(*(double *)data));
            else if (item_type == G_TYPE_UNICHAR)
                scm_set_car_x(out_iter, SCM_MAKE_CHAR(*(guint32 *)data));
            else if (item_type == G_TYPE_GTYPE) {
                gig_type_register(*(gsize *)data);
                scm_set_car_x(out_iter, scm_from_size_t(*(gsize *)data));
            }
            else
                UNHANDLED;
        }
        else {
            GIArgument _arg;
            GigTypeMeta _meta = meta->params[0];
            SCM elt;
            _arg.v_pointer = *(void **)data;

            gig_argument_c_to_scm(subr, argpos, &_meta, &_arg, &elt, -1);
            scm_set_car_x(out_iter, elt);
        }

        out_iter = scm_cdr(out_iter);
    }
    if (meta->is_transfer_ownership || meta->is_transfer_container) {
        if (meta->gtype == G_TYPE_LIST)
            g_list_free(arg->v_pointer);
        else
            g_slist_free(arg->v_pointer);
        arg->v_pointer = NULL;
    }
}

static void
c_pointer_to_scm(C2S_ARG_DECL)
{
    TRACE_C2S();
    if (arg->v_pointer == NULL && meta->is_nullable)
        *object = SCM_BOOL_F;
    else if (meta->gtype == G_TYPE_GTYPE)
        *object = scm_from_uintptr_t(arg->v_pointer);
    else if (meta->gtype == G_TYPE_CALLBACK)
        *object =
            gig_type_transfer_object(meta->gtype, arg->v_pointer, meta->is_transfer_ownership);
    else if (size != GIG_ARRAY_SIZE_UNKNOWN) {
        SCM bv = scm_c_make_bytevector(size);
        memcpy(SCM_BYTEVECTOR_CONTENTS(bv), arg->v_pointer, size);
        *object = bv;
    }
    else
        *object = scm_from_pointer(arg->v_pointer, NULL);
}

#define SCONSTX(NAME) scm_permanent_object(scm_c_define(#NAME, scm_from_int(NAME)))

void
gig_init_argument(void)
{
}
