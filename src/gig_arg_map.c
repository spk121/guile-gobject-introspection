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

#include <stdio.h>
#include "gig_arg_map.h"

const gchar dir_strings[GIG_ARG_DIRECTION_COUNT][9] = {
    [GIG_ARG_DIRECTION_VOID] = "VOID",
    [GIG_ARG_DIRECTION_INPUT] = "INPUT",
    [GIG_ARG_DIRECTION_INOUT] = "INOUT",
    [GIG_ARG_DIRECTION_PREALLOCATED_OUTPUT] = "PREALLOC",
    [GIG_ARG_DIRECTION_OUTPUT] = "OUTPUT"
};

const gchar tuple_strings[GIG_ARG_TUPLE_COUNT][11] = {
    [GIG_ARG_TUPLE_SINGLETON] = "SINGLETON",
    [GIG_ARG_TUPLE_ARRAY] = "ARRAY",
    [GIG_ARG_TUPLE_ARRAY_SIZE] = "ARRAY_SIZE"
};

const gchar presence_strings[GIG_ARG_PRESENCE_COUNT][9] = {
    [GIG_ARG_PRESENCE_REQUIRED] = "REQUIRED",
    [GIG_ARG_PRESENCE_OPTIONAL] = "OPTIONAL",
    [GIG_ARG_PRESENCE_IMPLICIT] = "IMPLICIT"
};

static GigArgMapEntry *arg_map_entry_new(void);
static void arg_map_entry_initialize(GigArgMapEntry *map);

static GigArgMapEntry *
arg_map_entry_new()
{
    GigArgMapEntry *entry = g_new(GigArgMapEntry, 1);
    arg_map_entry_initialize(entry);
    return entry;
}

static void
arg_map_entry_initialize(GigArgMapEntry *entry)
{
    memset(entry, 0, sizeof(GigArgMapEntry));
    entry->arg_info_index = -1;
    entry->array_length_index = -1;
    entry->cinvoke_input_index = -1;
    entry->cinvoke_output_index = -1;
    entry->gsubr_input_index = -1;
    entry->gsubr_output_index = -1;
}

static gboolean
fill_array_info(GigArgMapEntry *entry)
{
    g_assert_cmpint(entry->type_tag, ==, GI_TYPE_TAG_ARRAY);
    g_assert_true(entry->is_ptr);

    gboolean ret = TRUE;

    // So there are layers to all this ArgInfo stuff.
    // First, GIArgInfo tells us what type of array.
    entry->array_type = g_type_info_get_array_type(entry->type_info);
    entry->array_is_zero_terminated = g_type_info_is_zero_terminated(entry->type_info);
    entry->array_length_index = g_type_info_get_array_length(entry->type_info);
    entry->array_fixed_size = g_type_info_get_array_fixed_size(entry->type_info);

    if ((entry->transfer == GI_TRANSFER_CONTAINER)
        || entry->transfer == GI_TRANSFER_NOTHING)
        entry->transfer = GI_TRANSFER_NOTHING;
    else
        entry->transfer = GI_TRANSFER_EVERYTHING;

    // Second, we figure out what the element type of the array is.
    GITypeInfo *item_type_info = g_type_info_get_param_type(entry->type_info, 0);
    entry->item_type_tag = g_type_info_get_tag(item_type_info);
    entry->item_is_ptr = g_type_info_is_pointer(item_type_info);

    if (entry->item_is_ptr)
        entry->item_size = sizeof(void *);

    switch (entry->item_type_tag) {
    case GI_TYPE_TAG_INT8:
    case GI_TYPE_TAG_UINT8:
        entry->item_size = 1;
        break;
    case GI_TYPE_TAG_INT16:
    case GI_TYPE_TAG_UINT16:
        entry->item_size = 2;
        break;
    case GI_TYPE_TAG_INT32:
    case GI_TYPE_TAG_UINT32:
    case GI_TYPE_TAG_UNICHAR:
        entry->item_size = 4;
        break;
    case GI_TYPE_TAG_INT64:
    case GI_TYPE_TAG_UINT64:
        entry->item_size = 8;
        break;
    case GI_TYPE_TAG_FLOAT:
        entry->item_size = sizeof(float);
        break;
    case GI_TYPE_TAG_DOUBLE:
        entry->item_size = sizeof(double);
        break;
    case GI_TYPE_TAG_GTYPE:
        entry->item_size = sizeof(GType);
        break;
    case GI_TYPE_TAG_BOOLEAN:
        entry->item_size = sizeof(gboolean);
        break;
    case GI_TYPE_TAG_INTERFACE:
    {
        GIBaseInfo *referenced_base_info = g_type_info_get_interface(item_type_info);
        entry->referenced_base_type = g_base_info_get_type(referenced_base_info);

        switch (entry->referenced_base_type) {
        case GI_INFO_TYPE_ENUM:
        case GI_INFO_TYPE_FLAGS:

            g_assert_false(entry->item_is_ptr);
            entry->item_size = sizeof(int);
            break;
        case GI_INFO_TYPE_STRUCT:
            entry->referenced_object_type =
                g_registered_type_info_get_g_type(referenced_base_info);
            if (!entry->item_is_ptr)
                entry->item_size = g_struct_info_get_size(referenced_base_info);
            break;
        case GI_INFO_TYPE_UNION:
            entry->referenced_object_type =
                g_registered_type_info_get_g_type(referenced_base_info);
            if (!entry->item_is_ptr)
                entry->item_size = g_union_info_get_size(referenced_base_info);
            break;
        case GI_INFO_TYPE_OBJECT:
        case GI_INFO_TYPE_INTERFACE:
            entry->referenced_object_type =
                g_registered_type_info_get_g_type(referenced_base_info);
            if (!entry->item_is_ptr)
                entry->item_size = sizeof(void *);
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
        entry->type_tag = GI_TYPE_TAG_VOID;
    }

    g_base_info_unref(item_type_info);
    return ret;
}


static void
gig_arg_map_entry_apply_arg_info(GigArgMapEntry *e, GIArgInfo *ai)
{
    g_assert_nonnull(e);
    g_assert_nonnull(ai);

    e->name = g_strdup(g_base_info_get_name(ai));
    e->type_info = g_arg_info_get_type(ai);
    e->type_tag = g_type_info_get_tag(e->type_info);
    e->is_ptr = g_type_info_is_pointer(e->type_info);
    e->c_direction = g_arg_info_get_direction(ai);
    e->transfer = g_arg_info_get_ownership_transfer(ai);
    e->may_be_null = g_arg_info_may_be_null(ai);
    e->is_caller_allocates = g_arg_info_is_caller_allocates(ai);
}

// This function gathers information about return values that are arrays.
static void
gig_arg_map_entry_apply_callable_info(GigArgMapEntry *e, GICallableInfo *ci)
{
    g_assert_nonnull(e);
    g_assert_nonnull(ci);

    e->name = g_strdup("%return");
    e->type_info = g_callable_info_get_return_type(ci);
    e->type_tag = g_type_info_get_tag(e->type_info);
    e->is_ptr = g_type_info_is_pointer(e->type_info);
    e->c_direction = GI_DIRECTION_OUT;
    e->transfer = g_callable_info_get_caller_owns(ci);
    e->may_be_null = g_callable_info_may_return_null(ci);
    e->is_caller_allocates = FALSE;
}

// Gather information on how to map Scheme arguments to C arguments.
GigArgMap *
gig_arg_map_new(GICallableInfo *function_info)
{
    gsize arg_info_count = g_callable_info_get_n_args(function_info);
    GigArgMap *amap = g_new0(GigArgMap, 1);
    GPtrArray *entry_array = g_ptr_array_new();
    for (gsize arg_info_index = 0; arg_info_index < arg_info_count; arg_info_index++)
        g_ptr_array_add(entry_array, arg_map_entry_new());
    amap->return_val = arg_map_entry_new();

    // may-be-null parameters at the end of the C call can be made
    // optional parameters in the gsubr call.
    gboolean opt_flag = TRUE;
    for (gint arg_info_index = arg_info_count - 1; arg_info_index >= 0; arg_info_index--) {
        g_assert_cmpint(arg_info_index, <, entry_array->len);
        GIArgInfo *arg_info = g_callable_info_get_arg(function_info, arg_info_index);
        GigArgMapEntry *entry = g_ptr_array_index(entry_array, arg_info_index);
        gig_arg_map_entry_apply_arg_info(entry, arg_info);
        g_base_info_unref(arg_info);

        entry->arg_info_index = arg_info_index;
        GIDirection dir = entry->c_direction;
        if (dir == GI_DIRECTION_IN
            || dir == GI_DIRECTION_INOUT
            || (dir == GI_DIRECTION_OUT && entry->is_caller_allocates)) {
            if (opt_flag && entry->may_be_null)
                entry->presence = GIG_ARG_PRESENCE_OPTIONAL;
            else {
                entry->presence = GIG_ARG_PRESENCE_REQUIRED;
                opt_flag = FALSE;
            }
        }
        else {
            entry->presence = GIG_ARG_PRESENCE_IMPLICIT;
        }
    }

    gint cinvoke_input_index = 0;
    gint cinvoke_output_index = 0;
    gint gsubr_input_index = 0;
    gint gsubr_output_index = 0;
    for (gsize arg_info_index = 0; arg_info_index < arg_info_count; arg_info_index++) {
        g_assert_cmpint(arg_info_index, <, entry_array->len);
        GigArgMapEntry *entry = g_ptr_array_index(entry_array, arg_info_index);

        GIDirection dir = entry->c_direction;
        GITypeInfo *type_info = entry->type_info;
        GITypeTag type_tag = g_type_info_get_tag(type_info);
        gboolean is_caller_allocates = entry->is_caller_allocates;


        if (type_tag == GI_TYPE_TAG_ARRAY) {
            fill_array_info(entry);
            // Sometime a single SCM array or list will map to two
            // arguments: a C array pointer and a C size parameter.
            if (entry->array_length_index >= 0) {
                entry->tuple = GIG_ARG_TUPLE_ARRAY;
                entry->child = g_ptr_array_index(entry_array, entry->array_length_index);
                entry->child->tuple = GIG_ARG_TUPLE_ARRAY_SIZE;
                entry->child->presence = GIG_ARG_PRESENCE_IMPLICIT;
                entry->child->parent = entry;
            }
        }

        // Here we find the positions of this argument in the
        // g_function_info_invoke call.  Also, some output parameters
        // require a SCM container to be passed in to the SCM GSubr
        // call.
        if (dir == GI_DIRECTION_IN) {
            entry->s_direction = GIG_ARG_DIRECTION_INPUT;
            entry->cinvoke_input_index = cinvoke_input_index++;
        }
        else if (dir == GI_DIRECTION_INOUT) {
            entry->s_direction = GIG_ARG_DIRECTION_INOUT;
            entry->cinvoke_input_index = cinvoke_input_index++;
            entry->cinvoke_output_index = cinvoke_output_index++;
        }
        else if (dir == GI_DIRECTION_OUT && is_caller_allocates) {
            entry->s_direction = GIG_ARG_DIRECTION_PREALLOCATED_OUTPUT;
            entry->cinvoke_output_index = cinvoke_output_index++;
        }
        else {
            entry->s_direction = GIG_ARG_DIRECTION_OUTPUT;
            entry->cinvoke_output_index = cinvoke_output_index++;
        }
    }

    amap->return_val = arg_map_entry_new();
    gig_arg_map_entry_apply_callable_info(amap->return_val, function_info);
    if (amap->return_val->type_tag == GI_TYPE_TAG_ARRAY) {
        fill_array_info(amap->return_val);
        if (amap->return_val->array_length_index >= 0) {
            amap->return_val->tuple = GIG_ARG_TUPLE_ARRAY;
            amap->return_val->child =
                g_ptr_array_index(entry_array, amap->return_val->array_length_index);
            amap->return_val->child->tuple = GIG_ARG_TUPLE_ARRAY_SIZE;
            amap->return_val->child->presence = GIG_ARG_PRESENCE_IMPLICIT;
            amap->return_val->child->parent = amap->return_val;
        }
    }

    // We now can decide where these arguments appear in the SCM GSubr
    // call.
    for (gsize arg_info_index = 0; arg_info_index < arg_info_count; arg_info_index++) {
        g_assert_cmpint(arg_info_index, <, entry_array->len);
        GigArgMapEntry *entry = g_ptr_array_index(entry_array, arg_info_index);
        if (entry->s_direction == GIG_ARG_DIRECTION_INPUT ||
            entry->s_direction == GIG_ARG_DIRECTION_INOUT ||
            entry->s_direction == GIG_ARG_DIRECTION_PREALLOCATED_OUTPUT) {
            if (entry->tuple == GIG_ARG_TUPLE_SINGLETON || entry->tuple == GIG_ARG_TUPLE_ARRAY) {
                entry->gsubr_input_index = gsubr_input_index++;
                if (entry->presence == GIG_ARG_PRESENCE_REQUIRED)
                    amap->gsubr_required_input_count++;
                else if (entry->presence == GIG_ARG_PRESENCE_OPTIONAL)
                    amap->gsubr_optional_input_count++;
            }
        }
        else if (entry->s_direction == GIG_ARG_DIRECTION_OUTPUT) {
            if (entry->tuple == GIG_ARG_TUPLE_SINGLETON || entry->tuple == GIG_ARG_TUPLE_ARRAY)
                entry->gsubr_output_index = gsubr_output_index++;
        }
        else
            g_assert_not_reached();

    }


    g_assert_cmpint(amap->gsubr_required_input_count + amap->gsubr_optional_input_count, ==,
                    gsubr_input_index);
    amap->gsubr_output_count = gsubr_output_index;
    amap->cinvoke_input_count = cinvoke_input_index;
    amap->cinvoke_output_count = cinvoke_output_index;
    amap->len = entry_array->len;
    amap->pdata = (GigArgMapEntry **)(entry_array->pdata);
    g_ptr_array_free(entry_array, FALSE);

    return amap;
}

void
gig_arg_map_free(GigArgMap *amap)
{
    for (gint i = 0; i < amap->len; i++) {
        g_base_info_unref(amap->pdata[i]->type_info);
        free(amap->pdata[i]->name);
        free(amap->pdata[i]);
        amap->pdata[i] = NULL;
    }
    g_base_info_unref(amap->return_val->type_info);
    free(amap->return_val->name);
    free(amap->return_val);
    free(amap->pdata);
    amap->pdata = NULL;
    free(amap);
}

void
gig_arg_map_dump(const GigArgMap *amap)
{
    g_debug("Arg map %p", amap);
    g_debug(" Inputs required: %d, optional %d, Outputs %d", amap->gsubr_required_input_count,
            amap->gsubr_optional_input_count, amap->gsubr_output_count);
    g_debug(" C input params: %d, output params %d", amap->cinvoke_input_count,
            amap->cinvoke_output_count);
    for (gint i = 0; i < amap->len; i++) {
        GigArgMapEntry *entry = (GigArgMapEntry *)(amap->pdata[i]);
        g_debug(" Arg %d: '%s'  %s%s%s %s",
                i,
                entry->name,
                entry->is_ptr ? "pointer to " : "",
                entry->is_caller_allocates ? "caller allocated" : "",
                g_type_tag_to_string(entry->type_tag), entry->may_be_null ? "or NULL" : "");
        g_debug(" Arg %d: %10s %10s %10s Index %d, SCM In %d, Out %d, C In %d, Out %d",
                i,
                dir_strings[entry->s_direction],
                tuple_strings[entry->tuple],
                presence_strings[entry->presence],
                entry->arg_info_index, entry->gsubr_input_index, entry->gsubr_output_index,
                entry->cinvoke_input_index, entry->cinvoke_output_index);
    }
}

void
gig_arg_map_get_gsubr_args_count(const GigArgMap *amap, gint *required, gint *optional)
{
    g_assert_nonnull(amap);
    g_assert_nonnull(required);
    g_assert_nonnull(optional);
    *required = amap->gsubr_required_input_count;
    *optional = amap->gsubr_optional_input_count;
}

GigArgMapEntry *
gig_arg_map_get_entry(GigArgMap *amap, gint input)
{
    g_assert_nonnull(amap);
    gint i = 0;
    while (i < amap->len) {
        if (amap->pdata[i]->gsubr_input_index == input) {
            return amap->pdata[i];
        }
        i++;
    }
    g_assert_not_reached();
}

GigArgMapEntry *
gig_arg_map_get_output_entry(GigArgMap *amap, gint output)
{
    g_assert_nonnull(amap);

    gint i = 0;
    while (i < amap->len) {
        if (amap->pdata[i]->cinvoke_output_index == output) {
            return amap->pdata[i];
        }
        i++;
    }
    g_return_val_if_reached(NULL);
}

// If this output element is an array with another output element
// being its size, this returns TRUE and stores the index of the other
// argument.
gboolean
gig_arg_map_has_output_array_size_index(GigArgMap *amap, gint cinvoke_output_index,
                                        gint *cinvoke_output_array_size_index)
{
    g_assert_nonnull(amap);
    g_assert_nonnull(cinvoke_output_array_size_index);

    gint i = 0;
    while (i < amap->len) {
        if (amap->pdata[i]->cinvoke_output_index == cinvoke_output_index) {
            if (amap->pdata[i]->child) {
                *cinvoke_output_array_size_index = amap->pdata[i]->child->cinvoke_output_index;
                return TRUE;
            }
        }
        i++;
    }
    return FALSE;
}

// Get the number of required and optional gsubr arguments for this
// gsubr call.
void
gig_arg_map_get_cinvoke_args_count(const GigArgMap *amap, gint *cinvoke_input_count,
                                   gint *cinvoke_output_count)
{
    g_assert_nonnull(amap);
    g_assert_nonnull(cinvoke_input_count);
    g_assert_nonnull(cinvoke_output_count);

    *cinvoke_input_count = amap->cinvoke_input_count;
    *cinvoke_output_count = amap->cinvoke_output_count;
}

// For the gsubr argument at position INDEX, get the input and output
// index positions for this argument in the C function call.  Return
// TRUE if this gsubr argument is used in the C function call.
gboolean
gig_arg_map_get_cinvoke_indices(const GigArgMap *amap, gint gsubr_input_index,
                                gint *cinvoke_input_index, gint *cinvoke_output_index)
{
    g_assert_nonnull(amap);
    g_assert_nonnull(cinvoke_input_index);
    g_assert_nonnull(cinvoke_output_index);

    gint i = 0;
    while (i < amap->len) {
        if (amap->pdata[i]->gsubr_input_index == gsubr_input_index) {
            *cinvoke_input_index = amap->pdata[i]->cinvoke_input_index;
            *cinvoke_output_index = amap->pdata[i]->cinvoke_output_index;
            return (*cinvoke_input_index >= 0 || *cinvoke_output_index >= 0);
        }
        i++;
    }
    *cinvoke_input_index = -1;
    *cinvoke_output_index = -1;
    g_return_val_if_reached(FALSE);
}

// For the gsubr argument at position INDEX, if it is an array whose
// length is supposed to be computed before the C function is invoked,
// return the input and output positions for the array size in the C
// function call.  Return TRUE if this gsubr argument's array size is
// used in the C function call.
gboolean
gig_arg_map_get_cinvoke_array_length_indices(const GigArgMap *amap, gint gsubr_input_index,
                                             gint *cinvoke_input_index, gint *cinvoke_output_index)
{
    g_assert_nonnull(amap);
    g_assert_nonnull(cinvoke_input_index);
    g_assert_nonnull(cinvoke_output_index);

    gint arg_info_index = 0;
    while (arg_info_index < amap->len) {
        if (amap->pdata[arg_info_index]->gsubr_input_index == gsubr_input_index) {
            GigArgMapEntry *child = amap->pdata[arg_info_index]->child;
            if (child) {
                *cinvoke_input_index = child->cinvoke_input_index;
                *cinvoke_output_index = child->cinvoke_output_index;
                return (*cinvoke_input_index >= 0 || *cinvoke_output_index >= 0);
            }
            else {
                *cinvoke_input_index = -1;
                *cinvoke_output_index = -1;
                return FALSE;
            }
        }
        arg_info_index++;
    }
    *cinvoke_input_index = -1;
    *cinvoke_output_index = -1;
    g_return_val_if_reached(FALSE);
}

gboolean
gig_arg_map_has_gsubr_output_index(const GigArgMap *amap, gint cinvoke_output_index,
                                   gint *gsubr_output_index)
{
    g_assert_nonnull(amap);
    g_assert_nonnull(gsubr_output_index);

    gint i = 0;
    while (i < amap->len) {
        if (amap->pdata[i]->cinvoke_output_index == cinvoke_output_index) {
            gint j = amap->pdata[i]->gsubr_output_index;
            if (j >= 0) {
                *gsubr_output_index = j;
                return TRUE;
            }
            else
                return FALSE;

        }
        i++;
    }
    return FALSE;
}
