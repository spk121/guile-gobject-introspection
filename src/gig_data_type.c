// Copyright (C) 2019 Michael L. Gran

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

#include <glib-object.h>
#include "gig_data_type.h"
#include "gig_util.h"

GType g_type_unichar;
GType g_type_int16;
GType g_type_int32;
GType g_type_uint16;
GType g_type_uint32;
GType g_type_locale_string;

GType g_type_fixed_size_carray;
GType g_type_zero_terminated_carray;
GType g_type_length_carray;
GType g_type_list;
GType g_type_slist;
GType g_type_callback;

static void gig_type_meta_init_from_type_info(GigTypeMeta *type, GITypeInfo *ti);
static void gig_type_meta_init_from_basic_type_tag(GigTypeMeta *meta, GITypeTag tag);

void
gig_type_meta_init_from_arg_info(GigTypeMeta *meta, GIArgInfo *ai)
{
    GITypeInfo *type_info = g_arg_info_get_type(ai);
    GIDirection dir = g_arg_info_get_direction(ai);
    GITransfer transfer = g_arg_info_get_ownership_transfer(ai);

    gig_type_meta_init_from_type_info(meta, type_info);

    meta->is_in = (dir == GI_DIRECTION_IN || dir == GI_DIRECTION_INOUT);
    meta->is_out = (dir == GI_DIRECTION_OUT || dir == GI_DIRECTION_INOUT);
    meta->is_skip = g_arg_info_is_skip(ai);

    meta->is_caller_allocates = g_arg_info_is_caller_allocates(ai);
    meta->is_optional = g_arg_info_is_optional(ai);
    meta->is_nullable = g_arg_info_may_be_null(ai);

    meta->is_transfer_ownership = (transfer == GI_TRANSFER_EVERYTHING);
    meta->is_transfer_container = (transfer == GI_TRANSFER_CONTAINER ||
                                   transfer == GI_TRANSFER_EVERYTHING);

}

void
gig_type_meta_init_from_callable_info(GigTypeMeta *meta, GICallableInfo *ci)
{
    GITypeInfo *type_info = g_callable_info_get_return_type(ci);
    GITransfer transfer = g_callable_info_get_caller_owns(ci);

    gig_type_meta_init_from_type_info(meta, type_info);

    meta->is_in = FALSE;
    if (meta->gtype != G_TYPE_NONE && meta->gtype != G_TYPE_INVALID)
        meta->is_out = TRUE;
    else
        meta->is_out = FALSE;
    meta->is_skip = g_callable_info_skip_return(ci);

    meta->is_caller_allocates = FALSE;
    meta->is_optional = FALSE;
    meta->is_nullable = g_callable_info_may_return_null(ci);

    meta->is_transfer_ownership = (transfer == GI_TRANSFER_EVERYTHING);
    meta->is_transfer_container = (transfer == GI_TRANSFER_CONTAINER ||
                                   transfer == GI_TRANSFER_EVERYTHING);

}

void
add_params(GigTypeMeta *meta, gint n)
{
    meta->params = g_new0(GigTypeMeta, n);
    meta->n_params = n;
}

static void
add_child_params(GigTypeMeta *meta, GITypeInfo *type_info, gint n)
{
    GITypeInfo *param_type;
    add_params(meta, n);

    for (int i = 0; i < n; i++) {
        param_type = g_type_info_get_param_type((GITypeInfo *)type_info, i);
        gig_type_meta_init_from_type_info(meta->params, param_type);
        g_base_info_unref(param_type);

        if (meta->is_transfer_ownership && meta->is_transfer_container) {
            meta->params[i].is_transfer_ownership = 1;
            meta->params[i].is_transfer_container = 1;
        }
        else {
            meta->params[i].is_transfer_ownership = 0;
            meta->params[i].is_transfer_container = 0;
        }
    }
}

static void
gig_type_meta_init_from_basic_type_tag(GigTypeMeta *meta, GITypeTag tag)
{
#define T(TYPETAG,GTYPE,CTYPE)                                          \
    do {                                                                \
        if (tag == TYPETAG) {                                           \
            meta->gtype = GTYPE;                                        \
            meta->item_size = meta->is_ptr ? sizeof(gpointer) : sizeof (CTYPE); \
            return;                                                     \
        }                                                               \
    } while(FALSE)

    T(GI_TYPE_TAG_BOOLEAN, G_TYPE_BOOLEAN, gboolean);
    T(GI_TYPE_TAG_DOUBLE, G_TYPE_DOUBLE, gdouble);
    T(GI_TYPE_TAG_FLOAT, G_TYPE_FLOAT, gfloat);
    T(GI_TYPE_TAG_GTYPE, G_TYPE_GTYPE, GType);
    T(GI_TYPE_TAG_INT8, G_TYPE_CHAR, gint8);
    T(GI_TYPE_TAG_INT16, G_TYPE_INT16, gint16);
    T(GI_TYPE_TAG_INT32, G_TYPE_INT32, gint32);
    T(GI_TYPE_TAG_INT64, G_TYPE_INT64, gint64);
    T(GI_TYPE_TAG_UINT8, G_TYPE_UCHAR, guint8);
    T(GI_TYPE_TAG_UINT16, G_TYPE_UINT16, guint16);
    T(GI_TYPE_TAG_UINT32, G_TYPE_UINT32, guint32);
    T(GI_TYPE_TAG_UINT64, G_TYPE_UINT64, guint64);
    T(GI_TYPE_TAG_UNICHAR, G_TYPE_UNICHAR, gunichar);
    T(GI_TYPE_TAG_UTF8, G_TYPE_STRING, 0);
    T(GI_TYPE_TAG_FILENAME, G_TYPE_LOCALE_STRING, 0);
    T(GI_TYPE_TAG_ERROR, G_TYPE_ERROR, GError);
    g_error("Unhandled type '%s' %s %d", g_type_tag_to_string(tag), __FILE__, __LINE__);
#undef T
}

static void
gig_type_meta_init_from_type_info(GigTypeMeta *meta, GITypeInfo *type_info)
{
    GITypeTag tag = g_type_info_get_tag(type_info);
    meta->is_ptr = g_type_info_is_pointer(type_info);

    if (tag == GI_TYPE_TAG_VOID) {
        if (meta->is_ptr) {
            meta->gtype = G_TYPE_POINTER;
            meta->item_size = sizeof(gpointer);
        }
        else {
            meta->gtype = G_TYPE_NONE;
            meta->item_size = 0;
        }
    }
    else if (tag == GI_TYPE_TAG_ARRAY) {
        GIArrayType array_type = g_type_info_get_array_type(type_info);
        gint len;

        add_child_params(meta, type_info, 1);

        if (array_type == GI_ARRAY_TYPE_C) {
            if ((len = g_type_info_get_array_length(type_info)) != -1) {
                meta->gtype = G_TYPE_LENGTH_CARRAY;
                meta->length = len;
            }
            else if ((len = g_type_info_get_array_fixed_size(type_info)) != -1) {
                meta->gtype = G_TYPE_FIXED_SIZE_CARRAY;
                meta->length = len;
            }
            else if (g_type_info_is_zero_terminated(type_info)) {
                meta->gtype = G_TYPE_ZERO_TERMINATED_CARRAY;
            }
            else {
                g_warning("unsized C array");
                meta->gtype = G_TYPE_POINTER;
            }
        }
        else if (array_type == GI_ARRAY_TYPE_ARRAY)
            meta->gtype = G_TYPE_ARRAY;
        else if (array_type == GI_ARRAY_TYPE_BYTE_ARRAY)
            meta->gtype = G_TYPE_BYTE_ARRAY;
        else if (array_type == GI_ARRAY_TYPE_PTR_ARRAY)
            meta->gtype = G_TYPE_PTR_ARRAY;
        else
            g_assert_not_reached();
    }
    else if (tag == GI_TYPE_TAG_GHASH) {
        meta->gtype = G_TYPE_HASH_TABLE;
        add_child_params(meta, type_info, 2);
    }
    else if (tag == GI_TYPE_TAG_GLIST) {
        meta->gtype = G_TYPE_LIST;
        add_child_params(meta, type_info, 1);
    }
    else if (tag == GI_TYPE_TAG_GSLIST) {
        meta->gtype = G_TYPE_SLIST;
        add_child_params(meta, type_info, 1);
    }
    else if (tag == GI_TYPE_TAG_INTERFACE) {
        GIBaseInfo *referenced_base_info = g_type_info_get_interface(type_info);
        GIInfoType itype = g_base_info_get_type(referenced_base_info);
        if (itype == GI_INFO_TYPE_UNRESOLVED) {
            meta->gtype = G_TYPE_INVALID;
            meta->is_invalid = TRUE;
            g_warning("Unrepresentable type: %s, %s, %s",
                      g_base_info_get_name_safe(type_info),
                      g_base_info_get_name_safe(referenced_base_info),
                      g_info_type_to_string(itype));
        }
        else if (itype == GI_INFO_TYPE_ENUM || itype == GI_INFO_TYPE_FLAGS) {
            meta->gtype = g_registered_type_info_get_g_type(referenced_base_info);
            // Not all enum or flag types have an associated GType
            add_params(meta, 1);
            if (meta->gtype == G_TYPE_NONE || meta->gtype == 0) {
                if (itype == GI_INFO_TYPE_ENUM) {
                    meta->gtype = G_TYPE_ENUM;
                    gig_type_meta_init_from_basic_type_tag(meta->params, GI_TYPE_TAG_INT32);
                }
                else {
                    meta->gtype = G_TYPE_FLAGS;
                    gig_type_meta_init_from_basic_type_tag(meta->params, GI_TYPE_TAG_UINT32);
                }
            }
            else {
                gig_type_meta_init_from_basic_type_tag(meta->params,
                                                       g_enum_info_get_storage_type
                                                       (referenced_base_info));
            }
        }
        else if (itype == GI_INFO_TYPE_STRUCT) {
            meta->gtype = g_registered_type_info_get_g_type(referenced_base_info);
            if (!meta->is_ptr)
                meta->item_size = g_struct_info_get_size(referenced_base_info);
            else
                meta->item_size = sizeof(gpointer);
        }
        else if (itype == GI_INFO_TYPE_UNION) {
            meta->gtype = g_registered_type_info_get_g_type(referenced_base_info);
            if (!meta->is_ptr)
                meta->item_size = g_union_info_get_size(referenced_base_info);
            else
                meta->item_size = sizeof(gpointer);
        }
        else if (itype == GI_INFO_TYPE_OBJECT || itype == GI_INFO_TYPE_INTERFACE) {
            meta->gtype = g_registered_type_info_get_g_type(referenced_base_info);
            if (!meta->is_ptr)
                meta->item_size = 0;
            else
                meta->item_size = sizeof(gpointer);
        }
        else if (GI_IS_REGISTERED_TYPE_INFO(referenced_base_info)) {
            meta->gtype = g_registered_type_info_get_g_type(referenced_base_info);
        }
        else if (itype == GI_INFO_TYPE_CALLBACK) {
            meta->gtype = G_TYPE_CALLBACK;
            g_base_info_ref(referenced_base_info);
            meta->callable_info = referenced_base_info;
        }
        else {
            g_critical("Unhandled item type in %s:%d", __FILE__, __LINE__);
        }
        g_base_info_unref(referenced_base_info);
    }
    else
        gig_type_meta_init_from_basic_type_tag(meta, tag);

    //
    if (meta->gtype == 0) {
        if (meta->is_ptr) {
            meta->gtype = G_TYPE_POINTER;
            meta->item_size = sizeof(gpointer);
        }
        else {
            meta->gtype = G_TYPE_NONE;
            meta->item_size = 0;
        }

        if (meta->gtype == 0)
            g_warning("gtype of %s is zero", g_base_info_get_name(type_info));
    }
}

#define STRLEN 128
gchar gig_data_type_describe_buf[STRLEN];

const gchar *
gig_type_meta_describe(GigTypeMeta *meta)
{
    GString *s = g_string_new(NULL);
    g_string_append_printf(s, "%s%s%s",
                           meta->is_ptr ? "pointer to " : "",
                           meta->is_caller_allocates ? "caller allocated " : "",
                           g_type_name(meta->gtype));
    if (!G_TYPE_IS_FUNDAMENTAL(meta->gtype))
        g_string_append_printf(s, " of type %s", g_type_name(G_TYPE_FUNDAMENTAL(meta->gtype)));
    if (meta->is_nullable)
        g_string_append_printf(s, " or NULL");
    strncpy(gig_data_type_describe_buf, s->str, STRLEN - 1);
    gig_data_type_describe_buf[STRLEN - 1] = '\0';
    g_string_free(s, TRUE);
    return gig_data_type_describe_buf;
}

#undef STRLEN

G_DEFINE_BOXED_TYPE(GList, g_list, g_list_copy, g_list_free);
G_DEFINE_BOXED_TYPE(GSList, g_slist, g_slist_copy, g_slist_free);

void
gig_init_data_type(void)
{
    g_type_unichar = g_type_register_static_simple(G_TYPE_INT, "gunichar", 0, NULL, 0, NULL, 0);
    g_type_int16 = g_type_register_static_simple(G_TYPE_INT, "gint16", 0, NULL, 0, NULL, 0);
    g_type_int32 = g_type_register_static_simple(G_TYPE_INT, "gint32", 0, NULL, 0, NULL, 0);
    g_type_uint16 = g_type_register_static_simple(G_TYPE_UINT, "guint16", 0, NULL, 0, NULL, 0);
    g_type_uint32 = g_type_register_static_simple(G_TYPE_UINT, "guint32", 0, NULL, 0, NULL, 0);
    g_type_locale_string = g_type_register_static_simple(G_TYPE_STRING,
                                                         "locale-string", 0, NULL, 0, NULL, 0);

    // These 3 array types are all just aliases for GArray, but, their
    // types designate how that interacted with the GObject C FFI.
    g_type_fixed_size_carray = g_boxed_type_register_static("fixed-size-carray", g_array_ref, g_array_unref);
    g_type_zero_terminated_carray = g_boxed_type_register_static("zero-terminated-carray", g_array_ref, g_array_unref);
    g_type_length_carray = g_boxed_type_register_static("length+carray", g_array_ref, g_array_unref);

    g_type_list = g_list_get_type();
    g_type_slist = g_slist_get_type();

    g_type_callback = g_pointer_type_register_static("callback");
}
