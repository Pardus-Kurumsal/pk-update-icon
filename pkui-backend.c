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

#include <packagekit-glib2/packagekit.h>
#include "pkui-backend.h"

G_DEFINE_TYPE(PkuiBackend, pkui_backend, G_TYPE_OBJECT)

#define	PKUI_BACKEND_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), \
	PKUI_TYPE_BACKEND, PkuiBackendPrivate))

struct _PkuiBackendPrivate
{
	PkuiBackend	*backend;
	PkClient	*pk_client;
	guint		periodic_check_id;
	guint		updates_normal;
	guint		updates_important;
	guint		previous_updates_normal;
	guint		previous_updates_important;
	guint		startup_delay;
	guint		check_interval;
	gint64		last_check;
	gboolean	inhibit_check;
};

enum
{
	PROP_0,

	PROP_UPDATES_NORMAL,
	PROP_UPDATES_IMPORTANT,
	PROP_STARTUP_DELAY,
	PROP_CHECK_INTERVAL,
	PROP_INHIBIT_CHECK
};

enum
{
	STATE_CHANGED_SIGNAL,

	LAST_SIGNAL
};

static guint	pkui_backend_signals[LAST_SIGNAL] = { 0 };

static gboolean	periodic_check(gpointer data);

static void
pkui_backend_set_property(GObject *object, guint property_id,
    const GValue *value, GParamSpec *pspec)
{
	PkuiBackend	*self = PKUI_BACKEND(object);
	gboolean	inhibit_check;
	gint64		time_to_check;

	switch (property_id) {
	case PROP_STARTUP_DELAY:
		self->priv->startup_delay = g_value_get_uint(value);

		if (self->priv->periodic_check_id != 0) {
			g_source_remove(self->priv->periodic_check_id);
		}
		self->priv->periodic_check_id =
		    g_timeout_add_seconds(self->priv->startup_delay,
		    (GSourceFunc)periodic_check, self);
		break;
	case PROP_CHECK_INTERVAL:
		self->priv->check_interval = g_value_get_uint(value);

		/*
		 * reschedule only if the first run has been completed and
		 * checks are currently not inibited, otherwise the new
		 * interval will be picked up anyway
		 */
		if (!self->priv->inhibit_check && self->priv->last_check > 0) {
			time_to_check = g_get_real_time() -
			    self->priv->last_check;
			if (time_to_check <= 0)
				pkui_backend_check_now(self);
			else {
				if (self->priv->periodic_check_id != 0) {
					g_source_remove(self->priv->periodic_check_id);
				}
				self->priv->periodic_check_id =
				    g_timeout_add_seconds(time_to_check,
				    periodic_check, self);
			}
		}
		break;
	case PROP_INHIBIT_CHECK:
		inhibit_check = g_value_get_boolean(value);

		/*
		 * when changing self->priv->inhibit_check from FALSE to TRUE
		 * and the first run has been completed remove the periodic
		 * check, when changing from TRUE to FALSE either trigger a run
		 * immediately when more time than self->priv->check_interval
		 * has passed or just reschedule the next check
		 */
		if (!self->priv->inhibit_check && inhibit_check &&
		    self->priv->periodic_check_id != 0) {
			g_source_remove(self->priv->periodic_check_id);
			self->priv->periodic_check_id = 0;
		} else if (self->priv->inhibit_check && !inhibit_check) {
			self->priv->periodic_check_id =
			    g_timeout_add_seconds(self->priv->check_interval,
			    (GSourceFunc)periodic_check, self);
		}
		self->priv->inhibit_check = inhibit_check;
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void
pkui_backend_get_property(GObject *object, guint property_id, GValue *value,
    GParamSpec *pspec)
{
	PkuiBackend	*self = PKUI_BACKEND(object);

	switch (property_id) {
	case PROP_UPDATES_NORMAL:
		g_value_set_uint(value, self->priv->updates_normal);
		break;
	case PROP_UPDATES_IMPORTANT:
		g_value_set_uint(value, self->priv->updates_important);
		break;
	case PROP_STARTUP_DELAY:
		g_value_set_uint(value, self->priv->startup_delay);
		break;
	case PROP_CHECK_INTERVAL:
		g_value_set_uint(value, self->priv->check_interval);
		break;
	case PROP_INHIBIT_CHECK:
		g_value_set_boolean(value, self->priv->inhibit_check);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void
pkui_backend_finalize(GObject *gobject)
{
	PkuiBackend	*self = PKUI_BACKEND(gobject);

	if (self->priv->pk_client != NULL)
		g_object_unref(self->priv->pk_client);

	G_OBJECT_CLASS(pkui_backend_parent_class)->finalize(gobject);
}

static void
pkui_backend_class_init(PkuiBackendClass *klass)
{
	GObjectClass	*gobject_class = G_OBJECT_CLASS(klass);
	GParamSpec	*pspec;

	gobject_class->set_property = pkui_backend_set_property;
	gobject_class->get_property = pkui_backend_get_property;
	gobject_class->finalize = pkui_backend_finalize;

	pspec = g_param_spec_uint("updates-normal", "Normal updates",
	    "Number of normal package updates", 0, G_MAXUINT, 0,
	    G_PARAM_READABLE);
	g_object_class_install_property(gobject_class, PROP_UPDATES_NORMAL,
	    pspec);

	pspec = g_param_spec_uint("updates-important", "Important updates",
	    "Number of important package updates", 0, G_MAXUINT, 0,
	    G_PARAM_READABLE);
	g_object_class_install_property(gobject_class, PROP_UPDATES_IMPORTANT,
	    pspec);

	pspec = g_param_spec_uint("startup-delay", "Startup delay",
	    "Initial delay in seconds before the first check for new package "
	    "updates", 0, G_MAXUINT, 300,
	    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property(gobject_class, PROP_STARTUP_DELAY,
	    pspec);

	pspec = g_param_spec_uint("check-interval", "Check interval",
	    "Interval in seconds for checking for new package updates", 1,
	    G_MAXUINT, 86400, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property(gobject_class, PROP_CHECK_INTERVAL,
	    pspec);

	pspec = g_param_spec_boolean("inhibit-check", "Inhibit check",
	    "Whether to inhibit checks for new package updates", FALSE,
	    G_PARAM_READWRITE);
	g_object_class_install_property(gobject_class, PROP_INHIBIT_CHECK,
	    pspec);

	pkui_backend_signals[STATE_CHANGED_SIGNAL] =
	    g_signal_newv("state-changed", G_TYPE_FROM_CLASS(gobject_class),
	    G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, NULL,
	    NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0, NULL);

	g_type_class_add_private(klass, sizeof (PkuiBackendPrivate));
}

static void
pkui_backend_init(PkuiBackend *self)
{
	self->priv = PKUI_BACKEND_GET_PRIVATE(self);

	self->priv->pk_client = pk_client_new();
	self->priv->updates_normal = 0;
	self->priv->updates_important = 0;
	self->priv->periodic_check_id =
	    g_timeout_add_seconds(self->priv->startup_delay,
	    (GSourceFunc)periodic_check, self);
	self->priv->last_check = 0;
}

static gboolean
periodic_check(gpointer data)
{
	PkuiBackend	*self = PKUI_BACKEND(data);

	pkui_backend_check_now(self);

	/* rescheduling happens after results are available */
	self->priv->periodic_check_id = 0;
	return (FALSE);
}

static void
process_pk_package_info(PkPackage *pkg, gpointer *user_data)
{
	PkuiBackend	*self = PKUI_BACKEND(user_data);
	PkInfoEnum	type_info = pk_package_get_info(pkg);

	switch (type_info) {
	case PK_INFO_ENUM_LOW:
	case PK_INFO_ENUM_ENHANCEMENT:
	case PK_INFO_ENUM_NORMAL:
		self->priv->updates_normal++;
		break;
	case PK_INFO_ENUM_BUGFIX:
	case PK_INFO_ENUM_IMPORTANT:
	case PK_INFO_ENUM_SECURITY:
		self->priv->updates_important++;
		break;
	default:
		break;
	}
}

static void
get_updates_finished(GObject *object, GAsyncResult *res, gpointer user_data)
{
	PkClient	*client = PK_CLIENT(object);
	PkuiBackend	*self = PKUI_BACKEND(user_data);
	PkResults	*results = NULL;
	GError		*error = NULL;
	GPtrArray	*list = NULL;

	g_debug("PackageKit finished checking for updates");

	results = pk_client_generic_finish(PK_CLIENT(client), res, &error);
	if (results == NULL) {
		g_warning("error: %s\n", error->message);
		g_error_free(error);
		goto out;
	}

	list = pk_results_get_package_array(results);
	if (list == NULL)
		goto out;
	self->priv->previous_updates_normal = self->priv->updates_normal;
	self->priv->previous_updates_important = self->priv->updates_important;
	self->priv->updates_normal = 0;
	self->priv->updates_important = 0;
	g_ptr_array_foreach(list, (GFunc)process_pk_package_info, self);

	if (self->priv->updates_normal != self->priv->previous_updates_normal ||
	    self->priv->updates_important !=
	    self->priv->previous_updates_important) {
		g_debug("normal updates: %d, important updates: %d",
		    self->priv->updates_normal, self->priv->updates_important);
		g_debug("emit state-changed");

		g_signal_emit(self, pkui_backend_signals[STATE_CHANGED_SIGNAL],
		    0);
	}

out:
	if (results != NULL)
		g_object_unref(results);
	if (list != NULL)
		g_ptr_array_unref(list);

	self->priv->last_check = g_get_real_time();

	if (!self->priv->inhibit_check) {
		if (self->priv->periodic_check_id != 0) {
			g_source_remove(self->priv->periodic_check_id);
		}
		self->priv->periodic_check_id =
			g_timeout_add_seconds(self->priv->check_interval,
			    (GSourceFunc)periodic_check, self);
	}
}

PkuiBackend *
pkui_backend_new(guint startup_delay, guint check_interval)
{
	g_return_val_if_fail(check_interval > 0, NULL);

	return (g_object_new(PKUI_TYPE_BACKEND, "startup-delay", startup_delay,
	    "check-interval", check_interval, NULL));
}

guint
pkui_backend_get_updates_normal(PkuiBackend *self)
{
	guint	updates_normal;

	g_return_val_if_fail(PKUI_IS_BACKEND(self), 0);

	g_object_get(G_OBJECT(self), "updates-normal", &updates_normal, NULL);

	return (updates_normal);
}

guint
pkui_backend_get_updates_important(PkuiBackend *self)
{
	guint	updates_important;

	g_return_val_if_fail(PKUI_IS_BACKEND(self), 0);

	g_object_get(G_OBJECT(self), "updates-important", &updates_important,
	    NULL);

	return (updates_important);
}

void
pkui_backend_set_inhibit_check(PkuiBackend *self, gboolean inhibit_check)
{
	g_return_if_fail(PKUI_IS_BACKEND(self));

	g_object_set(G_OBJECT(self), "inhibit-check", inhibit_check, NULL);
}

gboolean
pkui_backend_get_inhibit_check(PkuiBackend *self)
{
	gboolean	inhibit_check;

	g_return_val_if_fail(PKUI_IS_BACKEND(self), FALSE);

	g_object_get(G_OBJECT(self), "inhibit-check", &inhibit_check, NULL);

	return (inhibit_check);
}

guint
pkui_backend_get_startup_interval(PkuiBackend *self)
{
	guint	startup_interval;

	g_return_val_if_fail(PKUI_IS_BACKEND(self), 0);

	g_object_get(G_OBJECT(self), "startup-interval", &startup_interval,
	    NULL);

	return (startup_interval);
}

void
pkui_backend_set_check_interval(PkuiBackend *self, guint check_interval)
{
	g_return_if_fail(PKUI_IS_BACKEND(self));

	g_object_set(G_OBJECT(self), "check-interval", check_interval, NULL);
}

guint
pkui_backend_get_check_interval(PkuiBackend *self)
{
	guint	check_interval;

	g_return_val_if_fail(PKUI_IS_BACKEND(self), 0);

	g_object_get(G_OBJECT(self), "check-interval", &check_interval, NULL);

	return (check_interval);
}

void
pkui_backend_check_now(PkuiBackend *self)
{
	g_return_if_fail(PKUI_IS_BACKEND(self));

	g_debug("querying PackageKit for updates");

	pk_client_get_updates_async(self->priv->pk_client,
	    pk_bitfield_value(PK_FILTER_ENUM_NONE), NULL, NULL, NULL,
	    get_updates_finished, self);
}
