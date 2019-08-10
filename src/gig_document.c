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
#include <glib.h>
#include <glib-object.h>
#include <girepository.h>
#include "gig_type.h"
#include "gig_util.h"
#include "gi_callable_info.h"
#include "gig_arg_map.h"
#include "gig_repository.h"

static void do_document(GIBaseInfo *info, const gchar *parent);

static void
document_nested(GIBaseInfo *parent)
{
#define DOCUMENT_NESTED(N, I)                               \
    do {                                                    \
        for (gint i = 0; i < N; i++)                        \
            do_document(I(parent, i), namespace);           \
    } while (0)

    gchar *namespace = gig_gname_to_scm_name(g_base_info_get_name(parent));
    scm_dynwind_free(namespace);

    gint n_methods, n_properties, n_signals, n_fields;
    GigRepositoryNested method, property, signal, field;

    gig_repository_nested_infos(parent, &n_methods, &method, &n_properties, &property,
                                &n_signals, &signal, &n_fields, &field);

    DOCUMENT_NESTED(n_methods, method);
    DOCUMENT_NESTED(n_properties, property);
    DOCUMENT_NESTED(n_signals, signal);
    DOCUMENT_NESTED(n_fields, field);

#undef DOCUMENT_NESTED
}

static void
document_arg_entry(const gchar *kind, GigArgMapEntry *entry)
{
    gchar *name = gig_gname_to_scm_name(entry->name);
    scm_dynwind_free(name);
    scm_printf(SCM_UNDEFINED, "<%s name=\"%s\" c:name=\"%s\">", kind, name, entry->name);
    scm_printf(SCM_UNDEFINED, "</%s>", kind);
}

static void
do_document(GIBaseInfo *info, const gchar *namespace)
{
#define FUNC "%document"
    gchar *scheme_name, *kind;
    GigArgMap *arg_map;
    gint in, out, req, opt;
    GIInfoType type = g_base_info_get_type(info);

    scm_dynwind_begin(0);
    switch (type) {
    case GI_INFO_TYPE_FUNCTION:
        if (g_callable_info_is_method(info))
            kind = "method";
        else
            kind = "function";

        scm_printf(SCM_UNDEFINED, "<%s name=\"%s\">", kind, g_base_info_get_name(info));
        scheme_name = scm_dynwind_or_bust(FUNC, gi_callable_info_make_name(info, NULL));

        if (g_callable_info_is_method(info))
            scm_printf(SCM_UNDEFINED, "<scheme><procedure name=\"%s\" long-name=\"%s:%s\">",
                       scheme_name, namespace, scheme_name);
        else if (namespace != NULL)
            scm_printf(SCM_UNDEFINED, "<scheme><procedure name=\"%s:%s\">", namespace,
                       scheme_name);
        else
            scm_printf(SCM_UNDEFINED, "<scheme><procedure name=\"%s\">", scheme_name);


        arg_map = gig_amap_new(info);
        scm_dynwind_unwind_handler((scm_t_pointer_finalizer) gig_amap_free,
                                   arg_map, SCM_F_WIND_EXPLICITLY);
        gig_amap_get_cinvoke_args_count(arg_map, &in, &out);
        gig_amap_get_gsubr_args_count(arg_map, &req, &opt);

        if (g_callable_info_is_method(info)) {
            scm_printf(SCM_UNDEFINED, "<argument name=\"self\">");
            scm_printf(SCM_UNDEFINED, "</argument>");
        }

        for (gint i = 0; i < req + opt; i++) {
            GigArgMapEntry *entry = gig_amap_get_entry(arg_map, i);
            document_arg_entry("argument", entry);
        }
        if (arg_map->return_val->type_tag != GI_TYPE_TAG_VOID)
            document_arg_entry("return", arg_map->return_val);

        for (gint i = 0; i < out; i++) {
            GigArgMapEntry *entry = gig_amap_get_output_entry(arg_map, i);
            document_arg_entry("return", entry);
        }
        scm_printf(SCM_UNDEFINED, "</procedure></scheme></%s>", kind);
        break;

    case GI_INFO_TYPE_STRUCT:
    case GI_INFO_TYPE_OBJECT:
    case GI_INFO_TYPE_INTERFACE:
    case GI_INFO_TYPE_UNION:
    {
        GType gtype = g_registered_type_info_get_g_type(info);
        if (gtype == G_TYPE_NONE)
            break;

        switch (type) {
        case GI_INFO_TYPE_STRUCT:
            kind = "record";
            break;
        case GI_INFO_TYPE_UNION:
            kind = "union";
            break;
        case GI_INFO_TYPE_INTERFACE:
            kind = "interface";
            break;
        case GI_INFO_TYPE_OBJECT:
            kind = "class";
            break;
        }

        scm_printf(SCM_UNDEFINED, "<%s name=\"%s\">", kind, g_base_info_get_name(info));
        scm_printf(SCM_UNDEFINED, "<scheme><type name=\"&lt;%s&gt;\" /></scheme>",
                   g_type_name(gtype));

        document_nested(info);

        scm_printf(SCM_UNDEFINED, "</%s>", kind);
        break;
    }

    case GI_INFO_TYPE_ENUM:
    case GI_INFO_TYPE_FLAGS:
        scm_printf(SCM_UNDEFINED, "<enumeration name=\"%s\">", g_base_info_get_name(info));
        scm_printf(SCM_UNDEFINED, "</enumeration>");
        break;

    case GI_INFO_TYPE_SIGNAL:
        scm_printf(SCM_UNDEFINED, "<signal name=\"%s\" />", g_base_info_get_name(info));
        break;
    case GI_INFO_TYPE_VFUNC:
    case GI_INFO_TYPE_PROPERTY:
        scm_printf(SCM_UNDEFINED, "<property name=\"%s\" />", g_base_info_get_name(info));
        break;
    case GI_INFO_TYPE_FIELD:
        scm_printf(SCM_UNDEFINED, "<field name=\"%s\" />", g_base_info_get_name(info));
        break;

    case GI_INFO_TYPE_CONSTANT:
    case GI_INFO_TYPE_CALLBACK:
    case GI_INFO_TYPE_VALUE:
    case GI_INFO_TYPE_ARG:
    case GI_INFO_TYPE_TYPE:
        break;
    case GI_INFO_TYPE_INVALID:
    case GI_INFO_TYPE_INVALID_0:
    default:
        g_critical("Unsupported irepository type %d", type);
        break;
    }
    scm_dynwind_end();
#undef FUNC
}

static SCM
_document(SCM info)
{
    GIBaseInfo *real_info = (GIBaseInfo *)gig_type_peek_object(info);
    do_document(real_info, NULL);
    return SCM_UNSPECIFIED;
}

void
gig_init_document()
{
    scm_c_define_gsubr("%document", 1, 0, 0, _document);
}
