#ifndef _GIG_OBJECT_H_
#define _GIG_OBJECT_H_

#include <libguile.h>
#include <glib.h>
#include <glib-object.h>
#include <girepository.h>

void gig_init_object();
SCM gig_property_define(GType type, GIPropertyInfo *info, const gchar *namespace, SCM defs);

#endif
