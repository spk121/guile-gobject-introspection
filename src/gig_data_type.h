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

extern GType g_type_unichar;
extern GType g_type_int16;
extern GType g_type_int32;
extern GType g_type_uint16;
extern GType g_type_uint32;
extern GType g_type_locale_string;

extern GType g_type_fixed_size_carray;
extern GType g_type_zero_terminated_carray;
extern GType g_type_length_carray;
extern GType g_type_list;
extern GType g_type_slist;
extern GType g_type_callback;

#define G_TYPE_UNICHAR (g_type_unichar)
#define G_TYPE_INT16 (g_type_int16)
#define G_TYPE_INT32 (g_type_int32)
#define G_TYPE_UINT16 (g_type_uint16)
#define G_TYPE_UINT32 (g_type_uint32)

#define G_TYPE_LENGTH_CARRAY (g_type_length_carray)
#define G_TYPE_ZERO_TERMINATED_CARRAY (g_type_zero_terminated_carray)
#define G_TYPE_FIXED_SIZE_CARRAY (g_type_fixed_size_carray)
#define G_TYPE_LIST (g_type_list)
#define G_TYPE_SLIST (g_type_slist)
#define G_TYPE_LOCALE_STRING (g_type_locale_string)
#define G_TYPE_CALLBACK (g_type_callback)

typedef struct _GigTypeMeta GigTypeMeta;
struct _GigTypeMeta
{
    GType gtype;
    guint16 is_ptr:1;

    // For argument and return values
    guint16 is_in:1;
    guint16 is_out:1;

    // For return values
    guint16 is_skip:1;          // TRUE when output is ignored

    // For pointers
    guint16 is_caller_allocates:1;
    guint16 is_optional:1;      // Out-only. Pass in NULL to ignore this.
    guint16 is_nullable:1;      // For in, can pass in NULL. For out, may return NULL.

    // For container types
    guint16 is_transfer_ownership:1;
    guint16 is_transfer_container:1;

    // Error status
    guint16 is_invalid:1;       // True when one of the arguments has
    // invalid type
    guint16 padding1:6;

    // For C array types
    guint16 length;
    guint16 item_size;

    // Subtypes and callables
    guint16 n_params;
    union
    {
        GigTypeMeta *params;
        GICallableInfo *callable_info;
    };
};

void gig_type_meta_init_from_arg_info(GigTypeMeta *type, GIArgInfo *ai);
void gig_type_meta_init_from_callable_info(GigTypeMeta *type, GICallableInfo *ci);
const char *gig_type_meta_describe(const GigTypeMeta *meta);
void gig_init_data_type(void);

G_END_DECLS
#endif
