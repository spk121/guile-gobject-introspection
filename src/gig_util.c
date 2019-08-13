#include <libguile.h>
#include <glib.h>
#include <glib-object.h>
#include <errno.h>
#include "gig_util.h"

/**
 * gig_constant_strip_prefix:
 * @name: the constant name.
 * @strip_prefix: the prefix to strip.
 *
 * Advances the pointer @name by strlen(@strip_prefix) characters.  If
 * the resulting name does not start with a letter or underscore, the
 * @name pointer will be rewound.  This is to ensure that the
 * resulting name is a valid identifier.  Hence the returned string is
 * a pointer into the string @name.
 *
 * Returns: the stripped constant name.
 */
const gchar *
gig_constant_strip_prefix(const gchar *name, const gchar *strip_prefix)
{
    gsize prefix_len, i;

    prefix_len = strlen(strip_prefix);

    /* Check so name starts with strip_prefix, if it doesn't:
     * return the rest of the part which doesn't match
     */
    for (i = 0; i < prefix_len; i++) {
        if (name[i] != strip_prefix[i] && name[i] != '_') {
            return &name[i];
        }
    }

    /* strip off prefix from value name, while keeping it a valid
     * identifier */
    for (i = prefix_len + 1; i > 0; i--) {
        if (g_ascii_isalpha(name[i - 1]) || name[i - 1] == '_') {
            return &name[i - 1];
        }
    }
    return name;
}

gchar *
gig_gname_to_scm_name(const gchar *gname)
{
    g_assert(gname != NULL);
    g_assert(strlen(gname) > 0);

    gsize len = strlen(gname);
    GString *str = g_string_new(NULL);
    gboolean was_lower = FALSE;

    for (gsize i = 0; i < len; i++) {
        if (g_ascii_islower(gname[i])) {
            g_string_append_c(str, gname[i]);
            was_lower = TRUE;
        }
        else if (gname[i] == '_' || gname[i] == '-') {
            g_string_append_c(str, '-');
            was_lower = FALSE;
        }
        else if (gname[i] == '?' || gname[i] == ':' || gname[i] == '%') {
            g_string_append_c(str, gname[i]);
            was_lower = FALSE;
        }
        else if (g_ascii_isdigit(gname[i])) {
            g_string_append_c(str, gname[i]);
            was_lower = FALSE;
        }
        else if (g_ascii_isupper(gname[i])) {
            if (was_lower)
                g_string_append_c(str, '-');
            g_string_append_c(str, g_ascii_tolower(gname[i]));
            was_lower = FALSE;
        }
    }
    return g_string_free(str, FALSE);
}

SCM
scm_c_list_ref(SCM list, gsize k)
{
    return scm_list_ref(list, scm_from_size_t(k));
}

int
scm_is_list(SCM obj)
{
    return scm_is_true(scm_list_p(obj));
}

gpointer
scm_dynwind_or_bust(const gchar *subr, gpointer mem)
{
    if (mem)
        scm_dynwind_free(mem);
    else {
        errno = ENOMEM;
        scm_syserror(subr);
    }
    return mem;
}

static SCM class_ref_proc = SCM_UNDEFINED;
static SCM class_set_proc = SCM_UNDEFINED;
static SCM srfi1_drop_right_proc = SCM_UNDEFINED;
static SCM module_reexport_proc = SCM_UNDEFINED;

SCM
scm_class_ref(SCM cls, SCM slot)
{
    if (SCM_UNBNDP(class_ref_proc))
        class_ref_proc = scm_c_public_ref("oop goops", "class-slot-ref");

    return scm_call_2(class_ref_proc, cls, slot);
}

SCM
scm_class_set_x(SCM cls, SCM slot, SCM val)
{
    if (SCM_UNBNDP(class_set_proc))
        class_set_proc = scm_c_public_ref("oop goops", "class-slot-set!");
    return scm_call_3(class_set_proc, cls, slot, val);
}

SCM
scm_drop_right_1(SCM lst)
{
    if (SCM_UNBNDP(srfi1_drop_right_proc))
        srfi1_drop_right_proc = scm_c_public_ref("srfi srfi-1", "drop-right");
    return scm_call_2(srfi1_drop_right_proc, lst, scm_from_int(1));
}

SCM
scm_c_reexport(const gchar *name, ...)
{
    if (SCM_UNBNDP(module_reexport_proc))
        module_reexport_proc = scm_c_public_ref("guile", "module-re-export!");

    va_list args;
    va_start(args, name);

    SCM current_module = scm_current_module();
    SCM syms = SCM_EOL;

    SCM public = scm_module_public_interface(current_module);
    SCM obarray = SCM_MODULE_OBARRAY(public);

    do {
        SCM sym = scm_from_utf8_symbol(name);

        if (scm_is_false(scm_hashq_get_handle(obarray, sym)))
            syms = scm_cons(scm_from_utf8_symbol(name), syms);

        name = va_arg(args, gchar *);
    } while (name != NULL);

    scm_call_2(module_reexport_proc, current_module, syms);
    va_end(args);

    return SCM_UNSPECIFIED;
}

SCM
scm_printf(SCM port, const gchar *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    gchar *_message = g_strdup_vprintf(fmt, args);
    SCM message = scm_from_utf8_string(_message);
    g_free(_message);
    scm_display(message, port);
}

const gchar *
g_base_info_get_name_safe(GIBaseInfo *info)
{
    GIInfoType type = g_base_info_get_type(info);
    switch (type) {
    case GI_INFO_TYPE_FUNCTION:
    case GI_INFO_TYPE_CALLBACK:
    case GI_INFO_TYPE_STRUCT:
    case GI_INFO_TYPE_BOXED:
    case GI_INFO_TYPE_ENUM:
    case GI_INFO_TYPE_FLAGS:
    case GI_INFO_TYPE_OBJECT:
    case GI_INFO_TYPE_INTERFACE:
    case GI_INFO_TYPE_CONSTANT:
    case GI_INFO_TYPE_UNION:
    case GI_INFO_TYPE_VALUE:
    case GI_INFO_TYPE_SIGNAL:
    case GI_INFO_TYPE_PROPERTY:
    case GI_INFO_TYPE_VFUNC:
    case GI_INFO_TYPE_FIELD:
    case GI_INFO_TYPE_ARG:
    case GI_INFO_TYPE_UNRESOLVED:
        return g_base_info_get_name(info);
        break;
    case GI_INFO_TYPE_TYPE:
    default:
        return "(unnamed)";
        break;
    }
}
