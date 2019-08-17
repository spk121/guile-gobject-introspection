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
#include "gig_callback.h"
#include "gig_data_type.h"
#include "gig_object.h"
#include "gig_type.h"
#include "gig_value.h"
#include "gig_arg_map.h"

#if GIG_DEBUG_TRANSFERS
#define TRACE_V2S() g_debug("[V2S] In '%s', on line %d while handling %s.", __func__, __LINE__, gig_type_meta_describe(meta))
#define TRACE_S2V() g_debug("[S2V] In '%s', on line %d while handling %s.", __func__, __LINE__, gig_type_meta_describe(meta))
#else
#define TRACE_V2S()
#define TRACE_S2V()
#endif

#define SURPRISING \
    do { \
    g_warning("Unusual argument type '%s' %s:%d", gig_type_meta_describe(meta), __FILE__, __LINE__); \
    } while(FALSE)

#define UNHANDLED                               \
    do { \
    g_error("Unhandled argument type '%s' %s:%d", gig_type_meta_describe(meta), __FILE__, __LINE__); \
    } while(FALSE)

// Fundamental types
static GigValueReturn scm_interface_to_value(SCM src, const GigTypeMeta *meta, GValue *dest);
static GigValueReturn scm_char_to_value(SCM src, const GigTypeMeta *meta, GValue *dest);
static GigValueReturn scm_boolean_to_value(SCM src, const GigTypeMeta *meta, GValue *dest);
static GigValueReturn scm_integer_to_value(SCM src, const GigTypeMeta *meta, GValue *dest);
static GigValueReturn scm_enum_to_value(SCM src, const GigTypeMeta *meta, GValue *dest);
static GigValueReturn scm_real_to_value(SCM src, const GigTypeMeta *meta, GValue *dest);
static GigValueReturn scm_string_to_value(SCM src, const GigTypeMeta *meta, GValue *dest);
static GigValueReturn scm_pointer_to_value(SCM src, const GigTypeMeta *meta, GValue *dest);
static GigValueReturn scm_boxed_to_value(SCM src, const GigTypeMeta *meta, GValue *dest);
static GigValueReturn scm_object_to_value(SCM src, const GigTypeMeta *meta, GValue *dest);
static GigValueReturn scm_variant_to_value(SCM src, const GigTypeMeta *meta, GValue *dest);

// Derived types
static GigValueReturn scm_list_to_value(SCM src, const GigTypeMeta *meta, GValue *dest);
static GigValueReturn scm_array_to_value(SCM src, const GigTypeMeta *meta, GValue *dest);

static GigValueReturn interface_value_to_scm(const GValue *src, const GigTypeMeta *meta,
                                             SCM *dest);
static GigValueReturn char_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest);
static GigValueReturn boolean_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest);
static GigValueReturn integer_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest);
static GigValueReturn enum_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest);
static GigValueReturn real_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest);
static GigValueReturn string_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest);
static GigValueReturn pointer_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest);
static GigValueReturn boxed_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest);
static GigValueReturn boxed_array_value_to_scm(const GValue *src, const GigTypeMeta *meta,
                                               SCM *dest);
static GigValueReturn boxed_byte_array_value_to_scm(const GValue *src, const GigTypeMeta *meta,
                                                    SCM *dest);
static GigValueReturn boxed_ptr_array_value_to_scm(const GValue *src, const GigTypeMeta *meta,
                                                   SCM *dest);
static GigValueReturn boxed_list_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest);

void
gig_value_preset_type(GigArgMapEntry *entry, GValue *val)
{
    GType type = entry->meta.gtype;
    GType fundamental_type = G_TYPE_FUNDAMENTAL(type);

    if (type == G_TYPE_INVALID)
        g_value_init(val, G_TYPE_VOID);
    else if (type == G_TYPE_ENUM)
        // G_TYPE_ENUM is a base class, and shouldn't be used directly
        g_value_init(val, G_TYPE_INT);
    else if (type == G_TYPE_FLAGS)
        // G_TYPE_FLAGS is a base class, and shouldn't be used directly
        g_value_init(val, G_TYPE_UINT);
    else if (type == G_TYPE_LENGTH_CARRAY || type == G_TYPE_FIXED_SIZE_CARRAY ||
             type == G_TYPE_ZERO_TERMINATED_CARRAY) {
        if (entry->meta.params[0].item_size == 0) {
            g_value_init(val, G_TYPE_PTR_ARRAY);
            GPtrArray *arr = g_ptr_array_new();
            g_value_set_boxed(val, arr);
        }
        else {
            g_value_init(val, G_TYPE_ARRAY);
            GArray *arr = g_array_new(FALSE, TRUE, entry->meta.params[0].item_size);
            g_value_set_boxed(val, arr);
        }
    }
    else if (type == G_TYPE_PTR_ARRAY) {
        g_value_init(val, G_TYPE_PTR_ARRAY);
        GPtrArray *arr = g_ptr_array_new();
        g_value_set_boxed(val, arr);
    }
    else if (type == G_TYPE_BYTE_ARRAY) {
        g_value_init(val, G_TYPE_BYTE_ARRAY);
        GByteArray *arr = g_byte_array_new();
        g_value_set_boxed(val, arr);
    }
    else
        g_value_init(val, type);
}


////////////////////////////////////////////////////////////////

GigValueReturn
gig_scm_to_value_full(SCM src, const GigTypeMeta *meta, GValue *dest)
{
    TRACE_S2V();

    GigValueReturn ret;

    GType type = meta->gtype;
    GType fundamental_type = G_TYPE_FUNDAMENTAL(type);

    if (fundamental_type == G_TYPE_INVALID)
        ret = GIG_VALUE_INVALID;
    else if (fundamental_type == G_TYPE_NONE || fundamental_type == G_TYPE_VOID)
        ret = GIG_VALUE_OK;
    else if (fundamental_type == G_TYPE_INTERFACE)
        ret = scm_interface_to_value(src, meta, dest);
    else if (fundamental_type == G_TYPE_CHAR || fundamental_type == G_TYPE_UCHAR)
        ret = scm_char_to_value(src, meta, dest);
    else if (fundamental_type == G_TYPE_BOOLEAN)
        ret = scm_boolean_to_value(src, meta, dest);
    else if (fundamental_type == G_TYPE_INT || fundamental_type == G_TYPE_UINT ||
             fundamental_type == G_TYPE_LONG || fundamental_type == G_TYPE_ULONG ||
             fundamental_type == G_TYPE_INT64 || fundamental_type == G_TYPE_UINT64)
        ret = scm_integer_to_value(src, meta, dest);
    else if (fundamental_type == G_TYPE_ENUM || fundamental_type == G_TYPE_FLAGS)
        ret = scm_enum_to_value(src, meta, dest);
    else if (fundamental_type == G_TYPE_FLOAT || fundamental_type == G_TYPE_DOUBLE)
        ret = scm_real_to_value(src, meta, dest);
    else if (fundamental_type == G_TYPE_STRING)
        ret = scm_string_to_value(src, meta, dest);
    else if (fundamental_type == G_TYPE_POINTER)
        ret = scm_pointer_to_value(src, meta, dest);
    else if (fundamental_type == G_TYPE_BOXED)
        ret = scm_boxed_to_value(src, meta, dest);
    else if (fundamental_type == G_TYPE_PARAM)
        ret = GIG_VALUE_UNIMPLEMENTED;
    else if (fundamental_type == G_TYPE_OBJECT)
        ret = scm_object_to_value(src, meta, dest);
    else if (fundamental_type == G_TYPE_VARIANT)
        ret = scm_variant_to_value(src, meta, dest);
    else
        ret = GIG_VALUE_INVALID;

    return ret;
}

static GigValueReturn
scm_interface_to_value(SCM src, const GigTypeMeta *meta, GValue *dest)
{
    TRACE_S2V();
    if (meta->is_nullable && scm_is_false(src)) {
        g_value_set_object(dest, NULL);
        return GIG_VALUE_OK;
    }
    else {
        g_value_set_object(dest, gig_type_peek_object(src));
        return GIG_VALUE_UNIMPLEMENTED;
    }
}

static GigValueReturn
scm_char_to_value(SCM src, const GigTypeMeta *meta, GValue *dest)
{
    TRACE_S2V();
    GType t = meta->gtype;

    if (!scm_is_integer(src) && !SCM_CHARP(src))
        return GIG_VALUE_WRONG_TYPE;

    if (t == G_TYPE_CHAR) {
        if (SCM_CHARP(src)) {
            if (SCM_CHAR(src) > 255)
                return GIG_VALUE_OUT_OF_RANGE;
            else {
                g_value_set_schar(dest, SCM_CHAR(src));
                return GIG_VALUE_OK;
            }
        }
        else if (!scm_is_signed_integer(src, INT8_MIN, INT8_MAX))
            return GIG_VALUE_OUT_OF_RANGE;
        else {
            g_value_set_schar(dest, scm_to_int8(src));
            return GIG_VALUE_OK;
        }
    }
    else if (t == G_TYPE_UCHAR) {
        if (SCM_CHARP(src)) {
            if (SCM_CHAR(src) > 255)
                return GIG_VALUE_OUT_OF_RANGE;
            else {
                g_value_set_uchar(dest, SCM_CHAR(src));
                return GIG_VALUE_OK;
            }
        }
        else if (!scm_is_unsigned_integer(src, 0, UINT8_MAX))
            return GIG_VALUE_OUT_OF_RANGE;
        else {
            g_value_set_uchar(dest, scm_to_uint8(src));
            return GIG_VALUE_OK;
        }
    }
    else
        g_assert_not_reached();
}

GigValueReturn
scm_boolean_to_value(SCM src, const GigTypeMeta *meta, GValue *dest)
{
    TRACE_S2V();

    if (!scm_is_eq(src, SCM_BOOL_T) && !scm_is_eq(src, SCM_BOOL_F))
        return GIG_VALUE_WRONG_TYPE;
    g_value_set_boolean(dest, scm_is_true(src));
    return GIG_VALUE_OK;
}

GigValueReturn
scm_integer_to_value(SCM src, const GigTypeMeta *meta, GValue *dest)
{
    TRACE_S2V();

    if (!scm_is_integer(src) && !SCM_CHARP(src))
        return GIG_VALUE_WRONG_TYPE;
    if ((meta->gtype == G_TYPE_INT && !scm_is_signed_integer(src, INT_MIN, INT_MAX))
        || (meta->gtype == G_TYPE_LONG && !scm_is_signed_integer(src, LONG_MIN, LONG_MAX))
        || (meta->gtype == G_TYPE_INT16 && !scm_is_signed_integer(src, INT16_MIN, INT16_MAX))
        || (meta->gtype == G_TYPE_INT32 && !scm_is_signed_integer(src, INT32_MIN, INT32_MAX))
        || (meta->gtype == G_TYPE_INT64 && !scm_is_signed_integer(src, INT64_MIN, INT64_MAX))
        || (meta->gtype == G_TYPE_UINT && !scm_is_unsigned_integer(src, 0, UINT_MAX))
        || (meta->gtype == G_TYPE_ULONG && !scm_is_unsigned_integer(src, 0, ULONG_MAX))
        || (meta->gtype == G_TYPE_UINT16 && !scm_is_unsigned_integer(src, 0, UINT16_MAX))
        || (meta->gtype == G_TYPE_UINT32 && !scm_is_unsigned_integer(src, 0, UINT32_MAX))
        || (meta->gtype == G_TYPE_UINT64 && !scm_is_unsigned_integer(src, 0, UINT64_MAX)))
        return GIG_VALUE_OUT_OF_RANGE;

#define T(GTYPE,TYPE,SCM_FUNC,VALUE_FUNC)                               \
    do {                                                                \
        if (meta->gtype == GTYPE) {                                     \
            VALUE_FUNC(dest, SCM_FUNC(src));                            \
        }                                                               \
    } while(0)

    T(G_TYPE_INT, gint, scm_to_int, g_value_set_int);
    T(G_TYPE_LONG, glong, scm_to_long, g_value_set_long);
    T(G_TYPE_INT16, gint, scm_to_int, g_value_set_int);
    T(G_TYPE_INT32, gint, scm_to_int, g_value_set_int);
    T(G_TYPE_UNICHAR, gint, SCM_CHAR, g_value_set_int);
    T(G_TYPE_INT64, gint64, scm_to_int64, g_value_set_int64);

    T(G_TYPE_UINT, guint, scm_to_uint, g_value_set_uint);
    T(G_TYPE_ULONG, gulong, scm_to_ulong, g_value_set_ulong);
    T(G_TYPE_UINT16, guint, scm_to_uint, g_value_set_uint);
    T(G_TYPE_UINT32, guint, scm_to_uint, g_value_set_uint);
    T(G_TYPE_UINT64, guint64, scm_to_uint64, g_value_set_uint64);

#undef T
    return GIG_VALUE_OK;
}

GigValueReturn
scm_enum_to_value(SCM src, const GigTypeMeta *meta, GValue *dest)
{
    TRACE_S2V();

    if (!scm_is_integer(src))
        return GIG_VALUE_WRONG_TYPE;

    // FIXME: could take a string
    // FIXME: could rangecheck
    if (G_TYPE_FUNDAMENTAL(meta->gtype) == G_TYPE_ENUM) {
        if (meta->gtype == G_TYPE_ENUM) {
            // G_TYPE_ENUM is an abstract base class, so
            // if that is the type, fall back to integer.
            g_value_set_int(dest, scm_to_int(src));
        }
        else {
            g_value_set_enum(dest, scm_to_int(src));
        }
    }
    else if (G_TYPE_FUNDAMENTAL(meta->gtype) == G_TYPE_FLAGS) {
        if (meta->gtype == G_TYPE_FLAGS) {
            // G_TYPE_FLAGS is an abstract base classs, so if
            // that is our type, fail back to integer
            g_value_set_uint(dest, scm_to_uint(src));
        }
        else {
            g_value_set_flags(dest, scm_to_uint(src));
        }
    }

    return GIG_VALUE_OK;
}

GigValueReturn
scm_real_to_value(SCM src, const GigTypeMeta *meta, GValue *dest)
{
    TRACE_S2V();

    if (!scm_is_real(src))
        return GIG_VALUE_WRONG_TYPE;
    if (meta->gtype == G_TYPE_FLOAT) {
        gdouble dtmp = scm_to_double(src);
        if (dtmp > FLT_MAX || dtmp < -FLT_MAX)
            return GIG_VALUE_OUT_OF_RANGE;
    }

#define T(GTYPE,TYPE,SCM_FUNC)                                          \
    do {                                                                \
        if (meta->gtype == GTYPE) {                                     \
            g_value_set_##TYPE(dest, SCM_FUNC(src));                    \
        }                                                               \
    } while(0)

    T(G_TYPE_FLOAT, float, scm_to_double);
    T(G_TYPE_DOUBLE, double, scm_to_double);
#undef T
    return GIG_VALUE_OK;
}

GigValueReturn
scm_string_to_value(SCM src, const GigTypeMeta *meta, GValue *dest)
{
    TRACE_S2V();

    if (meta->is_nullable && scm_is_false(src)) {
        g_value_set_static_string(dest, NULL);
        return GIG_VALUE_OK;
    }

    if (!scm_is_string(src) && !scm_is_bytevector(src))
        return GIG_VALUE_WRONG_TYPE;

    g_assert_cmpint(meta->is_ptr, ==, 1);

    char *p;
    if (meta->gtype == G_TYPE_LOCALE_STRING)
        p = scm_to_locale_string(src);
    else
        p = scm_to_utf8_string(src);

    if (meta->is_transfer_ownership)
        // If here, this GValue's contents will ultimately be freed by
        // the C function, not by the GValue.  We pretend it is a
        // static string, so that the GValue doesn't free it.
        g_value_set_static_string(dest, p);
    else
        g_value_take_string(dest, p);
    return GIG_VALUE_OK;
}

GigValueReturn
scm_pointer_to_value(SCM src, const GigTypeMeta *meta, GValue *dest)
{
    TRACE_S2V();

    if (meta->is_nullable && scm_is_false(src)) {
        g_value_set_pointer(dest, NULL);
        return GIG_VALUE_OK;
    }

    // Hit the special cases first,
    if (meta->gtype == G_TYPE_CALLBACK) {
        if (!scm_is_true(scm_procedure_p(src)))
            return GIG_VALUE_WRONG_TYPE;
        g_value_set_pointer(dest, gig_callback_get_ptr(meta->callable_info, src));
        return GIG_VALUE_OK;
    }
    else if (meta->gtype == G_TYPE_GTYPE) {
        g_value_set_pointer(dest, GSIZE_TO_POINTER(scm_to_gtype(src)));
        return GIG_VALUE_OK;
    }
    else {
        // As a fallback
        if (SCM_POINTER_P(src)) {
            g_value_set_pointer(dest, scm_to_pointer(src));
            return GIG_VALUE_OK;
        }
        else if (scm_is_bytevector(src)) {
            g_value_set_pointer(dest, SCM_BYTEVECTOR_CONTENTS(src));
            return GIG_VALUE_OK;
        }
        else
            return GIG_VALUE_WRONG_TYPE;
    }

    return GIG_VALUE_UNIMPLEMENTED;
}

static GigValueReturn
scm_boxed_to_value(SCM src, const GigTypeMeta *meta, GValue *dest)
{
    TRACE_S2V();

    if (meta->gtype == G_TYPE_LENGTH_CARRAY
        || meta->gtype == G_TYPE_ZERO_TERMINATED_CARRAY
        || meta->gtype == G_TYPE_FIXED_SIZE_CARRAY || meta->gtype == G_TYPE_ARRAY) {
        return scm_array_to_value(src, meta, dest);
    }
    else if (meta->gtype == G_TYPE_LIST || meta->gtype == G_TYPE_SLIST) {
        return scm_list_to_value(src, meta, dest);
    }
    else {
        // FIXME: ownership
        if (scm_is_false(src) && meta->is_nullable) {
            g_value_take_boxed(dest, NULL);
            return GIG_VALUE_OK;
        }
        else {
            g_value_take_boxed(dest, gig_type_peek_object(src));
            return GIG_VALUE_OK;
        }
    }
    return GIG_VALUE_UNIMPLEMENTED;
}

static GigValueReturn
scm_variant_to_value(SCM src, const GigTypeMeta *meta, GValue *dest)
{
    TRACE_S2V();
    GVariant *v = gig_type_peek_object(src);
    g_value_set_variant(dest, v);

    return GIG_VALUE_OK;
}

GigValueReturn
scm_array_to_value(SCM src, const GigTypeMeta *meta, GValue *dest)
{
    TRACE_S2V();

    GigValueReturn ret;

    if (scm_is_bytevector(src)) {
        gsize len = SCM_BYTEVECTOR_LENGTH(src) / meta->params[0].item_size;
        if (meta->gtype == G_TYPE_FIXED_SIZE_CARRAY && len != meta->length)
            return GIG_VALUE_WRONG_TYPE;

        GArray *arr = g_value_get_boxed(dest);
        //g_array_sized_new(meta->gtype == G_TYPE_ZERO_TERMINATED_CARRAY,
        //                                FALSE,
        //                                meta->params[0].item_size, len);
        if (meta->is_caller_allocates) {
            g_array_set_size(arr, len);
            arr->data = SCM_BYTEVECTOR_CONTENTS(src);
        }
        else
            g_array_append_vals(arr, SCM_BYTEVECTOR_CONTENTS(src), len);
        // A GValue frees a boxed GArray by unreffing it, so if we're
        // transferring ownership, so that a C procedure frees the
        // array contents, we ref it.

        //if (meta->is_transfer_ownership)
        //    g_array_ref(arr);
    }
    else if (scm_is_vector(src)) {
        gsize len = scm_c_vector_length(src);

        if (meta->gtype == G_TYPE_FIXED_SIZE_CARRAY && len != meta->length)
            return GIG_VALUE_WRONG_TYPE;

        scm_t_array_handle handle;
        gssize inc;
        const SCM *elt;

        //GArray *arr = g_array_sized_new(meta->gtype == G_TYPE_ZERO_TERMINATED_CARRAY,
        //                                TRUE,
        //                                meta->params[0].item_size, len);
        GArray *arr = g_value_get_boolean(dest);

        elt = scm_vector_elements(src, &handle, &len, &inc);
        for (gsize i = 0; i < len; i++, elt += inc) {
            GValue _dest = G_VALUE_INIT;
            switch (G_TYPE_FUNDAMENTAL(meta->params[0].gtype)) {
            case G_TYPE_INVALID:
            case G_TYPE_NONE:
                break;
            case G_TYPE_INTERFACE:
            {
                ret = scm_interface_to_value(*elt, &meta->params[0], &_dest);
                gpointer v = g_value_get_object(&_dest);
                g_array_append_val(arr, v);
            }
                break;
            case G_TYPE_CHAR:
            {
                ret = scm_char_to_value(*elt, &meta->params[0], &_dest);
                gint8 v = g_value_get_schar(&_dest);
                g_array_append_val(arr, v);
            }
                break;
            case G_TYPE_UCHAR:
            {
                ret = scm_char_to_value(*elt, &meta->params[0], &_dest);
                guint8 v = g_value_get_uchar(&_dest);
                g_array_append_val(arr, v);
            }
                break;
            case G_TYPE_BOOLEAN:
            {
                ret = scm_boolean_to_value(*elt, &meta->params[0], &_dest);
                gboolean v = g_value_get_boolean(&_dest);
                g_array_append_val(arr, v);
            }
                break;
            case G_TYPE_INT:
            {
                ret = scm_integer_to_value(*elt, &meta->params[0], &_dest);
                gint v = g_value_get_int(&_dest);
                g_array_append_val(arr, v);
            }
                break;
            case G_TYPE_UINT:
            {
                ret = scm_integer_to_value(*elt, &meta->params[0], &_dest);
                guint v = g_value_get_uint(&_dest);
                g_array_append_val(arr, v);
            }
                break;
            case G_TYPE_INT64:
            {
                ret = scm_integer_to_value(*elt, &meta->params[0], &_dest);
                gint64 v = g_value_get_int64(&_dest);
                g_array_append_val(arr, v);
            }
                break;
            case G_TYPE_UINT64:
            {
                ret = scm_integer_to_value(*elt, &meta->params[0], &_dest);
                guint64 v = g_value_get_uint64(&_dest);
                g_array_append_val(arr, v);
            }
            case G_TYPE_LONG:
            {
                ret = scm_integer_to_value(*elt, &meta->params[0], &_dest);
                glong v = g_value_get_long(&_dest);
                g_array_append_val(arr, v);
            }
                break;
            case G_TYPE_ULONG:
            {
                ret = scm_integer_to_value(*elt, &meta->params[0], &_dest);
                gulong v = g_value_get_ulong(&_dest);
                g_array_append_val(arr, v);
            }
                break;
            case G_TYPE_ENUM:
            {
                ret = scm_enum_to_value(*elt, &meta->params[0], &_dest);
                if (G_VALUE_HOLDS_ENUM(&_dest)) {
                    gint v = g_value_get_enum(&_dest);
                    g_array_append_val(arr, v);
                }
                else {
                    gint v = g_value_get_int(&_dest);
                    g_array_append_val(arr, v);
                }
            }
                break;
            case G_TYPE_FLAGS:
            {
                ret = scm_enum_to_value(*elt, &meta->params[0], &_dest);
                if (G_VALUE_HOLDS_FLAGS(&_dest)) {
                    guint v = g_value_get_flags(&_dest);
                    g_array_append_val(arr, v);
                }
                else {
                    guint v = g_value_get_uint(&_dest);
                    g_array_append_val(arr, v);
                }
            }
                break;
            case G_TYPE_FLOAT:
            {
                ret = scm_real_to_value(*elt, &meta->params[0], &_dest);
                gfloat v = g_value_get_float(&_dest);
                g_array_append_val(arr, v);
            }
                break;
            case G_TYPE_DOUBLE:
            {
                ret = scm_real_to_value(*elt, &meta->params[0], &_dest);
                gdouble v = g_value_get_double(&_dest);
                g_array_append_val(arr, v);
            }
                break;
            case G_TYPE_STRING:
            {
                ret = scm_string_to_value(*elt, &meta->params[0], &_dest);
                // Since '_dest' is a temp variable, we take responsibility for the allocated string.
                gchar *v = g_value_dup_string(&_dest);
                g_array_append_val(arr, v);
            }
                break;
            case G_TYPE_POINTER:
            {
                ret = scm_pointer_to_value(*elt, &meta->params[0], &_dest);
                gpointer v = g_value_get_pointer(&_dest);
                g_array_append_val(arr, v);
            }
                break;
            case G_TYPE_BOXED:
                // ret = transform_scm_boxed_to_value(src, meta, dest, free_list, &_size);
                ret = GIG_VALUE_UNIMPLEMENTED;
                break;
            case G_TYPE_PARAM:
                // ret = transform_scm_param_to_value(src, meta, dest, free_list);
                ret = GIG_VALUE_UNIMPLEMENTED;
                break;
            case G_TYPE_OBJECT:
                // ret = transform_scm_object_to_value(src, meta, dest, free_list);
                ret = GIG_VALUE_UNIMPLEMENTED;
                break;
            case G_TYPE_VARIANT:
            {
                ret = scm_variant_to_value(*elt, &meta->params[0], &_dest);
                GVariant *v = g_value_get_variant(&_dest);
                g_array_append_val(arr, v);
            }
                break;
            default:
                ret = GIG_VALUE_INVALID;
                break;
            }
            g_value_unset(&_dest);
            if (ret != GIG_VALUE_OK)
                return ret;
        }
        scm_array_handle_release(&handle);
        // g_value_take_boxed(dest, arr);
    }
    else if (scm_is_string(src)) {
        gsize len;
        if (meta->params[0].gtype != G_TYPE_UNICHAR)
            return GIG_VALUE_WRONG_TYPE;

        len = scm_c_string_length(src);
        //GArray *arr = g_array_sized_new(meta->gtype == G_TYPE_ZERO_TERMINATED_CARRAY,
        //                                TRUE,
        //                               sizeof(gunichar), len);
        GArray *arr = g_value_get_boxed(dest);
        for (gsize i = 0; i < len; i++)
            ((gunichar *)(arr->data))[i] = (gunichar)SCM_CHAR(scm_c_string_ref(src, i));
        // g_value_take_boxed(dest, arr);
    }

    return GIG_VALUE_OK;
}

static GigValueReturn
scm_object_to_value(SCM src, const GigTypeMeta *meta, GValue *dest)
{
    TRACE_S2V();
    if (g_type_is_a(meta->gtype, G_TYPE_OBJECT)) {
        if (scm_is_false(src)) {
            g_value_set_object(dest, NULL);
            return GIG_VALUE_OK;
        }
        else if (!G_TYPE_CHECK_INSTANCE_TYPE
                 (gig_type_peek_object(src), G_TYPE_FUNDAMENTAL(meta->gtype)))
            return GIG_VALUE_WRONG_TYPE;
        else {
            g_value_set_object(dest, gig_type_peek_object(src));
            return GIG_VALUE_OK;
        }
    }
    return GIG_VALUE_UNIMPLEMENTED;
}

GigValueReturn
scm_list_to_value(SCM src, const GigTypeMeta *meta, GValue *dest)
{
    TRACE_S2V();

    GigValueReturn ret;
    GList *list = NULL;
    GSList *slist = NULL;

    if (!scm_is_true(scm_list_p(src)))
        return GIG_VALUE_WRONG_TYPE;
    if (scm_is_null(src)) {
        g_value_set_pointer(dest, NULL);
        return GIG_VALUE_OK;
    }

#define APPEND(x)                                   \
    do {                                            \
        if (meta->gtype == G_TYPE_LIST)             \
            list = g_list_prepend(list, (gpointer)(x)); \
        else if (meta->gtype == G_TYPE_SLIST)       \
            slist = g_slist_prepend(slist, (gpointer)(x));  \
    } while (0)

#define REVERSE                                 \
    do {                                        \
        if (meta->gtype == G_TYPE_LIST)         \
            list = g_list_reverse(list);        \
        else if (meta->gtype == G_TYPE_SLIST)   \
            slist = g_slist_reverse(slist);     \
    } while (0)

    SCM car = scm_car(src);
    SCM rest = scm_cdr(src);
    while (scm_is_false(scm_null_p(rest))) {
        GValue _dest;

        switch (G_TYPE_FUNDAMENTAL(meta->params[0].gtype)) {
        case G_TYPE_INVALID:
        case G_TYPE_NONE:
            ret = GIG_VALUE_INVALID;
            break;
        case G_TYPE_INTERFACE:
            // ret = transform_scm_interface_to_arg(src, meta, dest, free_list);
            ret = GIG_VALUE_UNIMPLEMENTED;
            break;
        case G_TYPE_CHAR:
            ret = scm_char_to_value(car, &meta->params[0], &_dest);
            APPEND(GINT_TO_POINTER(g_value_get_schar(&_dest)));
            break;
        case G_TYPE_UCHAR:
            ret = scm_char_to_value(car, &meta->params[0], &_dest);
            APPEND(GUINT_TO_POINTER(g_value_get_uchar(&_dest)));
            break;
        case G_TYPE_BOOLEAN:
            ret = scm_boolean_to_value(car, &meta->params[0], &_dest);
            APPEND(GINT_TO_POINTER(g_value_get_boolean(&_dest)));
            break;
        case G_TYPE_INT:
            ret = scm_integer_to_value(car, &meta->params[0], &_dest);
            APPEND(GINT_TO_POINTER(g_value_get_int(&_dest)));
            break;
        case G_TYPE_UINT:
            ret = scm_integer_to_value(car, &meta->params[0], &_dest);
            APPEND(GUINT_TO_POINTER(g_value_get_uint(&_dest)));
            break;
        case G_TYPE_INT64:
        case G_TYPE_UINT64:
        case G_TYPE_LONG:
        case G_TYPE_ULONG:
        case G_TYPE_ENUM:
        case G_TYPE_FLAGS:
        case G_TYPE_FLOAT:
        case G_TYPE_DOUBLE:
            ret = GIG_VALUE_INVALID;
            break;
        case G_TYPE_STRING:
            ret = scm_string_to_value(car, &meta->params[0], &_dest);
            APPEND(g_value_get_string(&_dest));
            break;
        case G_TYPE_POINTER:
            ret = scm_pointer_to_value(car, &meta->params[0], &_dest);
            APPEND(g_value_get_pointer(&_dest));
            break;
        case G_TYPE_BOXED:
            // gsize _size;
            // ret = transform_scm_boxed_to_value(src, meta, dest, free_list, &_size);
            ret = GIG_VALUE_UNIMPLEMENTED;
            break;
        case G_TYPE_PARAM:
            // ret = transform_scm_param_to_value(src, meta, dest, free_list);
            ret = GIG_VALUE_UNIMPLEMENTED;
            break;
        case G_TYPE_OBJECT:
            // ret = transform_scm_object_to_value(src, meta, dest, free_list);
            ret = GIG_VALUE_UNIMPLEMENTED;
            break;
        case G_TYPE_VARIANT:
            // ret = transform_scm_variant_to_value(src, meta, dest, free_list);
            ret = GIG_VALUE_UNIMPLEMENTED;
            break;
        default:
            ret = GIG_VALUE_INVALID;
            break;
        }
        if (ret != GIG_VALUE_OK)
            return ret;
        car = scm_car(rest);
        rest = scm_cdr(rest);
    }
    REVERSE;
    if (meta->gtype == G_TYPE_LIST)
        g_value_take_boxed(dest, list);
    else
        g_value_take_boxed(dest, slist);

#undef APPEND
#undef REVERSE

    return GIG_VALUE_OK;
}

////////////////////////////////////////////////////////////////

GigValueReturn
gig_value_to_scm_full(const GValue *src, const GigTypeMeta *meta, SCM *dest)
{
    TRACE_V2S();

    GigValueReturn ret;
    GType type = meta->gtype;
    GType fundamental_type = G_TYPE_FUNDAMENTAL(type);

    if (fundamental_type == G_TYPE_VOID) {
        *dest = SCM_UNSPECIFIED;
        ret = GIG_VALUE_OK;
    }
    else if (fundamental_type == G_TYPE_INTERFACE)
        ret = interface_value_to_scm(src, meta, dest);
    else if (fundamental_type == G_TYPE_CHAR || fundamental_type == G_TYPE_UCHAR)
        ret = char_value_to_scm(src, meta, dest);
    else if (fundamental_type == G_TYPE_BOOLEAN)
        ret = boolean_value_to_scm(src, meta, dest);
    else if (fundamental_type == G_TYPE_INT || fundamental_type == G_TYPE_UINT ||
             fundamental_type == G_TYPE_LONG || fundamental_type == G_TYPE_ULONG ||
             fundamental_type == G_TYPE_INT64 || fundamental_type == G_TYPE_UINT64)
        ret = integer_value_to_scm(src, meta, dest);
    else if (fundamental_type == G_TYPE_ENUM || fundamental_type == G_TYPE_FLAGS)
        ret = enum_value_to_scm(src, meta, dest);
    else if (fundamental_type == G_TYPE_FLOAT || fundamental_type == G_TYPE_DOUBLE)
        ret = real_value_to_scm(src, meta, dest);
    else if (fundamental_type == G_TYPE_STRING)
        ret = string_value_to_scm(src, meta, dest);
    else if (fundamental_type == G_TYPE_POINTER)
        ret = pointer_value_to_scm(src, meta, dest);
    else if (fundamental_type == G_TYPE_BOXED)
        ret = boxed_value_to_scm(src, meta, dest);
    else if (fundamental_type == G_TYPE_OBJECT)
        ret = GIG_VALUE_UNIMPLEMENTED;
    else
        ret = GIG_VALUE_UNIMPLEMENTED;
    return ret;
}

void
gig_value_to_scm_full_with_error(const GValue *src, const GigTypeMeta *meta, SCM *dest,
                                 const gchar *subr)
{
    GigValueReturn ret = gig_value_to_scm_full(src, meta, dest);
    if (ret == GIG_VALUE_INVALID) {
        SCM type1 = scm_from_utf8_string(G_VALUE_TYPE_NAME(src));
        SCM type2 = scm_from_utf8_string(g_type_name(meta->gtype));
        scm_misc_error(subr, "internal type error: ~S != ~S", scm_list_2(type1, type2));
    }
    else if (ret == GIG_VALUE_UNIMPLEMENTED) {
        SCM type1 = scm_from_utf8_string(G_VALUE_TYPE_NAME(src));
        scm_misc_error(subr,
                       "internal conversion error: conversion from ~S to ~S is unimplemented",
                       scm_list_2(type1, scm_from_utf8_string(g_type_name(meta->gtype))));
    }
    else if (ret == GIG_VALUE_OUT_OF_RANGE)
        scm_misc_error(subr, "internal type range error", SCM_EOL);
    else if (ret == GIG_VALUE_WRONG_TYPE)
        scm_misc_error(subr, "internal type error", SCM_EOL);
}

static GigValueReturn
interface_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest)
{
    TRACE_V2S();
    *dest = gig_type_transfer_object(meta->gtype,
                                     g_value_get_object(src), meta->is_transfer_ownership);
    return GIG_VALUE_OK;
}

static GigValueReturn
char_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest)
{
    TRACE_V2S();
    if (G_TYPE_FUNDAMENTAL(meta->gtype) == G_TYPE_CHAR) {
        if (G_VALUE_HOLDS_CHAR(src)) {
            *dest = scm_from_int8(g_value_get_schar(src));
            return GIG_VALUE_OK;
        }
        else
            return GIG_VALUE_WRONG_TYPE;
    }
    else if (G_TYPE_FUNDAMENTAL(meta->gtype) == G_TYPE_UCHAR) {
        if (G_VALUE_HOLDS_UCHAR(src)) {
            *dest = scm_from_uint8(g_value_get_uchar(src));
            return GIG_VALUE_OK;
        }
        else
            return GIG_VALUE_WRONG_TYPE;
    }
    return GIG_VALUE_INVALID;
}

static GigValueReturn
boolean_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest)
{
    TRACE_V2S();
    if (G_VALUE_HOLDS_BOOLEAN(src)) {
        *dest = scm_from_bool(g_value_get_boolean(src));
        return GIG_VALUE_OK;
    }
    return GIG_VALUE_INVALID;
}

static GigValueReturn
integer_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest)
{
    TRACE_V2S();
    GType src_type = G_VALUE_TYPE(src);

    if (src_type == G_TYPE_UNICHAR) {
        *dest = SCM_MAKE_CHAR(g_value_get_int(src));  
        return GIG_VALUE_OK;
    }  
    // Remember that int16, etc are packed into their fundamental
    // types, so we don't have to do them specifically.
    else if (G_TYPE_FUNDAMENTAL(src_type) == G_TYPE_INT) {
        if (G_VALUE_HOLDS_INT(src)) {
            *dest = scm_from_int(g_value_get_int(src));
            return GIG_VALUE_OK;
        }
        else
            return GIG_VALUE_WRONG_TYPE;
    }
    else if (src_type == G_TYPE_INT64) {
        if (G_VALUE_HOLDS_INT64(src)) {
            *dest = scm_from_int64(g_value_get_int64(src));
            return GIG_VALUE_OK;
        }
        else
            return GIG_VALUE_WRONG_TYPE;
    }
    else if (src_type == G_TYPE_LONG) {
        if (G_VALUE_HOLDS_LONG(src)) {
            *dest = scm_from_long(g_value_get_long(src));
            return GIG_VALUE_OK;
        }
        else
            return GIG_VALUE_WRONG_TYPE;
    }
    else if (G_TYPE_FUNDAMENTAL(src_type) == G_TYPE_UINT)
        if (G_VALUE_HOLDS_UINT(src)) {
            *dest = scm_from_uint(g_value_get_uint(src));
            return GIG_VALUE_OK;
        }
        else
            return GIG_VALUE_WRONG_TYPE;
    else if (src_type == G_TYPE_UINT64) {
        if (G_VALUE_HOLDS_UINT64(src)) {
            *dest = scm_from_uint64(g_value_get_uint64(src));
            return GIG_VALUE_OK;
        }
        else
            return GIG_VALUE_WRONG_TYPE;
    }
    else if (src_type == G_TYPE_ULONG) {
        if (G_VALUE_HOLDS_ULONG(src)) {
            *dest = scm_from_ulong(g_value_get_ulong(src));
            return GIG_VALUE_OK;
        }
        else
            return GIG_VALUE_WRONG_TYPE;
    }

    return GIG_VALUE_INVALID;
}

static GigValueReturn
enum_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest)
{
    TRACE_V2S();
    GType src_type = G_VALUE_TYPE(src);
    GType fundamental_type = G_TYPE_FUNDAMENTAL(src_type);

    if (fundamental_type == G_TYPE_ENUM) {
        if (G_VALUE_HOLDS_ENUM(src)) {
            *dest = scm_from_int(g_value_get_enum(src));
            return GIG_VALUE_OK;
        }
        else
            return GIG_VALUE_WRONG_TYPE;
    }
    else if (fundamental_type == G_TYPE_INT) {
        if (G_VALUE_HOLDS_INT(src)) {
            *dest = scm_from_int(g_value_get_int(src));
            return GIG_VALUE_OK;
        }
        else
            return GIG_VALUE_WRONG_TYPE;
    }
    else if (fundamental_type == G_TYPE_FLAGS) {
        if (G_VALUE_HOLDS_FLAGS(src)) {
            *dest = scm_from_uint(g_value_get_flags(src));
            return GIG_VALUE_OK;
        }
        else
            return GIG_VALUE_WRONG_TYPE;
    }
    else if (fundamental_type == G_TYPE_UINT) {
        if (G_VALUE_HOLDS_UINT(src)) {
            *dest = scm_from_uint(g_value_get_uint(src));
            return GIG_VALUE_OK;
        }
        else
            return GIG_VALUE_WRONG_TYPE;
    }
    g_return_val_if_reached(GIG_VALUE_UNIMPLEMENTED);
}

static GigValueReturn
real_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest)
{
    TRACE_V2S();
    GType src_type = G_VALUE_TYPE(src);
    GType fundamental_type = G_TYPE_FUNDAMENTAL(src_type);

    if (fundamental_type == G_TYPE_FLOAT) {
        if (G_VALUE_HOLDS_FLOAT(src)) {
            *dest = scm_from_double(g_value_get_float(src));
            return GIG_VALUE_OK;
        }
        else
            return GIG_VALUE_WRONG_TYPE;
    }
    else if (fundamental_type == G_TYPE_DOUBLE) {
        if (G_VALUE_HOLDS_DOUBLE(src)) {
            *dest = scm_from_double(g_value_get_double(src));
            return GIG_VALUE_OK;
        }
        else
            return GIG_VALUE_WRONG_TYPE;
    }
    g_return_val_if_reached(GIG_VALUE_UNIMPLEMENTED);
}

static GigValueReturn
string_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest)
{
    TRACE_V2S();
    GType src_type = G_VALUE_TYPE(src);
    GType fundamental_type = G_TYPE_FUNDAMENTAL(src_type);

    if (fundamental_type == G_TYPE_STRING) {
        if (G_VALUE_HOLDS_STRING(src)) {
            if (meta->is_nullable && g_value_get_string(src) == NULL)
                *dest = SCM_BOOL_F;
            else if (src_type == G_TYPE_LOCALE_STRING)
                *dest = scm_from_locale_string(g_value_dup_string(src));
            else
                *dest = scm_from_utf8_string(g_value_dup_string(src));
            return GIG_VALUE_OK;
        }
        else
            return GIG_VALUE_WRONG_TYPE;
    }
    else
        g_return_val_if_reached(GIG_VALUE_UNIMPLEMENTED);
}

static GigValueReturn
pointer_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest)
{
    TRACE_V2S();
    GType src_type = G_VALUE_TYPE(src);
    GType fundamental_type = G_TYPE_FUNDAMENTAL(src_type);

    if (fundamental_type == G_TYPE_POINTER) {
        if ((g_value_get_pointer(src) == NULL) && meta->is_nullable) {
            *dest = SCM_BOOL_F;
            return GIG_VALUE_OK;
        }
        else if (src_type == G_TYPE_GTYPE) {
            *dest = scm_from_uintptr_t(g_value_get_pointer(src));
            return GIG_VALUE_OK;
        }
        else if (src_type == G_TYPE_CALLBACK) {
            *dest = gig_type_transfer_object(src_type, g_value_get_pointer(src),
                                             meta->is_transfer_ownership);
            return GIG_VALUE_OK;
        }
        else {
            *dest = scm_from_pointer(g_value_get_pointer(src), NULL);
            return GIG_VALUE_OK;
        }
    }
    else
        return GIG_VALUE_WRONG_TYPE;
}

static GigValueReturn
boxed_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest)
{
    TRACE_V2S();
    if (meta->gtype == G_TYPE_LENGTH_CARRAY
        || meta->gtype == G_TYPE_FIXED_SIZE_CARRAY
        || meta->gtype == G_TYPE_ZERO_TERMINATED_CARRAY || meta->gtype == G_TYPE_ARRAY) {
        boxed_array_value_to_scm(src, meta, dest);
    }
    else if (meta->gtype == G_TYPE_BYTE_ARRAY)
        boxed_byte_array_value_to_scm(src, meta, dest);
    else if (meta->gtype == G_TYPE_PTR_ARRAY)
        boxed_ptr_array_value_to_scm(src, meta, dest);
    else if (meta->gtype == G_TYPE_LIST || meta->gtype == G_TYPE_SLIST)
        boxed_list_to_scm(src, meta, dest);
    else
        *dest =
            gig_type_transfer_object(meta->gtype, g_value_get_boxed(src),
                                     meta->is_transfer_ownership);
    return GIG_VALUE_OK;
}

static GigValueReturn
boxed_array_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest)
{
    TRACE_V2S();
    GArray *arr = g_value_get_boxed(src);
    if (arr->len == 0 && meta->is_nullable) {
        *dest = SCM_BOOL_F;
        return GIG_VALUE_OK;
    }

#define TRANSFER(_type,_short_type)                                     \
    do {                                                                \
        gsize sz;                                                       \
        if (!g_size_checked_mul(&sz, arr->len, g_array_get_element_size(arr)) || sz == G_MAXSIZE) \
            scm_misc_error("%boxed-value->scm", "Array size overflow", SCM_EOL);               \
        if (sz == 0) \
            *dest = scm_make_ ## _short_type ## vector (scm_from_int(0), scm_from_int(0)); \
        else if (meta->is_transfer_ownership) \
            *dest = scm_take_ ## _short_type ## vector((_type *)(arr->data), arr->len); \
        else                                                            \
            *dest = scm_take_ ## _short_type ## vector((_type *)g_memdup(arr->data, sz), arr->len); \
    } while(0)

    GType item_type = meta->params[0].gtype;
    if (item_type == G_TYPE_NONE && meta->params[0].item_size > 0) {
        *dest = scm_c_make_vector(arr->len, SCM_BOOL_F);
        scm_t_array_handle handle;
        size_t len;
        ssize_t inc;
        SCM *elt;
        elt = scm_vector_writable_elements(*dest, &handle, &len, &inc);
        for (gsize k = 0; k < len; k++, elt += inc) {
            *elt = scm_c_make_bytevector(meta->params[0].item_size);
            memcpy(SCM_BYTEVECTOR_CONTENTS(*elt),
                   arr->data + k * meta->params[0].item_size, meta->params[0].item_size);
        }
        scm_array_handle_release(&handle);
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
        *dest = scm_c_make_vector(arr->len, SCM_BOOL_F);
        scm_t_array_handle handle;
        size_t len;
        ssize_t inc;
        SCM *elt;
        elt = scm_vector_writable_elements(*dest, &handle, &len, &inc);
        for (gsize k = 0; k < len; k++, elt += inc)
            *elt = ((gboolean *)(arr->data))[k] ? SCM_BOOL_T : SCM_BOOL_F;
        scm_array_handle_release(&handle);
    }
    else if (item_type == G_TYPE_UNICHAR) {
        *dest = scm_c_make_string(arr->len, SCM_MAKE_CHAR(0));
        for (gsize k = 0; k < arr->len; k++)
            scm_c_string_set_x(*dest, k, SCM_MAKE_CHAR(((gunichar *)(arr->data))[k]));
    }
    else if (item_type == G_TYPE_VARIANT) {
#if 0
        *dest = scm_c_make_vector(arr->len, SCM_BOOL_F);
        scm_t_array_handle handle;
        size_t len;
        ssize_t inc;
        SCM *elt;
        elt = scm_vector_writable_elements(*dest, &handle, &len, &inc);
        g_assert_nonnull(arr->data);

        GIArgument _arg;
        _arg.v_pointer = arr->data;

        for (gsize k = 0; k < len; k++, elt += inc, _arg.v_pointer += meta->item_size)
            gig_argument_c_to_scm(subr, argpos, &meta->params[0], &_arg, elt, -1);
#endif
        UNHANDLED;
    }
    else if (item_type == G_TYPE_STRING || item_type == G_TYPE_LOCALE_STRING) {
        *dest = scm_c_make_vector(arr->len, SCM_BOOL_F);
        scm_t_array_handle handle;
        gsize len;
        gssize inc;
        SCM *elt;

        elt = scm_vector_writable_elements(*dest, &handle, &len, &inc);
        g_assert(len == arr->len);

        for (gsize i = 0; i < arr->len; i++, elt += inc) {
            gchar *str = ((gchar **)(arr->data))[i];
            if (str) {
                if (item_type == G_TYPE_STRING)
                    *elt = scm_from_utf8_string(str);
                else
                    *elt = scm_from_locale_string(str);
            }
        }
        scm_array_handle_release(&handle);
    }
    else
        UNHANDLED;
    g_assert(!SCM_UNBNDP(*dest));
#undef TRANSFER
    return GIG_VALUE_OK;
}

static GigValueReturn
boxed_byte_array_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest)
{
    TRACE_V2S();
    GByteArray *byte_array = g_value_get_boxed(src);
    *dest = scm_c_make_bytevector(byte_array->len);
    memcpy(SCM_BYTEVECTOR_CONTENTS(*dest), byte_array->data, byte_array->len);
    return GIG_VALUE_OK;
}

static GigValueReturn
boxed_ptr_array_value_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest)
{
    TRACE_V2S();
    GPtrArray *ptr_array = g_value_get_boxed(src);
    *dest = scm_c_make_vector(ptr_array->len, SCM_BOOL_F);
    scm_t_array_handle handle;
    gsize len;
    gssize inc;
    SCM *elt;

    elt = scm_vector_writable_elements(*dest, &handle, &len, &inc);
    g_assert(len == ptr_array->len);

    for (gsize i = 0; i < ptr_array->len; i++, elt += inc)
        *elt = scm_from_pointer(ptr_array->pdata[i], NULL);
    scm_array_handle_release(&handle);
    return GIG_VALUE_OK;
}

static GigValueReturn
boxed_list_to_scm(const GValue *src, const GigTypeMeta *meta, SCM *dest)
{
    TRACE_V2S();
    // Actual conversion
    gpointer list = g_value_get_boxed(src), data;
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

    *dest = scm_make_list(scm_from_size_t(length), SCM_UNDEFINED);

    SCM out_iter = *dest;

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
#if 0
            GIArgument _arg;
            GigTypeMeta _meta = meta->params[0];
            SCM elt;
            _arg.v_pointer = *(void **)data;

            gig_argument_c_to_scm(subr, argpos, &_meta, &_arg, &elt, -1);
            scm_set_car_x(out_iter, elt);
#endif
            UNHANDLED;
        }

        out_iter = scm_cdr(out_iter);
    }
    return GIG_VALUE_OK;
}

////////////////////////////////////////////////////////////////

SCM
gig_value_param_as_scm(const GValue *gvalue, gboolean copy_boxed, const GParamSpec *pspec)
{
    if (G_IS_PARAM_SPEC_UNICHAR(pspec)) {
        scm_t_wchar u;

        u = g_value_get_uint(gvalue);
        return SCM_MAKE_CHAR(u);
    }
    else
        return gig_value_as_scm(gvalue, copy_boxed);

}

static int
gig_value_array_from_scm_list(GValue *value, SCM list)
{
    gssize len, i;
    GArray *array;

    len = scm_to_size_t(scm_length(list));

    array = g_array_new(FALSE, TRUE, sizeof(GValue));

    for (i = 0; i < len; ++i) {
        SCM item = scm_list_ref(list, scm_from_size_t(i));
        GType type;
        GValue item_value = { 0, };

        type = gig_type_get_gtype_from_obj(item);

        gig_value_from_scm(&item_value, item);

        g_array_append_val(array, item_value);
    }

    g_value_take_boxed(value, array);
    return 0;
}


/**
 * gig_value_to_scm_basic_type:
 * @value: the GValue object.
 * @handled: (out): TRUE if the return value is defined
 *
 * This function creates/returns a Python wrapper object that
 * represents the GValue passed as an argument limited to supporting basic types
 * like ints, bools, and strings.
 *
 * Returns: a PyObject representing the value.
 */
SCM
gig_value_to_scm_basic_type(const GValue *value, GType fundamental, gboolean *handled)
{
    *handled = TRUE;
    switch (fundamental) {
    case G_TYPE_CHAR:
        return scm_from_int8(g_value_get_schar(value));
    case G_TYPE_UCHAR:
        return scm_from_uint8(g_value_get_uchar(value));
    case G_TYPE_BOOLEAN:
        return scm_from_bool(g_value_get_boolean(value));
    case G_TYPE_INT:
        return scm_from_int(g_value_get_int(value));
    case G_TYPE_UINT:
        return scm_from_uint(g_value_get_uint(value));
    case G_TYPE_LONG:
        return scm_from_long(g_value_get_long(value));
    case G_TYPE_ULONG:
        return scm_from_ulong(g_value_get_ulong(value));
    case G_TYPE_INT64:
        return scm_from_int64(g_value_get_int64(value));
    case G_TYPE_UINT64:
        return scm_from_uint64(g_value_get_uint64(value));
    case G_TYPE_ENUM:
        /* return gi_genum_from_gtype (G_VALUE_TYPE (value), */
        /*                             g_value_get_enum (value)); */
        return scm_from_ulong(g_value_get_enum(value));
    case G_TYPE_FLAGS:
        /* return gi_gflags_from_gtype (G_VALUE_TYPE (value), */
        /*                              g_value_get_flags (value)); */
        return scm_from_ulong(g_value_get_flags(value));
    case G_TYPE_FLOAT:
        return scm_from_double(g_value_get_float(value));
    case G_TYPE_DOUBLE:
        return scm_from_double(g_value_get_double(value));
    case G_TYPE_STRING:
    {
        const gchar *str = g_value_get_string(value);
        if (str)
            return scm_from_utf8_string(str);
        else
            return SCM_BOOL_F;
    }
    default:
        *handled = FALSE;
        return SCM_BOOL_F;
    }
    g_return_val_if_reached(SCM_BOOL_F);
}

// This function creates and returns a Scheme value that
// represents the GValue passed as an argument.
static SCM
gig_value_to_scm_structured_type(const GValue *value, GType fundamental, gboolean copy_boxed)
{
    switch (fundamental) {
    case G_TYPE_INTERFACE:
    {
        gpointer obj = g_value_get_object(value);
        if (!obj)
            return SCM_BOOL_F;
        else if (g_type_is_a(G_VALUE_TYPE(value), G_TYPE_OBJECT))
            return gig_type_transfer_object(G_OBJECT_TYPE(obj), obj, GI_TRANSFER_NOTHING);
        else
            break;
    }
    case G_TYPE_POINTER:
        // If we get a simple pointer with no context information,
        // what can we do other than return a dumb pointer?
        return scm_from_pointer(g_value_get_pointer(value), NULL);
    case G_TYPE_PARAM:
    {
        GParamSpec *pspec = g_value_get_param(value);
        if (pspec)
            return gig_type_transfer_object(G_TYPE_PARAM, pspec, GI_TRANSFER_NOTHING);
        else
            return SCM_BOOL_F;
    }

    case G_TYPE_BOXED:
    {
        if (G_VALUE_HOLDS(value, G_TYPE_VALUE)) {
            GValue *n_value = g_value_get_boxed(value);
            return gig_value_as_scm(n_value, copy_boxed);
        }
        else if (G_VALUE_HOLDS(value, G_TYPE_GSTRING)) {
            GString *string = (GString *)g_value_get_boxed(value);
            return scm_from_utf8_stringn(string->str, string->len);
        }
        else {
            // if (copy_boxed) ...
            return gig_type_transfer_object(G_VALUE_TYPE(value),
                                            g_value_get_boxed(value), GI_TRANSFER_EVERYTHING);
        }
    }

    case G_TYPE_OBJECT:
    {
        gpointer obj = g_value_get_object(value);
        if (obj)
            return gig_type_transfer_object(G_OBJECT_TYPE(obj), obj, GI_TRANSFER_NOTHING);
        else
            return SCM_BOOL_F;
    }

#if 0
    case G_TYPE_VARIANT:
    {
        GVariant *v = g_value_get_variant(value);
        if (v == NULL) {
            Py_INCREF(Py_None);
            return Py_None;
        }
        return pygi_struct_new_from_g_type(G_TYPE_VARIANT, g_variant_ref(v), FALSE);
    }
#endif
    default:
    {
        // g_assert_not_reached ();
        /* PyGTypeMarshal *bm; */
        /* if ((bm = pyg_type_lookup(G_VALUE_TYPE(value)))) */
        /*  return bm->fromvalue(value); */
        break;
    }
    }

    const gchar *type_name = g_type_name(G_VALUE_TYPE(value));
    if (type_name == NULL)
        type_name = "(null)";
    scm_misc_error("gig_value_to_scm", "unknown type ~S",
                   scm_list_1(scm_from_utf8_string(type_name)));
    g_return_val_if_reached(SCM_BOOL_F);
}


/* Returns an SCM version of the GValue.  If COPY_BOXED,
   try to make a deep copy of the object. */
SCM
gig_value_as_scm(const GValue *value, gboolean copy_boxed)
{
    SCM guobj;
    gboolean handled;
    GType fundamental = G_TYPE_FUNDAMENTAL(G_VALUE_TYPE(value));

#if 0
    if (fundamental == G_TYPE_CHAR)
        return SCM_MAKE_CHAR(g_value_get_schar(value));
    else if (fundamental == G_TYPE_UCHAR)
        return SCM_MAKE_CHAR(g_value_get_uchar(value));
#endif

    guobj = gig_value_to_scm_basic_type(value, fundamental, &handled);
    if (!handled)
        guobj = gig_value_to_scm_structured_type(value, fundamental, copy_boxed);
    return guobj;
}

////////////////////////////////////////////////////////////////


/* Initializes and GValue with the information in an SCM.  The
   conversion relies on the GValue having a type already. */
GigValueReturn
gig_value_from_scm(GValue *value, SCM obj)
{
    g_assert(value != NULL);

    GType value_type = G_VALUE_TYPE(value);

    switch (G_TYPE_FUNDAMENTAL(value_type)) {
    case G_TYPE_CHAR:
    {
        if (!scm_is_exact_integer(obj))
            return GIG_VALUE_WRONG_TYPE;
        if (!scm_is_signed_integer(obj, G_MININT8, G_MAXINT8))
            return GIG_VALUE_OUT_OF_RANGE;
        gint8 temp = scm_to_int8(obj);
        g_value_set_schar(value, temp);
        return GIG_VALUE_OK;
    }
    case G_TYPE_UCHAR:
    {
        if (!scm_is_exact_integer(obj))
            return GIG_VALUE_WRONG_TYPE;
        if (!scm_is_unsigned_integer(obj, 0, G_MAXUINT8))
            return GIG_VALUE_OUT_OF_RANGE;
        guchar temp;
        temp = scm_to_uint8(obj);
        g_value_set_uchar(value, temp);
        return GIG_VALUE_OK;
    }
    case G_TYPE_BOOLEAN:
    {
        if (!scm_is_eq(obj, SCM_BOOL_T) && !scm_is_eq(obj, SCM_BOOL_F))
            return GIG_VALUE_WRONG_TYPE;
        gboolean temp;
        temp = scm_is_true(obj);
        g_value_set_boolean(value, temp);
        return GIG_VALUE_OK;
    }
    case G_TYPE_INT:
    {
        if (!scm_is_exact_integer(obj))
            return GIG_VALUE_WRONG_TYPE;
        if (!scm_is_signed_integer(obj, G_MININT, G_MAXINT))
            return GIG_VALUE_OUT_OF_RANGE;
        gint temp;
        temp = scm_to_int(obj);
        g_value_set_int(value, temp);
        return GIG_VALUE_OK;
    }
    case G_TYPE_UINT:
    {
        if (!scm_is_exact_integer(obj))
            return GIG_VALUE_WRONG_TYPE;
        if (!scm_is_unsigned_integer(obj, 0, G_MAXUINT))
            return GIG_VALUE_OUT_OF_RANGE;
        guint temp;
        temp = scm_to_uint(obj);
        g_value_set_uint(value, temp);
        return GIG_VALUE_OK;
    }
    case G_TYPE_LONG:
    {
        if (!scm_is_exact_integer(obj))
            return GIG_VALUE_WRONG_TYPE;
        if (!scm_is_signed_integer(obj, G_MINLONG, G_MAXLONG))
            return GIG_VALUE_OUT_OF_RANGE;
        glong temp;
        temp = scm_to_long(obj);
        g_value_set_long(value, temp);
        return GIG_VALUE_OK;
    }
    case G_TYPE_ULONG:
    {
        if (!scm_is_exact_integer(obj))
            return GIG_VALUE_WRONG_TYPE;
        if (!scm_is_unsigned_integer(obj, 0, G_MAXULONG))
            return GIG_VALUE_OUT_OF_RANGE;
        gulong temp;
        temp = scm_to_ulong(obj);
        g_value_set_ulong(value, temp);
        return GIG_VALUE_OK;
    }
    case G_TYPE_INT64:
    {
        if (!scm_is_exact_integer(obj))
            return GIG_VALUE_WRONG_TYPE;
        if (!scm_is_signed_integer(obj, G_MININT64, G_MAXINT64))
            return GIG_VALUE_OUT_OF_RANGE;
        gint64 temp;
        temp = scm_to_int64(obj);
        g_value_set_int64(value, temp);
        return GIG_VALUE_OK;
    }
    case G_TYPE_UINT64:
    {
        if (!scm_is_exact_integer(obj))
            return GIG_VALUE_WRONG_TYPE;
        if (!scm_is_unsigned_integer(obj, 0, G_MAXUINT64))
            return GIG_VALUE_OUT_OF_RANGE;
        guint64 temp;
        temp = scm_to_uint64(obj);
        g_value_set_uint64(value, temp);
        return GIG_VALUE_OK;
    }
    case G_TYPE_ENUM:
    {
        if (!scm_is_exact_integer(obj))
            return GIG_VALUE_WRONG_TYPE;
        if (!scm_is_unsigned_integer(obj, 0, G_MAXULONG))
            return GIG_VALUE_OUT_OF_RANGE;
        gint val;
        val = scm_to_ulong(obj);
        g_value_set_enum(value, val);
        return GIG_VALUE_OK;
    }
        break;
    case G_TYPE_FLAGS:
    {
        if (!scm_is_exact_integer(obj))
            return GIG_VALUE_WRONG_TYPE;
        if (!scm_is_unsigned_integer(obj, 0, G_MAXULONG))
            return GIG_VALUE_OUT_OF_RANGE;
        guint val = 0;
        val = scm_to_ulong(obj);
        g_value_set_flags(value, val);
        return GIG_VALUE_OK;
    }
        break;
    case G_TYPE_FLOAT:
    {
        if (!scm_is_true(scm_real_p(obj)))
            return GIG_VALUE_WRONG_TYPE;
        gdouble dval = scm_to_double(obj);
        if (dval < -G_MAXFLOAT || dval > G_MAXFLOAT)
            return GIG_VALUE_OUT_OF_RANGE;
        g_value_set_float(value, dval);
        return GIG_VALUE_OK;
    }
    case G_TYPE_DOUBLE:
    {
        if (!scm_is_true(scm_real_p(obj)))
            return GIG_VALUE_WRONG_TYPE;
        gdouble temp;
        temp = scm_to_double(obj);
        g_value_set_double(value, temp);
        return GIG_VALUE_OK;
    }
    case G_TYPE_STRING:
    {
        if (!scm_is_string(obj))
            return GIG_VALUE_WRONG_TYPE;
        gchar *temp = scm_to_utf8_string(obj);
        g_value_take_string(value, temp);
        return GIG_VALUE_OK;
    }
    case G_TYPE_POINTER:
    {
        if (SCM_POINTER_P(obj))
            g_value_set_pointer(value, scm_to_pointer(obj));
        else if (scm_is_true(scm_bytevector_p(obj)))
            g_value_set_pointer(value, SCM_BYTEVECTOR_CONTENTS(obj));
        else if (gig_type_get_gtype_from_obj(obj) > G_TYPE_INVALID)
            g_value_set_object(value, gig_type_peek_object(obj));
        else
            return GIG_VALUE_WRONG_TYPE;
        return GIG_VALUE_OK;
    }

    case G_TYPE_INTERFACE:
    case G_TYPE_OBJECT:
        /* we only handle interface types that have a GObject prereq */
        if (g_type_is_a(value_type, G_TYPE_OBJECT)) {
            if (scm_is_false(obj)) {
                g_value_set_object(value, NULL);
                return GIG_VALUE_OK;
            }
            else if (!G_TYPE_CHECK_INSTANCE_TYPE(gig_type_peek_object(obj), value_type))
                return GIG_VALUE_WRONG_TYPE;
            else {
                g_value_set_object(value, gig_type_peek_object(obj));
                return GIG_VALUE_OK;
            }
        }
        return GIG_VALUE_WRONG_TYPE;
        break;
    default:
        return GIG_VALUE_UNIMPLEMENTED;
        break;
    }

    return GIG_VALUE_UNIMPLEMENTED;
}

void
gig_value_from_scm_with_error(GValue *value, SCM obj, const gchar *subr, gint pos)
{
    GigValueReturn ret = gig_value_from_scm(value, obj);
    if (ret == GIG_VALUE_INVALID)
        scm_misc_error(subr, "cannot convert ~S: invalid gtype", scm_list_1(obj));
    else if (ret == GIG_VALUE_UNIMPLEMENTED)
        scm_misc_error(subr, "cannot convert ~S: conversion to argument type ~S is unimplemented",
                       scm_list_2(obj, scm_from_utf8_string(g_type_name(G_VALUE_TYPE(value)))));
    else if (ret == GIG_VALUE_OUT_OF_RANGE)
        scm_out_of_range(subr, obj);
    else if (ret == GIG_VALUE_WRONG_TYPE)
        scm_wrong_type_arg_msg(subr, pos, obj, g_type_name(G_VALUE_TYPE(value)));
}

#if 0
void
gig_value_from_arg(GValue *src, GIArgument *arg, const GigTypeMeta *meta, gsize size)
{
    GType src_type = G_VALUE_TYPE(src);

    switch (G_TYPE_FUNDAMENTAL(src_type)) {
    case G_TYPE_INVALID:
    case G_TYPE_NONE:
        arr->data = 0;
        break;
    case G_TYPE_INTERFACE:
        // ret = transform_scm_interface_to_arg(src, meta, dest, free_list);
        UNHANDLED;
        break;
    case G_TYPE_CHAR:
        arg->v_int8 = g_value_get_schar(src);
        break;
    case G_TYPE_UCHAR:
        arg->v_uint8 = g_value_get_uchar(src);
        break;
    case G_TYPE_BOOLEAN:
        arg->v_boolean = g_value_get_boolean(src);
        break;
    case G_TYPE_INT:
        if (meta->gtype == G_TYPE_INT)
            arg->v_int = g_value_get_int(src);
        else if (meta->gtype == G_TYPE_INT16)
            arg->v_int16 = g_value_get_int(src);
        else if (meta->gtype == G_TYPE_INT32)
            arg->v_int32 = g_value_get_int(src);
        else if (meta->gtype == G_TYPE_UNICHAR)
            arg->v_int32 = g_value_get_int(src);
        break;
    case G_TYPE_UINT:
        if (meta->gtype == G_TYPE_UINT)
            arg->v_uint = g_value_get_uint(src);
        else if (meta->gtype == G_TYPE_UINT16)
            arg->v_uint16 = g_value_get_uint(src);
        else if (meta->gtype == G_TYPE_UINT32)
            arg->v_uint32 = g_value_get_uint(src);
        break;
    case G_TYPE_LONG:
        arg->v_long = g_value_get_long(src);
        break;
    case G_TYPE_ULONG:
        arg->v_ulong = g_value_get_ulong(src);
        break;
    case G_TYPE_INT64:
        arg->v_int64 = g_value_get_int64(src);
        break;
    case G_TYPE_UINT64:
        arg->v_uint64 = g_value_get_uint64(src);
        break;
    case G_TYPE_ENUM:
        arg->v_int = g_value_get_enum(src);
        break;
    case G_TYPE_FLAGS:
        arg->v_int = g_value_get_flags(src);
        break;
    case G_TYPE_FLOAT:
        arg->v_float = g_value_get_float(src);
        break;
    case G_TYPE_DOUBLE:
        arg->v_double = g_value_get_double(src);
        break;
    case G_TYPE_STRING:
        arg->v_string = g_value_get_string(src);
        break;
    case G_TYPE_POINTER:
        arr->data = g_value_get_pointer(src);
        break;
    case G_TYPE_BOXED:
        arr->data = g_value_get_boxed(src);
        break;
    case G_TYPE_PARAM:
        // ret = transform_scm_param_to_arg(src, meta, dest, free_list);
        UNHANDLED;
        break;
    case G_TYPE_OBJECT:
        // ret = transform_scm_object_to_arg(src, meta, dest, free_list);
        UNHANDLED;
        break;
    case G_TYPE_VARIANT:
        // ret = transform_scm_variant_to_arg(src, meta, dest, free_list);
        UNHANDLED;
        break;
    default:
        UNHANDLED;
        break;
    }
}
#endif

void
gig_scm_to_value_full_with_error(SCM src, const GigTypeMeta *meta, GValue *dest, const gchar *subr,
                                 gint pos)
{
    GigValueReturn ret = gig_scm_to_value_full(src, meta, dest);
    if (ret == GIG_VALUE_INVALID)
        scm_misc_error(subr, "cannot convert ~S: invalid gtype", scm_list_1(src));
    else if (ret == GIG_VALUE_UNIMPLEMENTED)
        scm_misc_error(subr, "cannot convert ~S: conversion to argument type ~S is unimplemented",
                       scm_list_2(src, scm_from_utf8_string(g_type_name(meta->gtype))));
    else if (ret == GIG_VALUE_OUT_OF_RANGE)
        scm_out_of_range(subr, src);
    else if (ret == GIG_VALUE_WRONG_TYPE)
        scm_wrong_type_arg_msg(subr, pos, src, g_type_name(meta->gtype));
}
