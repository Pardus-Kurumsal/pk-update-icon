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

#include <glib/gi18n.h>
#include <libnotify/notify.h>
#include "pkui-backend.h"
#include "pkui-icon.h"

G_DEFINE_TYPE(PkuiIcon, pkui_icon, G_TYPE_OBJECT)

#ifndef	UPDATE_VIEWER_COMMAND
#define	UPDATE_VIEWER_COMMAND	""
#endif /* UPDATE_VIEWER_COMMAND */

#define	PKUI_ICON_GET_PRIVATE(obj)	(G_TYPE_INSTANCE_GET_PRIVATE((obj), \
					    PKUI_TYPE_ICON, PkuiIconPrivate))

struct _PkuiIconPrivate
{
	PkuiBackend	*backend;

	GtkStatusIcon	*status_icon;
	GtkWidget	*status_icon_popup_menu;
	NotifyNotification *notification;
	gchar		*update_viewer_command;
};

static GtkWidget*	icon_popup_menu_create(PkuiIcon *self);
static void	icon_popup_menu_popup(GtkStatusIcon *status_icon, guint button,
    guint activate_time, gpointer user_data);
static void	icon_activated(GtkStatusIcon *status_icon, gpointer user_data);
static void	hide_notification(PkuiIcon *self);
static void	backend_state_changed(PkuiBackend *backend, gpointer user_data);
static void	update_viewer_exited(GPid pid, gint status, gpointer user_data);

static void
pkui_icon_finalize(GObject *gobject)
{
	PkuiIcon	*self = PKUI_ICON(gobject);

	gtk_widget_destroy(self->priv->status_icon_popup_menu);
	g_object_unref(self->priv->status_icon_popup_menu);
	g_object_unref(self->priv->status_icon);
	if (self->priv->notification != NULL) {
		notify_notification_close(self->priv->notification, NULL);
		g_object_unref(self->priv->notification);
	}
	g_object_unref(self->priv->backend);
	g_free(self->priv->update_viewer_command);

	G_OBJECT_CLASS(pkui_icon_parent_class)->finalize(gobject);
}

static void
pkui_icon_class_init(PkuiIconClass *klass)
{
	GObjectClass	*gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->finalize = pkui_icon_finalize;

	g_type_class_add_private(klass, sizeof (PkuiIconPrivate));
}

static void
pkui_icon_init(PkuiIcon *self)
{
	self->priv = PKUI_ICON_GET_PRIVATE(self);

	gtk_window_set_default_icon_name("system-software-update");

	self->priv->status_icon_popup_menu = icon_popup_menu_create(self);
	g_object_ref(self->priv->status_icon_popup_menu);
	g_object_ref_sink(GTK_OBJECT(self->priv->status_icon_popup_menu));

	self->priv->status_icon = gtk_status_icon_new();
	gtk_status_icon_set_title(self->priv->status_icon,
	    _("Software Updates"));
	gtk_status_icon_set_visible(self->priv->status_icon, FALSE);
	g_signal_connect(G_OBJECT(self->priv->status_icon), "activate",
	    G_CALLBACK(icon_activated), self);
	g_signal_connect(G_OBJECT(self->priv->status_icon), "popup-menu",
	    G_CALLBACK(icon_popup_menu_popup), self);

	self->priv->notification = NULL;

	self->priv->backend = NULL;

	self->priv->update_viewer_command = NULL;
}

static void
exec_update_viewer(PkuiIcon *self)
{
	gchar		**argv = NULL;
	GError		*error = NULL;
	GPid		pid;

	g_return_if_fail(PKUI_IS_BACKEND(self->priv->backend));
	g_return_if_fail(self->priv->update_viewer_command != NULL);

	g_debug("executing command %s", self->priv->update_viewer_command);

	if (!g_shell_parse_argv(self->priv->update_viewer_command, NULL,
	    &argv, &error)) {
		g_warning("Could not parse command: %s", error->message);
		g_error_free(error);
		return;
	}

	if (!g_spawn_async(NULL, argv, NULL,
	    G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH, NULL, NULL, &pid,
	    &error)) {
		g_warning("Could not execute command: %s", error->message);
		g_error_free(error);
		return;
	}
	g_child_watch_add(pid, (GChildWatchFunc)update_viewer_exited, self);

	pkui_backend_set_inhibit_check(self->priv->backend, TRUE);
	hide_notification(self);
}

static void
about_dialog_show(GtkMenuItem *item, gpointer user_data)
{
	static const gchar	*copyright = "Copyright \xc2\xa9 2011 Guido "
	    "Berhoerster\nCopyright \xc2\xa9 2011 Pavol Rusnak\n";
	static const gchar	*authors[3] = {
	    "Guido Berhoerster <gber@opensuse.org>",
	    "Pavol Rusnak <stick@gk2.sk>",
	    NULL
	};
	static const gchar	*documenters[2] = {
	    "Guido Berhoerster <gber@opensuse.org>",
	    NULL
	};
	static const gchar	*license =
	    "Licensed under the GNU General Public License Version 2\n\n"
	    "This program is free software; you can redistribute it and/or "
	    "modify it under the terms of the GNU General Public License as "
	    "published by the Free Software Foundation; either version 2 of "
	    "the License, or (at your option) any later version.\n\n"
	    "This program is distributed in the hope that it will be useful, "
	    "but WITHOUT ANY WARRANTY; without even the implied warranty of "
	    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU "
	    "General Public License for more details.\n\n"
	    "You should have received a copy of the GNU General Public License "
	    "along with this program; if not, write to the Free Software "
	    "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, "
	    "MA 02110-1301 USA.";
	const gchar	*translators = _("translators");

	if (strcmp(translators, "translators") == 0)
	    translators = NULL;

	gtk_show_about_dialog (NULL, "version", VERSION, "copyright", copyright,
	    "authors", authors, "documenters", documenters,
	    "translator-credits", translators, "license", license,
	    "wrap-license", TRUE, NULL);
}

static void
backend_check_now(GtkMenuItem *menu_item, gpointer user_data)
{
	PkuiIcon	*self = PKUI_ICON(user_data);

	g_return_if_fail(PKUI_IS_BACKEND(self->priv->backend));

	pkui_backend_check_now(self->priv->backend);
}

static GtkWidget*
icon_popup_menu_create(PkuiIcon *self)
{
	GtkWidget	*popup_menu = gtk_menu_new();
	GtkWidget	*item;
	GtkWidget	*image;

	item = gtk_image_menu_item_new_with_mnemonic(_("_Check for Updates"));
	image = gtk_image_new_from_icon_name(GTK_STOCK_REFRESH,
	    GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
	    G_CALLBACK(backend_check_now), self);

	item = gtk_image_menu_item_new_with_mnemonic(_("_About"));
	image = gtk_image_new_from_icon_name(GTK_STOCK_ABOUT,
	    GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_menu_shell_append(GTK_MENU_SHELL(popup_menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
	    G_CALLBACK(about_dialog_show), self);

	gtk_widget_show_all(GTK_WIDGET(popup_menu));

	return (popup_menu);
}

static void
icon_popup_menu_popup(GtkStatusIcon *status_icon, guint button,
    guint activate_time, gpointer user_data)
{
	PkuiIcon	*self = PKUI_ICON(user_data);

	gtk_menu_popup(GTK_MENU(self->priv->status_icon_popup_menu), NULL, NULL,
	    NULL, NULL, button, activate_time);
}

static void
notification_handle_action(NotifyNotification *notification, gchar *action,
    gpointer user_data)
{
	PkuiIcon	*self = PKUI_ICON(user_data);

	if (strcmp(action, "install-updates") == 0) {
		exec_update_viewer(self);
	}
}

static void
update_notification(PkuiIcon *self, guint updates_normal,
    guint updates_important)
{
	gchar	*message;
	gchar	*message_a;
	gchar	*message_b;
	gchar	*title = updates_important ?
	    /* TRANSLATORS: This is a message without number mentioned */
	    ngettext("Important Software Update", "Important Software Updates",
	    updates_important + updates_normal) :
	    /* TRANSLATORS: This is a message without number mentioned */
	    ngettext("Software Update", "Software Updates", updates_important +
	    updates_normal);
	gchar	*icon = updates_important ? "software-update-urgent" :
	    "software-update-available";

	if (updates_important > 0) {
		if (updates_normal > 0) {
			message_a = g_strdup_printf(
			/*
			 * TRANSLATORS: This sentence contains two plurals.
			 * Texts related to these plurals are mixed. That is
			 * why it is split in three parts. Fill first two parts
			 * as you need, and use them as %s in the last one to
			 * construct a sentence. Note that if the first form of
			 * plural relates only to singular form, it is never
			 * used, and dedicated shorter sentences are used. */
			    ngettext("There is %d software update available,",
			    "There are %d software updates available,",
			    updates_normal + updates_important),
			    updates_normal + updates_important);
			/*
			 * TRANSLATORS: This is the sentence part in the
			 * middle, form of which is related to the first
			 * number. If your language does not need it, simply
			 * use it as space or so. */
			message_b = ngettext("of it", "of them",
			    updates_normal + updates_important);
			message = g_strdup_printf(
			/*
			 * TRANSLATORS: This forms the sentence. If you need to
			 * swap parts, use %3$s and %1$s etc. Plurals are
			 * related to second number. */
			    ngettext("%s %d %s is important.",
			    "%s %d %s are important.", updates_important),
			    message_a, updates_important, message_b);
			g_free(message_a);
		} else {
			message = g_strdup_printf(ngettext("There is %d "
			    "important software update available.",
			    "There are %d important software updates "
			    "available.", updates_important),
			    updates_important);
		}
	} else {
		message = g_strdup_printf(ngettext("There is %d software "
		    "update available.",
		    "There are %d software updates available.",
		    updates_normal), updates_normal);
	}

	gtk_status_icon_set_tooltip(self->priv->status_icon, message);
	gtk_status_icon_set_from_icon_name(self->priv->status_icon, icon);
	gtk_status_icon_set_visible(self->priv->status_icon, TRUE);

	if (self->priv->notification == NULL) {
		self->priv->notification = notify_notification_new(title,
		    message, icon
#if (NOTIFY_VERSION_MAJOR == 0 && NOTIFY_VERSION_MINOR < 7)
		    , NULL
#endif
		    );
		if (self->priv->update_viewer_command != NULL) {
			notify_notification_add_action(self->priv->notification,
			     "install-updates", ngettext("Install Update",
			     "Install Updates",
			     updates_normal + updates_important),
			     notification_handle_action, self, NULL);
		}
	} else
		notify_notification_update(self->priv->notification, title,
		    message, icon);

	notify_notification_set_timeout(self->priv->notification,
	    NOTIFY_EXPIRES_NEVER);
	notify_notification_set_urgency(self->priv->notification,
	    updates_important ? NOTIFY_URGENCY_CRITICAL :
	    NOTIFY_URGENCY_NORMAL);
	notify_notification_show(self->priv->notification, NULL);

	g_free(message);
}

static void
hide_notification(PkuiIcon *self)
{
	gtk_status_icon_set_visible(self->priv->status_icon, FALSE);
	notify_notification_close(self->priv->notification, NULL);
}


static void
backend_state_changed(PkuiBackend *backend, gpointer user_data)
{
	PkuiIcon	*self = PKUI_ICON(user_data);
	guint		updates_normal;
	guint		updates_important;

	g_return_if_fail(PKUI_IS_BACKEND(backend));

	updates_normal = pkui_backend_get_updates_normal(backend);
	updates_important = pkui_backend_get_updates_important(backend);
	if (updates_normal > 0 || updates_important > 0)
		update_notification(self, updates_normal, updates_important);
	else if (updates_normal + updates_important == 0)
		hide_notification(self);
}

static void
update_viewer_exited(GPid pid, gint status, gpointer user_data)
{
	PkuiIcon	*self = PKUI_ICON(user_data);

	g_return_if_fail(PKUI_IS_BACKEND(self->priv->backend));

	g_spawn_close_pid(pid);

	pkui_backend_check_now(self->priv->backend);
	pkui_backend_set_inhibit_check(self->priv->backend, FALSE);
}

static void
icon_activated(GtkStatusIcon *status_icon, gpointer user_data)
{
	PkuiIcon	*self = PKUI_ICON(user_data);

	if (self->priv->update_viewer_command != NULL) {
		exec_update_viewer(self);
	}
}

PkuiIcon *
pkui_icon_new(guint startup_delay, guint check_interval,
    const gchar *update_viewer_command)
{
	PkuiIcon	*icon = g_object_new(PKUI_TYPE_ICON, NULL);

	icon->priv->update_viewer_command = g_strdup((update_viewer_command !=
	    NULL) ? update_viewer_command : UPDATE_VIEWER_COMMAND);
	icon->priv->backend = pkui_backend_new(startup_delay, check_interval);
	g_signal_connect(icon->priv->backend, "state-changed",
	    G_CALLBACK(backend_state_changed), icon);

	return (icon);
}
