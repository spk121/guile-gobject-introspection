#ifndef GIG_VALUE_H_
#define GIG_VALUE_H_

#include <glib.h>
#include <libguile.h>
#include <glib-object.h>
#include <girepository.h>
#include "gig_data_type.h"
#include "gig_arg_map.h"

// *INDENT-OFF*
G_BEGIN_DECLS
// *INDENT-ON*

typedef enum _GigValueReturn
{
    GIG_VALUE_OK,
    GIG_VALUE_INVALID,
    GIG_VALUE_VOID,
    GIG_VALUE_UNIMPLEMENTED,
    GIG_VALUE_OUT_OF_RANGE,
    GIG_VALUE_WRONG_TYPE
} GigValueReturn;


void gig_value_preset_type(GigArgMapEntry *entry, GValue *val);
GigValueReturn gig_value_from_scm(GValue *value, SCM obj);
SCM gig_value_param_as_scm(const GValue *gvalue, gboolean copy_boxed, const GParamSpec *pspec);
void gig_value_from_scm_with_error(GValue *value, SCM obj, const gchar *subr, gint pos);
SCM gig_value_as_scm(const GValue *value, gboolean copy_boxed);

GigValueReturn gig_scm_to_value_full(SCM src, const GigTypeMeta *meta, GValue *dest);
GigValueReturn gig_value_to_scm_full(const GValue *src, const GigTypeMeta *meta, SCM *dest);
void gig_scm_to_value_full_with_error(SCM src, const GigTypeMeta *meta, GValue *dest,
                                      const gchar *subr, gint pos);
void gig_value_to_scm_full_with_error(const GValue *src, const GigTypeMeta *meta, SCM *dest,
                                      const gchar *subr);



#if 0

void gig_value_from_scm_with_error(GValue *dest, SCM src, const gchar *filename, gint pos);
GigValueReturn gig_value_from_scm_full(GValue *dest, SCM src, GigTypeMeta *meta,
                                       GSList ** free_list);
void gig_value_from_scm_full_with_error(GValue *dest, SCM src, GigTypeMeta *meta,
                                        GSList ** free_list, const gchar *funcname, gint pos);

SCM gig_value_as_scm(const GValue *value, gboolean copy_boxed);
void gig_value_param_to_scm(const GValue *src, SCM *dest, gboolean copy_boxed,
                            const GParamSpec *pspec);
void gig_value_to_scm_full(const GValue *src, SCM *dest, GigTypeMeta *meta);

void gig_init_value(void);



SCM gig_value_c2g(GValue *val);
SCM gig_value_to_scm_basic_type(const GValue *value, GType fundamental, gboolean *handled);
SCM gig_value_param_as_scm(const GValue *gvalue, gboolean copy_boxed, const GParamSpec *pspec);

SCM gig_value_as_scm(const GValue *value, gboolean copy_boxed);
GigValueReturn gig_value_from_scm(GValue *value, SCM obj);

GigValueReturn gig_scm_to_value(SCM src, GigTypeMeta *meta, GValue *dest);
void
gig_scm_to_value_with_error(SCM src, GigTypeMeta *meta, GValue *dest, const gchar *subr, gint pos);
#endif

void gig_init_value(void);

G_END_DECLS
#endif
