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

#include <ffi.h>
#include "gi_callable_info.h"
#include "gig_argument.h"
#include "gig_util.h"
#include "gig_arg_map.h"
#include "gig_function.h"
#include "gig_function_private.h"
#include "gig_type.h"
#include "gig_signal.h"
#include "gig_value.h"

#if GIG_DEBUG_TRANSFERS
#define TRACE_S2(name,meta) g_debug("[S2] In '%s', on line %d while unpacking '%s' %s.", __func__, __LINE__, name, gig_type_meta_describe(meta))
#define TRACE_2S(name,meta) g_debug("[2S] In '%s', on line %d while packing '%s' %s.", __func__, __LINE__, name, gig_type_meta_describe(meta))
#else
#define TRACE_S2(name,meta)
#define TRACE_2S(name,meta)
#endif

#define GIG_ARRAY_SIZE_UNKNOWN (-1)
typedef struct _GigFunction
{
    GIFunctionInfo *function_info;
    ffi_closure *closure;
    ffi_cif cif;
    gpointer function_ptr;
    gchar *name;
    ffi_type **atypes;
    GigArgMap *amap;
} GigFunction;

GHashTable *function_cache;

static GigGsubr *check_gsubr_cache(GICallableInfo *function_info, SCM self_type,
                                   gint *required_input_count, gint *optional_input_count,
                                   SCM *formals, SCM *specializers);
static GigGsubr *create_gsubr(GIFunctionInfo *function_info, const gchar *name, SCM self_type,
                              gint *required_input_count, gint *optional_input_count,
                              SCM *formals, SCM *specializers);
static void make_formals(GICallableInfo *, GigArgMap *, gint n_inputs, SCM self_type,
                         SCM *formals, SCM *specializers);
static void function_binding(ffi_cif *cif, gpointer ret, gpointer *ffi_args, gpointer user_data);

static void convert_input_vals(GigArgMap *amap, const gchar *subr, SCM s_args, GValue *in_vals,
                               GSList ** free_list, GValue *out_vals);
static void convert_input_val1(GigArgMap *amap, gint i, const gchar *name, SCM s_arg,
                               GValue *c_in_vals, GSList ** free_list, GValue *c_out_vals);

static SCM convert_output_vals(GIFunctionInfo *func_info, GigArgMap *amap, const gchar *name,
                               GValue *c_out_vals);
static SCM convert_output_val1(GIFunctionInfo *func_info, GigArgMap *amap, const gchar *name,
                               GValue *c_out_vals);
static SCM convert_return_val(GIFunctionInfo *func_info, GigArgMap *amap, const gchar *func_name,
                              GArray *out_args);

static SCM rebox_inout_vals(GIFunctionInfo *func_info, GigArgMap *amap,
                            const gchar *func_name, GValue *in_args, GValue *out_args, SCM s_args);
static void function_free(GigFunction *fn);
static void gig_fini_function(void);
static SCM gig_function_define1(const gchar *public_name, SCM proc, int opt, SCM formals,
                                SCM specializers);

static SCM proc4function(GIFunctionInfo *info, const gchar *name, SCM self_type,
                         int *req, int *opt, SCM *formals, SCM *specs);
static SCM proc4signal(GISignalInfo *info, const gchar *name, SCM self_type,
                       int *req, int *opt, SCM *formals, SCM *specs);
static gboolean function_invoke(GICallableInfo *func_info, GigArgMap *amap, GValue *self_val,
                                GValue *in_vals, GValue *out_vals, GValue *return_val,
                                GError **error);
static SCM function_call(GIFunctionInfo *info, GigArgMap *amap, const gchar *name, GObject *object,
                         SCM args, GError **error);
static void
set_value_types(GigArgMap *amap, GValue *self_val, GValue *in_vals, GValue *out_vals,
                GValue *return_val);

#define LOOKUP_DEFINITION(module)                                       \
    do {                                                                \
        SCM variable = scm_module_variable(module, name);               \
        if (scm_is_true(variable)) return scm_variable_ref(variable);   \
    } while (0)

static SCM
current_module_definition(SCM name)
{
    LOOKUP_DEFINITION(scm_current_module());
    return SCM_BOOL_F;
}

SCM
default_definition(SCM name)
{
    LOOKUP_DEFINITION(scm_current_module());
    LOOKUP_DEFINITION(scm_c_resolve_module("gi"));
    LOOKUP_DEFINITION(scm_c_resolve_module("guile"));
    return SCM_BOOL_F;
}

#undef LOOKUP_DEFINITION

SCM
gig_function_define(GType type, GICallableInfo *info, const gchar *namespace, SCM defs)
{
    scm_dynwind_begin(0);
    SCM def;
    gboolean is_method = g_callable_info_is_method(info);

    gchar *function_name, *method_name;
    function_name = scm_dynwind_or_bust("%gig-function-define",
                                        gi_callable_info_make_name(info, namespace));

    gint required_input_count, optional_input_count;
    SCM formals, specializers, self_type = SCM_UNDEFINED;

    if (is_method) {
        self_type = gig_type_get_scheme_type(type);
        g_return_val_if_fail(scm_is_true(self_type), defs);
        method_name = scm_dynwind_or_bust("%gig-function-define",
                                          gi_callable_info_make_name(info, NULL));
    }

    SCM proc;
    if (GI_IS_FUNCTION_INFO(info))
        proc = proc4function((GIFunctionInfo *)info, function_name, self_type,
                             &required_input_count, &optional_input_count,
                             &formals, &specializers);
    else if (GI_IS_SIGNAL_INFO(info))
        proc = proc4signal((GISignalInfo *)info, function_name, self_type,
                           &required_input_count, &optional_input_count, &formals, &specializers);
    else
        g_assert_not_reached();

    def = gig_function_define1(function_name, proc, optional_input_count, formals, specializers);
    if (!SCM_UNBNDP(def))
        defs = scm_cons(def, defs);
    g_debug("dynamically bound %s to %s with %d required and %d optional arguments",
            function_name, g_base_info_get_name(info), required_input_count, optional_input_count);

    if (is_method) {
        def = gig_function_define1(method_name, proc, optional_input_count, formals, specializers);
        if (!SCM_UNBNDP(def))
            defs = scm_cons(def, defs);
        g_debug("dynamically bound %s to %s with %d required and %d optional arguments",
                function_name, g_base_info_get_name(info), required_input_count,
                optional_input_count);
    }

    scm_dynwind_end();
    return defs;
}

// Given some function introspection information from a typelib file,
// this procedure creates a SCM wrapper for that procedure in the
// current module.
static SCM
gig_function_define1(const gchar *public_name, SCM proc, int opt, SCM formals, SCM specializers)
{
    g_return_val_if_fail(public_name != NULL, SCM_UNDEFINED);

    SCM sym_public_name = scm_from_utf8_symbol(public_name);
    SCM generic = default_definition(sym_public_name);
    if (!scm_is_generic(generic))
        generic = scm_call_2(ensure_generic_proc, generic, sym_public_name);

    SCM t_formals = formals, t_specializers = specializers;

    do {
        SCM mthd = scm_call_7(make_proc,
                              method_type,
                              kwd_specializers, t_specializers,
                              kwd_formals, t_formals,
                              kwd_procedure, proc);

        scm_call_2(add_method_proc, generic, mthd);

        if (scm_is_eq(t_formals, SCM_EOL))
            break;

        t_formals = scm_drop_right_1(t_formals);
        t_specializers = scm_drop_right_1(t_specializers);
    } while (opt-- > 0);

    scm_define(sym_public_name, generic);
    return sym_public_name;
}

static SCM
proc4function(GIFunctionInfo *info, const gchar *name, SCM self_type,
              int *req, int *opt, SCM *formals, SCM *specializers)
{
    GigGsubr *func_gsubr = check_gsubr_cache(info, self_type, req, opt,
                                             formals, specializers);
    if (!func_gsubr)
        func_gsubr = create_gsubr(info, name, self_type, req, opt, formals, specializers);

    if (!func_gsubr) {
        g_debug("Could not create a gsubr for %s", name);
        return SCM_BOOL_F;
    }

    return scm_c_make_gsubr(name, 0, 0, 1, func_gsubr);
}

static SCM
proc4signal(GISignalInfo *info, const gchar *name, SCM self_type, int *req, int *opt, SCM *formals,
            SCM *specializers)
{
    GigArgMap *amap;

    amap = scm_dynwind_or_bust("%proc4signal", gig_amap_new(info));
    gig_amap_s_input_count(amap, req, opt);
    *req = *req + 1;

    make_formals(info, amap, *req + *opt, self_type, formals, specializers);

    GigSignalSlot slots[] = { GIG_SIGNAL_SLOT_NAME };
    SCM values[1];

    // use base_info name without transformations, otherwise we could screw things up
    values[0] = scm_from_utf8_string(g_base_info_get_name(info));

    SCM signal = gig_make_signal(1, slots, values);

    // check for collisions
    SCM current_definition = current_module_definition(scm_from_utf8_symbol(name));
    if (scm_is_true(current_definition))
        for (SCM iter = scm_generic_function_methods(current_definition);
             scm_is_pair(iter); iter = scm_cdr(iter))
            if (scm_is_equal(*specializers, scm_method_specializers(scm_car(iter)))) {
                // we'd be overriding an already defined generic method, let's not do that
                scm_slot_set_x(signal, scm_from_utf8_symbol("procedure"),
                               scm_method_procedure(scm_car(iter)));
                break;
            }

    return signal;
}

static GigGsubr *
check_gsubr_cache(GICallableInfo *function_info, SCM self_type, gint *s_in_req,
                  gint *s_in_opt, SCM *formals, SCM *specializers)
{
    // Check the cache to see if this function has already been created.
    GigFunction *gfn = g_hash_table_lookup(function_cache, function_info);

    if (gfn == NULL)
        return NULL;

    gig_amap_s_input_count(gfn->amap, s_in_req, s_in_opt);
    if (g_callable_info_is_method(gfn->function_info))
        (*s_in_req)++;

    make_formals(gfn->function_info,
                 gfn->amap, *s_in_req + *s_in_opt, self_type, formals, specializers);

    return gfn->function_ptr;
}

static void
make_formals(GICallableInfo *callable,
             GigArgMap *argmap, gint n_inputs, SCM self_type, SCM *formals, SCM *specializers)
{
    SCM i_formal, i_specializer;

    i_formal = *formals = scm_make_list(scm_from_int(n_inputs), SCM_BOOL_F);
    i_specializer = *specializers = scm_make_list(scm_from_int(n_inputs), top_type);

    if (g_callable_info_is_method(callable)) {
        scm_set_car_x(i_formal, sym_self);
        scm_set_car_x(i_specializer, self_type);

        i_formal = scm_cdr(i_formal);
        i_specializer = scm_cdr(i_specializer);
        n_inputs--;
    }

    for (gint s = 0; s < n_inputs;
         s++, i_formal = scm_cdr(i_formal), i_specializer = scm_cdr(i_specializer)) {
        gint i;
        gig_amap_input_s2i(argmap, s, &i);
        GigArgMapEntry *entry = &argmap->pdata[i];
        gchar *formal = scm_dynwind_or_bust("%make-formals",
                                            gig_gname_to_scm_name(entry->name));
        scm_set_car_x(i_formal, scm_from_utf8_symbol(formal));
        // don't force types on nullable input, as #f can also be used to represent
        // NULL.
        if (entry->meta.is_nullable)
            continue;

        if (G_TYPE_IS_OBJECT(entry->meta.gtype)) {
            SCM s_type = gig_type_get_scheme_type(entry->meta.gtype);
            if (scm_is_true(s_type))
                scm_set_car_x(i_specializer, s_type);
        }
    }
}

static GigGsubr *
create_gsubr(GIFunctionInfo *function_info, const gchar *name, SCM self_type,
             gint *s_in_req, gint *s_in_opt, SCM *formals, SCM *specializers)
{
    GigFunction *gfn;
    ffi_type *ffi_ret_type;
    GigArgMap *amap;

    amap = gig_amap_new(function_info);
    if (amap == NULL) {
        g_debug("Cannot create gsubr for %s: is has invalid types", name);
        return NULL;
    }

    gfn = g_new0(GigFunction, 1);
    gfn->function_info = function_info;
    gfn->amap = amap;
    gfn->name = g_strdup(name);
    g_base_info_ref(function_info);

    gig_amap_s_input_count(gfn->amap, s_in_req, s_in_opt);
    if (g_callable_info_is_method(gfn->function_info))
        (*s_in_req)++;

    make_formals(gfn->function_info, gfn->amap, *s_in_req + *s_in_opt,
                 self_type, formals, specializers);

    // STEP 1
    // Allocate the block of memory that FFI uses to hold a closure
    // object, and set a pointer to the corresponding executable
    // address.
    gfn->closure = ffi_closure_alloc(sizeof(ffi_closure), &(gfn->function_ptr));

    g_return_val_if_fail(gfn->closure != NULL, NULL);
    g_return_val_if_fail(gfn->function_ptr != NULL, NULL);

    // STEP 2
    // Next, we begin to construct an FFI_CIF to describe the function
    // call.

    // Initialize the argument info vectors.
    gint have_args = 0;
    if (*s_in_req + *s_in_opt > 0) {
        gfn->atypes = g_new0(ffi_type *, 1);
        gfn->atypes[0] = &ffi_type_pointer;
        have_args = 1;
    }
    else
        gfn->atypes = NULL;

    // The return type is also SCM, for which we use a pointer.
    ffi_ret_type = &ffi_type_pointer;

    // Initialize the CIF Call Interface Struct.
    ffi_status prep_ok;
    prep_ok = ffi_prep_cif(&(gfn->cif), FFI_DEFAULT_ABI, have_args, ffi_ret_type, gfn->atypes);

    if (prep_ok != FFI_OK)
        scm_misc_error("gir-function-create-gsubr",
                       "closure call interface preparation error #~A",
                       scm_list_1(scm_from_int(prep_ok)));

    // STEP 3
    // Initialize the closure
    ffi_status closure_ok;
    closure_ok = ffi_prep_closure_loc(gfn->closure, &(gfn->cif), function_binding, gfn,
                                      gfn->function_ptr);

    if (closure_ok != FFI_OK)
        scm_misc_error("gir-function-create-gsubr",
                       "closure location preparation error #~A",
                       scm_list_1(scm_from_int(closure_ok)));

    g_hash_table_insert(function_cache, function_info, gfn);

    return gfn->function_ptr;
}

// This is the GICallable function wrapper.
// It converts FFI arguments to SCM arguments, converts those
// SCM arguments into GIArguments, calls the C function,
// and returns the results as an SCM packed into an FFI argument.
// Also, it converts GErrors into SCM misc-errors.
static void
function_binding(ffi_cif *cif, gpointer ret, gpointer *ffi_args, gpointer user_data)
{
    GigFunction *gfn = user_data;
    GObject *self = NULL;
    SCM s_args = SCM_UNDEFINED;

    g_assert(cif != NULL);
    g_assert(ret != NULL);
    g_assert(ffi_args != NULL);
    g_assert(user_data != NULL);

    guint n_args = cif->nargs;
    g_debug("Binding C function %s as %s with %d args", g_base_info_get_name(gfn->function_info),
            gfn->name, n_args);

    // we have either 0 args or 1 args, which is the already packed list
    g_assert(n_args <= 1);

    if (n_args)
        s_args = SCM_PACK(*(scm_t_bits *) (ffi_args[0]));

    if (SCM_UNBNDP(s_args))
        s_args = SCM_EOL;

    if (g_callable_info_is_method(gfn->function_info)) {
        self = gig_type_peek_object(scm_car(s_args));
        s_args = scm_cdr(s_args);
    }

    // Then invoke the actual function
    GError *err = NULL;
    SCM output = function_call(gfn->function_info, gfn->amap, gfn->name, self, s_args, &err);

    // If there is a GError, write an error and exit.
    if (err) {
        gchar str[256];
        memset(str, 0, 256);
        strncpy(str, err->message, 255);
        g_error_free(err);

        scm_misc_error(gfn->name, str, SCM_EOL);
        g_return_if_reached();
    }

    *(ffi_arg *) ret = SCM_UNPACK(output);
}


// This procedure calls the C function described in 'func_info' with
// the SCM arguments in s_args.
static SCM
function_call(GIFunctionInfo *func_info, GigArgMap *amap, const gchar *subr, GObject *self,
              SCM s_args, GError **error)
{
    GValue self_val = G_VALUE_INIT;
    GValue *in_vals = NULL;
    GValue *out_vals = NULL;
    GValue return_val = G_VALUE_INIT;
    GSList *free_list = NULL;
    SCM output;
    gboolean ok;
    gint args_count, required, optional;

    // Until we have an FFI that takes GValues natively, we have to
    // convert SCM->GValue->GIArgument.  This function is the
    // conversion from SCM->GValue.

    if (SCM_UNBNDP(s_args))
        args_count = 0;
    else
        args_count = scm_to_int(scm_length(s_args));
    gig_amap_s_input_count(amap, &required, &optional);
    if (args_count < required || args_count > required + optional)
        scm_error_num_args_subr(subr);

    in_vals = g_new0(GValue, amap->c_input_len);
    out_vals = g_new0(GValue, amap->c_output_len);
    set_value_types(amap, &self_val, in_vals, out_vals, &return_val);
    g_value_set_pointer(&self_val, self);
    convert_input_vals(amap, subr, s_args, in_vals, &free_list, out_vals);

    // Using the GValues, make the function call
    g_debug("Before invoking %s", g_base_info_get_name(func_info));
    for (gint c = 0; c < amap->c_input_len; c++) {
        g_debug(" Input Arg #%d, %s", c, G_VALUE_TYPE_NAME(&in_vals[c]));
    }
    for (gint c = 0; c < amap->c_output_len; c++) {
        g_debug(" Output Arg #%d, %s", c, G_VALUE_TYPE_NAME(&out_vals[c]));
    }
    ok = function_invoke(func_info, amap, &self_val, in_vals, out_vals, &return_val, error);
    g_debug("After invoking %s", g_base_info_get_name(func_info));
    for (gint c = 0; c < amap->c_input_len; c++) {
        g_debug(" Input Arg #%d, %s", c, G_VALUE_TYPE_NAME(&in_vals[c]));
    }
    for (gint c = 0; c < amap->c_output_len; c++) {
        g_debug(" Output Arg #%d, %s", c, G_VALUE_TYPE_NAME(&out_vals[c]));
    }
    if (ok) {
        // The output has 3 components
        // - the returned value
        // - any non-preallocated out parameters
        // - the inout parameters
        SCM output1 = SCM_EOL;
        SCM output2 = SCM_EOL;
        SCM output3 = SCM_EOL;
        gig_value_to_scm_full_with_error(&return_val, &amap->return_val.meta, &output1,
                                         g_base_info_get_name(func_info));
        if (scm_is_eq(output1, SCM_UNSPECIFIED))
            output1 = SCM_EOL;
        else
            output1 = scm_list_1(output1);
        output2 = convert_output_vals(func_info, amap, subr, out_vals);
        output3 = rebox_inout_vals(func_info, amap, subr, in_vals, out_vals, s_args);
        output = scm_append(scm_list_3(output1, output2, output3));
    }

    for (int i = 0; i < amap->c_input_len; i++)
        g_value_unset(&in_vals[i]);
    free(in_vals);
    for (int i = 0; i < amap->c_output_len; i++)
        g_value_unset(&out_vals[i]);
    free(out_vals);
    g_slist_free_full(free_list, free);

    if (!ok)
        return SCM_UNSPECIFIED;

    switch (scm_to_int(scm_length(output))) {
    case 0:
        return SCM_UNSPECIFIED;
    case 1:
        return scm_car(output);
    default:
        return scm_values(output);
    }
}

static void
set_value_types(GigArgMap *amap, GValue *self_val, GValue *in_vals, GValue *out_vals,
                GValue *return_val)
{
    g_value_init(self_val, G_TYPE_POINTER);
    for (int c = 0; c < amap->c_input_len; c++) {
        gint i;
        gig_amap_input_c2i(amap, c, &i);
        gig_value_preset_type(&amap->pdata[i], &in_vals[c]);
    }
    for (int c = 0; c < amap->c_output_len; c++) {
        gint i;
        gig_amap_output_c2i(amap, c, &i);
        gig_value_preset_type(&amap->pdata[i], &out_vals[c]);
    }
    gig_value_preset_type(&amap->return_val, return_val);
}

static gboolean
function_invoke(GICallableInfo *func_info, GigArgMap *amap, GValue *self_val, GValue *in_vals,
                GValue *out_vals, GValue *return_val, GError **error)
{
    GIArgument *in_args = NULL;
    GIArgument *out_args = NULL, *out_boxes = NULL;
    GIArgument return_arg;
    gint offset;

    // This function uses a list of input and output GValues to call
    // a C function described by the 'func_info'.  GObject FFI, uses
    // GIArguments to make the call, so we convert here.
    if (g_value_get_pointer(self_val) != NULL)
        offset = 1;
    else
        offset = 0;
    if (amap->c_input_len + offset > 0)
        in_args = g_new0(GIArgument, amap->c_input_len + offset);
    if (amap->c_output_len)
        out_args = g_new0(GIArgument, amap->c_output_len);

    if (g_value_get_pointer(self_val) != NULL)
        in_args[0].v_pointer = g_value_get_pointer(self_val);

    for (gint c = 0; c < amap->c_input_len; c++) {
        GType type = G_VALUE_TYPE(&in_vals[c]);
        gsize array_len;
        if (type == G_TYPE_ARRAY)
            array_len = ((GArray *)(g_value_get_boxed(&in_vals[c])))->len;
        else if (type == G_TYPE_BYTE_ARRAY)
            array_len = ((GByteArray *) (g_value_get_boxed(&in_vals[c])))->len;
        else if (type == G_TYPE_PTR_ARRAY)
            array_len = ((GPtrArray *)(g_value_get_boxed(&in_vals[c])))->len;
        else
            array_len = 0;
        gig_value2arg(&in_vals[c], &in_args[c + offset], &array_len);
    }

    for (gint c = 0; c < amap->c_output_len; c++) {
        GType type = G_VALUE_TYPE(&out_vals[c]);
        gsize array_len;
        if (type == G_TYPE_ARRAY)
            array_len = ((GArray *)(g_value_get_boxed(&out_vals[c])))->len;
        else if (type == G_TYPE_BYTE_ARRAY)
            array_len = ((GByteArray *) (g_value_get_boxed(&out_vals[c])))->len;
        else if (type == G_TYPE_PTR_ARRAY)
            array_len = ((GPtrArray *)(g_value_get_boxed(&out_vals[c])))->len;
        else
            array_len = 0;
        if (type)
            gig_value2arg(&out_vals[c], &out_args[c], &array_len);
    }

    // Since, in the Guile binding, we're allocating the output
    // parameters in most cases, here's where we make space for
    // immediate return arguments.  There's a trick here.  Sometimes
    // GLib expects to use these out_args directly, and sometimes it
    // expects out_args->v_pointer to point to allocated space.  I
    // allocate space for *all* the output arguments, even when not
    // needed.  It is easier than figuring out which output arguments
    // need allocation.
    out_boxes = g_new0(GIArgument, amap->c_output_len);
    for (guint c = 0; c < amap->c_output_len; c++)
        if (out_args[c].v_pointer == NULL)
            out_args[c].v_pointer = &out_boxes[c];

    // Finally, after much kerfuffle of SCM->arg conversion,
    // use GObject's ffi to call the C function.
    g_debug("Calling %s with %d input and %d output arguments",
            g_base_info_get_name(func_info), amap->c_input_len, amap->c_output_len);
    gboolean ok = g_function_info_invoke(func_info, in_args, amap->c_input_len + offset, out_args,
                                         amap->c_output_len, &return_arg, error);

    for (guint c = 0; c < amap->c_output_len; c++) {
        // Check if the FFI used this argument directly or indirectly.
        if (out_args[c].v_pointer == &out_boxes[c]) {
            memcpy(&out_args[c], &out_boxes[c], sizeof(GIArgument));
        }
    }
    free(out_boxes);

    gint i, child_i;
    gint child_c;
    gsize size = 0;

    for (guint c = 0; c < amap->c_output_len; c++) {
        GType type = G_VALUE_TYPE(&out_vals[c]);
        gig_amap_output_c2i(amap, c, &i);

        if (type == G_TYPE_INVALID) {
            type = amap->pdata[i].meta.gtype;
            if (type == G_TYPE_LENGTH_CARRAY
                || type == G_TYPE_ZERO_TERMINATED_CARRAY || type == G_TYPE_FIXED_SIZE_CARRAY) {
                GArray *arr = g_array_new(FALSE, TRUE, amap->pdata[i].meta.item_size);
                g_value_init(&out_vals[c], G_TYPE_ARRAY);
                g_value_set_boxed(&out_vals[c], arr);
                if (type == G_TYPE_FIXED_SIZE_CARRAY)
                    size = amap->pdata[i].meta.length;
                else if (type == G_TYPE_ZERO_TERMINATED_CARRAY)
                    size =
                        zero_terminated_carray_length(&amap->pdata[i].meta, out_args[c].v_pointer);
                else if (type == G_TYPE_LENGTH_CARRAY) {
                    if (gig_amap_output_child_c(amap, c, &child_c))
                        size = out_args[child_c].v_size;
                }
            }
            else if (type == G_TYPE_ENUM)
                g_value_init(return_val, G_TYPE_INT);
            else
                g_value_init(&out_vals[c], amap->pdata[i].meta.gtype);
        }

        gig_arg2value(&out_vals[c], &out_args[c], size);
    }

    GType type = G_VALUE_TYPE(return_val);
    if (type == G_TYPE_INVALID) {
        type = amap->return_val.meta.gtype;
        if (type == G_TYPE_LENGTH_CARRAY
            || type == G_TYPE_ZERO_TERMINATED_CARRAY || type == G_TYPE_FIXED_SIZE_CARRAY) {
            GArray *arr = g_array_new(FALSE, TRUE, amap->pdata[i].meta.item_size);
            g_value_init(return_val, G_TYPE_ARRAY);
            g_value_set_boxed(return_val, arr);
            if (type == G_TYPE_FIXED_SIZE_CARRAY)
                size = amap->pdata[i].meta.length;
            else if (type == G_TYPE_ZERO_TERMINATED_CARRAY)
                size = zero_terminated_carray_length(&amap->pdata[i].meta, return_arg.v_pointer);
            else if (type == G_TYPE_LENGTH_CARRAY) {
                size = 0;
                if (gig_amap_return_child_i(amap, &child_i)) {
                    gig_amap_output_i2c(amap, child_i, &child_c);
                    size = out_args[child_c].v_size;
                }
            }
        }
        else if (type == G_TYPE_ENUM)
            g_value_init(return_val, G_TYPE_INT);
        else
            g_value_init(return_val, amap->return_val.meta.gtype);
    }

    gig_arg2value(return_val, &return_arg, size);
    free(in_args);
    free(out_args);

    return ok;
}

static void
convert_input_vals(GigArgMap *amap,
                   const gchar *subr, SCM s_args,
                   GValue *c_in_vals, GSList ** free_list, GValue *c_out_vals)
{
    SCM s_arg;
    SCM s_rest = s_args;
    int s = 0;
    while (!scm_is_null(s_rest)) {
        s_arg = scm_car(s_rest);
        s_rest = scm_cdr(s_rest);
        convert_input_val1(amap, s, subr, s_arg, c_in_vals, free_list, c_out_vals);
        s++;
    }
    return;
}

static void
convert_input_val1(GigArgMap *amap, gint s, const gchar *name, SCM s_arg,
                   GValue *c_in_vals, GSList ** free_list, GValue *c_out_vals)
{
    g_assert_nonnull(amap);
    g_assert_nonnull(name);
    g_assert_cmpint(s, <, 20);

    // Convert an input scheme argument to a C invoke argument
    gsize size;
    gboolean is_input, is_output;
    gboolean c_in_pos, c_out_pos;
    gboolean inout, has_child;
    GigTypeMeta *meta;

    // Store the converted argument.
    int i;
    GValue *val;
    gig_amap_input_s2i(amap, s, &i);
    is_input = gig_amap_input_i2c(amap, i, &c_in_pos);
    is_output = gig_amap_output_i2c(amap, i, &c_out_pos);
    inout = is_input && is_output;
    meta = &amap->pdata[i].meta;
    TRACE_S2(amap->pdata[i].name, meta);

    if (is_input) {
        val = &c_in_vals[c_in_pos];
        gig_scm_to_value_full_with_error(s_arg, meta, val, name, s);
    }
    if (is_output) {
        if (inout)
            memmove(&c_out_vals[c_out_pos], val, sizeof(GValue));
        else {
            val = &c_out_vals[c_out_pos];
            gig_scm_to_value_full_with_error(s_arg, meta, val, name, s);
        }
    }

    if (meta->gtype == G_TYPE_LENGTH_CARRAY) {
        // Store the converted argument's length, if necessary
        GArray *arr = g_value_get_boxed(val);
        gsize len = arr->len;
        gint i_child;
        gig_amap_child_i(amap, i, &i_child);
        is_input = gig_amap_input_i2c(amap, i_child, &c_in_pos);
        is_output = gig_amap_output_i2c(amap, i_child, &c_out_pos);
        inout = is_input && is_output;
        meta = &amap->pdata[i_child].meta;
        TRACE_S2(amap->pdata[i_child].name, meta);

        if (is_input) {
            val = &c_in_vals[c_in_pos];
            gig_scm_to_value_full_with_error(scm_from_size_t(len), meta, val, "(%array-length)",
                                             0);
        }
        if (is_output) {
            if (inout)
                memmove(&c_out_vals[c_out_pos], val, sizeof(GValue));
            else {
                val = &c_out_vals[c_out_pos];
                gig_scm_to_value_full_with_error(scm_from_size_t(len), meta, val,
                                                 "(%array-length)", 0);
            }
        }
    }
}

static SCM
convert_output_vals(GIFunctionInfo *func_info, GigArgMap *amap,
                    const gchar *func_name, GValue *out_vals)
{
    SCM output = SCM_EOL;
    gint gsubr_output_index;
    gint c, s, i;
    gchar *name;

    for (c = 0; c < amap->c_output_len; c++) {
        if (!gig_amap_output_c2s(amap, c, &s))
            continue;
        gig_amap_output_c2i(amap, c, &i);

        GigArgMapEntry *entry = &amap->pdata[i];
        SCM obj;
        gint size_index;
        gsize size = GIG_ARRAY_SIZE_UNKNOWN;
        name = amap->pdata[i].name;

        TRACE_2S(entry->name, &entry->meta);

        if (gig_amap_child_i(amap, i, &size_index)) {
            GigArgMapEntry *size_entry;
            gint c_child;

            if (gig_amap_output_i2c(amap, size_index, &c_child)) {
                size_entry = &amap->pdata[size_index];
                name = size_entry->name;

                TRACE_2S(size_entry->name, &size_entry->meta);

                gig_value_to_scm_full_with_error(&out_vals[c_child], &size_entry->meta, &obj,
                                                 g_base_info_get_name(func_info));
                size = scm_to_int(obj);
            }
        }

        gig_value_to_scm_full_with_error(&out_vals[c], &entry->meta, &obj,
                                         g_base_info_get_name(func_info));
        output = scm_append(scm_list_2(output, scm_list_1(obj)));
    }
    return output;
}

// For INOUT args, if they came from SCM boxes, push the resulting
// outputs back into those boxes.
static SCM
rebox_inout_vals(GIFunctionInfo *func_info, GigArgMap *amap,
                 const gchar *func_name, GValue *in_vals, GValue *out_vals, SCM s_args)
{
    if (scm_is_null(s_args))
        return SCM_EOL;

    SCM output = SCM_EOL;

    // As far as I can tell, in INOUT args, the modified value is
    // stored in the input cinvoke arguments, while the output cinvoke
    // argument for that parameter is unused.

    for (guint c_input_pos = 0; c_input_pos < amap->c_input_len; c_input_pos++) {
        for (guint i = 0; i < amap->len; i++) {
            GigArgMapEntry *amap_entry = &(amap->pdata[i]);
            if ((amap_entry->is_c_input && (amap_entry->c_input_pos == c_input_pos))
                && (amap_entry->meta.is_in && amap_entry->meta.is_out)) {
                SCM obj;
                gsize size = GIG_ARRAY_SIZE_UNKNOWN;
                if (amap_entry->child != NULL) {
                    gint size_index = amap_entry->child->i;
                    g_assert_cmpint(size_index, >=, 0);

                    gig_value_to_scm_full_with_error(&in_vals[amap->pdata[size_index].c_input_pos],
                                                     &(amap->pdata[size_index].meta), &obj,
                                                     func_name);
                    size = scm_to_size_t(obj);
                }

                if (amap_entry->parent == NULL) {
                    gig_value_to_scm_full_with_error(&in_vals[amap_entry->c_input_pos],
                                                     &amap_entry->meta, &obj, func_name);
                    output = scm_append(scm_list_2(output, scm_list_1(obj)));
                }
                break;
            }
        }
    }
    return output;
}

void
gig_init_function(void)
{
    function_cache =
        g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)function_free);
    top_type = scm_c_public_ref("oop goops", "<top>");
    method_type = scm_c_public_ref("oop goops", "<method>");
    ensure_generic_proc = scm_c_public_ref("oop goops", "ensure-generic");
    make_proc = scm_c_public_ref("oop goops", "make");
    add_method_proc = scm_c_public_ref("oop goops", "add-method!");

    kwd_specializers = scm_from_utf8_keyword("specializers");
    kwd_formals = scm_from_utf8_keyword("formals");
    kwd_procedure = scm_from_utf8_keyword("procedure");

    sym_self = scm_from_utf8_symbol("self");

    atexit(gig_fini_function);
}

static void
function_free(GigFunction *gfn)
{
    g_free(gfn->name);
    gfn->name = NULL;

    ffi_closure_free(gfn->closure);
    gfn->closure = NULL;

    g_base_info_unref(gfn->function_info);
    g_free(gfn->atypes);
    gfn->atypes = NULL;

    // TODO: should we free gfn->amap?

    g_free(gfn);
}

static void
gig_fini_function(void)
{
    g_debug("Freeing functions");
    g_hash_table_remove_all(function_cache);
    function_cache = NULL;
}
