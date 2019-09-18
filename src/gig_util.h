#ifndef GIG_UTIL_H
#define GIG_UTIL_H

#include <glib.h>
#include <libguile.h>
#include <girepository.h>

// *INDENT-OFF*
G_BEGIN_DECLS
// *INDENT-ON*

G_GNUC_MALLOC gchar *gig_callable_info_make_name(GICallableInfo *info, const gchar *prefix);
const gchar *gig_constant_strip_prefix(const gchar *name, const gchar *strip_prefix);
gchar *gig_gname_to_scm_name(const gchar *gname);
SCM scm_c_list_ref(SCM list, gsize k);
gboolean scm_is_list(SCM obj);
gpointer scm_dynwind_or_bust(const gchar *subr, gpointer mem);
SCM scm_class_ref(SCM cls, SCM slot);
SCM scm_class_set_x(SCM cls, SCM slot, SCM val);
SCM scm_drop_right_1(SCM lst);
SCM scm_c_reexport(const gchar *name, ...);
void scm_printf(SCM port, const gchar *fmt, ...);
const gchar *g_base_info_get_name_safe(GIBaseInfo *info);
gchar *g_registered_type_info_get_qualified_name(GIRegisteredTypeInfo *info);

#define scm_is_equal(a,b) scm_is_true(scm_equal_p(a,b))

#define SCM_UNBND_TO_BOOL_F(obj) \
    do {                         \
        if (SCM_UNBNDP (obj))    \
            obj = SCM_BOOL_F;    \
    } while (0)                  \

G_END_DECLS
#endif
#define gig_debug_transfer(...)                                         \
    do {                                                                \
        g_log_structured(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,               \
                         "CODE_FILE", __FILE__,                         \
                         "CODE_LINE", __LINE__,                         \
                         "CODE_FUNC", __func__,                         \
                         "GIG_DOMAIN", "transfers",                     \
                         "MESSAGE", __VA_ARGS__);                       \
    } while (FALSE)
