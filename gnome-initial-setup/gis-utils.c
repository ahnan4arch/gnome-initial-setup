/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

#include "config.h"
#include "gis-utils.h"

#include <string.h>

#include <stdlib.h>

void
gis_copy_account_file (ActUser     *act_user,
                       const gchar *relative_path)
{
        const gchar *username;
        const gchar *homedir;
        GSList *dirs = NULL, *l;
        gchar *p, *tmp;
        gchar *argv[20];
        gint i;
        gchar *from;
        gchar *to;
        GError *error = NULL;

        username = act_user_get_user_name (act_user);
        homedir = act_user_get_home_dir (act_user);

        from = g_build_filename (g_get_home_dir (), relative_path, NULL);
        to = g_build_filename (homedir, relative_path, NULL);

        p = g_path_get_dirname (relative_path);
        while (strcmp (p, ".") != 0) {
                dirs = g_slist_prepend (dirs, g_build_filename (homedir, p, NULL));
                tmp = g_path_get_dirname (p);
                g_free (p);
                p = tmp;
        }

        i = 0;
        argv[i++] = "/usr/bin/pkexec";
        argv[i++] = "install";
        argv[i++] = "--owner";
        argv[i++] = (gchar *)username;
        argv[i++] = "--group";
        argv[i++] = (gchar *)username;
        argv[i++] = "--mode";
        argv[i++] = "755";
        argv[i++] = "--directory";
        for (l = dirs; l; l = l->next) {
                argv[i++] = l->data;
                if (i == 20) {
                        g_warning ("Too many subdirectories");
                        goto out;
                }
        }
        argv[i++] = NULL;

        if (!g_spawn_sync (NULL, argv, NULL, 0, NULL, NULL, NULL, NULL, NULL, &error)) {
                g_warning ("Failed to copy account data: %s", error->message);
                g_error_free (error);
                goto out;
        }

        i = 0;
        argv[i++] = "/usr/bin/pkexec";
        argv[i++] = "install";
        argv[i++] = "--owner";
        argv[i++] = (gchar *)username;
        argv[i++] = "--group";
        argv[i++] = (gchar *)username;
        argv[i++] = "--mode";
        argv[i++] = "755";
        argv[i++] = from;
        argv[i++] = to;
        argv[i++] = NULL;

        if (!g_spawn_sync (NULL, argv, NULL, 0, NULL, NULL, NULL, NULL, NULL, &error)) {
                g_warning ("Failed to copy account data: %s", error->message);
                g_error_free (error);
                goto out;
        }

out:
        g_slist_free_full (dirs, g_free);
        g_free (to);
        g_free (from);
}

GtkBuilder *
gis_builder (gchar *resource)
{
    GtkBuilder *builder;
    gchar *resource_path = g_strdup_printf ("/ui/%s.ui", resource);
    GError *error = NULL;

    builder = gtk_builder_new ();
    gtk_builder_add_from_resource (builder, resource_path, &error);

    g_free (resource_path);

    if (error != NULL) {
        g_printerr ("%s", error->message);
        exit (1);
    }

    return builder;
}


/* remove when this is landed in GTK+ itself */

#include <gtk/gtk.h>
#include <pango/pango.h>

static GtkTextTag *
gis_gtk_text_buffer_get_text_tag_from_pango (PangoAttrIterator *paiter)
{
  PangoAttribute *attr;
  GtkTextTag *tag = gtk_text_tag_new (NULL);

  if ( (attr = pango_attr_iterator_get (paiter, PANGO_ATTR_LANGUAGE)))
    g_object_set (tag, "language", pango_language_to_string ( ( (PangoAttrLanguage*)attr)->value), NULL);

  if ( (attr = pango_attr_iterator_get (paiter, PANGO_ATTR_FAMILY)))
    g_object_set (tag, "family", ( (PangoAttrString*)attr)->value, NULL);

  if ( (attr = pango_attr_iterator_get (paiter, PANGO_ATTR_STYLE)))
    g_object_set (tag, "style", ( (PangoAttrInt*)attr)->value, NULL);

  if ( (attr = pango_attr_iterator_get (paiter, PANGO_ATTR_WEIGHT)))
    g_object_set (tag, "weight", ( (PangoAttrInt*)attr)->value, NULL);

  if ( (attr = pango_attr_iterator_get (paiter, PANGO_ATTR_VARIANT)))
    g_object_set (tag, "variant", ( (PangoAttrInt*)attr)->value, NULL);

  if ( (attr = pango_attr_iterator_get (paiter, PANGO_ATTR_STRETCH)))
    g_object_set (tag, "stretch", ( (PangoAttrInt*)attr)->value, NULL);

  if ( (attr = pango_attr_iterator_get (paiter, PANGO_ATTR_SIZE)))
    g_object_set (tag, "size", ( (PangoAttrInt*)attr)->value, NULL);

  if ( (attr = pango_attr_iterator_get (paiter, PANGO_ATTR_FONT_DESC)))
    g_object_set (tag, "font-desc", ( (PangoAttrFontDesc*)attr)->desc, NULL);

  if ( (attr = pango_attr_iterator_get (paiter, PANGO_ATTR_FOREGROUND))) {
    GdkColor col = { 0,
                     ( (PangoAttrColor*)attr)->color.red,
                     ( (PangoAttrColor*)attr)->color.green,
                     ( (PangoAttrColor*)attr)->color.blue
    };

    g_object_set (tag, "foreground-gdk", &col, NULL);
  }

  if ( (attr = pango_attr_iterator_get (paiter, PANGO_ATTR_BACKGROUND))) {
    GdkColor col = { 0,
                     ( (PangoAttrColor*)attr)->color.red,
                     ( (PangoAttrColor*)attr)->color.green,
                     ( (PangoAttrColor*)attr)->color.blue
    };

    g_object_set (tag, "background-gdk", &col, NULL);
  }

  if ( (attr = pango_attr_iterator_get (paiter, PANGO_ATTR_UNDERLINE)))
    g_object_set (tag, "underline", ( (PangoAttrInt*)attr)->value, NULL);

  if ( (attr = pango_attr_iterator_get (paiter, PANGO_ATTR_STRIKETHROUGH)))
    g_object_set (tag, "strikethrough", (gboolean) ( ( (PangoAttrInt*)attr)->value != 0), NULL);

  if ( (attr = pango_attr_iterator_get (paiter, PANGO_ATTR_RISE)))
    g_object_set (tag, "rise", ( (PangoAttrInt*)attr)->value, NULL);

  if ( (attr = pango_attr_iterator_get (paiter, PANGO_ATTR_SCALE)))
    g_object_set (tag, "scale", ( (PangoAttrFloat*)attr)->value, NULL);

  return tag;
}

void
gis_gtk_text_buffer_insert_pango_text (GtkTextBuffer *buffer,
                                       GtkTextIter *iter,
                                       PangoAttrList *attrlist,
                                       gchar *text)
{
  GtkTextMark *mark;
  PangoAttrIterator *paiter;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));

  /* trivial, no markup */
  if (attrlist == NULL) {
    gtk_text_buffer_insert (buffer, iter, text, -1);
    return;
  }

  /* create mark with right gravity */
  mark = gtk_text_buffer_create_mark (buffer, NULL, iter, FALSE);
  paiter = pango_attr_list_get_iterator (attrlist);

  do {
    GtkTextTag *tag;
    GtkTextTag *tag_para;
    gint start, end;

    pango_attr_iterator_range (paiter, &start, &end);

    if (end == G_MAXINT)	/* last chunk */
      end = start-1; /* resulting in -1 to be passed to _insert */

    tag = gis_gtk_text_buffer_get_text_tag_from_pango (paiter);

    gtk_text_tag_table_add (gtk_text_buffer_get_tag_table (buffer), tag);

    tag_para = gtk_text_tag_table_lookup (gtk_text_buffer_get_tag_table (buffer), "para");
    gtk_text_buffer_insert_with_tags (buffer, iter, text+start, end - start, tag, tag_para, NULL);

    /* mark had right gravity, so it should be
     *	at the end of the inserted text now */
    gtk_text_buffer_get_iter_at_mark (buffer, iter, mark);
  } while (pango_attr_iterator_next (paiter));

  gtk_text_buffer_delete_mark (buffer, mark);
  pango_attr_iterator_destroy (paiter);
}