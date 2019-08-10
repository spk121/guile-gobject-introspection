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

#ifndef GIG_AMAP_H
#define GIG_AMAP_H

#include <glib.h>
#include <girepository.h>

// *INDENT-OFF*
G_BEGIN_DECLS
// *INDENT-ON*

// This module gathers information to help convert between 3 paradigms
//  - arguments as they appear in GIArgInfo lists
//  - arguments as they appear in SCM gsubr calls
//  - arguments as they appear in g_function_info_invoke calls
typedef enum
{
    GIG_ARG_DIRECTION_VOID,
    GIG_ARG_DIRECTION_INPUT,
    GIG_ARG_DIRECTION_INOUT,
    GIG_ARG_DIRECTION_PREALLOCATED_OUTPUT,
    GIG_ARG_DIRECTION_OUTPUT,
    GIG_ARG_DIRECTION_COUNT
} GigArgDirection;

typedef enum
{
    GIG_ARG_TUPLE_SINGLETON,
    GIG_ARG_TUPLE_ARRAY,
    GIG_ARG_TUPLE_ARRAY_SIZE,
    GIG_ARG_TUPLE_COUNT
} GigArgTuple;

typedef enum
{
    GIG_ARG_PRESENCE_REQUIRED,
    GIG_ARG_PRESENCE_OPTIONAL,
    GIG_ARG_PRESENCE_IMPLICIT,
    GIG_ARG_PRESENCE_COUNT
} GigArgPresence;

// This structure contains the information necessary for choosing how
// to convert between scheme and C arguments of a function or method
// call.
typedef struct _GigArgMapEntry GigArgMapEntry;
struct _GigArgMapEntry
{
    gchar *name;

    ////////////////////////////////////////////////////////////////
    // This block is similar to GIArgInfo, except it is generic for
    // both arguments and return types
    GITypeInfo *type_info;
    GITypeTag type_tag;
    gboolean is_ptr;
    // The direction of the C argument, which may differ from the SCM
    GIDirection c_direction;
    // Nothing, container, or everything
    GITransfer transfer;
    // For C 'in' values, whether NULL is a valid value.  For 'out'
    // values, whether NULL may be returned.
    gboolean may_be_null;
    // For C 'out' values, whether this argument is allocated by the
    // caller.
    gboolean is_caller_allocates;

    ////////////////////////////////////////////////////////////////
    // This block is additional data that is valid only for arrays

    // The array itself
    GIArrayType array_type;
    gsize array_fixed_size;
    gint array_length_index;
    gboolean array_is_zero_terminated;

    // The elements of the array
    GITransfer item_transfer;
    GITypeTag item_type_tag;
    gboolean item_is_ptr;
    gsize item_size;

    // The objects held by elements of the array
    GIInfoType referenced_base_type;
    GType referenced_object_type;

    ////////////////////////////////////////////////////////////////
    // This block is derived information about how to map
    // Scheme to C and back again

    GigArgDirection s_direction;
    // If this argument can be mapped standalone, or requires another
    // argument's information to map between C and Scheme
    GigArgTuple tuple;
    // If this arg is optional in the Scheme GSubr.
    GigArgPresence presence;
    // This arg's index in g_callable_info_get_arg()


    gint i;
    // This arg's position in input args of g_function_info_invoke
    gint c_input_pos;
    // This arg's position in the output args of g_function_info_invoke
    gint c_output_pos;
    // This arg's position in the Scheme GSubr
    gint s_input_pos;
    // This arg's position in the return values list
    gint s_output_pos;
    // When non-NULL, this is the entry of the array length argument
    // for this array argument.

    GigArgMapEntry *child;
    GigArgMapEntry *parent;
};

typedef struct _GigArgMap GigArgMap;
struct _GigArgMap
{
    gchar *name;

    // S arguments.
    gint s_input_req;
    gint s_input_opt;

    // S return values
    gint s_output_len;

    // For g_function_invoke call
    gint c_input_len;
    gint c_output_len;

    // An array of arg_map_entry
    GigArgMapEntry **pdata;
    gint len;

    GigArgMapEntry *return_val;
};

GigArgMap *gig_amap_new(GICallableInfo *function_info);
void gig_amap_free(GigArgMap *am);
void gig_amap_dump(const GigArgMap *am);

void gig_amap_get_gsubr_args_count(const GigArgMap *am, gint *gsubr_required_input_count,
                                   gint *gsubr_optional_input_count);
GigArgMapEntry *gig_amap_get_entry(GigArgMap *am, gint g_input_pos);
GigArgMapEntry *gig_amap_get_output_entry(GigArgMap *am, gint c_output_pos);
gboolean gig_amap_has_output_array_size_index(GigArgMap *am, gint c_output_pos,
                                              gint *cinvoke_output_array_size_index);
void gig_amap_get_cinvoke_args_count(const GigArgMap *am, gint *c_input_pos, gint *c_output_pos);
gboolean gig_amap_get_cinvoke_indices(const GigArgMap *am, gint s_input_pos,
                                      gint *c_input_pos, gint *c_output_pos);
gboolean gig_amap_get_cinvoke_array_length_indices(const GigArgMap *am, gint s_input_pos,
                                                   gint *c_input_pos, gint *c_output_pos);
gboolean gig_amap_has_s_output_pos(const GigArgMap *am, gint c_output_pos, gint *s_output_pos);
G_END_DECLS
#endif
