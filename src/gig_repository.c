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

#include <libguile.h>
#include <girepository.h>
#include "gig_type.h"
#include "gig_object.h"
#include "gig_function.h"
#include "gig_util.h"
#include "gig_constant.h"
#include "gig_flag.h"
#include "gig_repository.h"

static SCM
require(SCM lib, SCM version)
{
    gchar *_lib, *_version = NULL;
    GITypelib *tl;
    GError *error = NULL;
    SCM slib;

    scm_dynwind_begin(0);
    _lib = scm_dynwind_or_bust("require", scm_to_utf8_string(lib));
    if (!SCM_UNBNDP(version) && scm_is_true(version))
        _version = scm_dynwind_or_bust("require", scm_to_utf8_string(version));

    g_debug("requiring %s-%s", _lib, _version != NULL ? _version : "latest");
    tl = g_irepository_require(NULL, _lib, _version, 0, &error);

    if (tl == NULL) {
        SCM err = scm_from_utf8_string(error->message);
        g_error_free(error);
        scm_misc_error("require", "~A", scm_list_1(err));
    }
    scm_dynwind_end();

    return SCM_UNSPECIFIED;
}

static SCM
infos(SCM lib)
{
    scm_dynwind_begin(0);
    gchar *_lib = scm_dynwind_or_bust("infos", scm_to_utf8_string(lib));
    gint n = g_irepository_get_n_infos(NULL, _lib);
    SCM infos = SCM_EOL;

    for (gint i = 0; i < n; i++) {
        GIBaseInfo *info = g_irepository_get_info(NULL, _lib, i);
        if (g_base_info_is_deprecated(info)) {
            g_base_info_unref(info);
            continue;
        }
        infos = scm_cons(gig_type_transfer_object(GI_TYPE_BASE_INFO, info, GI_TRANSFER_EVERYTHING),
                         infos);
    }
    scm_dynwind_end();

    return scm_reverse_x(infos, SCM_EOL);
}

typedef enum _LoadFlags
{
    LOAD_INFO_ONLY = 0,
    LOAD_METHODS = 1 << 0,
    LOAD_PROPERTIES = 1 << 1,
    LOAD_SIGNALS = 1 << 2,
    LOAD_FIELDS = 1 << 3,
    LOAD_EVERYTHING = LOAD_METHODS | LOAD_PROPERTIES | LOAD_SIGNALS | LOAD_FIELDS
} LoadFlags;

void
gig_repository_nested_infos(GIBaseInfo *base,
                            gint *n_methods,
                            GigRepositoryNested *method,
                            gint *n_properties,
                            GigRepositoryNested *property,
                            gint *n_signals,
                            GigRepositoryNested *signal,
                            gint *n_fields, GigRepositoryNested *field)
{
    switch (g_base_info_get_type(base)) {
    case GI_INFO_TYPE_STRUCT:
        *n_methods = g_struct_info_get_n_methods(base);
        *method = (GigRepositoryNested)g_struct_info_get_method;
        *n_properties = *n_signals = *n_fields = 0;
        *property = *signal = *field = NULL;
        break;
    case GI_INFO_TYPE_UNION:
        *n_methods = g_union_info_get_n_methods(base);
        *method = (GigRepositoryNested)g_union_info_get_method;
        *n_properties = *n_signals = *n_fields = 0;
        *property = *signal = *field = NULL;
        break;
    case GI_INFO_TYPE_ENUM:
    case GI_INFO_TYPE_FLAGS:
        *n_methods = g_enum_info_get_n_methods(base);
        *method = (GigRepositoryNested)g_enum_info_get_method;
        *n_properties = *n_signals = *n_fields = 0;
        *property = *signal = *field = NULL;
        break;
    case GI_INFO_TYPE_INTERFACE:
        *n_methods = g_interface_info_get_n_methods(base);
        *method = (GigRepositoryNested)g_interface_info_get_method;
        *n_properties = *n_signals = *n_fields = 0;
        *property = *signal = *field = NULL;
        break;
    case GI_INFO_TYPE_OBJECT:
        *n_methods = g_object_info_get_n_methods(base);
        *method = (GigRepositoryNested)g_object_info_get_method;
        *n_properties = g_object_info_get_n_properties(base);
        *property = (GigRepositoryNested)g_object_info_get_property;
        *n_signals = g_object_info_get_n_signals(base);
        *signal = (GigRepositoryNested)g_object_info_get_signal;
        *n_fields = 0;
        *field = NULL;
        break;
    default:
        *n_methods = *n_properties = *n_signals = *n_fields = 0;
        *method = *property = *signal = *field = NULL;
    }
}

SCM
load_info(GIBaseInfo *info, LoadFlags flags, SCM defs)
{
    g_return_val_if_fail(info != NULL, defs);

    GIBaseInfo *parent = g_base_info_get_container(info);
    GType parent_gtype = G_TYPE_INVALID;
    const gchar *parent_name = NULL;
    if (parent) {
        parent_gtype = g_registered_type_info_get_g_type(parent);
        parent_name = g_base_info_get_name(parent);
    }

    switch (g_base_info_get_type(info)) {
    case GI_INFO_TYPE_CALLBACK:
        g_debug("Unsupported irepository type 'CALLBACK'");
        break;
    case GI_INFO_TYPE_FUNCTION:
    case GI_INFO_TYPE_SIGNAL:
        defs = gig_function_define(parent_gtype, info, parent_name, defs);
        break;
    case GI_INFO_TYPE_PROPERTY:
        g_assert(G_TYPE_IS_CLASSED(parent_gtype));
        gig_property_define(parent_gtype, info, parent_name, defs);
        break;
    case GI_INFO_TYPE_STRUCT:
    {
        GType gtype = g_registered_type_info_get_g_type(info);
        if (gtype == G_TYPE_NONE) {
            g_debug("Not loading struct type '%s' because is has no GType",
                    g_base_info_get_name(info));
            break;
        }
        defs = gig_type_define(gtype, defs);
        if (g_struct_info_get_size(info) > 0) {
            GQuark size_quark = g_quark_from_string("size");
            g_type_set_qdata(gtype, size_quark, GSIZE_TO_POINTER(g_struct_info_get_size(info)));
        }
        goto recursion;
    }
    case GI_INFO_TYPE_ENUM:
    case GI_INFO_TYPE_FLAGS:
    {
        GType gtype = g_registered_type_info_get_g_type(info);
        if (gtype == G_TYPE_NONE)
            defs = gig_define_enum(info, defs);
        else
            defs = gig_type_define(gtype, defs);
        defs = gig_define_enum_conversions(info, gtype, defs);
        goto recursion;
    }
    case GI_INFO_TYPE_OBJECT:
    {
        GType gtype = g_registered_type_info_get_g_type(info);
        const gchar *namespace = g_base_info_get_name(info);
        if (gtype == G_TYPE_NONE) {
            g_debug("Not loading object type '%s' because is has no GType", namespace);
            break;
        }
        defs = gig_type_define(gtype, defs);
        goto recursion;
    }
    case GI_INFO_TYPE_INTERFACE:
    {
        GType gtype = g_registered_type_info_get_g_type(info);
        if (gtype == G_TYPE_NONE) {
            g_debug("Not loading interface type '%s' because is has no GType",
                    g_base_info_get_name(info));
            break;
        }
        defs = gig_type_define(gtype, defs);
        goto recursion;
    }
    case GI_INFO_TYPE_CONSTANT:
        defs = gig_constant_define(info, defs);
        break;
    case GI_INFO_TYPE_UNION:
    {
        GType gtype = g_registered_type_info_get_g_type(info);
        if (gtype == G_TYPE_NONE) {
            g_debug("Not loading union type '%s' because is has no GType",
                    g_base_info_get_name(info));
            break;
        }
        defs = gig_type_define(gtype, defs);
        if (g_union_info_get_size(info) > 0) {
            GQuark size_quark = g_quark_from_string("size");
            g_type_set_qdata(gtype, size_quark, GSIZE_TO_POINTER(g_union_info_get_size(info)));
        }
        goto recursion;
    }
    case GI_INFO_TYPE_VALUE:
        g_critical("Unsupported irepository type 'VALUE'");
        break;
    case GI_INFO_TYPE_VFUNC:
        g_critical("Unsupported irepository type 'VFUNC'");
        break;
    case GI_INFO_TYPE_FIELD:
        g_critical("Unsupported irepository type 'FIELD'");
        break;
    case GI_INFO_TYPE_ARG:
        g_critical("Unsupported irepository type 'ARG'");
        break;
    case GI_INFO_TYPE_TYPE:
        g_critical("Unsupported irepository type 'TYPE'");
        break;
    case GI_INFO_TYPE_INVALID:
    case GI_INFO_TYPE_INVALID_0:
    default:
        g_critical("Unsupported irepository type %d", g_base_info_get_type(info));
        break;
    }

  end:
    return defs;

  recursion:
    {
#define LOAD_NESTED(F, N, I)                                    \
        do {                                                    \
            if (flags & F)                                      \
                for (gint i = 0; i < N; i++)                    \
                    defs = load_info(I(info, i), flags, defs);  \
        } while (0)

        gint n_methods, n_properties, n_signals, n_fields;
        GigRepositoryNested method, property, signal, field;

        gig_repository_nested_infos(info, &n_methods, &method, &n_properties, &property,
                                    &n_signals, &signal, &n_fields, &field);

        LOAD_NESTED(LOAD_METHODS, n_methods, method);
        LOAD_NESTED(LOAD_PROPERTIES, n_properties, property);
        LOAD_NESTED(LOAD_SIGNALS, n_signals, signal);
        LOAD_NESTED(LOAD_FIELDS, n_fields, field);
#undef LOAD_NESTED
        goto end;
    }

}

static SCM
load(SCM info, SCM flags)
{
    LoadFlags load_flags;
    if (SCM_UNBNDP(flags))
        load_flags = LOAD_EVERYTHING;
    else
        load_flags = scm_to_uint(flags);

    GIBaseInfo *base_info = (GIBaseInfo *)gig_type_peek_object(info);

    return load_info(base_info, load_flags, SCM_EOL);
}

static SCM
info(SCM lib, SCM name)
{
    gchar *_lib, *_name;
    GIBaseInfo *info;
    scm_dynwind_begin(0);
    _lib = scm_dynwind_or_bust("info", scm_to_utf8_string(lib));
    _name = scm_dynwind_or_bust("info", scm_to_utf8_string(name));

    info = g_irepository_find_by_name(NULL, _lib, _name);
    if (info == NULL)
        scm_misc_error("info",
                       "could not load ~A from ~A, did you forget to require or perhaps misspell?",
                       scm_list_2(name, lib));
    scm_dynwind_end();

    return gig_type_transfer_object(GI_TYPE_BASE_INFO, info, GI_TRANSFER_EVERYTHING);
}

static SCM
get_search_path(void)
{
    GSList *slist = g_irepository_get_search_path();
    SCM entry;
    SCM output = SCM_EOL;

    if (slist == NULL)
        return SCM_EOL;
    do {
        entry = scm_from_utf8_string(slist->data);
        output = scm_cons(entry, output);
    } while ((slist = g_slist_next(slist)));
    return scm_reverse_x(output, SCM_EOL);
}

static SCM
prepend_search_path(SCM s_dir)
{
    gchar *dir;

    SCM_ASSERT_TYPE(scm_is_string(s_dir), s_dir, SCM_ARG1,
                    "typelib-prepend-search-path", "string");

    dir = scm_to_utf8_string(s_dir);
    g_irepository_prepend_search_path(dir);
    g_free(dir);

    return SCM_UNSPECIFIED;
}

void
gig_init_repository()
{
    scm_c_define_gsubr("require", 1, 1, 0, require);
    scm_c_define_gsubr("infos", 1, 0, 0, infos);
    scm_c_define_gsubr("info", 2, 0, 0, info);
    scm_c_define_gsubr("%load-info", 1, 1, 0, load);
    scm_c_define_gsubr("get-search-path", 0, 0, 0, get_search_path);
    scm_c_define_gsubr("prepend-search-path!", 1, 0, 0, prepend_search_path);

#define D(x) scm_permanent_object(scm_c_define(#x, scm_from_uint(x)))

    D(LOAD_INFO_ONLY);
    D(LOAD_METHODS);
    D(LOAD_PROPERTIES);
    D(LOAD_SIGNALS);
    D(LOAD_FIELDS);
    D(LOAD_EVERYTHING);
}
