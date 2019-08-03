#include "gig_signal.h"
#include "gig_object.h"
#include "gig_value.h"
#include "gig_type.h"
#include "gig_argument.h"
#include "gig_flag.h"

/******************
 *  Signal Specs  *
 ******************/
static SCM signal_slot_syms[GIG_SIGNAL_SLOT_COUNT];

SCM
gig_signal_ref(SCM signal, GigSignalSlot slot)
{
    return scm_slot_ref(signal, signal_slot_syms[slot]);
}

static SCM signal_accu_first_wins;
static SCM signal_accu_true_handled;

static SCM make_signal_proc;

static gboolean
scm_signal_accu(GSignalInvocationHint * ihint,
                GValue *seed, const GValue *element, gpointer procedure)
{
    SCM _seed, _element, result;
    _seed = gig_value_as_scm(seed, FALSE);
    _element = gig_value_as_scm(element, FALSE);

    result = scm_call_2(SCM_PACK_POINTER(procedure), _seed, _element);
    switch (scm_c_nvalues(result)) {
    case 0:
        return TRUE;
    case 1:
        g_return_val_if_fail(!gig_value_from_scm(seed, result), FALSE);
        return TRUE;
    case 2:
    {
        gboolean ret = scm_is_true(result);
        SCM next_seed = scm_c_value_ref(result, 2);
        g_return_val_if_fail(!gig_value_from_scm(seed, next_seed), FALSE);
        return ret;
    }
    default:
        g_return_val_if_reached(FALSE);
    }
}

GigSignalSpec *
gig_signalspec_from_obj(SCM obj)
{
    gchar *name;
    GType return_type;
    guint n_params;
    GType *params;
    SCM sparams, saccu;
    GSignalFlags flags;
    GigSignalSpec *spec = NULL;

    SCM_ASSERT_TYPE(SCM_IS_A_P(obj, gig_signal_type), obj, SCM_ARG1, "%scm->signalspec", "signal");

    name = scm_to_utf8_string(gig_signal_ref(obj, GIG_SIGNAL_SLOT_NAME));
    return_type = scm_to_gtype(gig_signal_ref(obj, GIG_SIGNAL_SLOT_RETURN_TYPE));
    sparams = gig_signal_ref(obj, GIG_SIGNAL_SLOT_PARAM_TYPES);
    saccu = gig_signal_ref(obj, GIG_SIGNAL_SLOT_ACCUMULATOR);
    n_params = scm_to_uint(scm_length(sparams));
    params = g_new0(GType, n_params);

    for (guint i = 0; i < n_params; i++, sparams = scm_cdr(sparams))
        params[i] = scm_to_gtype(scm_car(sparams));

    flags = gig_flags_to_uint(gig_signal_ref(obj, GIG_SIGNAL_SLOT_FLAGS));

    spec = g_new0(GigSignalSpec, 1);
    spec->signal_name = name;
    spec->signal_flags = flags;
    if (SCM_UNBNDP(saccu) || scm_is_false(saccu)) {
        spec->accumulator = NULL;
        spec->accu_data = NULL;
    }
    else if (scm_is_eq(saccu, signal_accu_first_wins)) {
        spec->accumulator = g_signal_accumulator_first_wins;
        spec->accu_data = NULL;
    }
    else if (scm_is_eq(saccu, signal_accu_true_handled)) {
        spec->accumulator = g_signal_accumulator_true_handled;
        spec->accu_data = NULL;
    }
    else if (scm_is_true(scm_procedure_p(saccu))) {
        spec->accumulator = scm_signal_accu;
        spec->accu_data = SCM_UNPACK_POINTER(saccu);
    }
    spec->return_type = return_type;
    spec->n_params = n_params;
    spec->param_types = params;
    return spec;
}

void
gig_free_signalspec(GigSignalSpec *spec)
{
    if (spec) {
        if (spec->param_types) {
            g_free(spec->param_types);
            spec->param_types = NULL;
        }
        g_free(spec);
    }
}

SCM
gig_make_signal(gsize n_slots, GigSignalSlot *slots, SCM *slot_values)
{
    SCM args = scm_make_list(scm_from_size_t(n_slots * 2), SCM_UNDEFINED);
    SCM iter = args;

    for (gsize i = 0; i < n_slots; i++, iter = scm_cddr(iter)) {
        SCM key_iter = iter, val_iter = scm_cdr(iter);
        scm_set_car_x(key_iter, scm_symbol_to_keyword(signal_slot_syms[slots[i]]));
        scm_set_car_x(val_iter, slot_values[i]);
    }

    return scm_apply_0(make_signal_proc, args);
}


/******************
 *    Closures    *
 ******************/

typedef void (*GigClosureExceptionHandler)(GValue *ret, guint n_param_values,
                                           const GValue *params);


typedef struct _GigClosure
{
    GClosure closure;
    SCM callback;
    SCM swap_data;              /* other object for gtk_signal_connect__object */
    GigClosureExceptionHandler exception_handler;
    GISignalInfo *signal_info;
} GigClosure;

static GISignalInfo *
lookup_signal_from_g_type(GType g_type, const gchar *signal_name)
{
    GIRepository *repository;
    GIBaseInfo *info;
    GISignalInfo *signal_info = NULL;

    repository = g_irepository_get_default();
    info = g_irepository_find_by_gtype(repository, g_type);
    if (info == NULL)
        return NULL;

    if (GI_IS_OBJECT_INFO(info))
        signal_info = g_object_info_find_signal((GIObjectInfo *)info, signal_name);
    else if (GI_IS_INTERFACE_INFO(info))
        signal_info = g_interface_info_find_signal((GIInterfaceInfo *)info, signal_name);

    g_base_info_unref(info);
    return signal_info;
}

static void
signal_closure_invalidate(gpointer data, GClosure *closure)
{
    GigClosure *pc = (GigClosure *) closure;

    pc->callback = SCM_BOOL_F;
    pc->swap_data = SCM_BOOL_F;

    g_base_info_unref(pc->signal_info);
    pc->signal_info = NULL;
}

static void
signal_closure_marshal(GClosure *closure,
                       GValue *return_value,
                       guint n_param_values,
                       const GValue *param_values, gpointer invocation_hint, gpointer marshal_data)
{
    GigClosure *pc = (GigClosure *) closure;
    SCM params, ret = SCM_BOOL_F;
    guint i;
    GISignalInfo *signal_info;
    gint n_sig_info_args;
    gint sig_info_highest_arg;
#if 0
    GSList *list_item = NULL;
#endif
    GSList *pass_by_ref_structs = NULL;


    // Here we take the parameters in *param_values and call the
    // Scheme function stored in *closure.

    signal_info = pc->signal_info;
    if (signal_info) {
        n_sig_info_args = g_callable_info_get_n_args(signal_info);
        g_assert_cmpint(n_sig_info_args, >=, 0);
        /* the first argument to a signal callback is instance,
         * but instance is not counted in the introspection data */
        sig_info_highest_arg = n_sig_info_args + 1;
        g_assert_cmpint(sig_info_highest_arg, ==, n_param_values);
    }

    /* construct a scheme list for the parameter values */
    params = SCM_EOL;
    // FIXME: handle swap
    /* gboolean swap = G_CCLOSURE_SWAP_DATA(closure); */
    for (i = 0; i < n_param_values; i++) {
        SCM item = gig_value_as_scm(&param_values[i], FALSE);
        if (scm_is_false(item)) {
            if (i == 0)         // self or this
                goto out;
        }
        params = scm_cons(item, params);
    }
    params = scm_reverse_x(params, SCM_EOL);
    g_debug("invoking callback with %d arguments", scm_to_int(scm_length(params)));
    ret = scm_apply_0(pc->callback, params);

    if (G_IS_VALUE(return_value) && gig_value_from_scm(return_value, ret) != 0) {
        scm_misc_error("callback", "can't convert return value to desired type", SCM_EOL);
    }

  out:
    g_slist_free(pass_by_ref_structs);
}

GClosure *
gig_signal_closure_new(SCM instance, GType g_type, const gchar *signal_name, SCM callback)
{
    GClosure *closure = NULL;
    GigClosure *gig_closure = NULL;
    GISignalInfo *signal_info = NULL;

    g_return_val_if_fail(scm_is_true(instance), NULL);

    signal_info = lookup_signal_from_g_type(g_type, signal_name);

    closure = g_closure_new_simple(sizeof(GigClosure), NULL);
    g_closure_add_invalidate_notifier(closure, NULL, signal_closure_invalidate);
    g_closure_set_marshal(closure, signal_closure_marshal);

    gig_closure = (GigClosure *) closure;

    gig_closure->signal_info = signal_info;
    gig_closure->callback = callback;

    return closure;
}


/******************
 * Initialization *
 ******************/
void
gig_init_signal()
{
    gig_signal_type = scm_c_public_ref("gi oop", "<signal>");
    make_signal_proc = scm_c_public_ref("gi oop", "make-signal");

    signal_slot_syms[GIG_SIGNAL_SLOT_NAME] = scm_from_utf8_symbol("name");
    signal_slot_syms[GIG_SIGNAL_SLOT_FLAGS] = scm_from_utf8_symbol("flags");
    signal_slot_syms[GIG_SIGNAL_SLOT_ACCUMULATOR] = scm_from_utf8_symbol("accumulator");
    signal_slot_syms[GIG_SIGNAL_SLOT_RETURN_TYPE] = scm_from_utf8_symbol("return-type");
    signal_slot_syms[GIG_SIGNAL_SLOT_PARAM_TYPES] = scm_from_utf8_symbol("param-types");

    signal_accu_first_wins = scm_from_utf8_symbol("first-wins");
    signal_accu_true_handled = scm_from_utf8_symbol("true-handled");

}
