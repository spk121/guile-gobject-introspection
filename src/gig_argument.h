#ifndef GIG_GIARGUMENT_H
#define GIG_GIARGUMENT_H

#include <girepository.h>
#include <libguile.h>
#include "gig_arg_map.h"

// *INDENT-OFF*
G_BEGIN_DECLS
// *INDENT-ON*

#define GIG_ARRAY_SIZE_UNKNOWN ((gsize)-1)

#define S2C_ARG_DECL const gchar *subr, gint argpos,    \
        GigTypeMeta *meta, SCM object,               \
        GPtrArray *must_free, GIArgument *arg, gsize *size
#define S2C_ARGS subr, argpos, meta, object, must_free, arg, size

#define C2S_ARG_DECL const gchar *subr, gint argpos,    \
        GigTypeMeta *meta, GIArgument *arg,       \
        SCM *object, gsize size
#define C2S_ARGS subr, argpos, meta, arg, object, size

void gig_argument_scm_to_c(S2C_ARG_DECL);
void gig_argument_c_to_scm(C2S_ARG_DECL);
char *gig_argument_describe_arg(GIArgInfo *arg_info);
char *gig_argument_describe_return(GITypeInfo *type_info, GITransfer transfer, gboolean null_ok,
                                   gboolean skip);
void gig_argument_preallocate_output_arg_and_object(GIArgInfo *arg_info, GIArgument *arg,
                                                    SCM *obj);

void gig_init_argument(void);

G_END_DECLS
#endif
