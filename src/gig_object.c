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

#include "gig_object.h"
#include "gig_type.h"
#include "gig_util.h"
#include "gig_signal.h"
#include "gig_value.h"
#include "gig_function_private.h"

GQuark gig_user_object_properties;

typedef struct _GigUserObjectInitInfo
{
    GType type;
    GPtrArray *properties;
    GPtrArray *signals;
} GigUserObjectInitInfo;

static SCM
gig_object_take(GObject *object)
{
    return gig_type_transfer_object(G_OBJECT_TYPE(object), object, GI_TRANSFER_EVERYTHING);
}


static SCM
gig_object_ref(GObject *object)
{
    return gig_type_transfer_object(G_OBJECT_TYPE(object), object, GI_TRANSFER_NOTHING);
}

static GObject *
gig_object_peek(SCM object)
{
    return gig_type_peek_typed_object(object, gig_object_type);
}

static GParamSpec *
gig_paramspec_peek(SCM object)
{
    return gig_type_peek_typed_object(object, gig_paramspec_type);
}

static SCM
gig_i_scm_make_gobject(SCM s_gtype, SCM s_prop_keylist)
{
#define FUNC "%make-gobject"
    GType type;
    GObject *obj;
    GObjectClass *class;
    gsize n_prop;
    const gchar **keys;
    GValue *values;

    type = scm_to_gtype(s_gtype);

    SCM_ASSERT_TYPE(G_TYPE_IS_CLASSED(type), s_gtype, SCM_ARG1, FUNC,
                    "typeid derived from G_TYPE_OBJECT or scheme type derived from <GObject>");

    if (scm_is_false(gig_type_get_scheme_type(type)))
        scm_misc_error(FUNC, "type ~S lacks introspection", scm_list_1(s_gtype));

    scm_dynwind_begin(0);

    if (scm_is_true(scm_list_p(s_prop_keylist))) {
        class = g_type_class_ref(type);
        scm_dynwind_unwind_handler(g_type_class_unref, class, SCM_F_WIND_EXPLICITLY);

        n_prop = scm_to_size_t(scm_length(s_prop_keylist)) / 2;
        keys = scm_dynwind_or_bust(FUNC, calloc(n_prop, sizeof(char *)));
        values = scm_dynwind_or_bust(FUNC, calloc(n_prop, sizeof(GValue)));

        SCM iter = s_prop_keylist;
        for (gsize i = 0; i < n_prop; i++, iter = scm_cddr(iter)) {
            SCM key = scm_car(iter);
            SCM s_value = scm_cadr(iter);

            SCM_ASSERT_TYPE(scm_is_true(scm_keyword_p(key)), key, SCM_ARGn, FUNC, "keyword");

            key = scm_symbol_to_string(scm_keyword_to_symbol(key));

            keys[i] = scm_dynwind_or_bust(FUNC, scm_to_utf8_string(key));
            GParamSpec *pspec = g_object_class_find_property(class, keys[i]);
            if (!pspec) {
                scm_misc_error(FUNC, "unknown object parameter ~S", scm_list_1(key));
            }
            else {
                GValue *value = &values[i];
                if (G_IS_PARAM_SPEC_CHAR(pspec)) {
                    g_value_init(value, G_TYPE_CHAR);
                    g_value_set_schar(value, scm_to_int8(s_value));
                }
                else if (G_IS_PARAM_SPEC_UCHAR(pspec)) {
                    g_value_init(value, G_TYPE_UCHAR);
                    g_value_set_uchar(value, scm_to_uint8(s_value));
                }
                else if (G_IS_PARAM_SPEC_INT(pspec)) {
                    g_value_init(value, G_TYPE_INT);
                    g_value_set_int(value, scm_to_int(s_value));
                }
                else if (G_IS_PARAM_SPEC_UINT(pspec)) {
                    g_value_init(value, G_TYPE_UINT);
                    g_value_set_uint(value, scm_to_uint(s_value));
                }
                else if (G_IS_PARAM_SPEC_LONG(pspec)) {
                    g_value_init(value, G_TYPE_LONG);
                    g_value_set_uint(value, scm_to_long(s_value));
                }
                else if (G_IS_PARAM_SPEC_ULONG(pspec)) {
                    g_value_init(value, G_TYPE_ULONG);
                    g_value_set_ulong(value, scm_to_ulong(s_value));
                }
                else if (G_IS_PARAM_SPEC_INT64(pspec)) {
                    g_value_init(value, G_TYPE_INT64);
                    g_value_set_int64(value, scm_to_int64(s_value));
                }
                else if (G_IS_PARAM_SPEC_UINT64(pspec)) {
                    g_value_init(value, G_TYPE_UINT64);
                    g_value_set_uint64(value, scm_to_uint64(s_value));
                }
                else if (G_IS_PARAM_SPEC_FLOAT(pspec)) {
                    g_value_init(value, G_TYPE_FLOAT);
                    g_value_set_float(value, scm_to_double(s_value));
                }
                else if (G_IS_PARAM_SPEC_DOUBLE(pspec)) {
                    g_value_init(value, G_TYPE_DOUBLE);
                    g_value_set_double(value, scm_to_double(s_value));
                }
                else if (G_IS_PARAM_SPEC_ENUM(pspec)) {
                    g_value_init(value, G_PARAM_SPEC_VALUE_TYPE(pspec));
                    g_value_set_enum(value, scm_to_uint64(s_value));
                }
                else if (G_IS_PARAM_SPEC_FLAGS(pspec)) {
                    g_value_init(value, G_PARAM_SPEC_VALUE_TYPE(pspec));
                    g_value_set_flags(value, scm_to_ulong(s_value));
                }
                else if (G_IS_PARAM_SPEC_STRING(pspec)) {
                    g_value_init(value, G_TYPE_STRING);
                    g_value_set_string(value, scm_to_utf8_string(s_value));
                }
                else if (G_IS_PARAM_SPEC_OBJECT(pspec)) {
                    GType src_type = gig_type_get_gtype_from_obj(s_value);
                    GType dest_type = G_PARAM_SPEC_VALUE_TYPE(pspec);
                    if (g_type_is_a(src_type, dest_type)) {
                        g_value_init(value, dest_type);
                        g_value_set_object(value, gig_object_peek(s_value));
                    }
                    else
                        scm_misc_error(FUNC,
                                       "unable to convert parameter ~S of type ~S into a ~S",
                                       scm_list_3(s_value,
                                                  scm_from_utf8_string(g_type_name(src_type)),
                                                  scm_from_utf8_string(g_type_name(dest_type))));
                }
                else
                    scm_misc_error(FUNC, "unable to convert parameter ~S", scm_list_1(s_value));
            }
        }
    }
    else {
        n_prop = 0;
        keys = NULL;
        values = NULL;
    }

    obj = g_object_new_with_properties(type, n_prop, keys, values);
    scm_dynwind_end();

    g_assert(obj);

    return gig_object_take(obj);
#undef FUNC
}

static GParamSpec *
get_paramspec(const GObject *self, const gchar *prop)
{
    GObjectClass *oclass = G_OBJECT_GET_CLASS(self);
    return g_object_class_find_property(oclass, prop);
}

static SCM
gig_i_scm_get_pspec(SCM self, SCM prop)
{
    GObject *obj;
    gchar *name;
    GParamSpec *pspec;

    SCM_ASSERT(SCM_IS_A_P(self, gig_object_type), self, SCM_ARG1, "%get-pspec");

    scm_dynwind_begin(0);
    obj = gig_object_peek(self);
    name = scm_dynwind_or_bust("%get-pspec", scm_to_utf8_string(prop));
    pspec = get_paramspec(obj, name);
    if (!pspec) {
        scm_misc_error("%get-pspec",
                       "object of type ~A does not have a property ~S",
                       scm_list_2(SCM_CLASS_OF(self), prop));
    }
    scm_dynwind_end();

    return gig_type_transfer_object(G_PARAM_SPEC_TYPE(pspec), pspec, GI_TRANSFER_NOTHING);
}

static SCM
gig_i_scm_get_property(SCM self, SCM property)
{
    SCM ret;

    GParamSpec *pspec;
    GObject *obj;

    GValue value = { 0, };

    SCM_ASSERT(SCM_IS_A_P(self, gig_object_type), self, SCM_ARG1, "%get-property");
    SCM_ASSERT(SCM_IS_A_P(property, gig_paramspec_type), property, SCM_ARG2, "%get-property");
    obj = gig_object_peek(self);
    pspec = gig_paramspec_peek(property);
    if (!pspec || pspec != get_paramspec(obj, pspec->name)) {
        scm_misc_error("%get-property",
                       "object of type ~A does not have a property ~A",
                       scm_list_2(SCM_CLASS_OF(self), property));
    }
    if (!(pspec->flags & G_PARAM_READABLE)) {
        scm_misc_error("%get-property", "property ~A is not readable", scm_list_1(property));
    }

    g_value_init(&value, G_PARAM_SPEC_VALUE_TYPE(pspec));
    g_object_get_property(obj, pspec->name, &value);
    ret = gig_value_param_as_scm(&value, TRUE, pspec);
    g_value_unset(&value);

    return ret;
}

static SCM
gig_i_scm_set_property_x(SCM self, SCM property, SCM svalue)
{
    GParamSpec *pspec;
    GObject *obj;

    GValue value = { 0, };

    SCM_ASSERT(SCM_IS_A_P(self, gig_object_type), self, SCM_ARG1, "%set-property!");
    SCM_ASSERT(SCM_IS_A_P(property, gig_paramspec_type), property, SCM_ARG2, "%set-property");

    obj = gig_object_peek(self);
    pspec = gig_paramspec_peek(property);
    if (!pspec || pspec != get_paramspec(obj, pspec->name)) {
        scm_misc_error("%set-property!",
                       "object of type ~A does not have a property ~A",
                       scm_list_2(SCM_CLASS_OF(self), property));
    }
    if (!(pspec->flags & G_PARAM_WRITABLE)) {
        scm_misc_error("%set-property!", "property ~A is not writable", scm_list_1(property));
    }

    g_value_init(&value, G_PARAM_SPEC_VALUE_TYPE(pspec));
    gig_value_from_scm_with_error("%set-property!", &value, svalue, SCM_ARG3);
    g_object_set_property(obj, pspec->name, &value);

    return SCM_UNSPECIFIED;
}

static void
make_new_signal(GigSignalSpec *signal_spec, gpointer user_data)
{
    GType instance_type = GPOINTER_TO_SIZE(user_data);
    g_signal_newv(signal_spec->signal_name, instance_type, signal_spec->signal_flags, NULL,     /* closure */
                  signal_spec->accumulator,
                  signal_spec->accu_data,
                  NULL, signal_spec->return_type, signal_spec->n_params, signal_spec->param_types);
}

static void
gig_user_object_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    GValue *properties = g_object_get_qdata(object, gig_user_object_properties);
    GValue *property = properties + property_id - 1;
    g_value_copy(property, value);
}

static void
gig_user_object_set_property(GObject *object, guint property_id,
                             const GValue *value, GParamSpec *pspec)
{
    GValue *properties = g_object_get_qdata(object, gig_user_object_properties);
    GValue *property = properties + property_id - 1;
    g_param_value_convert(pspec, value, property, FALSE);
}

static void
gig_user_object_dispose(GObject *object)
{
    GType type, parent_type;
    gpointer _parent_class;
    GObjectClass *parent_class;

    type = G_OBJECT_TYPE(object);
    parent_type = g_type_parent(type);

    g_assert(G_TYPE_IS_CLASSED(type));
    g_assert(G_TYPE_IS_CLASSED(parent_type));

    g_info("dispose is currently just calling the parent's dispose");

    g_debug("disposing parent type");
    _parent_class = g_type_class_ref(parent_type);
    parent_class = G_OBJECT_CLASS(_parent_class);

    parent_class->dispose(object);

    g_type_class_unref(_parent_class);
}

static void
gig_user_object_finalize(GObject *object)
{
}

static void
gig_user_class_init(GObjectClass *class, gpointer class_info)
{
    GType type = G_TYPE_FROM_CLASS(class);
    GigUserObjectInitInfo *init_info = class_info;
    gsize n_properties = init_info->properties->len;
    GParamSpec **properties = (GParamSpec **)init_info->properties->pdata;

    class->set_property = gig_user_object_set_property;
    class->get_property = gig_user_object_get_property;
    class->dispose = gig_user_object_dispose;
    class->finalize = gig_user_object_finalize;

    /* Since the parent type could be anything, some pointer math is
     * required to figure out where our part of the object class is
     * located. */
    g_ptr_array_foreach(init_info->signals, (GFunc) make_new_signal, GSIZE_TO_POINTER(type));

    for (gsize i = 1; i <= n_properties; i++)
        g_object_class_install_property(class, i, properties[i - 1]);
}

static void
gig_user_object_init(GTypeInstance *instance, gpointer class_ptr)
{
    GType type = G_TYPE_FROM_CLASS(class_ptr);
    GType parent_type = g_type_parent(type);
    guint n_properties;
    GParamSpec **properties;
    GTypeQuery query;

    g_type_query(parent_type, &query);
    properties = g_object_class_list_properties(class_ptr, &n_properties);

    GValue *instance_properties = g_new0(GValue, n_properties);
    g_object_set_qdata(G_OBJECT(instance), gig_user_object_properties, instance_properties);

    for (guint i = 0; i < n_properties; i++) {
        const GValue *_default;
        _default = g_param_spec_get_default_value(properties[i]);
        g_value_init(instance_properties + i, properties[i]->value_type);
        g_value_copy(_default, instance_properties + i);
    }
}

static GType
gig_user_object_define(const gchar *type_name,
                       GType parent_type, GPtrArray *properties, GPtrArray *signals)
{
    GTypeInfo type_info;
    GigUserObjectInitInfo *class_init_info;
    GTypeQuery query;
    GType new_type;

    memset(&type_info, 0, sizeof(type_info));

    /* This data will needed when the class is dynamically instantiated. */
    class_init_info = g_new0(GigUserObjectInitInfo, 1);
    class_init_info->properties = properties;
    class_init_info->signals = signals;

    type_info.class_data = class_init_info;

    /* Register it. */
    g_type_query(parent_type, &query);
    type_info.class_size = query.class_size;
    type_info.instance_size = query.instance_size;
    type_info.class_init = (GClassInitFunc) gig_user_class_init;
    type_info.instance_init = gig_user_object_init;
    new_type = g_type_register_static(parent_type, type_name, &type_info, 0);
    class_init_info->type = new_type;

    return new_type;
}

static SCM
gig_i_scm_define_type(SCM s_type_name, SCM s_parent_type, SCM s_properties, SCM s_signals)
{
    gchar *type_name;
    GType parent_type;
    GType new_type;
    gsize n_properties, n_signals;
    GPtrArray *properties;
    GPtrArray *signals;

    SCM_ASSERT(scm_is_string(s_type_name), s_type_name, SCM_ARG1, "%define-type");
    SCM_ASSERT(SCM_SUBCLASSP(s_parent_type, gig_object_type), s_parent_type, SCM_ARG2,
               "%define-type");

    type_name = scm_to_utf8_string(s_type_name);

    parent_type = scm_to_gtype(s_parent_type);

    if (scm_is_false(gig_type_get_scheme_type(parent_type)))
        scm_misc_error("%define-type", "type ~S is dupe", scm_list_1(s_parent_type));

    SCM_UNBND_TO_BOOL_F(s_properties);
    SCM_UNBND_TO_BOOL_F(s_signals);

    SCM_ASSERT_TYPE(scm_is_false(s_properties) ||
                    scm_is_list(s_properties),
                    s_properties, SCM_ARG3, "%define-type", "list of param specs or #f");

    SCM_ASSERT_TYPE(scm_is_false(s_signals) ||
                    scm_is_list(s_signals),
                    s_signals, SCM_ARG4, "%define-type", "list of signal specs or #f");

    properties = g_ptr_array_new();
    signals = g_ptr_array_new_with_free_func((GDestroyNotify)gig_free_signalspec);

    if (scm_is_list(s_properties)) {
        n_properties = scm_to_size_t(scm_length(s_properties));
        SCM iter = s_properties;
        for (gsize i = 0; i < n_properties; i++) {
            GParamSpec *pspec = gig_paramspec_peek(scm_car(iter));
            g_ptr_array_add(properties, pspec);
            iter = scm_cdr(iter);
        }
    }

    if (scm_is_list(s_signals)) {
        n_signals = scm_to_size_t(scm_length(s_signals));
        for (gsize i = 0; i < n_signals; i++) {
            GigSignalSpec *sspec;
            sspec = gig_signalspec_from_obj(scm_list_ref(s_signals, scm_from_size_t(i)));
            g_ptr_array_add(signals, sspec);
        }
    }

    new_type = gig_user_object_define(type_name, parent_type, properties, signals);

    gig_type_define(new_type, SCM_UNDEFINED);
    return gig_type_get_scheme_type(new_type);
}

void
signal_lookup(char *proc, GObject *self,
              SCM signal, SCM detail, guint *c_signal, GSignalQuery *query_info, GQuark *c_detail)
{
    SCM s_name = gig_signal_ref(signal, GIG_SIGNAL_SLOT_NAME);
    gchar *name = scm_to_utf8_string(s_name);

    *c_signal = g_signal_lookup(name, G_OBJECT_TYPE(self));
    g_free(name);

    if (c_signal == 0)
        scm_misc_error(proc, "~A: unknown signal name ~A",
                       scm_list_2(gig_object_ref(self), s_name));

    g_signal_query(*c_signal, query_info);

    if ((query_info->signal_flags & G_SIGNAL_DETAILED) && scm_is_symbol(detail)) {
        SCM detail_str = scm_symbol_to_string(detail);
        gchar *_detail = scm_to_utf8_string(detail_str);
        *c_detail = g_quark_from_string(_detail);
        g_free(_detail);
    }
    else
        *c_detail = 0;
}

static SCM
gig_i_scm_connect(SCM self, SCM signal, SCM sdetail, SCM callback, SCM s_after, SCM swap_data)
{
    GObject *obj;
    gboolean after;
    GClosure *closure;
    gulong handlerid;
    GSignalQuery query_info;
    guint sigid;
    GQuark detail;

    SCM_ASSERT(SCM_IS_A_P(self, gig_object_type), self, SCM_ARG1, "%connect");
    SCM_ASSERT(SCM_IS_A_P(signal, gig_signal_type), signal, SCM_ARG2, "%connect");

    obj = gig_object_peek(self);

    signal_lookup("%connect", obj, signal, sdetail, &sigid, &query_info, &detail);

    after = !SCM_UNBNDP(s_after) && scm_to_bool(s_after);
    closure = gig_signal_closure_new(self, query_info.itype, query_info.signal_name, callback);

    handlerid = g_signal_connect_closure_by_id(obj, sigid, detail, closure, after);

    return scm_from_ulong(handlerid);
}

static SCM
gig_i_scm_emit(SCM self, SCM signal, SCM s_detail, SCM args)
{
    GObject *obj;
    GValue *values, retval = G_VALUE_INIT;
    GSignalQuery query_info;
    guint sigid;
    GQuark detail;

    SCM_ASSERT(SCM_IS_A_P(self, gig_object_type), self, SCM_ARG1, "%emit");
    SCM_ASSERT(SCM_IS_A_P(signal, gig_signal_type), signal, SCM_ARG2, "%emit");

    obj = gig_object_peek(self);

    signal_lookup("%emit", obj, signal, s_detail, &sigid, &query_info, &detail);

    if (SCM_UNBNDP(args))
        args = SCM_EOL;
    if (!(query_info.signal_flags & G_SIGNAL_DETAILED || SCM_UNBNDP(s_detail)))
        args = scm_cons(s_detail, args);

    if (scm_to_uint(scm_length(args)) != query_info.n_params)
        scm_misc_error("%emit", "~A: signal ~A has ~d params, but ~d were supplied",
                       scm_list_4(self, signal, scm_from_uint32(query_info.n_params),
                                  scm_length(args)));

    values = g_new0(GValue, query_info.n_params + 1);
    g_value_init(values, G_OBJECT_TYPE(obj));
    gig_value_from_scm_with_error("%emit", values, self, SCM_ARG1);
    SCM iter = args;
    for (guint i = 0; i < query_info.n_params; i++, iter = scm_cdr(iter)) {
        g_value_init(values + i + 1, query_info.param_types[i]);
        gig_value_from_scm_with_error("%emit", values + i + 1, iter, SCM_ARGn);
    }

    if (query_info.return_type != G_TYPE_NONE)
        g_value_init(&retval, query_info.return_type);
    g_signal_emitv(values, sigid, detail, &retval);

    if (query_info.return_type != G_TYPE_NONE)
        return gig_value_as_scm(&retval, FALSE);
    else
        return SCM_UNSPECIFIED;
}

static SCM sym_value;
static SCM ensure_accessor_proc;

static SCM do_define_property(const gchar *, SCM, SCM, SCM);

SCM
gig_property_define(GType type, GIPropertyInfo *info, const gchar *namespace, SCM defs)
{
    SCM formals, specializers;
    GObjectClass *class;
    GParamSpec *prop;
    SCM s_prop, def;

    const gchar *name = g_base_info_get_name(info);
    gchar *long_name;

    SCM self_type = gig_type_get_scheme_type(type);

    scm_dynwind_begin(0);

    class = g_type_class_ref(type);
    scm_dynwind_unwind_handler(g_type_class_unref, class, SCM_F_WIND_EXPLICITLY);
    long_name = scm_dynwind_or_bust("%gig-property-define",
                                    g_strdup_printf("%s:%s", namespace, name));
    long_name = scm_dynwind_or_bust("%gig-property-define", gig_gname_to_scm_name(long_name));

    prop = g_object_class_find_property(class, name);
    g_assert(prop != NULL);

    s_prop = gig_type_transfer_object(G_PARAM_SPEC_TYPE(prop), prop, GI_TRANSFER_NOTHING);

    def = do_define_property(long_name, s_prop, self_type, top_type);
    if (!SCM_UNBNDP(def))
        defs = scm_cons(def, defs);
    g_debug("dynamically bound %s to property %s of %s", long_name, name, g_type_name(type));
    def = do_define_property(name, s_prop, self_type, top_type);
    if (!SCM_UNBNDP(def))
        defs = scm_cons(def, defs);
    g_debug("dynamically bound %s to property %s of %s", name, name, g_type_name(type));

    scm_dynwind_end();
    return defs;
}

static SCM
do_define_property(const gchar *public_name, SCM prop, SCM self_type, SCM value_type)
{
    g_return_val_if_fail(public_name != NULL, SCM_UNDEFINED);

    SCM sym_public_name, formals, specializers, generic, proc, setter;

    sym_public_name = scm_from_utf8_symbol(public_name);
    generic = scm_call_2(ensure_accessor_proc, default_definition(sym_public_name),
                         sym_public_name);

    // getter
    proc = scm_procedure(prop);
    formals = scm_list_1(sym_self);
    specializers = scm_list_1(self_type);

    scm_call_2(add_method_proc, generic,
               scm_call_7(make_proc, method_type,
                          kwd_specializers, specializers,
                          kwd_formals, formals, kwd_procedure, proc));

    // setter
    setter = scm_setter(prop);
    formals = scm_list_2(sym_self, sym_value);
    specializers = scm_list_2(self_type, value_type);

    scm_call_2(add_method_proc, scm_setter(generic),
               scm_call_7(make_proc, method_type,
                          kwd_specializers, specializers,
                          kwd_formals, formals, kwd_procedure, setter));

    scm_define(sym_public_name, generic);
    return sym_public_name;
}

void
gig_init_object()
{
    gig_user_object_properties = g_quark_from_static_string("GigObject::properties");

    sym_value = scm_from_utf8_symbol("value");

    ensure_accessor_proc = scm_c_public_ref("oop goops", "ensure-accessor");

    scm_c_define_gsubr("%make-gobject", 1, 1, 0, gig_i_scm_make_gobject);
    scm_c_define_gsubr("%object-get-pspec", 2, 0, 0, gig_i_scm_get_pspec);
    scm_c_define_gsubr("%get-property", 2, 0, 0, gig_i_scm_get_property);
    scm_c_define_gsubr("%set-property!", 3, 0, 0, gig_i_scm_set_property_x);
    scm_c_define_gsubr("%connect", 4, 2, 0, gig_i_scm_connect);
    scm_c_define_gsubr("%emit", 2, 1, 1, gig_i_scm_emit);
    scm_c_define_gsubr("%define-object-type", 2, 2, 0, gig_i_scm_define_type);
}
