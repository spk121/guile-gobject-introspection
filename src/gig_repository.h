#ifndef GIG_REPOSITORY_H
#define GIG_REPOSITORY_H

#include <glib.h>
#include <girepository.h>

typedef GIBaseInfo *(*GigRepositoryNested)(GIBaseInfo *info, gint n);

/**
 * gig_repository_nested_infos:
 * @base: the info to look up nested infos in
 * @n_methods: (out): the number of methods in base
 * @n_properties: (out): the number of properties in base
 * @n_signals: (out): the number of signals in base
 * @n_fields: (out): the number of fields in base
 * @method: (out) (nullable): function by which methods are retrieved
 * @property: (out) (nullable): function by which properties are retrieved
 * @signal: (out) (nullable): function by which signals are retrieved
 * @field: (out) (nullable): function by which field are retrieved
 * Fetches all information of nested infos in BASE.
 */
void gig_repository_nested_infos(GIBaseInfo *base,
                                 gint *n_methods,
                                 GigRepositoryNested *method,
                                 gint *n_properties,
                                 GigRepositoryNested *property,
                                 gint *n_signals,
                                 GigRepositoryNested *signal,
                                 gint *n_fields, GigRepositoryNested *field);


#endif
