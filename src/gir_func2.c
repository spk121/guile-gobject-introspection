// gir_func2.c - introspection of typelib files
// Copyright (C) 2018 Michael L. Gran

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
#include "gi_giargument.h"
#include "gir_type.h"
#include "gir_func2.h"
#include "gir_function.h"
#include "gi_gobject.h"
#include "gi_gtype.h"
#include "gi_gstruct.h"
#include "gir_method.h"
#include "gir_constant.h"
#include "gir_flag.h"

#define GIR_FUNC2_INIT_HASH_TABLE_SIZE 10

GSList *all_fo_types = NULL;

static SCM get_hash_table(const char *name);
static void hash_table_insert(const char *table_name, const char *namespace_,
    const char *parent,
    GIBaseInfo *info);
static gchar *type_public_name(GIBaseInfo *info);
static gchar *type_class_public_name(GIBaseInfo *info);
static gchar *callable_public_name(const char *namespace_, const char *parent, GICallableInfo *info);

static void document_callback_info(GString **export, const char *namespace_, const char *parent, GICallableInfo *info);
static void document_callable_info(GString **export, const char *namespace_, const char *parent, GICallableInfo *info, gboolean method);
static void export_type_info(GString **export, const char *namespace_, const char *parent, GIRegisteredTypeInfo *info, GIInfoType type);

static const char *abbrev_namespace(const char *namespace_) __attribute__((const));
static char *gname_to_scm_name(const char *gname);


#define MAX_GERROR_MSG 100
static char gerror_msg[MAX_GERROR_MSG];

static void
store_gerror_message(const char *msg)
{
    memset(gerror_msg, 0, MAX_GERROR_MSG);
    strncpy(gerror_msg, msg, MAX_GERROR_MSG - 1);
    if (strlen(msg) >= MAX_GERROR_MSG - 1)
    {
        gerror_msg[MAX_GERROR_MSG - 2] = '.';
        gerror_msg[MAX_GERROR_MSG - 3] = '.';
        gerror_msg[MAX_GERROR_MSG - 4] = '.';
    }
}

// Returns, as a list of strings, the directories that GIRepository
// searches for *.typelib files.
static SCM
scm_get_typelib_search_path(void)
{
    GSList *slist = g_irepository_get_search_path();
    SCM entry;
    SCM output = SCM_EOL;

    if (slist == NULL)
        return SCM_EOL;
    do
    {
        entry = scm_from_utf8_string(slist->data);
        output = scm_append(scm_list_2(output, scm_list_1(entry)));
    } while ((slist = g_slist_next(slist)));
    return output;
}

// Adds a directory to the search path that GIRepository will use to
// search for *.typelib files
static SCM
scm_prepend_typelib_search_path(SCM s_dir)
{
    char *dir;
    SCM_ASSERT_TYPE(scm_is_string(s_dir), s_dir, SCM_ARG1, "prepend-typelib-search-path", "string");
    dir = scm_to_utf8_string(s_dir);
    g_irepository_prepend_search_path(dir);
    return SCM_UNSPECIFIED;
}

static gchar *
type_public_name(GIBaseInfo *info)
{
    char *public_name;
    public_name = g_strdup_printf("%s:gtype", g_base_info_get_name(info));
    return public_name;
}

static gchar *
type_class_public_name(GIBaseInfo *info)
{
    char *public_name;

    public_name = g_strdup_printf("<%s>", g_base_info_get_name(info));

    return public_name;
}

static gchar *
type_class_predicate_name(GIBaseInfo *info)
{
    char *public_name;

    public_name = g_strdup_printf("%s?", g_base_info_get_name(info));

    return public_name;
}

typedef struct _GirPredicate
{
    ffi_closure *closure;
    ffi_cif cif;
    void *function_ptr;
    SCM fo_type;
} GirPredicate;

// This is the core of a dynamically generated type predicate.
// It converts an FFI argument to a SCM foreign object.
// And checks if that foreign object has the type this
// predicate is testing for.
static void gir_predicate_binding(ffi_cif *cif, void *ret, void **ffi_args,
    void *user_data)
{
    GirPredicate *gp = user_data;

    g_assert (cif != NULL);
    g_assert (ret != NULL);
    g_assert (ffi_args != NULL);
    g_assert (user_data != NULL);

    unsigned int n_args = cif->nargs;

    g_assert (n_args == 1);

    SCM arg = SCM_PACK(*(scm_t_bits *)(ffi_args[0]));

    if (SCM_IS_A_P(arg, gp->fo_type))
        *(ffi_arg *)ret = SCM_UNPACK(SCM_BOOL_T);
    else
        *(ffi_arg *)ret = SCM_UNPACK(SCM_BOOL_F);
}

static void *
gir_type_create_predicate(const char *name, SCM fo_type)
{
    GirPredicate *gp = g_new0(GirPredicate, 1);

    ffi_type **ffi_args = NULL;
    ffi_type *ffi_ret_type;

    // STEP 1
    // Allocate the block of memory that FFI uses to hold a closure object,
    // and set a pointer to the corresponding executable address.
    gp->fo_type = fo_type;
    gp->closure = ffi_closure_alloc(sizeof(ffi_closure),
        &(gp->function_ptr));

    g_return_val_if_fail(gp->closure != NULL, NULL);
    g_return_val_if_fail(gp->function_ptr != NULL, NULL);

    // STEP 2
    // Next, we begin to construct an FFI_CIF to describe the function call.

    // Initialize the argument info vectors.
    ffi_args = g_new0(ffi_type *, 1);

    // Our argument will be SCM, so we use pointer storage.
    ffi_args[0] = &ffi_type_pointer;
    // The return type is also SCM, for which we use a pointer.
    ffi_ret_type = &ffi_type_pointer;

    // Initialize the CIF Call Interface Struct.
    ffi_status prep_ok;
    prep_ok = ffi_prep_cif(&(gp->cif),
        FFI_DEFAULT_ABI,
        1,
        ffi_ret_type,
        ffi_args);

    if (prep_ok != FFI_OK)
        scm_misc_error("gir-type-create-predicate",
            "closure call interface preparation error #~A",
            scm_list_1(scm_from_int(prep_ok)));

    // STEP 3
    // Initialize the closure
    ffi_status closure_ok;
    closure_ok = ffi_prep_closure_loc(gp->closure,
        &(gp->cif),
        gir_predicate_binding,
        gp,                 // The 'user-data' passed to the function
        gp->function_ptr);

    if (closure_ok != FFI_OK)
        scm_misc_error("gir-type-create-predicate",
            "closure location preparation error #~A",
            scm_list_1(scm_from_int(closure_ok)));

    g_debug ("Created predicate %s", name);
    return gp->function_ptr;
}


static void
gir_typelib_define_type(GType gtype, GIBaseInfo *info)
{
    g_base_info_ref(info);
    g_type_set_qdata(gtype, gtype_base_info_key, info);
    SCM s_gtype = gi_gtype_c2g(gtype);

    // Make a variable to hold the type.
    gchar *type_name = type_public_name(info);
    scm_permanent_object(scm_c_define(type_name, s_gtype));
    g_debug("created new GType instance %s", type_name);

    // Make a foreign object type for instances of this GType.
    // All of our custom introspected foreign object types will
    // have the same 3 slots: sptr, valid, and extra
    gchar *type_class_name = type_class_public_name(info);
    SCM sname = scm_from_utf8_symbol(type_class_name);
    SCM slots = scm_list_3(scm_from_utf8_symbol("sptr"), scm_from_utf8_symbol("valid"), scm_from_utf8_symbol ("extra"));
    SCM fo_type = scm_make_foreign_object_type(sname, slots, NULL);
    g_debug("Creating a new GType foreign object type: %p %s", SCM_UNPACK_POINTER(fo_type), type_class_name);
    scm_gtype_set_scheme_type_x (s_gtype, fo_type);
    scm_permanent_object(scm_c_define(type_class_name, scm_gtype_get_scheme_type(s_gtype)));

    // Make a predicate for this type.
    all_fo_types = g_slist_append(all_fo_types, SCM_UNPACK_POINTER(fo_type));
    gchar *predicate_name = type_class_predicate_name(info);
    gpointer func = gir_type_create_predicate(predicate_name, fo_type);
    if (func)
        scm_c_define_gsubr(predicate_name, 1, 0, 0, func);

    scm_c_export(type_name, type_class_name, predicate_name, NULL);
    g_free(predicate_name);
    g_free(type_class_name);
    g_free(type_name);
}

static SCM
scm_load_typelib(SCM s_namespace, SCM s_version)
{
    gchar *namespace_;
    gchar *version;
    GITypelib *tl;
    GError *error = NULL;

    SCM_ASSERT_TYPE(scm_is_string(s_namespace), s_namespace, SCM_ARG1, "load-typelib", "string");
    SCM_ASSERT_TYPE(scm_is_string(s_version), s_version, SCM_ARG2, "load-typelib", "string");

    namespace_ = scm_to_utf8_string(s_namespace);
    version = scm_to_utf8_string(s_version);

    tl = g_irepository_require(NULL, namespace_, version, 0, &error);
    if (tl == NULL)
    {
        free(version);
        free(namespace_);
        store_gerror_message(error->message);
        g_error_free(error);
        scm_misc_error("load-typelib", gerror_msg, SCM_EOL);
        return SCM_UNSPECIFIED;
    }

    g_debug("Loading irepository %s %s", namespace_, version);
    int n = g_irepository_get_n_infos(NULL, namespace_);
    for (int i = 0; i < n; i++)
    {
        GIBaseInfo *info;
        GIInfoType type;
        info = g_irepository_get_info(NULL, namespace_, i);
        if (g_base_info_is_deprecated(info))
        {
            g_base_info_unref(info);
            continue;
        }
        type = g_base_info_get_type(info);
        switch (type)
        {
        case GI_INFO_TYPE_CALLBACK:
            hash_table_insert("%gi-callbacks", namespace_, NULL, info);
            break;
        case GI_INFO_TYPE_FUNCTION:
            gir_function_define_gsubr(namespace_, NULL, info);
            break;
        case GI_INFO_TYPE_STRUCT:
        {
            GType gtype = g_registered_type_info_get_g_type(info);
            if (gtype == G_TYPE_NONE)
            {
                g_debug("Not loading struct type '%s' because is has no GType",
                    g_base_info_get_name(info));
                g_base_info_unref(info);
                break;
            }
            gir_typelib_define_type(gtype, info);

            gint n_methods = g_struct_info_get_n_methods(info);
            for (gint m = 0; m < n_methods; m++)
            {
                GIFunctionInfo *func_info = g_struct_info_get_method(info, m);
                if (g_function_info_get_flags(func_info) & GI_FUNCTION_IS_METHOD)
                    gir_method_table_insert(gtype, func_info);
                else
                    gir_function_define_gsubr(namespace_, g_base_info_get_name(info), func_info);
            }
        }
        break;
        case GI_INFO_TYPE_ENUM:
        case GI_INFO_TYPE_FLAGS:
            gir_flag_define(info);
            break;
        case GI_INFO_TYPE_OBJECT:
        {
            GType gtype = g_registered_type_info_get_g_type(info);
            if (gtype == G_TYPE_NONE)
            {
                g_debug("Not loading object type '%s' because is has no GType",
                    g_base_info_get_name(info));
                g_base_info_unref(info);
                break;
            }
            gir_typelib_define_type(gtype, info);

            gint n_methods = g_object_info_get_n_methods(info);
            for (gint m = 0; m < n_methods; m++)
            {
                GIFunctionInfo *func_info = g_object_info_get_method(info, m);
                if (g_function_info_get_flags(func_info) & GI_FUNCTION_IS_METHOD)
                    gir_method_table_insert(gtype, func_info);
                else
                    gir_function_define_gsubr(namespace_, g_base_info_get_name(info), func_info);
            }
#if 0
            gint n_signals = g_object_info_get_n_signals(info);
            for (gint m = 0; m < n_signals; m++) {
                GISignalInfo *sig_info = g_object_info_get_signal(info, m);
                if (!(g_signal_info_get_flags(sig_info) & G_SIGNAL_DEPRECATED)) {
                    if (!insert_into_signal_table(gtype,
                                                  sig_info,
                                                  &is_new_method))
                        g_base_info_unref(sig_info);
                    else
                        export_signal_info(&export,
                                           g_base_info_get_name(info),
                                           sig_info,
                                           is_new_method);
                }
            }
#endif
        }
        break;
        case GI_INFO_TYPE_INTERFACE:
            hash_table_insert("%gi-interfaces", namespace_, NULL, info);
            break;
        case GI_INFO_TYPE_CONSTANT:
            gir_constant_define(info);
            break;
        case GI_INFO_TYPE_UNION:
        {
            GType gtype = g_registered_type_info_get_g_type(info);
            if (gtype == G_TYPE_NONE)
            {
                g_debug("Not loading union type '%s' because is has no GType",
                    g_base_info_get_name(info));
                g_base_info_unref(info);
                break;
            }
            gir_typelib_define_type(gtype, info);

            gint n_methods = g_union_info_get_n_methods(info);
            for (gint m = 0; m < n_methods; m++)
            {
                GIFunctionInfo *func_info = g_object_info_get_method(info, m);
                if (g_function_info_get_flags(func_info) & GI_FUNCTION_IS_METHOD)
                    gir_method_table_insert(gtype, func_info);
                else
                    gir_function_define_gsubr(namespace_, g_base_info_get_name(info), func_info);
            }
        }
        break;
        case GI_INFO_TYPE_VALUE:
            g_critical("Unsupported irepository type 'VALUE'");
            break;
        case GI_INFO_TYPE_SIGNAL:
            g_critical("Unsupported irepository type 'SIGNAL'");
            break;
        case GI_INFO_TYPE_VFUNC:
            g_critical("Unsupported irepository type 'VFUNC'");
            break;
        case GI_INFO_TYPE_PROPERTY:
            g_critical("Unsupported irepository type 'PROPERTY'");
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
            g_critical("Unsupported irepository type %d", type);
            break;
        }
    }
    free(version);
    free(namespace_);

    return SCM_UNSPECIFIED;
}

// This returns a Guile hash table with the variable name NAME,
// creating it if necessary.
static SCM
get_hash_table(const char *name)
{
    SCM hashtable;

    g_assert(name != NULL);

    hashtable = scm_module_variable(scm_current_module(),
                                    scm_from_utf8_symbol(name));

    if (scm_is_false(hashtable))
    {
        g_debug("Creating hash table %s", name);
        scm_permanent_object(scm_c_define(name, scm_c_make_hash_table(GIR_FUNC2_INIT_HASH_TABLE_SIZE)));
        hashtable = scm_module_variable(scm_current_module(),
                                        scm_from_utf8_symbol(name));
    }

    g_assert(scm_is_true(scm_hash_table_p(scm_variable_ref(hashtable))));

    return scm_variable_ref(hashtable);
}

static void
hash_table_insert(const char *table_name, const char *namespace_,
    const char *parent,
    GIBaseInfo *info)
{
    SCM h;
    gchar *full_name;
    SCM s_info;

    g_assert(table_name != NULL);
    g_assert(info != NULL);

    h = get_hash_table(table_name);

    if (parent)
        full_name = g_strdup_printf("%s-%s-%s",
                                    namespace_,
                                    parent,
                                    g_base_info_get_name(info));
    else
        full_name = g_strdup_printf("%s-%s",
                                    namespace_,
                                    g_base_info_get_name(info));

    s_info = scm_from_pointer(info,
                              (scm_t_pointer_finalizer)g_base_info_unref);
    g_debug("%s[%s] = %p", table_name, full_name, (char *)info);
    scm_hash_set_x(h, scm_from_utf8_string(full_name), s_info);
    free(full_name);
}

static SCM
scm_document_typelib(SCM s_namespace, SCM s_version)
{
    gchar *namespace_;
    gchar *version;
    GITypelib *tl;
    GError *error = NULL;
    GString *export;

    SCM_ASSERT(scm_is_string(s_namespace), s_namespace, SCM_ARG1, "document-typelib");
    SCM_ASSERT(scm_is_string(s_version), s_version, SCM_ARG2, "document-typelib");
    namespace_ = scm_to_utf8_string(s_namespace);
    version = scm_to_utf8_string(s_version);

    tl = g_irepository_require(NULL, namespace_, version, 0, &error);
    if (tl == NULL)
    {
        free(version);
        free(namespace_);
        store_gerror_message(error->message);
        g_error_free(error);
        scm_misc_error("document-typelib", gerror_msg, SCM_EOL);
        return SCM_UNSPECIFIED;
    }

    export = g_string_new_len(NULL, 128 * 1024);
    g_string_append_printf(export, "%s %s\n\n", namespace_, version);

    int n = g_irepository_get_n_infos(NULL, namespace_);
    for (int i = 0; i < n; i++)
    {
        GIBaseInfo *info;
        GIInfoType type;
        info = g_irepository_get_info(NULL, namespace_, i);
        if (g_base_info_is_deprecated(info))
        {
	    g_string_append_printf(export,
				   "Not importing '%s' because it is deprecated.\n\n",
				   g_base_info_get_name(info));
            g_base_info_unref(info);
            continue;
        }
        type = g_base_info_get_type(info);
        switch (type)
        {
        case GI_INFO_TYPE_CALLBACK:
            document_callback_info(&export, namespace_, NULL, info);
            break;
        case GI_INFO_TYPE_FUNCTION:
	  document_callable_info(&export, namespace_, NULL, info, 0);
            break;
        case GI_INFO_TYPE_STRUCT:
        {
            GType gtype = g_registered_type_info_get_g_type(info);
            if (gtype == G_TYPE_NONE)
            {
                g_debug("Not importing struct type '%s' because is has no GType",
                    g_base_info_get_name(info));
                g_base_info_unref(info);
                break;
            }
            g_base_info_ref(info);
            export_type_info(&export, namespace_, NULL, info, GI_INFO_TYPE_STRUCT);
            gint n_methods = g_struct_info_get_n_methods(info);
            for (gint m = 0; m < n_methods; m++)
            {
                GIFunctionInfo *func_info = g_struct_info_get_method(info, m);
                if (g_function_info_get_flags(func_info)
                    & GI_FUNCTION_IS_METHOD)
                    document_callable_info(&export,
                                       namespace_,
                                       g_base_info_get_name(info),
					   func_info, 1);
                else
                    document_callable_info(&export,
                                         namespace_,
                                         g_base_info_get_name(info),
					   func_info, 0);
            }
        }
        break;
        case GI_INFO_TYPE_ENUM:
        case GI_INFO_TYPE_FLAGS:
            gir_flag_document(&export, info);
            break;
        case GI_INFO_TYPE_OBJECT:
        {
            GType gtype = g_registered_type_info_get_g_type(info);
            if (gtype == G_TYPE_NONE)
            {
                g_debug("Not importing object type '%s' because is has no GType",
                        g_base_info_get_name(info));
                g_base_info_unref(info);
                break;
            }
            export_type_info(&export, namespace_, NULL, info, GI_INFO_TYPE_OBJECT);
            gint n_methods = g_object_info_get_n_methods(info);
            for (gint m = 0; m < n_methods; m++)
            {
                GIFunctionInfo *func_info = g_object_info_get_method(info, m);
                if (g_function_info_get_flags(func_info) & GI_FUNCTION_IS_METHOD)
                    document_callable_info(&export,
                                       namespace_,
                                       g_base_info_get_name(info),
					   func_info, 1);
                else
                    document_callable_info(&export,
                                         namespace_,
                                         g_base_info_get_name(info),
					   func_info, 0);
            }
#if 0
            gint n_signals = g_object_info_get_n_signals(info);
            for (gint m = 0; m < n_signals; m++) {
                GISignalInfo *sig_info = g_object_info_get_signal(info, m);
                if (!(g_signal_info_get_flags(sig_info) & G_SIGNAL_DEPRECATED)) {
                    if (!insert_into_signal_table(gtype,
                                                  sig_info,
                                                  &is_new_method))
                        g_base_info_unref(sig_info);
                    else
                        export_signal_info(&export,
                                           g_base_info_get_name(info),
                                           sig_info,
                                           is_new_method);
                }
            }
#endif
        }
        break;
        case GI_INFO_TYPE_INTERFACE:
            // export_interface_info(&export, g_base_info_get_name(info), info);
            break;
        case GI_INFO_TYPE_CONSTANT:
            gir_constant_document(&export, namespace_, NULL, info);
            break;
        case GI_INFO_TYPE_UNION:
        {
            GType gtype = g_registered_type_info_get_g_type(info);
            if (gtype == G_TYPE_NONE)
            {
                g_debug("Not importing union type '%s' because is has no GType",
                        g_base_info_get_name(info));
                g_base_info_unref(info);
                break;
            }
            export_type_info(&export, namespace_, NULL, info, GI_INFO_TYPE_UNION);
            gint n_methods = g_union_info_get_n_methods(info);
            for (gint m = 0; m < n_methods; m++)
            {
                GIFunctionInfo *func_info = g_object_info_get_method(info, m);
                if (g_function_info_get_flags(func_info) & GI_FUNCTION_IS_METHOD)
		  document_callable_info(&export, namespace_, g_base_info_get_name(info), func_info, 1);
                else
		  document_callable_info(&export, namespace_, g_base_info_get_name(info), func_info, 0);
            }
        }
        break;
        case GI_INFO_TYPE_VALUE:
            g_critical("Unsupported irepository type 'VALUE'");
            break;
        case GI_INFO_TYPE_SIGNAL:
            g_critical("Unsupported irepository type 'SIGNAL'");
            break;
        case GI_INFO_TYPE_VFUNC:
            g_critical("Unsupported irepository type 'VFUNC'");
            break;
        case GI_INFO_TYPE_PROPERTY:
            g_critical("Unsupported irepository type 'PROPERTY'");
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
            g_critical("Unsupported irepository type %d", type);
            break;
        }
    }
    free(version);
    free(namespace_);

    return scm_take_locale_string(g_string_free(export, FALSE));
}


static const char *
abbrev_namespace(const char *namespace_)
{
    const char *g = "G";

    if (strcasecmp(namespace_, "glib") == 0)
        return g;
    return namespace_;
}


// #define FIGURE_OUT_ALL_ARG_TYPES
#ifdef FIGURE_OUT_ALL_ARG_TYPES
struct _arg_info_func_name
{
    GIArgInfo *ai;
    char *name;
};
static GPtrArray *gi_arg_infos = NULL;
#endif

#if 0
static guint
hash_key_func(gconstpointer v)
{
    guint hash = 5381;
    int i = 0;
    unsigned char *p = v;
    while (*p) {
        hash = hash * 33 + *p;
        p++;
        if (i++ > 100)
            break;
    }
    g_debug("string hash of %s is %u", (char *)v, hash);
    return hash;
}

static gboolean
hash_equal_func(gconstpointer a, gconstpointer b)
{
    return a == b;
}
#endif

/* Convert the type of names that GTK uses into Guile-like names */
static char *
gname_to_scm_name(const char *gname)
{
    size_t len = strlen(gname);
    GString *str = g_string_new(NULL);
    gboolean was_lower = FALSE;

    for (size_t i = 0; i < len; i++)
    {
        if (g_ascii_islower(gname[i]))
        {
            g_string_append_c(str, gname[i]);
            was_lower = TRUE;
        }
        else if (gname[i] == '_' || gname[i] == '-')
        {
            g_string_append_c(str, '-');
            was_lower = FALSE;
        }
        else if (g_ascii_isdigit(gname[i]))
        {
            g_string_append_c(str, gname[i]);
            was_lower = FALSE;
        }
        else if (g_ascii_isupper(gname[i]))
        {
            if (was_lower)
                g_string_append_c(str, '-');
            g_string_append_c(str, g_ascii_tolower(gname[i]));
            was_lower = FALSE;
        }
    }
    return g_string_free(str, FALSE);
}




static void
export_type_info(GString **export, const char *namespace_, const char *parent, GIRegisteredTypeInfo *info, GIInfoType type)
{
    g_assert(parent == NULL);

    char *public_name = type_public_name (info);
    char *class_name = type_class_public_name (info);
    g_string_append_printf(*export,
        "(define %s\n  (gi-lookup-type \"%s-%s\"))\n\n",
        public_name,
        namespace_,
        g_base_info_get_name(info));
    g_free (public_name);
    g_free (class_name);
}


static void
document_callable_arguments(GString **export, GICallableInfo *info, gboolean style)
{
    gint n_args;
    GIArgInfo *arg;
    GIDirection dir;
    GITypeInfo *type_info;
    char *name;

    n_args = g_callable_info_get_n_args(info);

    if (style)
        g_string_append(*export, ";; ARGS: \n");
    else
        g_string_append(*export, "   ARGS: \n");

    for (int i = 0; i < n_args; i++)
    {
        arg = g_callable_info_get_arg(info, i);
        dir = g_arg_info_get_direction(arg);
        type_info = g_arg_info_get_type(arg);
        if (!(dir == GI_DIRECTION_OUT)
	    || (dir == GI_DIRECTION_OUT && g_arg_info_is_caller_allocates(arg)))
        {
            if (style)
                g_string_append(*export, ";;   ");
            else
                g_string_append(*export, "     ");

            name = gname_to_scm_name(g_base_info_get_name(arg));
            g_string_append(*export, name);
            g_string_append_c(*export, ' ');
            char *desc = gi_giargument_describe_arg_in(arg);
            g_string_append_printf(*export, " - %s", desc);
            g_free(desc);
            if (dir == GI_DIRECTION_INOUT)
                g_string_append(*export, "[INOUT] ");
            else if (dir == GI_DIRECTION_OUT)
                g_string_append(*export, "[OUT]");
            free(name);
            if (i + 1 < n_args)
                g_string_append(*export, ", ");

            g_string_append_c(*export, '\n');
        }
        g_base_info_unref(type_info);
        g_base_info_unref(arg);
    }

    type_info = g_callable_info_get_return_type(info);
    if (style)
        g_string_append_printf(*export, ";; RETURN: %s%s\n",
            g_type_tag_to_string(g_type_info_get_tag(type_info)),
            g_type_info_is_pointer(type_info) ? "*" : "");
    else
        g_string_append_printf(*export, "   RETURN: %s%s\n",
            g_type_tag_to_string(g_type_info_get_tag(type_info)),
            g_type_info_is_pointer(type_info) ? "*" : "");
    g_base_info_unref(type_info);

    for (int i = 0; i < n_args; i++)
    {
        arg = g_callable_info_get_arg(info, i);
        dir = g_arg_info_get_direction(arg);
        type_info = g_arg_info_get_type(arg);
        if (dir == GI_DIRECTION_OUT && !g_arg_info_is_caller_allocates(arg))
        {
            if (style)
                g_string_append(*export, ";;   ");
            else
                g_string_append(*export, "     ");

            name = gname_to_scm_name(g_base_info_get_name(arg));
            g_string_append(*export, name);
            g_string_append_c(*export, ' ');
            char *desc = gi_giargument_describe_arg_in(arg);
            g_string_append_printf(*export, " - %s", desc);
            g_free(desc);
            free(name);
            if (i + 1 < n_args)
                g_string_append(*export, ", ");

            g_string_append_c(*export, '\n');
        }
        g_base_info_unref(type_info);
        g_base_info_unref(arg);
    }

    if (!style)
        g_string_append(*export, "\n");

}

static gchar *
callback_public_name(const char *namespace_, const char *parent, GICallableInfo *info)
{
    char *public_name;
    char *tmp_str;

    if (parent)
    {
        tmp_str = g_strdup_printf("%s-%s", parent, g_base_info_get_name(info));
        public_name = gname_to_scm_name(tmp_str);
        g_free(tmp_str);
    }
    else
    {
        tmp_str = g_strdup_printf("%s", g_base_info_get_name(info));
        public_name = gname_to_scm_name(tmp_str);
        g_free(tmp_str);
    }
    return public_name;
}

static void
document_callback_info(GString **export, const char *namespace_, const char *parent, GICallableInfo *info)
{
    char *lookup_name;
    char *public_name;

    if (parent)
        lookup_name = g_strdup_printf("%s-%s-%s", namespace_, parent, g_base_info_get_name(info));
    else
        lookup_name = g_strdup_printf("%s-%s", namespace_, g_base_info_get_name(info));

    public_name = callback_public_name(namespace_, parent, info);

    g_string_append_printf(*export, "CALLBACK %s\n", public_name);

    GITypeInfo *return_type = g_callable_info_get_return_type(info);
    g_assert(return_type);
    g_base_info_unref(return_type);

    document_callable_arguments(export, info, FALSE);
    g_free(lookup_name);
    g_free(public_name);
    g_string_append_printf(*export, "\n");
}

static gchar*
callable_public_name(const char *namespace_, const char *parent, GICallableInfo *info)
{
    char *public_name, *tmp_str, *tmp_str2;
    GITypeInfo *return_type;

    // For the callable names, we want a lowercase string of the form
    // 'func-name-with-hyphens'
    return_type = g_callable_info_get_return_type(info);
    g_assert(return_type);

    if (parent)
    {
#ifdef LONG_PUBLIC_NAMES
        tmp_str = g_strdup_printf("%s%s",
                                  abbrev_namespace(namespace_),
                                  parent);
#else
        tmp_str = g_strdup(parent);
#endif
        tmp_str2 = gname_to_scm_name(g_base_info_get_name(info));
        if (g_type_info_get_tag(return_type) == GI_TYPE_TAG_BOOLEAN
            && !g_type_info_is_pointer(return_type))
            public_name = g_strdup_printf("%s-%s?", tmp_str, tmp_str2);
        else
            public_name = g_strdup_printf("%s-%s", tmp_str, tmp_str2);
        g_free(tmp_str);
        g_free(tmp_str2);
    }
    else
    {
        if (g_type_info_get_tag(return_type) == GI_TYPE_TAG_BOOLEAN
            && !g_type_info_is_pointer(return_type))
#ifdef LONG_PUBLIC_NAMES
            tmp_str = g_strdup_printf("%s-%s?", abbrev_namespace(namespace_),
                                      g_base_info_get_name(info));
#else
            tmp_str = g_strdup_printf("%s?", g_base_info_get_name(info));
#endif
            else
#ifdef LONG_PUBLIC_NAMES
            tmp_str = g_strdup_printf("%s-%s", abbrev_namespace(namespace_),
                                      g_base_info_get_name(info));
#else
            tmp_str = g_strdup_printf("%s",
                                      g_base_info_get_name(info));
#endif
        public_name = gname_to_scm_name(tmp_str);
        g_free(tmp_str);
    }

    g_base_info_unref(return_type);
    return public_name;
}

static void
document_callable_info(GString **export, const char *namespace_, const char *parent, GICallableInfo *info,
		       gboolean method)
{
    gint n_args;
    GIArgInfo *arg;
    char *public_name;
    GITypeInfo *return_type;

    n_args = g_callable_info_get_n_args(info);
    return_type = g_callable_info_get_return_type(info);
    g_assert(return_type);

    if (method)
      public_name = gir_method_public_name(info);
    else
      public_name = gir_function_make_name(parent, info);
    if (method)
      g_string_append_printf(*export, "%s's METHOD %s", parent, public_name);
    else
      g_string_append_printf(*export, "PROCEDURE %s", public_name);

    for (int i = 0; i < n_args; i++)
    {
        arg = g_callable_info_get_arg(info, i);
        GIDirection dir = g_arg_info_get_direction(arg);
        if (dir == GI_DIRECTION_IN || dir == GI_DIRECTION_INOUT
            || (dir == GI_DIRECTION_OUT && g_arg_info_is_caller_allocates(arg)))
        {
            char *arg_name;
            g_string_append_c(*export, ' ');
            arg_name = gname_to_scm_name(g_base_info_get_name(arg));
            if (dir == GI_DIRECTION_OUT)
                g_string_append(*export, "out-");
            g_string_append(*export, arg_name);
            free(arg_name);
        }
#ifdef FIGURE_OUT_ALL_ARG_TYPES
        struct _arg_info_func_name *aifn = g_new(struct _arg_info_func_name, 1);
        aifn->ai = arg;
        aifn->name = g_strdup(c_function_name);
        g_ptr_array_add(gi_arg_infos, aifn);
#else
        g_base_info_unref(arg);
#endif
    }

    g_string_append_c(*export, '\n');

    document_callable_arguments(export, info, FALSE);

    g_string_append(*export, "\n\n");

    g_base_info_unref(return_type);
    g_free(public_name);
}

/* FIXME: this is a very sigmal way to export signal info */
static void
export_signal_info(GString **export, char *parent, GISignalInfo *info)
{
#if 0    
    gint n_args;
    GIArgInfo *arg;

    n_args = g_callable_info_get_n_args(info);

    char *c_function_name;
    if (parent)
        c_function_name = g_strdup_printf("%s-%s-signal", parent, g_base_info_get_name(info));
    else
        c_function_name = g_strdup_printf("%s-signal", g_base_info_get_name(info));

    char *name = gname_to_scm_constant_name(c_function_name);

    g_string_append_printf(*export, "(define (%s", name);
    free(name);

    GITypeInfo *return_type = g_callable_info_get_return_type(info);
    g_assert(return_type);
    if (g_type_info_get_tag(return_type) == GI_TYPE_TAG_BOOLEAN && !g_type_info_is_pointer(return_type))
        g_string_append_c(*export, '?');
    g_base_info_unref(return_type);

    g_string_append_printf(*export, " self");

    for (int i = 0; i < n_args; i++)
    {
        arg = g_callable_info_get_arg(info, i);
        GIDirection dir = g_arg_info_get_direction(arg);
        if (dir == GI_DIRECTION_IN || dir == GI_DIRECTION_INOUT)
        {
            g_string_append_c(*export, ' ');
            name = gname_to_scm_name(g_base_info_get_name(arg));
            g_string_append(*export, name);
            free(name);
        }
        g_base_info_unref(arg);
    }

    g_string_append_c(*export, ')');
    g_string_append_c(*export, '\n');

    g_string_append_printf(*export, "  (gi-signal-send self \n");
    g_string_append_printf(*export, "     (gi-signal-prepare \"%s\"", g_base_info_get_name(info));

    for (int i = 0; i < n_args; i++)
    {
        arg = g_callable_info_get_arg(info, i);
        GIDirection dir = g_arg_info_get_direction(arg);
        if (dir == GI_DIRECTION_IN || dir == GI_DIRECTION_INOUT)
        {
            g_string_append_c(*export, ' ');
            name = gname_to_scm_name(g_base_info_get_name(arg));
            g_string_append(*export, name);
            free(name);
        }
        g_base_info_unref(arg);
    }

    g_string_append(*export, ")))\n\n");
#endif    
}

static SCM
scm_gi_struct_ref(SCM s_ptr, SCM s_type_name, SCM s_field_name)
{
#if 0
    char *full_type_name = NULL;

    SCM_ASSERT(SCM_POINTER_P(s_ptr), s_ptr, SCM_ARG1,
        "gi-struct-ref");
    SCM_ASSERT(scm_is_string(s_namespace), s_namespace, SCM_ARG2,
        "gi-struct-ref");
    SCM_ASSERT(scm_is_string(s_type_name), s_type_name, SCM_ARG3,
        "gi-struct-ref");
    SCM_ASSERT(scm_is_string(s_field_name), s_field_name, SCM_ARG4,
        "gi-struct-ref");

#ifdef PREFIX_NAME_IN_HASH
    {
        char *namespace_ = scm_to_utf8_string(s_namespace);
        char *type_name = scm_to_utf8_string(s_type_name);
        full_type_name = g_strdup_printf("%s-%s", namespace_, type_name);
        free(type_name);
        free(namespace_);
    }
#else
    full_type_name = scm_to_utf8_string(s_type_name);
#endif

    GIStructInfo *si = g_hash_table_lookup(gi_structs, full_type_name);
    g_free(full_type_name);
    full_type_name = NULL;

    if (!si) {
        scm_misc_error("gi-struct-ref",
            "unknown struct type '~a' in ~a",
            scm_list_2(s_type_name, s_namespace));
        return SCM_BOOL_F;
    }

    gint n_fields = g_struct_info_get_n_fields(si);
    gint i = 0;
    GIFieldInfo *fi = NULL;
    char *field_name = scm_to_utf8_string(s_field_name);

    while (i < n_fields) {
        fi = g_struct_info_get_field(si, i);
        g_assert(fi != NULL);

        g_debug("field name search: %s == %s ?",
            field_name,
            g_base_info_get_name(fi));
        if (strcmp(g_base_info_get_name(fi), field_name) == 0) {
            break;
        }
        g_base_info_unref(fi);
        fi = NULL;
        i++;
    }

    free(field_name);
    if (i >= n_fields) {
        scm_misc_error("gi-struct-ref",
            "unknown field '~a' in struct '~a' in ~a",
            scm_list_3(s_field_name, s_type_name, s_namespace));
        return SCM_BOOL_F;
    }
    else {
        gboolean ok;
        GIArgument arg;
        void *ptr = scm_to_pointer(s_ptr);
        ok = g_field_info_get_field(fi, ptr, &arg);
        g_base_info_unref(fi);
        fi = NULL;

        if (!ok) {
            scm_misc_error("gi-struct-ref",
                "cannot unpack field '~a' in struct '~a'",
                scm_list_2(s_field_name, s_type_name));
            return SCM_BOOL_F;
        }
        else {
            GITypeInfo *ti = g_field_info_get_type(fi);
            SCM output = gi_giargument_to_object(&arg, ti, FALSE);
            g_base_info_unref(ti);
            return output;
        }
    }
#endif
    /* never get here */
    g_return_val_if_reached(SCM_BOOL_F);
}

static SCM
scm_gi_struct_set(SCM s_ptr, SCM s_namespace, SCM s_type_name, SCM s_field_name, SCM s_value)
{
#if 0
    char *full_type_name = NULL;

    // SCM_VALIDATE_POINTER (1, s_ptr);
    SCM_ASSERT(scm_is_string(s_namespace), s_namespace, SCM_ARG2,
        "gi-struct-set");
    SCM_ASSERT(scm_is_string(s_type_name), s_type_name, SCM_ARG3,
        "gi-struct-set");
    SCM_ASSERT(scm_is_string(s_field_name), s_field_name, SCM_ARG4,
        "gi-struct-set");

#ifdef PREFIX_NAME_IN_HASH
    {
        char *namespace_ = scm_to_utf8_string(s_namespace);
        char *type_name = scm_to_utf8_string(s_type_name);
        full_type_name = g_strdup_printf("%s-%s", namespace_, type_name);
        free(type_name);
        free(namespace_);
    }
#else
    full_type_name = scm_to_utf8_string(s_type_name);
#endif

    GIStructInfo *si = g_hash_table_lookup(gi_structs, full_type_name);
    g_free(full_type_name);
    full_type_name = NULL;

    if (!si) {
        scm_misc_error("gi-struct-ref",
            "unknown struct type '~a' in ~a",
            scm_list_2(s_type_name, s_namespace));
        return SCM_BOOL_F;
    }

    gint n_fields = g_struct_info_get_n_fields(si);
    gint i = 0;
    GIFieldInfo *fi = NULL;
    char *field_name = scm_to_utf8_string(s_field_name);

    while (i < n_fields) {
        fi = g_struct_info_get_field(si, i);
        g_assert(fi != NULL);

        g_debug("field name search: %s == %s ?",
            field_name,
            g_base_info_get_name(fi));
        if (strcmp(g_base_info_get_name(fi), field_name) == 0) {
            break;
        }
        g_base_info_unref(fi);
        fi = NULL;
        i++;
    }

    free(field_name);
    if (i >= n_fields) {
        scm_misc_error("gi-struct-set",
            "unknown field '~a' in struct '~a' in ~a",
            scm_list_3(s_field_name, s_type_name, s_namespace));
        return SCM_BOOL_F;
    }
    else {
        gboolean ok;
        GIArgument arg;
        GITypeInfo *ti = g_field_info_get_type(fi);
        arg = gi_argument_from_object("gi-struct-set",
            s_value,
            ti,
            GI_TRANSFER_NOTHING);
        g_base_info_unref(ti);
        ti = NULL;
        void *ptr = scm_to_pointer(s_ptr);
        ok = g_field_info_set_field(fi, ptr, &arg);
        g_base_info_unref(fi);
        fi = NULL;

        if (!ok) {
            scm_misc_error("gi-struct-set",
                "cannot set field '~a' in struct '~a' to '~a'",
                scm_list_3(s_field_name, s_type_name, s_value));
            return SCM_BOOL_F;
        }
        else {
            return SCM_BOOL_T;
        }
    }
#endif
    /* never get here */
    g_return_val_if_reached(SCM_BOOL_F);
}

static gboolean
function_info_typecheck_args(GIFunctionInfo *func_info, SCM s_args, char **errstr)
{
#if 0
    GIDirection dir;
    GIArgInfo *arg_info;
    gboolean type_ok = FALSE;

    int n_args = g_callable_info_get_n_args((GICallableInfo *)func_info);
    int i_input = 0;

    for (int i = 0; i < n_args; i++)
    {
        arg_info = g_callable_info_get_arg((GICallableInfo *)func_info, i);
        g_assert(arg_info != NULL);

        dir = g_arg_info_get_direction(arg_info);

        if (dir == GI_DIRECTION_IN || dir == GI_DIRECTION_INOUT)
        {
            SCM arg = scm_list_ref(s_args, scm_from_int(i_input));
            type_ok = gi_giargument_check_scm_type(arg, arg_info, errstr);
            i_input++;
        }
        g_base_info_unref(arg_info);
        if (!type_ok)
        {
            char *tmp = *errstr;
            *errstr = g_strdup_printf("In arg %d %s", i + 1, tmp);
            g_free(tmp);
        }
        break;
    }
    return type_ok;
#else
    return FALSE;
#endif
}

static void
function_info_release_args(GIFunctionInfo *func_info, GIArgument *args)
{
#if 0
    GIDirection dir;
    GIArgInfo *arg_info;
    GITypeInfo *type_info;

    int n_args = g_callable_info_get_n_args((GICallableInfo *)func_info);
    int i_input = 0;

    for (int i = 0; i < n_args; i++) {
        arg_info = g_callable_info_get_arg((GICallableInfo *)func_info, i);
        type_info = g_arg_info_get_type(arg_info);

        dir = g_arg_info_get_direction(arg_info);

        if (dir == GI_DIRECTION_IN || dir == GI_DIRECTION_INOUT) {
            gi_giargument_release(&(args[i]),
                type_info,
                g_arg_info_get_ownership_transfer(arg_info),
                dir);
            i_input++;
        }
        g_base_info_unref(arg_info);
        g_base_info_unref(type_info);
    }
#endif
}

static void
unload_repository(const char *category, GHashTable **p_hash_table)
{
    if (*p_hash_table)
    {
        g_debug("destroying %s hash table", category);
        g_hash_table_destroy(*p_hash_table);
        *p_hash_table = NULL;
    }
}

static SCM
scm_gi_unload_repositories(void)
{
    return SCM_UNSPECIFIED;
}

static SCM
scm_gi_lookup_callback_info(SCM s_type_name)
{
    SCM_ASSERT(scm_is_string(s_type_name), s_type_name, SCM_ARG1, "gi-lookup-callback-info");
    SCM h, val;

    h = get_hash_table("%gi-callbacks");
    val = scm_hash_ref(h, s_type_name, SCM_BOOL_F);
    if (scm_is_false(val))
        scm_misc_error("gi-lookup-callback-info",
            "Cannot find a callback type named '~a'",
            scm_list_1(s_type_name));
    return val;
}

#ifdef FIGURE_OUT_ALL_ARG_TYPES
static SCM
scm_dump_all_arg_types(void)
{
    guint len = gi_arg_infos->len;
    if (len == 0)
        return SCM_UNSPECIFIED;

    FILE *fp = fopen("arg_infos.txt", "wt");

    for (guint i = 0; i < len; i++)
    {
        struct _arg_info_func_name *aifn = gi_arg_infos->pdata[i];
        GIArgInfo *ai = aifn->ai;
        GIDirection dir = g_arg_info_get_direction(ai);
        GITypeInfo *ti = g_arg_info_get_type(ai);
        gboolean skip = g_arg_info_is_skip(ai);
        if (skip)
            continue;

        fprintf(fp, "%-11s", g_type_tag_to_string(g_type_info_get_tag(ti)));
        if (g_type_info_is_pointer(ti))
            fprintf(fp, "* ");
        else
            fprintf(fp, "  ");

        if (dir == GI_DIRECTION_IN)
            fprintf(fp, "IN    ");
        else if (dir == GI_DIRECTION_INOUT)
            fprintf(fp, "INOUT ");
        else if (dir == GI_DIRECTION_OUT)
            fprintf(fp, "OUT   ");

        if (g_type_info_get_tag(ti) == GI_TYPE_TAG_ARRAY)
        {
            fprintf(fp, "LEN %3d SIZE %3d ", g_type_info_get_array_length(ti), g_type_info_get_array_fixed_size(ti));
            if (g_type_info_is_zero_terminated(ti))
                fprintf(fp, "ZERO_TERM ");
            else
                fprintf(fp, "          ");
            GIArrayType arrt = g_type_info_get_array_type(ti);
            if (arrt == GI_ARRAY_TYPE_C)
                fprintf(fp, "C      ");
            else if (arrt == GI_ARRAY_TYPE_BYTE_ARRAY)
                fprintf(fp, "BYTE   ");
            else if (arrt == GI_ARRAY_TYPE_ARRAY)
                fprintf(fp, "GArray ");
            else if (arrt == GI_ARRAY_TYPE_PTR_ARRAY)
                fprintf(fp, "PTR    ");

            GITypeInfo *pti = g_type_info_get_param_type(ti, 0);
            fprintf(fp, "%-11s", g_type_tag_to_string(g_type_info_get_tag(pti)));
            if (g_type_info_is_pointer(pti))
                fprintf(fp, "* ");
            else
                fprintf(fp, "  ");
            GIBaseInfo *pbi = g_type_info_get_interface(pti);
            if (pbi)
            {
                GIInfoType pit = g_base_info_get_type(pbi);
                if (pit == GI_INFO_TYPE_INVALID)
                    fprintf(fp, "INVALID   ");
                else if (pit == GI_INFO_TYPE_FUNCTION)
                    fprintf(fp, "FUNCTION  ");
                else if (pit == GI_INFO_TYPE_CALLBACK)
                    fprintf(fp, "CALLBACK  ");
                else if (pit == GI_INFO_TYPE_STRUCT)
                    fprintf(fp, "STRUCT    ");
                else if (pit == GI_INFO_TYPE_STRUCT)
                    fprintf(fp, "BOXED     ");
                else if (pit == GI_INFO_TYPE_ENUM)
                    fprintf(fp, "ENUM      ");
                else if (pit == GI_INFO_TYPE_FLAGS)
                    fprintf(fp, "FLAGS     ");
                else if (pit == GI_INFO_TYPE_OBJECT)
                    fprintf(fp, "OBJECT    ");
                else if (pit == GI_INFO_TYPE_INTERFACE)
                    fprintf(fp, "INTERFACE ");
                else if (pit == GI_INFO_TYPE_CONSTANT)
                    fprintf(fp, "CONSTANT  ");
                else if (pit == GI_INFO_TYPE_UNION)
                    fprintf(fp, "UNION     ");
                else if (pit == GI_INFO_TYPE_VALUE)
                    fprintf(fp, "VALUE     ");
                else if (pit == GI_INFO_TYPE_SIGNAL)
                    fprintf(fp, "SIGNAL    ");
                else if (pit == GI_INFO_TYPE_VFUNC)
                    fprintf(fp, "VFUNC     ");
                else if (pit == GI_INFO_TYPE_PROPERTY)
                    fprintf(fp, "PROPERTY  ");
                else if (pit == GI_INFO_TYPE_FIELD)
                    fprintf(fp, "FIELD     ");
                else if (pit == GI_INFO_TYPE_ARG)
                    fprintf(fp, "ARG       ");
                else if (pit == GI_INFO_TYPE_TYPE)
                    fprintf(fp, "TYPE      ");
                fprintf(fp, "%-11s ", g_base_info_get_name(pbi));
            }
        }

        GIBaseInfo *bi = g_type_info_get_interface(ti);
        if (bi)
        {
            GIInfoType it = g_base_info_get_type(bi);
            if (it == GI_INFO_TYPE_INVALID)
                fprintf(fp, "INVALID   ");
            else if (it == GI_INFO_TYPE_FUNCTION)
                fprintf(fp, "FUNCTION  ");
            else if (it == GI_INFO_TYPE_CALLBACK)
                fprintf(fp, "CALLBACK  ");
            else if (it == GI_INFO_TYPE_STRUCT)
                fprintf(fp, "STRUCT    ");
            else if (it == GI_INFO_TYPE_STRUCT)
                fprintf(fp, "BOXED     ");
            else if (it == GI_INFO_TYPE_ENUM)
                fprintf(fp, "ENUM      ");
            else if (it == GI_INFO_TYPE_FLAGS)
                fprintf(fp, "FLAGS     ");
            else if (it == GI_INFO_TYPE_OBJECT)
                fprintf(fp, "OBJECT    ");
            else if (it == GI_INFO_TYPE_INTERFACE)
                fprintf(fp, "INTERFACE ");
            else if (it == GI_INFO_TYPE_CONSTANT)
                fprintf(fp, "CONSTANT  ");
            else if (it == GI_INFO_TYPE_UNION)
                fprintf(fp, "UNION     ");
            else if (it == GI_INFO_TYPE_VALUE)
                fprintf(fp, "VALUE     ");
            else if (it == GI_INFO_TYPE_SIGNAL)
                fprintf(fp, "SIGNAL    ");
            else if (it == GI_INFO_TYPE_VFUNC)
                fprintf(fp, "VFUNC     ");
            else if (it == GI_INFO_TYPE_PROPERTY)
                fprintf(fp, "PROPERTY  ");
            else if (it == GI_INFO_TYPE_FIELD)
                fprintf(fp, "FIELD     ");
            else if (it == GI_INFO_TYPE_ARG)
                fprintf(fp, "ARG       ");
            else if (it == GI_INFO_TYPE_TYPE)
                fprintf(fp, "TYPE      ");
            fprintf(fp, "%-11s ", g_base_info_get_name(bi));
        }

        gboolean null = g_arg_info_may_be_null(ai);
        if (null)
            fprintf(fp, "NULL_OK ");
        else
            fprintf(fp, "        ");
        gboolean caller_allocate = g_arg_info_is_caller_allocates(ai);
        if (caller_allocate)
            fprintf(fp, "ALLOC ");
        else
            fprintf(fp, "      ");
        gboolean optional = g_arg_info_is_optional(ai);
        if (optional)
            fprintf(fp, "OPT ");
        else
            fprintf(fp, "    ");
        GITransfer transfer = g_arg_info_get_ownership_transfer(ai);
        if (transfer == GI_TRANSFER_NOTHING)
            fprintf(fp, "CONST   ");
        else if (transfer == GI_TRANSFER_CONTAINER)
            fprintf(fp, "SHALLOW ");
        else if (transfer == GI_TRANSFER_EVERYTHING)
            fprintf(fp, "DEEP    ");
        fprintf(fp, "%s %s ", g_base_info_get_name(ai), aifn->name);
        fprintf(fp, "\n");
    }
    fclose(fp);
    return SCM_UNSPECIFIED;
}
#endif


void gir_init_func2(void)
{
#ifdef FIGURE_OUT_ALL_ARG_TYPES
    gi_arg_infos = g_ptr_array_new();
#endif
    scm_c_define_gsubr("get-typelib-search-path", 0, 0, 0, scm_get_typelib_search_path);
    scm_c_define_gsubr("prepend-typelib-search-path", 1, 0, 0, scm_prepend_typelib_search_path);
    scm_c_define_gsubr("load-typelib", 2, 0, 0, scm_load_typelib);
    scm_c_define_gsubr("document-typelib", 2, 0, 0, scm_document_typelib);
    scm_c_define_gsubr("gi-lookup-callback-info", 1, 0, 0, scm_gi_lookup_callback_info);

#if 0
    scm_permanent_object(scm_c_define("%gi-callbacks", scm_c_make_hash_table(GIR_FUNC2_INIT_HASH_TABLE_SIZE)));
    scm_permanent_object(scm_c_define("%gi-structs", scm_c_make_hash_table(GIR_FUNC2_INIT_HASH_TABLE_SIZE)));
    scm_permanent_object(scm_c_define("%gi-objects", scm_c_make_hash_table(GIR_FUNC2_INIT_HASH_TABLE_SIZE)));
    scm_permanent_object(scm_c_define("%gi-unions", scm_c_make_hash_table(GIR_FUNC2_INIT_HASH_TABLE_SIZE)));
    scm_permanent_object(scm_c_define("%gi-interfaces", scm_c_make_hash_table(GIR_FUNC2_INIT_HASH_TABLE_SIZE)));
#endif

    //scm_c_define_gsubr("gi-load-repository", 2, 0, 0,
    //                    scm_gi_load_repository);
    scm_c_define_gsubr("gi-unload-repositories", 0, 0, 0,
        scm_gi_unload_repositories);
    scm_c_define_gsubr("gi-struct-ref", 4, 0, 0, scm_gi_struct_ref);
    scm_c_define_gsubr("gi-struct-set", 5, 0, 0, scm_gi_struct_set);
#ifdef FIGURE_OUT_ALL_ARG_TYPES
    scm_c_define_gsubr("gi-dump-arg-types", 0, 0, 0, scm_dump_all_arg_types);
#endif

    scm_c_export("document-typelib", NULL);
}