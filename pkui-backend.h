/*
 * (C) 2011 Guido Berhoerster <gber@opensuse.org>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef	__PKUI_BACKEND_H
#define	__PKUI_BACKEND_H

#include <glib-object.h>
#include <packagekit-glib2/packagekit.h>

G_BEGIN_DECLS

#define	PKUI_TYPE_BACKEND		(pkui_backend_get_type())
#define	PKUI_BACKEND(obj)		(G_TYPE_CHECK_INSTANCE_CAST((obj), \
	PKUI_TYPE_BACKEND, PkuiBackend))
#define	PKUI_IS_BACKEND(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
	PKUI_TYPE_BACKEND))
#define	PKUI_BACKEND_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST((klass), \
	PKUI_TYPE_BACKEND, PkuiBackendClass))
#define	PKUI_IS_BACKEND_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), \
	PKUI_TYPE_BACKEND))
#define	PKUI_BACKEND_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), \
	PKUI_TYPE_BACKEND, PkuiBackendClass))

typedef struct _PkuiBackend		PkuiBackend;
typedef struct _PkuiBackendClass	PkuiBackendClass;
typedef struct _PkuiBackendPrivate	PkuiBackendPrivate;

struct _PkuiBackend
{
	GObject			parent_instance;
	PkuiBackendPrivate	*priv;
};

struct _PkuiBackendClass
{
	GObjectClass	parent_class;
};

GType		pkui_backend_get_type(void);
PkuiBackend	*pkui_backend_new(guint startup_delay, guint interval);
guint		pkui_backend_get_updates_normal(PkuiBackend *self);
guint		pkui_backend_get_updates_important(PkuiBackend *self);
void		pkui_backend_set_inhibit_check(PkuiBackend *self,
    gboolean inhibit_check);
gboolean	pkui_backend_get_inhibit_check(PkuiBackend *self);
void		pkui_backend_set_check_interval(PkuiBackend *self,
    guint check_interval);
guint		pkui_backend_get_check_interval(PkuiBackend *self);
guint		pkui_backend_get_startup_interval(PkuiBackend *self);
void		pkui_backend_check_now(PkuiBackend *self);

G_END_DECLS

#endif /* __PKUI_BACKEND_H */
