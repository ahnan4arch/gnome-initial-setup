/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Red Hat
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 */

/* Summary page {{{1 */

#define PAGE_ID "summary"

#include "config.h"
#include "summary-resources.h"
#include "gis-summary-page.h"

#include <glib/gstdio.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <locale.h>
#include <stdlib.h>
#include <errno.h>

#include <act/act-user-manager.h>

#include <gdm/gdm-client.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>

#include "pk.h"

#define SERVICE_NAME "gdm-password"

G_DEFINE_TYPE (GisSummaryPage, gis_summary_page, GIS_TYPE_PAGE);

#define GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GIS_TYPE_SUMMARY_PAGE, GisSummaryPagePrivate))

struct _GisSummaryPagePrivate {
  ActUser *user_account;
  const gchar *user_password;
};

#define OBJ(type,name) ((type)gtk_builder_get_object(GIS_PAGE(page)->builder,(name)))
#define WID(name) OBJ(GtkWidget*,name)

static gboolean
connect_to_gdm (GdmGreeter      **greeter,
                GdmUserVerifier **user_verifier)
{
  GdmClient *client;

  GError *error = NULL;
  gboolean res = FALSE;

  client = gdm_client_new ();

  *greeter = gdm_client_get_greeter_sync (client, NULL, &error);
  if (error != NULL)
    goto out;

  *user_verifier = gdm_client_get_user_verifier_sync (client, NULL, &error);
  if (error != NULL)
    goto out;

  res = TRUE;

 out:
  if (error != NULL) {
    g_warning ("Failed to open connection to GDM: %s", error->message);
    g_error_free (error);
  }

  return res;
}

static void
request_info_query (GisSummaryPage  *page,
                    GdmUserVerifier *user_verifier,
                    const char      *question,
                    gboolean         is_secret)
{
  /* TODO: pop up modal dialog */
  g_debug ("user verifier asks%s question: %s",
           is_secret ? " secret" : "",
           question);
}

static void
on_info (GdmUserVerifier *user_verifier,
         const char      *service_name,
         const char      *info,
         GisSummaryPage  *page)
{
  g_debug ("PAM module info: %s\n", info);
}

static void
on_problem (GdmUserVerifier *user_verifier,
            const char      *service_name,
            const char      *problem,
            GisSummaryPage  *page)
{
  g_warning ("PAM module error: %s\n", problem);
}

static void
on_info_query (GdmUserVerifier *user_verifier,
               const char      *service_name,
               const char      *question,
               GisSummaryPage  *page)
{
  request_info_query (page, user_verifier, question, FALSE);
}

static void
on_secret_info_query (GdmUserVerifier *user_verifier,
                      const char      *service_name,
                      const char      *question,
                      GisSummaryPage  *page)
{
  GisSummaryPagePrivate *priv = page->priv;
  gboolean should_send_password = priv->user_password != NULL;

  g_debug ("PAM module secret info query: %s\n", question);
  if (should_send_password) {
    g_debug ("sending password\n");
    gdm_user_verifier_call_answer_query (user_verifier,
                                         service_name,
                                         priv->user_password,
                                         NULL, NULL, NULL);
    g_clear_pointer (&priv->user_password, (GDestroyNotify) g_free);
  } else {
    request_info_query (page, user_verifier, question, TRUE);
  }
}

static void
on_session_opened (GdmGreeter     *greeter,
                   const char     *service_name,
                   GisSummaryPage *page)
{
  gdm_greeter_call_start_session_when_ready_sync (greeter, service_name,
                                                  TRUE, NULL, NULL);
}

static void
log_user_in (GisSummaryPage *page)
{
  GisSummaryPagePrivate *priv = page->priv;
  GError *error = NULL;
  GdmGreeter *greeter;
  GdmUserVerifier *user_verifier;

  if (!connect_to_gdm (&greeter, &user_verifier)) {
    g_warning ("No GDM connection; not initiating login");
    return;
  }

  if (error != NULL) {
    g_warning ("Could not set PAM_AUTHTOK: %s", error->message);
    return;
  }

  g_signal_connect (user_verifier, "info",
                    G_CALLBACK (on_info), page);
  g_signal_connect (user_verifier, "problem",
                    G_CALLBACK (on_problem), page);
  g_signal_connect (user_verifier, "info-query",
                    G_CALLBACK (on_info_query), page);
  g_signal_connect (user_verifier, "secret-info-query",
                    G_CALLBACK (on_secret_info_query), page);

  g_signal_connect (greeter, "session-opened",
                    G_CALLBACK (on_session_opened), page);

  gdm_user_verifier_call_begin_verification_for_user_sync (user_verifier,
                                                           SERVICE_NAME,
                                                           act_user_get_user_name (priv->user_account),
                                                           NULL, &error);

  if (error != NULL) {
    g_warning ("Could not begin verification: %s", error->message);
    return;
  }
}

static void
add_setup_done_file (void)
{
  gchar *gis_done_path;
  GError *error = NULL;

  gis_done_path = g_build_filename (g_get_user_config_dir (),
                                    "gnome-initial-setup-done",
                                    NULL);

  if (!g_file_set_contents (gis_done_path, "yes", -1, &error)) {
      g_warning ("Unable to create %s: %s", gis_done_path, error->message);
      g_clear_error (&error);
  }

  g_free (gis_done_path);
}

static gchar *
getting_started_package_name (void)
{
  gchar *lc = setlocale (LC_MESSAGES, NULL);
  gchar *locale = gnome_normalize_locale (lc);
  gchar *package_name;

  if (strcmp (locale, "en"))
    package_name = g_strdup ("gnome-getting-started-docs");
  else
    package_name = g_strdup_printf ("gnome-getting-started-docs-%s", locale);

  g_free (locale);
  return package_name;
}

static void
download_language_pack (GisSummaryPage *page)
{
  gchar *package_name = getting_started_package_name ();
  gchar *package_names[2] = { package_name, NULL };
  PackageKitModify *proxy;
  GError *error = NULL;

  proxy = package_kit_modify_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                     G_DBUS_PROXY_FLAGS_NONE,
                                                     "org.freedesktop.PackageKit",
                                                     "/org/freedesktop/PackageKit",
                                                     NULL, NULL);

  if (!package_kit_modify_call_install_package_names_sync (proxy,
                                                           0,
                                                           (const gchar *const *) package_names,
                                                           "hide-confirm-search,hide-confirm-deps,hide-confirm-install,hide-progress,hide-finished,hide-warning",
                                                           NULL, &error)) {

    /* we need to get a better api than this broken pile of trash */
    if (!(g_dbus_error_is_remote_error (error) &&
          g_strcmp0 (g_dbus_error_get_remote_error (error), "org.freedesktop.PackageKit.Modify.Failed") == 0 &&
          g_dbus_error_strip_remote_error (error) &&
          g_strcmp0 (error->message, "package already found") == 0))
      g_warning ("Error installing packages: %s", error->message);
  }

  g_clear_error (&error);
  g_free (package_name);
}

static void
done_cb (GtkButton *button, GisSummaryPage *page)
{
  gchar *file;

  /* the tour is triggered by $XDG_CONFIG_HOME/run-welcome-tour */
  file = g_build_filename (g_get_user_config_dir (), "run-welcome-tour", NULL);
  g_file_set_contents (file, "yes", -1, NULL);
  g_free (file);

  switch (gis_driver_get_mode (GIS_PAGE (page)->driver))
    {
    case GIS_DRIVER_MODE_NEW_USER:
      log_user_in (page);
      break;
    case GIS_DRIVER_MODE_EXISTING_USER:
      add_setup_done_file ();
      g_application_quit (G_APPLICATION (GIS_PAGE (page)->driver));
    default:
      break;
    }
}

static void
prepare_cb (GisAssistant   *assistant,
            GisPage        *which_page,
            GisSummaryPage *this_page)
{
  if (GIS_PAGE (which_page) == GIS_PAGE (this_page))
    {
      GisSummaryPagePrivate *priv = this_page->priv;
      gis_driver_get_user_permissions (GIS_PAGE (this_page)->driver,
                                       &priv->user_account,
                                       &priv->user_password);

      download_language_pack (this_page);
    }
}

static GtkBuilder *
gis_summary_page_get_builder (GisPage *page)
{
  GtkBuilder *builder = gtk_builder_new ();

  char *filename = g_build_filename (UIDIR, "summary-distro.ui", NULL);
  GError *error = NULL;

  if (gtk_builder_add_from_file (builder, filename, &error))
    goto out;

  if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
    g_warning ("Error while loading summary override: %s", error->message);

  g_clear_error (&error);

  builder = GIS_PAGE_CLASS (gis_summary_page_parent_class)->get_builder (page);

 out:
  g_free (filename);
  return builder;
}

static void
gis_summary_page_constructed (GObject *object)
{
  GisSummaryPage *page = GIS_SUMMARY_PAGE (object);
  GisAssistant *assistant = gis_driver_get_assistant (GIS_PAGE (page)->driver);

  G_OBJECT_CLASS (gis_summary_page_parent_class)->constructed (object);

  gtk_container_add (GTK_CONTAINER (page), WID ("summary-page"));

  g_signal_connect_object (assistant, "prepare", G_CALLBACK (prepare_cb), page, 0);

  g_signal_connect (WID("summary-start-button"), "clicked", G_CALLBACK (done_cb), page);

  gis_page_set_complete (GIS_PAGE (page), TRUE);

  gtk_widget_show (GTK_WIDGET (page));
}

static void
gis_summary_page_locale_changed (GisPage *page)
{
  gis_page_set_title (GIS_PAGE (page), _("Thank You"));
}

static void
gis_summary_page_class_init (GisSummaryPageClass *klass)
{
  GisPageClass *page_class = GIS_PAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  page_class->page_id = PAGE_ID;
  page_class->get_builder = gis_summary_page_get_builder;
  page_class->locale_changed = gis_summary_page_locale_changed;
  object_class->constructed = gis_summary_page_constructed;

  g_type_class_add_private (object_class, sizeof(GisSummaryPagePrivate));
}

static void
gis_summary_page_init (GisSummaryPage *page)
{
  g_resources_register (summary_get_resource ());
  page->priv = GET_PRIVATE (page);
}

void
gis_prepare_summary_page (GisDriver *driver)
{
  gis_driver_add_page (driver,
                       g_object_new (GIS_TYPE_SUMMARY_PAGE,
                                     "driver", driver,
                                     NULL));
}
