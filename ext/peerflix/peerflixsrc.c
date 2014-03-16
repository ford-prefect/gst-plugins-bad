/* GStreamer
 * Copyright (C) 2014 Arun Raghavan <arun@accosted.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:element-peerflixsrc
 *
 * Element that takes a torrent file or URL and uses peerflix to present it as
 * a streamable HTTP stream.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 playbin uri=torrent+http://...
 * ]|
 * </refsect2>
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#include <gst/pbutils/missing-plugins.h>

#include "peerflixsrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_peerflix_src_debug);
#define GST_CAT_DEFAULT gst_peerflix_src_debug

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_PEERFLIX_PATH,
  PROP_PORT,
};

#define DEFAULT_PEERFLIX_PATH "peerflix"
#define DEFAULT_PORT 10000

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

static void gst_peerflix_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * spec);
static void gst_peerflix_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * spec);
static void gst_peerflix_src_reset (GstPeerflixSrc * src);
static GstStateChangeReturn
gst_peerflix_src_change_state (GstElement * element, GstStateChange trans);
static void
gst_peerflix_src_uri_handler_init (gpointer g_iface, gpointer iface_data);

#define gst_peerflix_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstPeerflixSrc, gst_peerflix_src, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER,
        gst_peerflix_src_uri_handler_init));

static void
gst_peerflix_src_dispose (GObject * object)
{
  GstPeerflixSrc *src = GST_PEERFLIX_SRC_CAST (object);

  G_OBJECT_CLASS (parent_class)->dispose ((GObject *) src);
}

static void
gst_peerflix_src_finalize (GObject * object)
{
  GstPeerflixSrc *src = GST_PEERFLIX_SRC_CAST (object);

  g_free (src->uri);
  g_free (src->location);

  G_OBJECT_CLASS (parent_class)->finalize ((GObject *) src);
}

static void
gst_peerflix_src_class_init (GstPeerflixSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *element_class;

  gobject_class = (GObjectClass *) klass;
  element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));

  gst_element_class_set_static_metadata (element_class,
      "Peerflix source", "Source", "Peerflix torrent streaming source",
      "Arun Raghavan <arun@accosted.com>");

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_peerflix_src_change_state);

  gobject_class->dispose = gst_peerflix_src_dispose;
  gobject_class->finalize = gst_peerflix_src_finalize;
  gobject_class->set_property = gst_peerflix_src_set_property;
  gobject_class->get_property = gst_peerflix_src_get_property;

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "Torrent location URI",
          "Location of the torrent file to use as a URI", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PEERFLIX_PATH,
      g_param_spec_string ("peerflix-path", "Peerflix path",
          "Path to the peerflix binary", DEFAULT_PEERFLIX_PATH,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PORT,
      g_param_spec_int ("port", "Port",
          "Port number to use for peerflix HTTP stream", 1024, 65535,
          DEFAULT_PORT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_peerflix_src_init (GstPeerflixSrc * src)
{
  GstPadTemplate *templ = gst_static_pad_template_get (&src_template);
  src->ghostpad = gst_ghost_pad_new_no_target_from_template ("src", templ);
  gst_object_unref (templ);
  gst_element_add_pad (GST_ELEMENT_CAST (src), src->ghostpad);

  /* haven't added a source yet, make it is detected as a source meanwhile */
  GST_OBJECT_FLAG_SET (src, GST_ELEMENT_FLAG_SOURCE);

  src->peerflix_path = g_strdup (DEFAULT_PEERFLIX_PATH);
  src->port = DEFAULT_PORT;

  src->uri = NULL;
  src->location = NULL;
  src->started = FALSE;
}

static gchar *
read_stream_url (GstPeerflixSrc * src, int outfd)
{
  GIOChannel *iochannel;
  gchar *str, *ret;
  gsize len;
  GError *err = NULL;

  iochannel = g_io_channel_unix_new (outfd);
  if (g_io_channel_read_line (iochannel, &str, &len, NULL, &err) !=
      G_IO_STATUS_NORMAL) {
    GST_ERROR_OBJECT (src, "Could not get stream URL: %s", err->message);
    g_error_free (err);
    return NULL;
  }

  if (len < 24) {
    GST_ERROR_OBJECT (src, "Unexpected peerflix output: '%s'", str);
    return NULL;
  }

  ret = g_strdup (&str[23]);

  g_free (str);
  g_io_channel_shutdown (iochannel, FALSE, NULL);

  return ret;
}

static gboolean
gst_peerflix_src_start_peerflix (GstPeerflixSrc * src)
{
  gint outfd;
  gchar *str;
  gchar *argv[] = { src->peerflix_path, src->location, (gchar *) "-p", NULL,
    (gchar *) "-q", NULL
  };
  GError *err = NULL;

  g_assert (!src->started);

  argv[3] = str = g_strdup_printf ("%d", src->port);
  if (!g_spawn_async_with_pipes (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL,
          NULL, &src->child_pid, NULL, &outfd, NULL, &err)) {
    GST_ERROR_OBJECT (src, "Could not start peerflix: %s", err->message);
    g_free (str);
    g_error_free (err);
    return FALSE;
  }
  g_free (str);

  /* Point souphttpsrc to the HTTP stream peerflix gives us */
  str = read_stream_url (src, outfd);
  if (!str)
    return FALSE;
  g_object_set (src->souphttpsrc, "location", str, NULL);
  g_free (str);

  /* TODO: Could parse out the port and store it, in case the port we requested
   * is not the port that was used. */

  src->started = TRUE;

  return TRUE;
}

static void
gst_peerflix_src_stop_peerflix (GstPeerflixSrc * src)
{
  g_assert (src->started);

  /* TODO: Is there a GLib way to do this? */
  kill (src->child_pid, SIGTERM);
  /* Doesn't actually terminate child - we need to do that ourselves */
  g_spawn_close_pid (src->child_pid);

  src->started = FALSE;
}

static void
gst_peerflix_src_reset (GstPeerflixSrc * src)
{
  if (src->started)
    gst_peerflix_src_stop_peerflix (src);
}

static gboolean
gst_peerflix_src_create_elements (GstPeerflixSrc * src)
{
  GstPad *pad = NULL;

  if (src->elements_created)
    return TRUE;

  GST_DEBUG_OBJECT (src, "Creating internal elements");

  src->souphttpsrc = gst_element_factory_make ("souphttpsrc", NULL);
  if (src->souphttpsrc == NULL)
    goto missing_element;

  gst_bin_add (GST_BIN_CAST (src), src->souphttpsrc);

  pad = gst_element_get_static_pad (src->souphttpsrc, "src");
  gst_ghost_pad_set_target (GST_GHOST_PAD (src->ghostpad), pad);
  gst_object_unref (pad);

  src->elements_created = TRUE;
  return TRUE;

missing_element:
  gst_element_post_message (GST_ELEMENT_CAST (src),
      gst_missing_element_message_new (GST_ELEMENT_CAST (src), "souphttpsrc"));
  GST_ELEMENT_ERROR (src, CORE, MISSING_PLUGIN,
      (("Missing element '%s' - check your GStreamer installation."),
          "souphttpsrc"), (NULL));
  return FALSE;
}

static GstStateChangeReturn
gst_peerflix_src_change_state (GstElement * element, GstStateChange trans)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstPeerflixSrc *src = GST_PEERFLIX_SRC_CAST (element);

  switch (trans) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_peerflix_src_create_elements (src))
        return GST_STATE_CHANGE_FAILURE;

      if (!gst_peerflix_src_start_peerflix (src)) {
        GST_ELEMENT_ERROR (src, RESOURCE, FAILED, ("Could not start peerflix"),
            (NULL));
        return GST_STATE_CHANGE_FAILURE;
      }

      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, trans);

  switch (trans) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_peerflix_src_reset (src);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_peerflix_src_reset (src);
      break;
    default:
      break;
  }

  return ret;
}

static void
gst_peerflix_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPeerflixSrc *src = GST_PEERFLIX_SRC_CAST (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_free (src->location);
      src->location = g_value_dup_string (value);
      break;
    case PROP_PEERFLIX_PATH:
      g_free (src->peerflix_path);
      src->peerflix_path = g_value_dup_string (value);
      break;
    case PROP_PORT:
      src->port = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_peerflix_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstPeerflixSrc *src = GST_PEERFLIX_SRC_CAST (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, src->location);
      break;
    case PROP_PEERFLIX_PATH:
      g_value_set_string (value, src->peerflix_path);
      break;
    case PROP_PORT:
      g_value_set_int (value, src->port);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

gboolean
gst_peerflix_src_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_peerflix_src_debug, "peerflixsrc", 0,
      "PeerflixSrc");
  return gst_element_register (plugin, "peerflixsrc", GST_RANK_NONE,
      gst_peerflix_src_get_type ());
}

/* URI handler interface */

static gboolean
gst_peerflix_src_set_uri (GstPeerflixSrc * src, const gchar * uri,
    GError ** error)
{
  src->uri = g_strdup (uri);

  if (!g_str_has_prefix (uri, "torrent+"))
    goto bad_uri;

  src->location = g_strdup (&uri[8]);
  g_object_notify (G_OBJECT (src), "location");

  return TRUE;

bad_uri:
  {
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
        ("error parsing uri %s", uri));
    g_set_error_literal (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "Could not parse torrent URI");
    return FALSE;
  }
}

static GstURIType
gst_peerflix_src_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_peerflix_src_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { "torrent+http", "torrent+https",
    "torrent+file", NULL
  };

  return protocols;
}

static gchar *
gst_peerflix_src_uri_get_uri (GstURIHandler * handler)
{
  GstPeerflixSrc *src = GST_PEERFLIX_SRC (handler);

  return g_strdup (src->uri);
}

static gboolean
gst_peerflix_src_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  return gst_peerflix_src_set_uri (GST_PEERFLIX_SRC (handler), uri, error);
}

static void
gst_peerflix_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = GST_DEBUG_FUNCPTR (gst_peerflix_src_uri_get_type);
  iface->get_protocols = GST_DEBUG_FUNCPTR (gst_peerflix_src_uri_get_protocols);
  iface->get_uri = GST_DEBUG_FUNCPTR (gst_peerflix_src_uri_get_uri);
  iface->set_uri = GST_DEBUG_FUNCPTR (gst_peerflix_src_uri_set_uri);
}
