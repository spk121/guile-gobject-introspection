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

#include "gig_hash.h"
#include "gig_type.h"

static SCM direct_sym;
static SCM int_sym;
static SCM int64_sym;
static SCM double_sym;
static SCM str_sym;

gboolean
check_key_enum(SCM symbol)
{
    return (scm_is_eq(symbol, direct_sym)
            || scm_is_eq(symbol, int_sym)
            || scm_is_eq(symbol, int64_sym)
            || scm_is_eq(symbol, double_sym)
            || scm_is_eq(symbol, str_sym));
}

SCM
scm_hash_make_ghashtable(SCM hash_type)
{
    GHashFunc hash_func;
    GEqualFunc key_equal_func;
    GHashTable *hash_table;

    SCM_ASSERT_TYPE(scm_is_symbol(hash_type), hash_type, SCM_ARG1, "make-ghashtable", "symbol");

    if (!check_key_enum(hash_type))
        scm_out_of_range("make-ghashtable", hash_type);

    if (scm_is_eq(hash_type, direct_sym)) {
        hash_func = g_direct_hash;
        key_equal_func = g_direct_equal;
    }
    else if (scm_is_eq(hash_type, int_sym)) {
        hash_func = g_int_hash;
        key_equal_func = g_int_equal;
    }
    else if (scm_is_eq(hash_type, int64_sym)) {
        hash_func = g_int64_hash;
        key_equal_func = g_int64_equal;
    }
    else if (scm_is_eq(hash_type, double_sym)) {
        hash_func = g_double_hash;
        key_equal_func = g_double_equal;
    }
    else if (scm_is_eq(hash_type, str_sym)) {
        hash_func = g_str_hash;
        key_equal_func = g_str_equal;
    }
    else
        abort();
    hash_table = g_hash_table_new(hash_func, key_equal_func);
    return gig_type_transfer_object(G_TYPE_HASH_TABLE, hash_table, GI_TRANSFER_EVERYTHING);
}

void
gig_init_hash(void)
{
    direct_sym = scm_from_utf8_symbol("direct");
    int_sym = scm_from_utf8_symbol("int");
    int64_sym = scm_from_utf8_symbol("int64");
    double_sym = scm_from_utf8_symbol("double");
    str_sym = scm_from_utf8_symbol("str");

    scm_c_define_gsubr("make-ghashtable", 1, 0, 0, scm_hash_make_ghashtable);
    scm_c_export("make-ghahstable", NULL);
}
