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

static SCM convert_output_args(GIFunctionInfo *func_info, GigArgMap *amap, const gchar *name,
                               GArray *out_args);
static void object_list_to_c_args(GIFunctionInfo *func_info, GigArgMap *amap, const gchar *subr,
                                  SCM s_args, GArray *in_args, GPtrArray *cinvoke_free_array,
                                  GArray *out_args);
static void
store_argument(gint invoke_in, gint invoke_out, gboolean inout, GIArgument *arg,
               GArray *cinvoke_input_arg_array, GPtrArray *cinvoke_free_array,
               GArray *cinvoke_output_arg_array);
static SCM rebox_inout_args(GIFunctionInfo *func_info, GigArgMap *amap,
                            const gchar *func_name, GArray *in_args, GArray *out_args, SCM s_args);
static void function_free(GigFunction *fn);
static void gig_fini_function(void);
static SCM gig_function_define1(const gchar *public_name, SCM proc, int opt, SCM formals,
                                SCM specializers);

static SCM proc4function(GIFunctionInfo *info, const gchar *name, SCM self_type,
                         int *req, int *opt, SCM *formals, SCM *specs);
static SCM proc4signal(GISignalInfo *info, const gchar *name, SCM self_type,
                       int *req, int *opt, SCM *formals, SCM *specs);

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

    return scm_c_make_gsubr(name, 0, 0, 1, func_gsubr);
}

static SCM
proc4signal(GISignalInfo *info, const gchar *name, SCM self_type, int *req, int *opt, SCM *formals,
            SCM *specializers)
{
    GigArgMap *amap;

    amap = scm_dynwind_or_bust("%proc4signal", gig_amap_new(info));
    gig_amap_s_input_count(amap, req, opt);
    (*req)++;

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
check_gsubr_cache(GICallableInfo *function_info, SCM self_type, gint *required_input_count,
                  gint *optional_input_count, SCM *formals, SCM *specializers)
{
    // Check the cache to see if this function has already been created.
    GigFunction *gfn = g_hash_table_lookup(function_cache, function_info);

    if (gfn == NULL)
        return NULL;

    gig_amap_s_input_count(gfn->amap, required_input_count, optional_input_count);

    if (g_callable_info_is_method(gfn->function_info))
        (*required_input_count)++;

    make_formals(gfn->function_info,
                 gfn->amap,
                 *required_input_count + *optional_input_count, self_type, formals, specializers);

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

    for (gint i = 0; i < n_inputs;
         i++, i_formal = scm_cdr(i_formal), i_specializer = scm_cdr(i_specializer)) {
        GigArgMapEntry *entry = gig_amap_get_input_entry_by_s(argmap, i);
        gchar *formal = scm_dynwind_or_bust("%make-formals",
                                            gig_gname_to_scm_name(entry->name));
        scm_set_car_x(i_formal, scm_from_utf8_symbol(formal));
        // don't force types on nullable input, as #f can also be used to represent
        // NULL.
        if (entry->may_be_null)
            continue;

        if (entry->type_tag == GI_TYPE_TAG_INTERFACE) {
            GIBaseInfo *iface = g_type_info_get_interface(entry->type_info);
            if (!GI_IS_REGISTERED_TYPE_INFO(iface))
                continue;
            GType gtype = g_registered_type_info_get_g_type((GIRegisteredTypeInfo *) iface);
            SCM s_type = gig_type_get_scheme_type(gtype);
            if (scm_is_true(s_type))
                scm_set_car_x(i_specializer, s_type);
        }
    }
}

static GigGsubr *
create_gsubr(GIFunctionInfo *function_info, const gchar *name, SCM self_type,
             gint *required_input_count, gint *optional_input_count,
             SCM *formals, SCM *specializers)
{
    GigFunction *gfn;
    ffi_type *ffi_ret_type;

    gfn = g_new0(GigFunction, 1);
    gfn->function_info = function_info;
    gfn->amap = gig_amap_new(function_info);
    gfn->name = g_strdup(name);
    g_base_info_ref(function_info);

    gig_amap_s_input_count(gfn->amap, required_input_count, optional_input_count);

    if (g_callable_info_is_method(gfn->function_info))
        (*required_input_count)++;

    make_formals(gfn->function_info, gfn->amap, *required_input_count + *optional_input_count,
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
    if (*required_input_count + *optional_input_count > 0) {
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


SCM
gig_function_invoke(GIFunctionInfo *func_info, GigArgMap *amap, const gchar *name, GObject *self,
                    SCM args, GError **error)
{
    GArray *cinvoke_input_arg_array;
    GPtrArray *cinvoke_free_array;
    GArray *cinvoke_output_arg_array;
    GIArgument *out_args, *out_boxes;
    GIArgument return_arg;
    SCM output;
    gboolean ok;

    cinvoke_input_arg_array = g_array_new(FALSE, TRUE, sizeof(GIArgument));
    cinvoke_output_arg_array = g_array_new(FALSE, TRUE, sizeof(GIArgument));
    cinvoke_free_array = g_ptr_array_new_with_free_func(g_free);

    // Convert the scheme arguments into C.
    object_list_to_c_args(func_info, amap, name, args, cinvoke_input_arg_array,
                          cinvoke_free_array, cinvoke_output_arg_array);
    // For methods calls, the object gets inserted as the 1st argument.
    if (self) {
        GIArgument self_arg;
        self_arg.v_pointer = self;
        g_array_prepend_val(cinvoke_input_arg_array, self_arg);
    }

    // Since, in the Guile binding, we're allocating the output
    // parameters in most cases, here's where we make space for
    // immediate return arguments.  There's a trick here.  Sometimes
    // GLib expects to use these out_args directly, and sometimes it
    // expects out_args->v_pointer to point to allocated space.  I
    // allocate space for *all* the output arguments, even when not
    // needed.  It is easier than figuring out which output arguments
    // need allocation.
    out_args = (GIArgument *)(cinvoke_output_arg_array->data);
    out_boxes = g_new0(GIArgument, cinvoke_output_arg_array->len);
    g_ptr_array_insert(cinvoke_free_array, 0, out_boxes);
    for (guint i = 0; i < cinvoke_output_arg_array->len; i++)
        if (out_args[i].v_pointer == NULL)
            out_args[i].v_pointer = &out_boxes[i];

    // Make the actual call.
    // Use GObject's ffi to call the C function.
    g_debug("Calling %s with %d input and %d output arguments",
            name, cinvoke_input_arg_array->len, cinvoke_output_arg_array->len);
    gig_amap_dump(amap);
    ok = g_function_info_invoke(func_info, (GIArgument *)(cinvoke_input_arg_array->data),
                                cinvoke_input_arg_array->len,
                                (GIArgument *)(cinvoke_output_arg_array->data),
                                cinvoke_output_arg_array->len, &return_arg, error);

    // Here is where I check to see if I used the allocated
    // output argument space created above.
    for (guint i = 0; i < cinvoke_output_arg_array->len; i++)
        if (out_args[i].v_pointer == &out_boxes[i]) {
            memcpy(&out_args[i], &out_boxes[i], sizeof(GIArgument));
        }

    if (ok) {
        SCM s_return;
        gsize sz = -1;
        if (amap->return_val.array_length_index >= 0) {
            g_assert_cmpint(amap->return_val.array_length_index, <, amap->len);
            gsize idx = amap->pdata[amap->return_val.array_length_index].c_output_pos;
            sz = ((GIArgument *)(cinvoke_output_arg_array->data))[idx].v_size;
        }

        gig_argument_c_to_scm(name, -1, &amap->return_val, &return_arg, &s_return, sz);
        if (scm_is_eq(s_return, SCM_UNSPECIFIED))
            output = SCM_EOL;
        else
            output = scm_list_1(s_return);

        SCM output2 = convert_output_args(func_info, amap, name, cinvoke_output_arg_array);
        SCM output3 = rebox_inout_args(func_info, amap, name, cinvoke_input_arg_array,
                                       cinvoke_output_arg_array, args);
        output = scm_append(scm_list_3(output, output2, output3));
    }

    g_array_free(cinvoke_input_arg_array, TRUE);
    g_array_free(cinvoke_output_arg_array, TRUE);
    g_ptr_array_free(cinvoke_free_array, TRUE);

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

// This is the core of a dynamically generated GICallable function wrapper.
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
    SCM output = gig_function_invoke(gfn->function_info, gfn->amap, gfn->name, self, s_args, &err);

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

static void
object_to_c_arg(GigArgMap *amap, gint s, const gchar *name, SCM obj,
                GArray *cinvoke_input_arg_array, GPtrArray *cinvoke_free_array,
                GArray *cinvoke_output_arg_array)
{
    // Convert an input scheme argument to a C invoke argument
    GIArgument arg;
    GigArgMapEntry *entry;
    gsize size;
    gint c_invoke_in, c_invoke_out;
    gboolean is_in, is_out;
    gboolean inout;

    entry = gig_amap_get_input_entry_by_s(amap, s);
    gig_argument_scm_to_c(name, s, entry, obj, cinvoke_free_array, &arg, &size);

    // Store the converted argument.
    is_in = gig_amap_input_s2c(amap, s, &c_invoke_in);
    is_out = gig_amap_input_s_2_output_c(amap, s, &c_invoke_out);
    if (!is_in)
        c_invoke_in = -1;
    if (!is_out)
        c_invoke_out = -1;

    // Input/Output arguments have an extra implied level of
    // indirection.
    inout = c_invoke_in >= 0 && c_invoke_out >= 0;
    store_argument(c_invoke_in, c_invoke_out, inout, &arg,
                   cinvoke_input_arg_array, cinvoke_free_array, cinvoke_output_arg_array);

    // If this argument is an array with an associated size, store the
    // array size as well.
    is_in = gig_amap_input_s_2_child_input_c(amap, s, &c_invoke_in);
    is_out = gig_amap_input_s_2_child_output_c(amap, s, &c_invoke_out);
    if (!is_in)
        c_invoke_in = -1;
    if (!is_out)
        c_invoke_out = -1;
    if (c_invoke_in >= 0 || c_invoke_out >= 0) {
        GigArgMapEntry *size_entry = &amap->pdata[entry->array_length_index];
        GIArgument size_arg;
        gsize dummy_size;

        gig_argument_scm_to_c(name, s, size_entry, scm_from_size_t(size), cinvoke_free_array,
                              &size_arg, &dummy_size);

        store_argument(c_invoke_in, c_invoke_out, inout, &size_arg,
                       cinvoke_input_arg_array, cinvoke_free_array, cinvoke_output_arg_array);
    }
}

static void
store_argument(gint invoke_in, gint invoke_out, gboolean inout, GIArgument *arg,
               GArray *cinvoke_input_arg_array, GPtrArray *cinvoke_free_array,
               GArray *cinvoke_output_arg_array)
{
    GIArgument *parg;

    if (invoke_in >= 0) {
        if (inout) {
            gpointer *dup = g_memdup(arg, sizeof(GIArgument));
            parg = &g_array_index(cinvoke_input_arg_array, GIArgument, invoke_in);
            parg->v_pointer = dup;

            g_ptr_array_insert(cinvoke_free_array, 0, dup);

            parg = &g_array_index(cinvoke_output_arg_array, GIArgument, invoke_out);
            parg->v_pointer = 0;
        }
        else {
            parg = &g_array_index(cinvoke_input_arg_array, GIArgument, invoke_in);
            *parg = *arg;
        }
    }
    else if (invoke_out >= 0) {
        parg = &g_array_index(cinvoke_output_arg_array, GIArgument, invoke_out);
        *parg = *arg;
    }
}

static void
object_list_to_c_args(GIFunctionInfo *func_info,
                      GigArgMap *amap,
                      const gchar *subr, SCM s_args,
                      GArray *cinvoke_input_arg_array,
                      GPtrArray *cinvoke_free_array, GArray *cinvoke_output_arg_array)
{
    g_assert_nonnull(func_info);
    g_assert_nonnull(amap);
    g_assert_nonnull(subr);
    g_assert_nonnull(cinvoke_input_arg_array);
    g_assert_nonnull(cinvoke_free_array);
    g_assert_nonnull(cinvoke_output_arg_array);

    gint args_count, required, optional;
    if (SCM_UNBNDP(s_args))
        args_count = 0;
    else
        args_count = scm_to_int(scm_length(s_args));
    gig_amap_s_input_count(amap, &required, &optional);
    if (args_count < required || args_count > required + optional)
        scm_error_num_args_subr(subr);

    gint input_len, output_len;
    gig_amap_c_count(amap, &input_len, &output_len);

    g_array_set_size(cinvoke_input_arg_array, input_len);
    g_array_set_size(cinvoke_output_arg_array, output_len);

    for (gint i = 0; i < args_count; i++) {
        SCM obj = scm_list_ref(s_args, scm_from_int(i));
        object_to_c_arg(amap, i, subr, obj, cinvoke_input_arg_array, cinvoke_free_array,
                        cinvoke_output_arg_array);

    }
    return;
}

static SCM
convert_output_args(GIFunctionInfo *func_info, GigArgMap *amap,
                    const gchar *func_name, GArray *out_args)
{
    SCM output = SCM_EOL;
    gint s_output_pos;

    for (guint c_output_pos = 0; c_output_pos < out_args->len; c_output_pos++) {
        if (!gig_amap_output_c2s(amap, c_output_pos, &s_output_pos))
            continue;

        GigArgMapEntry *entry = gig_amap_get_output_entry_by_c(amap, c_output_pos);
        GIArgument *ob = (GIArgument *)(out_args->data);
        SCM obj;
        gint size_index;
        gsize size = GIG_ARRAY_SIZE_UNKNOWN;

        if (gig_amap_output_child_c(amap, c_output_pos, &size_index)) {
            // We need to know the size argument before we can process
            // this array argument.
            GigArgMapEntry *size_entry = gig_amap_get_output_entry_by_c(amap, size_index);
            gig_argument_c_to_scm(func_name, size_index, size_entry, &ob[size_index], &obj, -1);
            size = scm_to_int(obj);
        }

        gig_argument_c_to_scm(func_name, c_output_pos, entry, &ob[c_output_pos], &obj, size);
        output = scm_append(scm_list_2(output, scm_list_1(obj)));
    }
    return output;
}

// For INOUT args, if they came from SCM boxes, push the resulting
// outputs back into those boxes.
static SCM
rebox_inout_args(GIFunctionInfo *func_info, GigArgMap *amap,
                 const gchar *func_name, GArray *in_args, GArray *out_args, SCM s_args)
{
    if (scm_is_null(s_args))
        return SCM_EOL;

    SCM output = SCM_EOL;

    // As far as I can tell, in INOUT args, the modified value is
    // stored in the input cinvoke arguments, while the output cinvoke
    // argument for that parameter is unused.

    for (guint c_input_pos = 0; c_input_pos < out_args->len; c_input_pos++) {
        for (guint i = 0; i < amap->len; i++) {
            GigArgMapEntry *amap_entry = &amap->pdata[i];
            if ((amap_entry->c_input_pos == c_input_pos)
                && (amap_entry->c_direction == GI_DIRECTION_INOUT)) {
                GIArgument *ob = (GIArgument *)(in_args->data);
                SCM obj;
                gsize size = GIG_ARRAY_SIZE_UNKNOWN;
                if (amap_entry->child != NULL) {
                    gint size_index = amap_entry->child->i;
                    g_assert_cmpint(size_index, >=, 0);

                    gig_argument_c_to_scm(func_name, size_index, &amap->pdata[size_index],
                                          ob[amap->pdata[size_index].c_input_pos].v_pointer, &obj,
                                          GIG_ARRAY_SIZE_UNKNOWN);
                    size = scm_to_size_t(obj);
                }

                if (amap_entry->parent == NULL) {
                    gig_argument_c_to_scm(func_name, c_input_pos, amap_entry,
                                          ob[amap_entry->c_input_pos].v_pointer, &obj, size);
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
