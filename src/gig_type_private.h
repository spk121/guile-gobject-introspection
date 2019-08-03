#ifndef _GIG_TYPE_PRIVATE_H_
#define _GIG_TYPE_PRIVATE_H_

#include <glib.h>
#include <glib-object.h>
#include <ffi.h>
#include <libguile.h>

typedef struct _GigBoxedFuncs
{
    ffi_type *atypes[1];

    ffi_closure *copy_closure;
    ffi_cif copy_cif;
    void *copy;

    ffi_closure *free_closure;
    ffi_cif free_cif;
    void *free;
} GigBoxedFuncs;

GigBoxedFuncs *_boxed_funcs_for_type(GType type);
void _free_boxed_funcs();

SCM make_class_proc;
SCM kwd_name;
SCM sym_obarray;

#endif
