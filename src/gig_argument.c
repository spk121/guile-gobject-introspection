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

#include <glib.h>
#include "gig_argument.h"
#include "gig_value.h"
#include "gig_type.h"

#if GIG_DEBUG_TRANSFERS
#define TRACE_V2A() g_debug("[V2A] In '%s', on line %d while handling %s.", __func__, __LINE__, G_VALUE_TYPE_NAME(val))
#define TRACE_A2V() g_debug("[A2V] In '%s', on line %d while handling %s.", __func__, __LINE__, G_VALUE_TYPE_NAME(val))
#else
#define TRACE_V2A()
#define TRACE_A2V()
#endif

void
gig_value2arg(GValue *val, GIArgument *arg, gsize *size)
{
    TRACE_V2A();
    g_assert_nonnull(val);
    g_assert_nonnull(arg);
    GType type = G_VALUE_TYPE(val);
    GType fundamental_type = G_TYPE_FUNDAMENTAL(type);

    if (fundamental_type == G_TYPE_INVALID || fundamental_type == G_TYPE_NONE)
        g_error("Unimplemented %s %d", __FILE__, __LINE__);
    else if (fundamental_type == G_TYPE_VOID)
        arg->v_pointer = 0;
    else if (fundamental_type == G_TYPE_INTERFACE)
        arg->v_pointer = g_value_get_object(val);
    else if (fundamental_type == G_TYPE_CHAR)
        arg->v_int8 = g_value_get_schar(val);
    else if (fundamental_type == G_TYPE_UCHAR)
        arg->v_uint8 = g_value_get_uchar(val);
    else if (fundamental_type == G_TYPE_BOOLEAN)
        arg->v_boolean = g_value_get_boolean(val);
    else if (fundamental_type == G_TYPE_INT) {
        if (type == G_TYPE_INT)
            arg->v_int = g_value_get_int(val);
        else if (type == G_TYPE_INT16)
            arg->v_int16 = g_value_get_int(val);
        else if (type == G_TYPE_INT32)
            arg->v_int32 = g_value_get_int(val);
        else if (type == G_TYPE_UNICHAR)
            arg->v_int32 = g_value_get_int(val);
    }
    else if (fundamental_type == G_TYPE_UINT) {
        if (type == G_TYPE_UINT)
            arg->v_uint = g_value_get_uint(val);
        else if (type == G_TYPE_UINT16)
            arg->v_uint16 = g_value_get_uint(val);
        else if (type == G_TYPE_UINT32)
            arg->v_uint32 = g_value_get_uint(val);
    }
    else if (fundamental_type == G_TYPE_LONG)
        arg->v_long = g_value_get_long(val);
    else if (fundamental_type == G_TYPE_ULONG)
        arg->v_ulong = g_value_get_ulong(val);
    else if (fundamental_type == G_TYPE_INT64)
        arg->v_int64 = g_value_get_int64(val);
    else if (fundamental_type == G_TYPE_UINT64)
        arg->v_uint64 = g_value_get_uint64(val);
    else if (fundamental_type == G_TYPE_ENUM)
        arg->v_int = g_value_get_enum(val);
    else if (fundamental_type == G_TYPE_FLAGS)
        arg->v_int = g_value_get_flags(val);
    else if (fundamental_type == G_TYPE_FLOAT)
        arg->v_float = g_value_get_float(val);
    else if (fundamental_type == G_TYPE_DOUBLE)
        arg->v_double = g_value_get_double(val);
    else if (fundamental_type == G_TYPE_STRING)
        arg->v_string = g_value_get_string(val);
    else if (fundamental_type == G_TYPE_POINTER)
        arg->v_pointer = g_value_get_pointer(val);
    else if (fundamental_type == G_TYPE_BOXED) {
        if (type == G_TYPE_FIXED_SIZE_CARRAY
            || type == G_TYPE_ZERO_TERMINATED_CARRAY || type == G_TYPE_LENGTH_CARRAY)
            g_assert_not_reached();
        else if (type == G_TYPE_ARRAY) {
            GArray *arr = g_value_get_boxed(val);
            arg->v_pointer = arr->data;
            if (size)
                *size = arr->len;
        }
        else if (type == G_TYPE_BYTE_ARRAY) {
            GByteArray *arr = g_value_get_boxed(val);
            arg->v_pointer = arr->data;
            if (size)
                *size = arr->len;
        }
        else if (type == G_TYPE_PTR_ARRAY) {
            GPtrArray *arr = g_value_get_boxed(val);
            arg->v_pointer = arr->pdata;
            if (size)
                *size = arr->len;
        }
        else
            arg->v_pointer = g_value_get_boxed(val);
    }
    else if (fundamental_type == G_TYPE_PARAM)
        // ret = transform_scm_param_to_arg(src, meta, val, free_list);
        g_error("Unimplemented %s %d", __FILE__, __LINE__);
    else if (fundamental_type == G_TYPE_OBJECT)
        // ret = transform_scm_object_to_arg(src, meta, val, free_list);
        g_error("Unimplemented %s %d", __FILE__, __LINE__);
    else if (fundamental_type == G_TYPE_VARIANT)
        // ret = transform_scm_variant_to_arg(src, meta, val, free_list);
        g_error("Unimplemented %s %d", __FILE__, __LINE__);
    else
        g_error("Unimplemented %s %d", __FILE__, __LINE__);
}

gsize
zero_terminated_carray_length(GigTypeMeta *meta, gpointer array)
{
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
    }
        break;
    }
    return length;
}


void
gig_arg2value(GValue *val, GIArgument *arg, gsize array_len)
{
    TRACE_A2V();
    g_assert_nonnull(val);
    g_assert_nonnull(arg);

    GType type = G_VALUE_TYPE(val);
    GType fundamental_type = G_TYPE_FUNDAMENTAL(type);

    if (fundamental_type == G_TYPE_INVALID || fundamental_type == G_TYPE_NONE)
        g_error("Unimplemented %s %d", __FILE__, __LINE__);
    else if (fundamental_type == G_TYPE_VOID)
        // Do nothing
        ;
    else if (fundamental_type == G_TYPE_INTERFACE)
        g_value_set_object(val, arg->v_pointer);
    else if (fundamental_type == G_TYPE_CHAR)
        g_value_set_schar(val, arg->v_int8);
    else if (fundamental_type == G_TYPE_UCHAR)
        g_value_set_uchar(val, arg->v_uint8);
    else if (fundamental_type == G_TYPE_BOOLEAN)
        g_value_set_boolean(val, arg->v_boolean);
    else if (fundamental_type == G_TYPE_INT) {
        if (type == G_TYPE_INT)
            g_value_set_int(val, arg->v_int);
        else if (type == G_TYPE_INT16)
            g_value_set_int(val, arg->v_int16);
        else if (type == G_TYPE_INT32)
            g_value_set_int(val, arg->v_int32);
        else if (type == G_TYPE_UNICHAR)
            g_value_set_int(val, arg->v_int32);
        else
            g_assert_not_reached();
    }
    else if (fundamental_type == G_TYPE_UINT) {
        if (type == G_TYPE_UINT)
            g_value_set_uint(val, arg->v_uint);
        else if (type == G_TYPE_UINT16)
            g_value_set_uint(val, arg->v_uint16);
        else if (type == G_TYPE_UINT32)
            g_value_set_uint(val, arg->v_uint32);
        else
            g_assert_not_reached();
    }
    else if (fundamental_type == G_TYPE_LONG)
        g_value_set_long(val, arg->v_long);
    else if (fundamental_type == G_TYPE_ULONG)
        g_value_set_ulong(val, arg->v_ulong);
    else if (fundamental_type == G_TYPE_INT64)
        g_value_set_int64(val, arg->v_int64);
    else if (fundamental_type == G_TYPE_UINT64)
        g_value_set_uint64(val, arg->v_uint64);
    else if (fundamental_type == G_TYPE_ENUM)
        g_value_set_enum(val, arg->v_int);
    else if (fundamental_type == G_TYPE_FLAGS)
        g_value_set_flags(val, arg->v_uint);
    else if (fundamental_type == G_TYPE_FLOAT)
        g_value_set_enum(val, arg->v_float);
    else if (fundamental_type == G_TYPE_DOUBLE)
        g_value_set_enum(val, arg->v_double);
    else if (fundamental_type == G_TYPE_STRING)
        g_value_take_string(val, arg->v_string);
    else if (fundamental_type == G_TYPE_POINTER)
        g_value_set_pointer(val, arg->v_pointer);
    else if (fundamental_type == G_TYPE_BOXED) {
        // We expect VAL's box to already contain the right
        // type of array with the correct size set.
        if (type == G_TYPE_LENGTH_CARRAY
            || type == G_TYPE_FIXED_SIZE_CARRAY || type == G_TYPE_ZERO_TERMINATED_CARRAY)
            g_error("Unimplemented %s %d", __FILE__, __LINE__);
        else if (type == G_TYPE_ARRAY) {
            GArray *arr = g_value_get_boxed(val);
            g_assert_nonnull(arr);
            if (arg->v_pointer != arr->data)
                g_array_append_vals(arr, arg->v_pointer, array_len);
        }
        else if (type == G_TYPE_BYTE_ARRAY) {
            GByteArray *arr = g_value_get_boxed(val);
            g_assert_nonnull(arr);
            if (arg->v_pointer)
                g_byte_array_append(arr, arg->v_pointer, array_len);
        }
        else if (type == G_TYPE_PTR_ARRAY) {
            GPtrArray *arr = g_value_get_boxed(val);
            g_assert_nonnull(arr);
            if (arg->v_pointer)
                for (int i = 0; i < array_len; i++)
                    g_ptr_array_add(arr, ((gpointer **)(arg->v_pointer))[i]);
        }
        else {
            g_value_take_boxed(val, arg->v_pointer);
        }
    }
    else if (fundamental_type == G_TYPE_PARAM)
        // ret = transform_scm_param_to_arg(arg, meta, val, free_list);
        g_error("Unimplemented %s %d", __FILE__, __LINE__);
    else if (fundamental_type == G_TYPE_OBJECT)
        // ret = transform_scm_object_to_arg(arg, meta, val, free_list);
        g_error("Unimplemented %s %d", __FILE__, __LINE__);
    else if (fundamental_type == G_TYPE_VARIANT)
        // ret = transform_scm_variant_to_arg(arg, meta, val, free_list);
        g_error("Unimplemented %s %d", __FILE__, __LINE__);
    else
        g_error("Unimplemented %s %d", __FILE__, __LINE__);
}

void
gig_init_argument(void)
{

}
