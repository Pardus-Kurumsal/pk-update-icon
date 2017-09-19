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

#ifndef	__PKUI_ICON_H
#define	__PKUI_ICON_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define	PKUI_TYPE_ICON			(pkui_icon_get_type())
#define	PKUI_ICON(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), \
	PKUI_TYPE_ICON, PkuiIcon))
#define	PKUI_IS_ICON(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), \
	PKUI_TYPE_ICON))
#define	PKUI_ICON_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), \
	PKUI_TYPE_ICON, PkuiIconClass))
#define	PKUI_IS_ICON_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), \
	PKUI_TYPE_ICON))
#define	PKUI_ICON_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS((obj), \
	PKUI_TYPE_ICON, PkuiIconClass))

typedef struct _PkuiIcon	PkuiIcon;
typedef struct _PkuiIconClass	PkuiIconClass;
typedef struct _PkuiIconPrivate	PkuiIconPrivate;

struct _PkuiIcon
{
	GObject		parent_instance;
	PkuiIconPrivate	*priv;
};

struct _PkuiIconClass
{
	GObjectClass	parent_class;
};

GType		pkui_icon_get_type(void);
PkuiIcon	*pkui_icon_new(guint, guint, const gchar *);

G_END_DECLS

#endif /* __PKUI_ICON_H */
