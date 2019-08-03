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
#include "gig_value.h"
#include "gig_type.h"
#include "gig_object.h"
#include "gig_flag.h"

#ifndef FLT_MAX
#define FLT_MAX 3.402823466e+38F
#endif

/* GValue: a container holding a GType and an associated GValue. */

#define GIG_VALUE_WRONG_TYPE -1
#define GIG_VALUE_OUT_OF_RANGE -2

/**
 * gig_value_from_scm:
 * @value: the GValue object to store the converted value in.
 * @obj: the Python object to convert.
 *
 * This function converts a generic SCM value and stores the result in a
 * GValue.  The GValue must be initialised in advance with
 * g_value_init().  If the Python object can't be converted to the
 * type of the GValue, then an error is returned.
 *
 * Returns: 0 on success, -1 on error.
 */
int
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
        return 0;
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
        return 0;
    }
    case G_TYPE_BOOLEAN:
    {
        if (!scm_is_eq(obj, SCM_BOOL_T) && !scm_is_eq(obj, SCM_BOOL_F))
            return GIG_VALUE_WRONG_TYPE;
        gboolean temp;
        temp = scm_is_true(obj);
        g_value_set_boolean(value, temp);
        return 0;
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
        return 0;
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
        return 0;
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
        return 0;
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
        return 0;
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
        return 0;
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
        return 0;
    }
    case G_TYPE_ENUM:
    {
        if (!SCM_IS_A_P(obj, gig_enum_type))
            return GIG_VALUE_WRONG_TYPE;
        g_value_set_enum(value, gig_enum_to_int(obj));
        return 0;
    }
    case G_TYPE_FLAGS:
    {
        if (!SCM_IS_A_P(obj, gig_flags_type))
            return GIG_VALUE_WRONG_TYPE;
        g_value_set_flags(value, gig_flags_to_uint(obj));
        return 0;
    }
    case G_TYPE_FLOAT:
    {
        if (!scm_is_true(scm_real_p(obj)))
            return GIG_VALUE_WRONG_TYPE;
        gdouble dval = scm_to_double(obj);
        if (dval < -G_MAXFLOAT || dval > G_MAXFLOAT)
            return GIG_VALUE_OUT_OF_RANGE;
        g_value_set_float(value, dval);
        return 0;
    }
    case G_TYPE_DOUBLE:
    {
        if (!scm_is_true(scm_real_p(obj)))
            return GIG_VALUE_WRONG_TYPE;
        gdouble temp;
        temp = scm_to_double(obj);
        g_value_set_double(value, temp);
        return 0;
    }
    case G_TYPE_STRING:
    {
        if (!scm_is_string(obj))
            return GIG_VALUE_WRONG_TYPE;
        gchar *temp = scm_to_utf8_string(obj);
        g_value_take_string(value, temp);
        return 0;
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
    }

    case G_TYPE_INTERFACE:
    case G_TYPE_OBJECT:
        /* we only handle interface types that have a GObject prereq */
        if (g_type_is_a(value_type, G_TYPE_OBJECT)) {
            if (scm_is_false(obj)) {
                g_value_set_object(value, NULL);
                return 0;
            }
            else if (!G_TYPE_CHECK_INSTANCE_TYPE(gig_type_peek_object(obj), value_type))
                return GIG_VALUE_WRONG_TYPE;
            else {
                g_value_set_object(value, gig_type_peek_object(obj));
                return 0;
            }
        }
        else
            return GIG_VALUE_WRONG_TYPE;
        break;
    default:
        g_critical("unhandled value type");
        return GIG_VALUE_WRONG_TYPE;
        break;
    }

    return 0;
}

void
gig_value_from_scm_with_error(const gchar *subr, GValue *value, SCM obj, gint pos)
{
    gint res = gig_value_from_scm(value, obj);
    switch (res) {
    case 0:
        return;
    case GIG_VALUE_WRONG_TYPE:
        scm_wrong_type_arg(subr, pos, obj);
        break;
    case GIG_VALUE_OUT_OF_RANGE:
        scm_out_of_range_pos(subr, obj, scm_from_int(pos));
    }
}


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

        g_value_init(&item_value, type);
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
        return gig_int_to_enum(g_value_get_enum(value), G_VALUE_TYPE(value));
    case G_TYPE_FLAGS:
        return gig_uint_to_flags(g_value_get_flags(value), G_VALUE_TYPE(value));
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
