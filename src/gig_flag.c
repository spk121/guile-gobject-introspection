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

#include <inttypes.h>
#include <libguile.h>
#include "gig_flag.h"
#include "gig_type.h"
#include "gig_type_private.h"
#include "gig_util.h"

static SCM enum_to_number;
static SCM flags_to_number;
static SCM number_to_enum;
static SCM number_to_flags;
static SCM symbol_to_enum;
static SCM list_to_flags;

GHashTable *gig_flag_hash = NULL;

gint
gig_enum_to_int(SCM val)
{
    return scm_to_int(scm_call_1(enum_to_number, val));
}

guint
gig_flags_to_uint(SCM val)
{
    return scm_to_uint(scm_call_1(flags_to_number, val));
}

SCM
gig_int_to_enum(gint v, GType gtype)
{
    SCM type = gig_type_get_scheme_type(gtype);
    SCM val = scm_from_int(v);
    return scm_call_2(number_to_enum, type, val);
}

SCM
gig_uint_to_flags(guint v, GType gtype)
{
    SCM type = gig_type_get_scheme_type(gtype);
    SCM val = scm_from_uint(v);
    return scm_call_2(number_to_flags, type, val);
}

static gchar *
enum_info_to_class_name(GIEnumInfo *info)
{
    const gchar *namespace = g_base_info_get_namespace(info);
    const gchar *prefix = g_irepository_get_c_prefix(NULL, namespace);
    // use '%' to avoid name clashes
    return g_strdup_printf("<%%%s%s>", prefix, g_base_info_get_name(info));
}

static SCM
gig_flag_ref(GIEnumInfo *info)
{
    gig_define_enum(info, SCM_UNDEFINED);
    gchar *name = enum_info_to_class_name(info);
    gpointer type = g_hash_table_lookup(gig_flag_hash, name);
    g_free(name);
    return SCM_PACK_POINTER(type);
}

SCM
gig_int_to_enum_with_info(gint v, GIEnumInfo *info)
{
    SCM type = gig_flag_ref(info);
    SCM val = scm_from_int(v);
    return scm_call_2(number_to_enum, type, val);
}

SCM
gig_uint_to_flags_with_info(guint v, GIEnumInfo *info)
{
    SCM type = gig_flag_ref(info);
    SCM val = scm_from_uint(v);
    return scm_call_2(number_to_flags, type, val);
}

SCM
gig_symbol_to_enum(SCM type, SCM symbol)
{
    return scm_call_2(symbol_to_enum, type, symbol);
}

SCM
gig_list_to_flags(SCM type, SCM list)
{
    return scm_call_2(list_to_flags, type, list);
}

void
gig_flag_fini(void)
{
    g_hash_table_remove_all(gig_flag_hash);
}

void
gig_init_flag(void)
{
    enum_to_number = scm_c_public_ref("gi types", "enum->number");
    flags_to_number = scm_c_public_ref("gi types", "flags->number");
    number_to_enum = scm_c_public_ref("gi types", "number->enum");
    symbol_to_enum = scm_c_public_ref("gi types", "symbol->enum");
    number_to_flags = scm_c_public_ref("gi types", "number->flags");
    list_to_flags = scm_c_public_ref("gi types", "list->flags");

    gig_flag_hash = g_hash_table_new(g_str_hash, g_str_equal);
    atexit(gig_flag_fini);
}

SCM
gig_define_enum(GIEnumInfo *info, SCM defs)
{
    gint n_values = g_enum_info_get_n_values(info);
    gint i = 0;
    GIValueInfo *vi = NULL;
    gchar *_key;
    SCM key, val;
    SCM class;

    gchar *name = enum_info_to_class_name(info);
    gpointer _name, _type;
    if (g_hash_table_lookup_extended(gig_flag_hash, name, &_name, &_type)) {
        g_free(name);
        defs = scm_cons(SCM_PACK_POINTER(_type), defs);
        return defs;
    }

    switch (g_base_info_get_type(info)) {
    case GI_INFO_TYPE_ENUM:
        class = scm_call_4(make_class_proc, scm_list_1(gig_enum_type), SCM_EOL, kwd_name,
                           scm_from_utf8_symbol(name));
        break;
    case GI_INFO_TYPE_FLAGS:
        class = scm_call_4(make_class_proc, scm_list_1(gig_flags_type), SCM_EOL, kwd_name,
                           scm_from_utf8_symbol(name));
        break;
    default:
        g_assert_not_reached();
    }

    SCM obarray = scm_make_hash_table(scm_from_int(n_values));

    while (i < n_values) {
        vi = g_enum_info_get_value(info, i);
        _key = gig_gname_to_scm_name(g_base_info_get_name(vi));
        key = scm_from_utf8_symbol(_key);
        gint64 _val = g_value_info_get_value(vi);
        val = scm_from_int64(_val);

        g_debug("defining flag/enum %s and %" PRId64, _key, _val);
        scm_hashq_set_x(obarray, key, val);

        g_base_info_unref(vi);
        g_free(_key);
        i++;
    }

    scm_class_set_x(class, sym_obarray, obarray);

    scm_define(scm_class_name(class), class);
    defs = scm_cons(scm_class_name(class), defs);

    g_hash_table_insert(gig_flag_hash, name, SCM_UNPACK_POINTER(class));

    return defs;
}
