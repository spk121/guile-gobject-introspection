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
    [GIG_ARG_DIRECTION_VOID] = "void",
    [GIG_ARG_DIRECTION_INPUT] = "input",
    [GIG_ARG_DIRECTION_INOUT] = "inout",
    [GIG_ARG_DIRECTION_PREALLOCATED_OUTPUT] = "prealloc",
    [GIG_ARG_DIRECTION_OUTPUT] = "output"
};

const gchar tuple_strings[GIG_ARG_TUPLE_COUNT][11] = {
    [GIG_ARG_TUPLE_SINGLETON] = "singleton",
    [GIG_ARG_TUPLE_ARRAY] = "array",
    [GIG_ARG_TUPLE_ARRAY_SIZE] = "array_size"
};

const gchar presence_strings[GIG_ARG_PRESENCE_COUNT][9] = {
    [GIG_ARG_PRESENCE_REQUIRED] = "required",
    [GIG_ARG_PRESENCE_OPTIONAL] = "optional",
    [GIG_ARG_PRESENCE_IMPLICIT] = "implicit"
};

static GigArgMap *arg_map_allocate(gsize n);
static void arg_map_apply_function_info(GigArgMap *amap, GIFunctionInfo *func_info);
static void arg_map_determine_argument_presence(GigArgMap *amap);
static void arg_map_compute_c_invoke_positions(GigArgMap *amap);
static void arg_map_compute_s_call_positions(GigArgMap *amap);
static GigArgMapEntry *arg_map_entry_new(void);
static void arg_map_entry_init(GigArgMapEntry *map);

static GigArgMapEntry *
arg_map_entry_new()
{
    GigArgMapEntry *entry = g_new(GigArgMapEntry, 1);
    arg_map_entry_init(entry);
    return entry;
}

static void
arg_map_entry_init(GigArgMapEntry *entry)
{
    memset(entry, 0, sizeof(GigArgMapEntry));
    entry->array_length_index = -1;
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
gig_amap_entry_apply_arg_info(GigArgMapEntry *e, GIArgInfo *ai)
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
gig_amap_entry_apply_callable_info(GigArgMapEntry *e, GICallableInfo *ci)
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
gig_amap_new(GICallableInfo *function_info)
{
    GigArgMap *amap;
    gsize n;

    n = g_callable_info_get_n_args(function_info);
    amap = arg_map_allocate(n);
    amap->name = g_strdup(g_base_info_get_name(function_info));
    arg_map_apply_function_info(amap, function_info);
    arg_map_determine_argument_presence(amap);
    arg_map_compute_c_invoke_positions(amap);
    arg_map_compute_s_call_positions(amap);
    return amap;
}

static GigArgMap *
arg_map_allocate(gsize n)
{
    GigArgMap *amap;

    amap = g_new0(GigArgMap, 1);
    amap->pdata = g_new0(GigArgMapEntry, n);
    amap->len = n;
    for (gsize i = 0; i < n; i++) {
        arg_map_entry_init(&amap->pdata[i]);
        amap->pdata[i].i = i;
    }
    arg_map_entry_init(&amap->return_val);

    return amap;
}

static void
arg_map_apply_function_info(GigArgMap *amap, GIFunctionInfo *func_info)
{
    gint i, n;
    GIArgInfo *arg_info;
    GigArgMapEntry *entry;

    n = amap->len;

    for (i = 0; i < n; i++) {
        arg_info = g_callable_info_get_arg(func_info, i);
        GITypeInfo *type_info = g_arg_info_get_type(arg_info);
        GITypeTag type_tag = g_type_info_get_tag(type_info);
        entry = &amap->pdata[i];
        gig_amap_entry_apply_arg_info(entry, arg_info);
        g_base_info_unref(type_info);
        g_base_info_unref(arg_info);
        if (type_tag == GI_TYPE_TAG_ARRAY)
            fill_array_info(entry);
    }

    gig_amap_entry_apply_callable_info(&amap->return_val, func_info);
    if (amap->return_val.type_tag == GI_TYPE_TAG_ARRAY)
        fill_array_info(&amap->return_val);
}


static void
arg_map_determine_argument_presence(GigArgMap *amap)
{
    GigArgMapEntry *entry;
    gboolean opt_flag = TRUE;
    gint i, n;

    n = amap->len;

    // may-be-null parameters at the end of the C call can be made
    // optional parameters in the gsubr call.
    for (i = n - 1; i >= 0; i--) {
        entry = &amap->pdata[i];
        entry->tuple = GIG_ARG_TUPLE_SINGLETON;
        entry->i = i;
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

    // In C, if there is an array defined as a pointer and a
    // length parameter, it becomes a single S parameter.
    for (i = 0; i < n; i++) {
        entry = &amap->pdata[i];

        if (entry->type_tag == GI_TYPE_TAG_ARRAY) {
            // Sometime a single SCM array or list will map to two
            // arguments: a C array pointer and a C size parameter.
            if (entry->array_length_index >= 0) {
                entry->tuple = GIG_ARG_TUPLE_ARRAY;
                entry->child = &amap->pdata[entry->array_length_index];
                entry->child->tuple = GIG_ARG_TUPLE_ARRAY_SIZE;
                entry->child->presence = GIG_ARG_PRESENCE_IMPLICIT;
                entry->child->is_s_output = 0;
                entry->child->parent = entry;
            }
        }
    }

    if (amap->return_val.type_tag == GI_TYPE_TAG_ARRAY) {
        if (amap->return_val.array_length_index >= 0) {
            amap->return_val.tuple = GIG_ARG_TUPLE_ARRAY;
            amap->return_val.child = &amap->pdata[amap->return_val.array_length_index];
            amap->return_val.child->tuple = GIG_ARG_TUPLE_ARRAY_SIZE;
            amap->return_val.child->presence = GIG_ARG_PRESENCE_IMPLICIT;
            amap->return_val.child->is_s_output = 0;
            amap->return_val.child->parent = &amap->return_val;
        }
    }

}

static void
arg_map_compute_c_invoke_positions(GigArgMap *amap)
{
    gint n = amap->len;

    gint c_input_pos = 0;
    gint c_output_pos = 0;

    for (gsize i = 0; i < n; i++) {
        GigArgMapEntry *entry = &amap->pdata[i];
        GIDirection dir = entry->c_direction;
        gboolean is_caller_allocates = entry->is_caller_allocates;

        // Here we find the positions of this argument in the
        // g_function_info_invoke call.  Also, some output parameters
        // require a SCM container to be passed in to the SCM GSubr
        // call.
        if (dir == GI_DIRECTION_IN) {
            entry->s_direction = GIG_ARG_DIRECTION_INPUT;
            entry->is_c_input = 1;
            entry->c_input_pos = c_input_pos++;
        }
        else if (dir == GI_DIRECTION_INOUT) {
            entry->s_direction = GIG_ARG_DIRECTION_INOUT;
            entry->is_c_input = 1;
            entry->c_input_pos = c_input_pos++;
            entry->is_c_output = 1;
            entry->c_output_pos = c_output_pos++;
        }
        else if (dir == GI_DIRECTION_OUT && is_caller_allocates) {
            entry->s_direction = GIG_ARG_DIRECTION_PREALLOCATED_OUTPUT;
            entry->is_c_output = 1;
            entry->c_output_pos = c_output_pos++;
        }
        else {
            entry->s_direction = GIG_ARG_DIRECTION_OUTPUT;
            entry->is_c_output = 1;
            entry->c_output_pos = c_output_pos++;
        }
    }

    amap->c_input_len = c_input_pos;
    amap->c_output_len = c_output_pos;
}

static void
arg_map_compute_s_call_positions(GigArgMap *amap)
{
    gsize arg_info_count = amap->len;
    gint s_input_pos = 0;
    gint s_output_pos = 0;
    // We now can decide where these arguments appear in the SCM GSubr
    // call.
    for (gsize i = 0; i < arg_info_count; i++) {
        GigArgMapEntry *entry = &amap->pdata[i];
        if (entry->s_direction == GIG_ARG_DIRECTION_INPUT ||
            entry->s_direction == GIG_ARG_DIRECTION_INOUT ||
            entry->s_direction == GIG_ARG_DIRECTION_PREALLOCATED_OUTPUT) {
            if (entry->tuple == GIG_ARG_TUPLE_SINGLETON || entry->tuple == GIG_ARG_TUPLE_ARRAY) {
                entry->is_s_input = 1;
                entry->s_input_pos = s_input_pos++;
                if (entry->presence == GIG_ARG_PRESENCE_REQUIRED)
                    amap->s_input_req++;
                else if (entry->presence == GIG_ARG_PRESENCE_OPTIONAL)
                    amap->s_input_opt++;
            }
        }
        else if (entry->s_direction == GIG_ARG_DIRECTION_OUTPUT) {
            if (entry->tuple == GIG_ARG_TUPLE_SINGLETON || entry->tuple == GIG_ARG_TUPLE_ARRAY) {
                entry->is_s_output = 1;
                entry->s_output_pos = s_output_pos++;
            }
        }
        else
            g_assert_not_reached();

    }

    amap->s_output_len = s_output_pos;
    g_assert_cmpint(amap->s_input_req + amap->s_input_opt, ==, s_input_pos);
}

void
gig_amap_free(GigArgMap *amap)
{
    for (gint i = 0; i < amap->len; i++) {
        g_base_info_unref(amap->pdata[i].type_info);
        free(amap->pdata[i].name);
    }
    g_base_info_unref(amap->return_val.type_info);
    free(amap->return_val.name);
    free(amap->pdata);
    free(amap->name);
    amap->pdata = NULL;
    free(amap);
}

void
gig_amap_dump(const GigArgMap *amap)
{
    g_debug("Arg map for '%s'", amap->name);
    g_debug(" SCM inputs required: %d, optional: %d, outputs: %d", amap->s_input_req,
            amap->s_input_opt, amap->s_output_len);
    g_debug(" C inputs: %d, outputs: %d", amap->c_input_len, amap->c_output_len);
    for (gint i = 0; i < amap->len; i++) {
        const GigArgMapEntry *entry = &amap->pdata[i];
        GString *s = g_string_new(NULL);
        g_string_append_printf(s, " Arg %d: '%s' %s%s%s",
                               i,
                               entry->name,
                               entry->is_ptr ? "pointer to " : "",
                               entry->is_caller_allocates ? "caller allocated " : "",
                               g_type_tag_to_string(entry->type_tag));
        if (entry->may_be_null)
            g_string_append_printf(s, " or NULL");
        g_string_append_printf(s, ", %s, %s, %s",
                               dir_strings[entry->s_direction],
                               tuple_strings[entry->tuple], presence_strings[entry->presence]);
        if (entry->is_c_input)
            g_string_append_printf(s, ", C input %d", entry->c_input_pos);
        if (entry->is_c_output)
            g_string_append_printf(s, ", C output %d", entry->c_output_pos);
        if (entry->is_s_input)
            g_string_append_printf(s, ", SCM input %d", entry->s_input_pos);
        if (entry->is_s_output)
            g_string_append_printf(s, ", S output %d", entry->c_output_pos);
        g_debug("%s", s->str);
        g_string_free(s, TRUE);
    }
    if (amap->return_val.type_tag != GI_TYPE_TAG_VOID) {
        GigArgMapEntry *entry = &amap->return_val;
        GString *s = g_string_new(NULL);
        g_string_append_printf(s, " Return: '%s' %s%s%s",
                               entry->name,
                               entry->is_ptr ? "pointer to " : "",
                               entry->is_caller_allocates ? "caller allocated " : "",
                               g_type_tag_to_string(entry->type_tag));
        if (entry->may_be_null)
            g_string_append_printf(s, " or NULL");
        g_string_append_printf(s, ", %s, %s, %s",
                               dir_strings[entry->s_direction],
                               tuple_strings[entry->tuple], presence_strings[entry->presence]);
        g_debug("%s", s->str);
        g_string_free(s, TRUE);
    }
}

void
gig_amap_s_input_count(const GigArgMap *amap, gint *required, gint *optional)
{
    g_assert_nonnull(amap);
    g_assert_nonnull(required);
    g_assert_nonnull(optional);
    *required = amap->s_input_req;
    *optional = amap->s_input_opt;
}

GigArgMapEntry *
gig_amap_get_input_entry_by_s(GigArgMap *amap, gint spos)
{
    g_assert_nonnull(amap);
    gint i = 0;
    while (i < amap->len) {
        if (amap->pdata[i].is_s_input && (amap->pdata[i].s_input_pos == spos)) {
            return &amap->pdata[i];
        }
        i++;
    }
    g_assert_not_reached();
}

GigArgMapEntry *
gig_amap_get_output_entry_by_c(GigArgMap *amap, gint cpos)
{
    g_assert_nonnull(amap);

    gint i = 0;
    while (i < amap->len) {
        if (amap->pdata[i].is_c_output && (amap->pdata[i].c_output_pos == cpos)) {
            return &amap->pdata[i];
        }
        i++;
    }
    g_return_val_if_reached(NULL);
}

// If this output element is an array with another output element
// being its size, this returns TRUE and stores the index of the other
// argument.
gboolean
gig_amap_output_child_c(GigArgMap *amap, gint c_output_pos, gint *cinvoke_output_array_size_index)
{
    g_assert_nonnull(amap);
    g_assert_nonnull(cinvoke_output_array_size_index);

    gint i = 0;
    while (i < amap->len) {
        if (amap->pdata[i].is_c_output && (amap->pdata[i].c_output_pos == c_output_pos)) {
            if (amap->pdata[i].child) {
                *cinvoke_output_array_size_index = amap->pdata[i].child->c_output_pos;
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
gig_amap_c_count(const GigArgMap *amap, gint *c_input_len, gint *c_output_len)
{
    g_assert_nonnull(amap);
    g_assert_nonnull(c_input_len);
    g_assert_nonnull(c_output_len);

    *c_input_len = amap->c_input_len;
    *c_output_len = amap->c_output_len;
}

// For the gsubr argument at position INDEX, get the input and output
// index positions for this argument in the C function call.  Return
// TRUE if this gsubr argument is used in the C function call.

gboolean
gig_amap_input_s_2_input_c(const GigArgMap *amap, gint s_input_pos, gint *c_input_pos)
{
    g_assert_nonnull(amap);
    g_assert_nonnull(c_input_pos);

    gint i = 0;
    while (i < amap->len) {
        if (amap->pdata[i].is_s_input && (amap->pdata[i].s_input_pos == s_input_pos)) {
            if (amap->pdata[i].is_c_input) {
                *c_input_pos = amap->pdata[i].c_input_pos;
                return TRUE;
            }
            else
                return FALSE;
        }
        i++;
    }
    g_return_val_if_reached(FALSE);
}

gboolean
gig_amap_input_s_2_output_c(const GigArgMap *amap, gint s_input_pos, gint *c_output_pos)
{
    g_assert_nonnull(amap);
    g_assert_nonnull(c_output_pos);

    gint i = 0;
    while (i < amap->len) {
        if (amap->pdata[i].is_s_input && (amap->pdata[i].s_input_pos == s_input_pos)) {
            if (amap->pdata[i].is_c_output) {
                *c_output_pos = amap->pdata[i].c_output_pos;
                return TRUE;
            }
            else
                return FALSE;
        }
        i++;
    }
    g_return_val_if_reached(FALSE);
}

// For the gsubr argument at position INDEX, if it is an array whose
// length is supposed to be computed before the C function is invoked,
// return the input and output positions for the array size in the C
// function call.  Return TRUE if this gsubr argument's array size is
// used in the C function call.
gboolean
gig_amap_input_s_2_child_input_c(const GigArgMap *amap, gint s_input_pos, gint *c_input_pos)
{
    g_assert_nonnull(amap);
    g_assert_nonnull(c_input_pos);

    gint i = 0;
    while (i < amap->len) {
        if (amap->pdata[i].is_s_input && (amap->pdata[i].s_input_pos == s_input_pos)) {
            GigArgMapEntry *child = amap->pdata[i].child;
            if (child) {
                if (child->is_c_input) {
                    *c_input_pos = child->c_input_pos;
                    return TRUE;
                }
                else
                    return FALSE;
            }
            else
                return FALSE;
        }
        i++;
    }
    g_return_val_if_reached(FALSE);
}

gboolean
gig_amap_input_s_2_child_output_c(const GigArgMap *amap, gint s_input_pos, gint *c_output_pos)
{
    g_assert_nonnull(amap);
    g_assert_nonnull(c_output_pos);

    gint i = 0;
    while (i < amap->len) {
        if (amap->pdata[i].is_s_input && (amap->pdata[i].s_input_pos == s_input_pos)) {
            GigArgMapEntry *child = amap->pdata[i].child;
            if (child) {
                if (child->is_c_output) {
                    *c_output_pos = child->c_output_pos;
                    return TRUE;
                }
                else
                    return FALSE;
            }
            else
                return FALSE;
        }
        i++;
    }
    g_return_val_if_reached(FALSE);
}

////////////////////////////////////////////////////////////////

gboolean
gig_amap_input_i2c(const GigArgMap *amap, gint i, gint *cpos)
{
    if (i < 0 || i >= amap->len)
        g_return_val_if_reached(FALSE);
    if (!amap->pdata[i].is_c_input)
        return FALSE;
    *cpos = amap->pdata[i].c_input_pos;
    return TRUE;
}

gboolean
gig_amap_input_i2s(const GigArgMap *amap, gint i, gint *spos)
{
    if (i < 0 || i >= amap->len)
        g_return_val_if_reached(FALSE);
    if (!amap->pdata[i].is_s_input)
        return FALSE;
    *spos = amap->pdata[i].s_input_pos;
    return TRUE;
}

gboolean
gig_amap_input_c2i(const GigArgMap *amap, gint cpos, gint *i)
{
    if (cpos < 0 || cpos >= amap->c_input_len) {
        g_return_val_if_reached(FALSE);
    }
    gint j = 0;
    while (j < amap->len) {
        if (amap->pdata[j].is_c_input && (amap->pdata[j].c_input_pos == cpos)) {
            *i = j;
            return TRUE;
        }
        j++;
    }
    return FALSE;
}


gboolean
gig_amap_input_s2i(const GigArgMap *amap, gint spos, gint *i)
{
    if (spos < 0 || spos >= amap->s_input_req + amap->s_input_opt) {
        g_return_val_if_reached(FALSE);
    }
    gint j = 0;
    while (j < amap->len) {
        if (amap->pdata[j].is_s_input && (amap->pdata[j].s_input_pos == spos)) {
            *i = j;
            return TRUE;
        }
        j++;
    }
    return FALSE;
}

gboolean
gig_amap_input_c2s(const GigArgMap *amap, gint cpos, gint *spos)
{
    if (cpos < 0 || cpos >= amap->c_input_len) {
        g_return_val_if_reached(FALSE);
    }
    gint j = 0;
    while (j < amap->len) {
        if (amap->pdata[j].is_c_input && (amap->pdata[j].c_input_pos == cpos)) {
            if (amap->pdata[j].is_s_input) {
                *spos = amap->pdata[j].s_input_pos;
                return TRUE;
            }
            else {
                return FALSE;
            }
        }
        j++;
    }
    return FALSE;
}

gboolean
gig_amap_input_s2c(const GigArgMap *am, gint spos, gint *cpos)
{
    if (spos < 0 || spos >= am->s_input_req + am->s_input_opt) {
        g_return_val_if_reached(FALSE);
    }
    int j = 0;
    while (j < am->len) {
        if (am->pdata[j].is_s_input && (am->pdata[j].s_input_pos == spos)) {
            if (am->pdata[j].is_c_input) {
                *cpos = am->pdata[j].c_input_pos;
                return TRUE;
            }
            else
                return FALSE;
        }
        j++;
    }
    return FALSE;
}

////////////////////////////////////////////////////////////////

gboolean
gig_amap_output_i2c(const GigArgMap *amap, gint i, gint *cpos)
{
    if (i < 0 || i >= amap->len)
        g_return_val_if_reached(FALSE);
    if (!amap->pdata[i].is_c_output)
        return FALSE;
    *cpos = amap->pdata[i].c_output_pos;
    return TRUE;
}

gboolean
gig_amap_output_i2s(const GigArgMap *amap, gint i, gint *spos)
{
    if (i < 0 || i >= amap->len)
        g_return_val_if_reached(FALSE);
    if (!amap->pdata[i].is_s_output)
        return FALSE;
    *spos = amap->pdata[i].s_output_pos;
    return TRUE;
}

gboolean
gig_amap_output_c2i(const GigArgMap *amap, gint cpos, gint *i)
{
    if (cpos < 0 || cpos >= amap->c_output_len)
        g_return_val_if_reached(FALSE);
    gint j = 0;
    while (j < amap->len) {
        if (amap->pdata[j].is_c_output && (amap->pdata[j].c_output_pos == cpos)) {
            *i = j;
            return TRUE;
        }
        j++;
    }
    return FALSE;
}


gboolean
gig_amap_output_s2i(const GigArgMap *amap, gint spos, gint *i)
{
    if (spos < 0 || spos >= amap->s_output_len)
        g_return_val_if_reached(FALSE);
    gint j = 0;
    while (j < amap->len) {
        if (amap->pdata[j].is_s_output && (amap->pdata[j].s_output_pos == spos)) {
            *i = j;
            return TRUE;
        }
        j++;
    }
    return FALSE;
}

gboolean
gig_amap_output_c2s(const GigArgMap *amap, gint cpos, gint *spos)
{
    if (cpos < 0 || cpos >= amap->c_output_len)
        g_return_val_if_reached(FALSE);
    gint j = 0;
    while (j < amap->len) {
        if (amap->pdata[j].is_c_output && (amap->pdata[j].c_output_pos == cpos)) {
            if (amap->pdata[j].is_s_output) {
                *spos = amap->pdata[j].s_output_pos;
                return TRUE;
            }
            else
                return FALSE;
        }
        j++;
    }
    return FALSE;
}

gboolean
gig_amap_output_s2c(const GigArgMap *am, gint spos, gint *cpos)
{
    if (spos < 0 || spos >= am->s_output_len)
        g_return_val_if_reached(FALSE);
    int j = 0;
    while (j < am->len) {
        if (am->pdata[j].is_s_output && (am->pdata[j].s_output_pos == spos)) {
            if (am->pdata[j].is_c_output) {
                *cpos = am->pdata[j].c_output_pos;
                return TRUE;
            }
            else
                return FALSE;
        }
        j++;
    }
    return FALSE;
}

////////////////////////////////////////////////////////////////

gboolean
gig_amap_child_i(const GigArgMap *am, gint i, gint *ichild)
{
    if (am->pdata[i].child == NULL)
        return FALSE;
    *ichild = am->pdata[i].child->i;
    return TRUE;
}

gboolean
gig_amap_return_child_i(const GigArgMap *am, gint *ichild)
{
    if (am->return_val.child == NULL)
        return FALSE;
    *ichild = am->return_val.child->i;
    return TRUE;
}
