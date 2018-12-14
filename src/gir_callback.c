#include <libguile.h>
#include <ffi.h>
#include "gir_callback.h"
#include "gi_giargument.h"

static SCM scm_call_proc (void *user_data);
static SCM scm_handler_proc (void *user_data, SCM key, SCM params);

GSList *callback_list = NULL;

static ffi_type *
type_info_to_ffi_type(GITypeInfo *type_info);


// This is the core of a dynamically generated callback funcion.
// It converts FFI arguments to SCM arguments, calls a SCM function
// and then returns the result.
void callback_binding(ffi_cif *cif, void *ret, void **ffi_args,
    void *user_data)
{
    GirCallback *gcb = user_data;
    SCM s_args = SCM_EOL;
    SCM s_ret;

    g_assert (cif != NULL);
    g_assert (ret != NULL);
    g_assert (ffi_args != NULL);
    g_assert (user_data != NULL);

    g_debug("in callback C->SCM binding");
    unsigned int n_args = cif->nargs;

    g_assert (n_args >= 0);

    for (unsigned int i = 0; i < n_args; i++)
    {
        SCM s_entry = SCM_BOOL_F;
        GIArgument giarg;
        GIArgInfo *arg_info;

        // Did I need this block? Or can I just
        // do giarg.v_pointer = ffi_args[i] for all cases?

        if (cif->arg_types[i] == &ffi_type_pointer)
            giarg.v_pointer = ffi_args[i];
        else if (cif->arg_types[i] == &ffi_type_void)
            giarg.v_pointer = ffi_args[i];
        else if (cif->arg_types[i] == &ffi_type_sint)
            giarg.v_int = (int)ffi_args[i];
        else if (cif->arg_types[i] == &ffi_type_sint8)
            giarg.v_int8 = (gint8)ffi_args[i];
        else if (cif->arg_types[i] == &ffi_type_uint8)
            giarg.v_uint8 = (guint8)ffi_args[i];
        else if (cif->arg_types[i] == &ffi_type_sint16)
            giarg.v_int16 = (gint16)ffi_args[i];
        else if (cif->arg_types[i] == &ffi_type_uint16)
            giarg.v_uint16 = (guint16)ffi_args[i];
        else if (cif->arg_types[i] == &ffi_type_sint32)
            giarg.v_int32 = (gint32)ffi_args[i];
        else if (cif->arg_types[i] == &ffi_type_uint32)
            giarg.v_uint32 = (guint32)ffi_args[i];
        else if (cif->arg_types[i] == &ffi_type_sint64)
            giarg.v_int64 = (gint64)ffi_args[i];
        else if (cif->arg_types[i] == &ffi_type_uint64)
            giarg.v_uint64 = (guint64)ffi_args[i];
        else if (cif->arg_types[i] == &ffi_type_float)
        {
            float val;
            val = *(float *)ffi_args[i];
            giarg.v_float = val;
        }
        else if (cif->arg_types[i] == &ffi_type_double)
        {
            float val;
            val = *(double *)ffi_args[i];
            giarg.v_double = val;
        }
        else
        {
            g_critical("Unhandled FFI type in %s: %d", __FILE__, __LINE__);
            giarg.v_pointer = ffi_args[i];
        }

        arg_info = g_callable_info_get_arg(gcb->callback_info, i);
        gi_giargument_convert_arg_to_object(&giarg, arg_info, &s_entry);
        s_args = scm_append(scm_list_2(s_args, scm_list_1(s_entry)));
        g_base_info_unref(arg_info);
    }

    s_ret = scm_c_catch(SCM_BOOL_T,
        scm_call_proc, SCM_UNPACK_POINTER(scm_cons (gcb->s_func, s_args)),
        scm_handler_proc, NULL, NULL, NULL);
    if (scm_is_false(s_ret))
        *(ffi_arg *)ret = FALSE;
    else
    {
        GIArgument giarg;
        GITypeInfo *ret_type_info = g_callable_info_get_return_type(gcb->callback_info);
        GIArgumentStatus ret2;
        ret2 = gi_giargument_convert_return_type_object_to_arg(s_ret,
                                                               ret_type_info,
                                                               g_callable_info_get_caller_owns(gcb->callback_info),
                                                               g_callable_info_may_return_null(gcb->callback_info),
                                                               g_callable_info_skip_return(gcb->callback_info),
                                                               &giarg);
        g_base_info_unref(ret_type_info);

        // I'm pretty sure I don't need a big type case/switch block here.
        // I'll try brutally coercing the data, and see what happens.
        *(ffi_arg *)ret = giarg.v_uint64;
    }
}

static SCM
scm_call_proc (void *user_data)
{
    SCM func_args_pair = SCM_PACK_POINTER(user_data);
    return scm_apply_0(scm_car(func_args_pair), scm_cdr(func_args_pair));
}

static SCM
scm_handler_proc (void *user_data, SCM key, SCM params)
{
    g_critical("scheme procedure threw error in C callback");
    return SCM_BOOL_F;
}

// This procedure uses CALLBACK_INFO to create a dynamic FFI C closure
// to use as an entry point to the scheme procedure S_FUNC.
GirCallback *gir_callback_new(GICallbackInfo *callback_info, SCM s_func)
{
    GirCallback *gcb = g_new0(GirCallback, 1);
    ffi_type **ffi_args = NULL;
    ffi_type *ffi_ret_type;
    gint n_args = g_callable_info_get_n_args(callback_info);

    {
        SCM s_name = scm_procedure_name(s_func);
        if (scm_is_string(s_name))
        {
            char *name = scm_to_utf8_string(scm_symbol_to_string(s_name));
            g_debug("Constructing C Callback for %s", name);
            free(name);
        }
        else
            g_debug("Construction a C Callback for an anonymous procedure");
    }

    gcb->s_func = s_func;
    gcb->callback_info = callback_info;
    g_base_info_ref(callback_info);

    // STEP 1
    // Allocate the block of memory that FFI uses to hold a closure object,
    // and set a pointer to the corresponding executable address.
    gcb->closure = ffi_closure_alloc(sizeof(ffi_closure),
        &(gcb->callback_ptr));

    g_return_val_if_fail(gcb->closure != NULL, NULL);
    g_return_val_if_fail(gcb->callback_ptr != NULL, NULL);

    // STEP 2
    // Next, we begin to construct an FFI_CIF to describe the function call.

    // Initialize the argument info vectors.
    if (n_args > 0)
        ffi_args = g_new0(ffi_type *, n_args);
    for (int i = 0; i < n_args; i++)
    {
        GIArgInfo *cb_arg_info = g_callable_info_get_arg(callback_info, i);
        GITypeInfo *cb_type_info = g_arg_info_get_type(cb_arg_info);
        ffi_args[i] = type_info_to_ffi_type(cb_type_info);
        g_base_info_unref(cb_arg_info);
        g_base_info_unref(cb_type_info);
    }

    GITypeInfo *ret_type_info = g_callable_info_get_return_type(callback_info);
    ffi_ret_type = type_info_to_ffi_type(ret_type_info);
    g_base_info_unref(ret_type_info);

    // Initialize the CIF Call Interface Struct.
    ffi_status prep_ok;
    prep_ok = ffi_prep_cif(&(gcb->cif),
        FFI_DEFAULT_ABI,
        n_args,
        ffi_ret_type,
        ffi_args);

    if (prep_ok != FFI_OK)
        scm_misc_error("gir-callback-new",
            "closure call interface preparation error #~A",
            scm_list_1(scm_from_int(prep_ok)));

    // STEP 3
    // Initialize the closure
    ffi_status closure_ok;
    closure_ok = ffi_prep_closure_loc(gcb->closure,
        &(gcb->cif),
        callback_binding,
        gcb,                 // The 'user-data' passed to the function
        gcb->callback_ptr);

    if (closure_ok != FFI_OK)
        scm_misc_error("gir-callback-new",
            "closure location preparation error #~A",
            scm_list_1(scm_from_int(closure_ok)));

#ifdef DEBUG_CALLBACKS
    gcb->callback_info_ptr_as_uint = GPOINTER_TO_UINT(gcb->callback_ptr);
    gcb->closure_ptr_as_uint = GPOINTER_TO_UINT(gcb->closure);
    gcb->callback_ptr_as_uint = GPOINTER_TO_UINT(gcb->closure);
#endif

    return gcb;
}

void *gir_callback_get_ptr(GICallbackInfo *cb_info, SCM s_func)
{
    g_assert(cb_info != NULL);
    g_assert(scm_is_true(scm_procedure_p(s_func)));

    // Lookup s_func in the callback cache.
    GSList *x = callback_list;
    GirCallback *gcb;
    GIInfoType cb_typeinfo = g_base_info_get_type(cb_info);
    GIInfoType gcb_typeinfo;

    // A callback is only a 'match' if it is the same Scheme produre
    // as well as the same GObject C Callback type.
    while (x != NULL)
    {
        gcb = x->data;
        if (scm_is_eq(gcb->s_func, s_func))
        {
            gcb_typeinfo = g_base_info_get_type(gcb->callback_info);
            if (cb_typeinfo == gcb_typeinfo)
                return gcb->callback_ptr;
        }
        x = x->next;
    }

    // Create a new entry if necessary.
    gcb = gir_callback_new(cb_info, s_func);
    callback_list = g_slist_prepend(callback_list, gcb);
    return gcb->callback_ptr;
}

static ffi_type *
type_info_to_ffi_type(GITypeInfo *type_info)
{
    GITypeTag type_tag = g_type_info_get_tag(type_info);
    gboolean is_ptr = g_type_info_is_pointer(type_info);

    ffi_type *rettype = NULL;
    if (is_ptr)
        return &ffi_type_pointer;
    else
    {
        switch (type_tag)
        {
        case GI_TYPE_TAG_VOID:
            rettype = &ffi_type_void;
            break;
        case GI_TYPE_TAG_BOOLEAN:
            rettype = &ffi_type_sint;
            break;
        case GI_TYPE_TAG_INT8:
            rettype = &ffi_type_sint8;
            break;
        case GI_TYPE_TAG_UINT8:
            rettype = &ffi_type_uint8;
            break;
        case GI_TYPE_TAG_INT16:
            rettype = &ffi_type_sint16;
            break;
        case GI_TYPE_TAG_UINT16:
            rettype = &ffi_type_uint16;
            break;
        case GI_TYPE_TAG_INT32:
            rettype = &ffi_type_sint32;
            break;
        case GI_TYPE_TAG_UINT32:
            rettype = &ffi_type_uint32;
            break;
        case GI_TYPE_TAG_INT64:
            rettype = &ffi_type_sint64;
            break;
        case GI_TYPE_TAG_UINT64:
            rettype = &ffi_type_uint64;
            break;
        case GI_TYPE_TAG_FLOAT:
            rettype = &ffi_type_float;
            break;
        case GI_TYPE_TAG_DOUBLE:
            rettype = &ffi_type_double;
            break;
        case GI_TYPE_TAG_GTYPE:
            if (sizeof(GType) == sizeof(guint32))
                rettype = &ffi_type_sint32;
            else
                rettype = &ffi_type_sint64;
            break;
        case GI_TYPE_TAG_UTF8:
        case GI_TYPE_TAG_FILENAME:
        case GI_TYPE_TAG_ARRAY:
            g_critical("Unhandled FFI type in %s: %d", __FILE__, __LINE__);
            g_abort();
            break;
        case GI_TYPE_TAG_INTERFACE:
        {
            GIBaseInfo *base_info = g_type_info_get_interface(type_info);
            GIInfoType base_info_type = g_base_info_get_type(base_info);
            if (base_info_type == GI_INFO_TYPE_ENUM)
                rettype = &ffi_type_sint;
            else if (base_info_type == GI_INFO_TYPE_FLAGS)
                rettype = &ffi_type_uint;
            else
            {
                g_critical("Unhandled FFI type in %s: %d", __FILE__, __LINE__);
                g_abort();
            }
            g_base_info_unref(base_info);
            break;
        }
        case GI_TYPE_TAG_GLIST:
        case GI_TYPE_TAG_GSLIST:
        case GI_TYPE_TAG_GHASH:
        case GI_TYPE_TAG_ERROR:
            g_critical("Unhandled FFI type in %s: %d", __FILE__, __LINE__);
            g_abort();
            break;
        case GI_TYPE_TAG_UNICHAR:
            if (sizeof(gunichar) == sizeof(guint32))
                rettype = &ffi_type_uint32;
            else
            {
                g_critical("Unhandled FFI type in %s: %d", __FILE__, __LINE__);
                g_abort();
            }
            break;
        default:
            g_critical("Unhandled FFI type in %s: %d", __FILE__, __LINE__);
            g_abort();
        }
    }

    return rettype;
}

static SCM
scm_is_registered_callback_p(SCM s_proc)
{
    if (!scm_is_true(scm_procedure_p (s_proc)))
        scm_wrong_type_arg_msg("is-registered-callback?", 0, s_proc, "procedure");

    // Lookup s_func in the callback cache.
    GSList *x = callback_list;
    GirCallback *gcb;    

    while (x != NULL)
    {
        gcb = x->data;
        if (scm_is_eq(gcb->s_func, s_proc))
        {
            return SCM_BOOL_T;
        }
        x = x->next;
    }
    return SCM_BOOL_F;
}

static SCM
scm_get_registered_callback_closure_pointer(SCM s_proc)
{
    if (!scm_is_true(scm_procedure_p (s_proc)))
        scm_wrong_type_arg_msg("get-registered-callback-closure-pointer", 0, s_proc, "procedure");

    // Lookup s_func in the callback cache.
    GSList *x = callback_list;
    GirCallback *gcb;

    // If you use the same scheme procedure for different callbacks,
    // you're just going to get one closure pointer.
    while (x != NULL)
    {
        gcb = x->data;
        if (scm_is_eq(gcb->s_func, s_proc))
            return scm_from_pointer(gcb->callback_ptr, NULL);
        x = x->next;
    }
    return SCM_BOOL_F;
}

void
gir_init_callback(void)
{
    scm_c_define_gsubr("is-registered-callback?", 1, 0, 0,
        scm_is_registered_callback_p);
    scm_c_define_gsubr("get-registered-callback-closure-pointer", 1, 0, 0,
        scm_get_registered_callback_closure_pointer);
}