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

#ifndef GIG_DATA_TYPE_H
#define GIG_DATA_TYPE_H
#include <glib.h>
#include <girepository.h>

// *INDENT-OFF*
G_BEGIN_DECLS
// *INDENT-ON*

typedef struct _GigTypeMeta GigTypeMeta;
struct _GigTypeMeta
{
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
};

void gig_type_meta_init_from_arg_info(GigTypeMeta * type, GIArgInfo *ai);
void gig_type_meta_init_from_callable_info(GigTypeMeta * type, GICallableInfo *ci);
const char *gig_type_meta_describe(GigTypeMeta * meta);

G_END_DECLS
#endif
