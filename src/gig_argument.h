#ifndef GIG_GIARGUMENT_H
#define GIG_GIARGUMENT_H

#include <girepository.h>
#include <libguile.h>
#include "gig_arg_map.h"

// *INDENT-OFF*
G_BEGIN_DECLS
// *INDENT-ON*

// Given VAL, a GValue whos type has already been initialized
// and whose boxed containers, such as GArrays, have already been initialized,
// set ARG.
void gig_arg2value(GValue *val, GIArgument *arg, gsize array_len);

// Given VAL, a GValue whose type has already been initialized
// and whose boxed containers, such as GArray, have already been initialized,
// returns a GIArgument containing the contents of VAL.  Also setting
// *array_len, when necessary.
void gig_value2arg(GValue *val, GIArgument *arg, gsize *array_len);

gsize zero_terminated_carray_length(GigTypeMeta *meta, gpointer array);

void gig_init_argument(void);

G_END_DECLS
#endif
