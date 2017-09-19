/*
 * Copyright (C) 2011 Guido Berhoerster <gber@opensuse.org>
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

#include <locale.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <unique/unique.h>
#include <libnotify/notify.h>
#include "pkui-icon.h"

int
main(int argc, char **argv)
{
	PkuiIcon	*icon;
	UniqueApp	*app = NULL;
	int		exitval = 0;
	gboolean	version = FALSE;
	gchar		*update_viewer_command = NULL;
	gint		startup_delay = 300;
	gint		check_interval = 86400;
	GOptionContext	*context;
	GError		*error = NULL;
	const GOptionEntry options[] = {
		{ "command", 'c', 0, G_OPTION_ARG_STRING,
		    &update_viewer_command, N_("Command for starting the "
		    "software update viewer"), "command" },
		{ "delay", 'd', 0, G_OPTION_ARG_INT, &startup_delay,
		    N_("Set the delay in seconds before the first check for "
		    "updates"), "delay" },
		{ "interval", 'i', 0, G_OPTION_ARG_INT, &check_interval,
		    N_("Set the interval in seconds between checks for "
		    "updates"), "interval" },
		{ "version", 'v', 0, G_OPTION_ARG_NONE, &version,
		    N_("Print the version number and exit"), NULL },
		{ NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, 0 }
	};

	setlocale(LC_ALL, "");

	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
	textdomain(PACKAGE);

	gtk_init(&argc, &argv);

	context = g_option_context_new(_("- display notifications about "
	    "software updates"));
	g_option_context_add_main_entries(context, options, PACKAGE);
	g_option_context_parse(context, &argc, &argv, &error);
	g_option_context_free(context);
	if (error) {
		g_printerr("Error parsing command line options: %s\n",
		    error->message);
		g_error_free(error);
		exitval = 1;
		goto out;
	}

	if (startup_delay < 0) {
		g_printerr("Error parsing command line options: delay must be "
		    "greater or equal to zero\n");
		exitval = 1;
		goto out;
	}

	if (check_interval < 0) {
		g_printerr("Error parsing command line options: interval must "
		    "be greater or equal to zero\n");
		exitval = 1;
		goto out;
	}

	if (version) {
		g_print("%s %s\n", PACKAGE, VERSION);
		goto out;
	}

	app = unique_app_new(APP_NAME, NULL);
	if (unique_app_is_running(app)) {
		g_printerr("Another instance of pk-update-icon is already "
		    "running, exiting\n");
		exitval = 1;
		goto out;
	}

	notify_init(PACKAGE);

	icon = pkui_icon_new(startup_delay, check_interval,
	    update_viewer_command);

	gtk_main();

	g_object_unref(icon);
out:
	if (app != NULL)
		g_object_unref(app);
	if (notify_is_initted())
		notify_uninit ();
	g_free(update_viewer_command);

	return (exitval);
}
