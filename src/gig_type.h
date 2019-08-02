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

#ifndef GIG_TYPE_H
#define GIG_TYPE_H

#include <girepository.h>

// *INDENT-OFF*
G_BEGIN_DECLS
// *INDENT-ON*

SCM gig_enum_type;
SCM gig_flags_type;
SCM gig_object_type;
SCM gig_interface_type;
SCM gig_paramspec_type;

G_GNUC_MALLOC gchar *gig_type_document_type_from_gtype(GType gtype);
G_GNUC_MALLOC gchar *gig_type_class_name_from_gtype(GType gtype);
void gig_type_register(GType gtype);
SCM gig_type_define(GType gtype, SCM defs);
GType gig_type_get_gtype_from_obj(SCM x);
SCM gig_type_get_scheme_type(GType gtype);
SCM gig_type_transfer_object(GType gtype, gpointer obj, GITransfer transfer);
gpointer gig_type_peek_object(SCM obj);
GType scm_to_gtype(SCM x);
GType gig_type_get_gtype_from_obj(SCM x);
SCM gig_type_get_scheme_type(GType gtype);
SCM gig_type_transfer_object(GType gtype, gpointer obj, GITransfer transfer);
gpointer gig_type_peek_object(SCM obj);
gpointer gig_type_peek_typed_object(SCM obj, SCM expected);
void gig_init_types(void);

G_END_DECLS
#endif
