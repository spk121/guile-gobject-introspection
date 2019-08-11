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

#include "gig_data_type.h"


static gboolean
fill_array_info(GigTypeMeta * meta)
{
    g_assert_cmpint(meta->type_tag, ==, GI_TYPE_TAG_ARRAY);
    g_assert_true(meta->is_ptr);

    gboolean ret = TRUE;

    // So there are layers to all this ArgInfo stuff.
    // First, GIArgInfo tells us what type of array.
    meta->array_type = g_type_info_get_array_type(meta->type_info);
    meta->array_is_zero_terminated = g_type_info_is_zero_terminated(meta->type_info);
    meta->array_length_index = g_type_info_get_array_length(meta->type_info);
    meta->array_fixed_size = g_type_info_get_array_fixed_size(meta->type_info);

    if ((meta->transfer == GI_TRANSFER_CONTAINER)
        || meta->transfer == GI_TRANSFER_NOTHING)
        meta->transfer = GI_TRANSFER_NOTHING;
    else
        meta->transfer = GI_TRANSFER_EVERYTHING;

    // Second, we figure out what the element type of the array is.
    GITypeInfo *item_type_info = g_type_info_get_param_type(meta->type_info, 0);
    meta->item_type_tag = g_type_info_get_tag(item_type_info);
    meta->item_is_ptr = g_type_info_is_pointer(item_type_info);

    if (meta->item_is_ptr)
        meta->item_size = sizeof(void *);

    switch (meta->item_type_tag) {
    case GI_TYPE_TAG_INT8:
    case GI_TYPE_TAG_UINT8:
        meta->item_size = 1;
        break;
    case GI_TYPE_TAG_INT16:
    case GI_TYPE_TAG_UINT16:
        meta->item_size = 2;
        break;
    case GI_TYPE_TAG_INT32:
    case GI_TYPE_TAG_UINT32:
    case GI_TYPE_TAG_UNICHAR:
        meta->item_size = 4;
        break;
    case GI_TYPE_TAG_INT64:
    case GI_TYPE_TAG_UINT64:
        meta->item_size = 8;
        break;
    case GI_TYPE_TAG_FLOAT:
        meta->item_size = sizeof(float);
        break;
    case GI_TYPE_TAG_DOUBLE:
        meta->item_size = sizeof(double);
        break;
    case GI_TYPE_TAG_GTYPE:
        meta->item_size = sizeof(GType);
        break;
    case GI_TYPE_TAG_BOOLEAN:
        meta->item_size = sizeof(gboolean);
        break;
    case GI_TYPE_TAG_INTERFACE:
    {
        GIBaseInfo *referenced_base_info = g_type_info_get_interface(item_type_info);
        meta->referenced_base_type = g_base_info_get_type(referenced_base_info);

        switch (meta->referenced_base_type) {
        case GI_INFO_TYPE_ENUM:
        case GI_INFO_TYPE_FLAGS:

            g_assert_false(meta->item_is_ptr);
            meta->item_size = sizeof(int);
            break;
        case GI_INFO_TYPE_STRUCT:
            meta->referenced_object_type = g_registered_type_info_get_g_type(referenced_base_info);
            if (!meta->item_is_ptr)
                meta->item_size = g_struct_info_get_size(referenced_base_info);
            break;
        case GI_INFO_TYPE_UNION:
            meta->referenced_object_type = g_registered_type_info_get_g_type(referenced_base_info);
            if (!meta->item_is_ptr)
                meta->item_size = g_union_info_get_size(referenced_base_info);
            break;
        case GI_INFO_TYPE_OBJECT:
        case GI_INFO_TYPE_INTERFACE:
            meta->referenced_object_type = g_registered_type_info_get_g_type(referenced_base_info);
            if (!meta->item_is_ptr)
                meta->item_size = sizeof(void *);
            break;
        default:
            g_critical("Unhandled item type in %s:%d", __FILE__, __LINE__);
            ret = FALSE;
        }
        g_base_info_unref(referenced_base_info);
        break;
    }
    case GI_TYPE_TAG_UTF8:
    case GI_TYPE_TAG_FILENAME:
        break;

    case GI_TYPE_TAG_ARRAY:
    case GI_TYPE_TAG_GLIST:
    case GI_TYPE_TAG_GSLIST:
    case GI_TYPE_TAG_GHASH:
        g_critical("nested array objects are unhandled in %s:%d", __FILE__, __LINE__);
        ret = FALSE;
        break;

    default:
        g_critical("Unhandled item type in %s:%d", __FILE__, __LINE__);
        ret = FALSE;
    }

    if (ret == FALSE) {
        // Set the unhandled array type to be a simple null pointer.
        meta->type_tag = GI_TYPE_TAG_VOID;
    }

    g_base_info_unref(item_type_info);
    return ret;
}


void
gig_type_meta_init_from_arg_info(GigTypeMeta * meta, GIArgInfo *ai)
{
    g_assert_nonnull(meta);
    g_assert_nonnull(ai);

    meta->type_info = g_arg_info_get_type(ai);
    meta->type_tag = g_type_info_get_tag(meta->type_info);
    meta->is_ptr = g_type_info_is_pointer(meta->type_info);
    meta->c_direction = g_arg_info_get_direction(ai);
    meta->transfer = g_arg_info_get_ownership_transfer(ai);
    meta->may_be_null = g_arg_info_may_be_null(ai);
    meta->is_caller_allocates = g_arg_info_is_caller_allocates(ai);
    meta->array_length_index = -1;


    if (meta->type_tag == GI_TYPE_TAG_ARRAY)
        fill_array_info(meta);

}

void
gig_type_meta_init_from_callable_info(GigTypeMeta * meta, GICallableInfo *ci)
{
    g_assert_nonnull(meta);
    g_assert_nonnull(ci);

    meta->type_info = g_callable_info_get_return_type(ci);
    meta->type_tag = g_type_info_get_tag(meta->type_info);
    meta->is_ptr = g_type_info_is_pointer(meta->type_info);
    meta->c_direction = GI_DIRECTION_OUT;
    meta->transfer = g_callable_info_get_caller_owns(ci);
    meta->may_be_null = g_callable_info_may_return_null(ci);
    meta->is_caller_allocates = FALSE;
    meta->array_length_index = -1;

    if (meta->type_tag == GI_TYPE_TAG_ARRAY)
        fill_array_info(meta);
}

const char *
gig_type_meta_describe(GigTypeMeta * meta)
{
    gchar buf[80];
    g_snprintf(buf, "%s%s%s%s",
               meta->is_ptr ? "pointer to " : "",
               meta->is_caller_allocates ? "caller allocated " : "",
               g_type_tag_to_string(meta->type_tag), meta->may_be_null ? " or NULL" : "");
    return buf;
}
