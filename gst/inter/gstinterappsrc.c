/* GStreamer
 * Copyright (C) 2011 David Schleef <ds@entropywave.com>
 * Copyright (C) 2014 Arun Raghavan <mail@@arunraghavan.net>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstinterappsrc
 *
 * The interappsrc element is a source element used in connection with a
 * interappsink element in a different pipeline, allowing communication of
 * data between the two pipelines.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v interappsrc ! kateenc ! oggmux ! filesink location=out.ogv
 * ]|
 * 
 * The interappsrc element cannot be used effectively with gst-launch,
 * as it requires a second pipeline in the application to send data.
 * See the gstintertest.c example in the gst-plugins-bad source code for
 * more details.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include "gstinterappsrc.h"

GST_DEBUG_CATEGORY_STATIC (gst_inter_app_src_debug_category);
#define GST_CAT_DEFAULT gst_inter_app_src_debug_category

/* prototypes */
static void gst_inter_app_src_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_inter_app_src_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_inter_app_src_finalize (GObject * object);

static gboolean gst_inter_app_src_start (GstBaseSrc * src);
static gboolean gst_inter_app_src_stop (GstBaseSrc * src);
static GstFlowReturn
gst_inter_app_src_create (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf);

#define DEFAULT_TIMEOUT 2000000000      /* 2000 ms */

enum
{
  PROP_0,
  PROP_CHANNEL
};

/* pad templates */
static GstStaticPadTemplate gst_inter_app_src_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

/* class initialization */
#define parent_class gst_inter_app_src_parent_class
G_DEFINE_TYPE (GstInterAppSrc, gst_inter_app_src, GST_TYPE_BASE_SRC);

static void
gst_inter_app_src_class_init (GstInterAppSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_inter_app_src_debug_category, "interappsrc", 0,
      "debug category for interappsrc element");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_inter_app_src_src_template));

  gst_element_class_set_static_metadata (element_class,
      "Internal communication source",
      "Source",
      "Virtual source for internal process communication",
      "Arun Raghavan <mail@arunraghavan.net>");

  gobject_class->set_property = gst_inter_app_src_set_property;
  gobject_class->get_property = gst_inter_app_src_get_property;
  gobject_class->finalize = gst_inter_app_src_finalize;
  base_src_class->start = GST_DEBUG_FUNCPTR (gst_inter_app_src_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_inter_app_src_stop);
  base_src_class->create = GST_DEBUG_FUNCPTR (gst_inter_app_src_create);

  g_object_class_install_property (gobject_class, PROP_CHANNEL,
      g_param_spec_string ("channel", "Channel",
          "Channel name to match inter src and sink elements",
          "default", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_inter_app_src_init (GstInterAppSrc * interappsrc)
{
  gst_base_src_set_format (GST_BASE_SRC (interappsrc), GST_FORMAT_TIME);
  gst_base_src_set_live (GST_BASE_SRC (interappsrc), TRUE);

  interappsrc->channel = g_strdup ("default");
}

void
gst_inter_app_src_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstInterAppSrc *interappsrc = GST_INTER_APP_SRC (object);

  switch (property_id) {
    case PROP_CHANNEL:
      g_free (interappsrc->channel);
      interappsrc->channel = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_inter_app_src_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstInterAppSrc *interappsrc = GST_INTER_APP_SRC (object);

  switch (property_id) {
    case PROP_CHANNEL:
      g_value_set_string (value, interappsrc->channel);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_inter_app_src_finalize (GObject * object)
{
  GstInterAppSrc *interappsrc = GST_INTER_APP_SRC (object);

  g_free (interappsrc->channel);
  interappsrc->channel = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_inter_app_src_start (GstBaseSrc * src)
{
  GstInterAppSrc *interappsrc = GST_INTER_APP_SRC (src);

  GST_DEBUG_OBJECT (interappsrc, "start");

  interappsrc->surface = gst_inter_surface_get (interappsrc->channel);

  return TRUE;
}

static gboolean
gst_inter_app_src_stop (GstBaseSrc * src)
{
  GstInterAppSrc *interappsrc = GST_INTER_APP_SRC (src);

  GST_DEBUG_OBJECT (interappsrc, "stop");

  gst_inter_surface_unref (interappsrc->surface);
  interappsrc->surface = NULL;

  return TRUE;
}

static GstFlowReturn
gst_inter_app_src_create (GstBaseSrc * src, guint64 offset, guint size,
    GstBuffer ** buf)
{
  GstInterAppSrc *interappsrc = GST_INTER_APP_SRC (src);
  GstBuffer *buffer = NULL;
  GstCaps *caps = NULL;
  gboolean changed;

  GST_DEBUG_OBJECT (interappsrc, "create");

  caps = gst_deferred_client_get_caps (&interappsrc->surface->app_client,
      &changed, TRUE);

  buffer = gst_deferred_client_get_buffer (&interappsrc->surface->app_client);

  if (changed) {
    GST_DEBUG_OBJECT (interappsrc, "Got caps: %" GST_PTR_FORMAT, caps);

    if (!gst_base_src_set_caps (src, caps)) {
      GST_ERROR_OBJECT (interappsrc, "Failed to set caps");
      gst_caps_unref (caps);
      return GST_FLOW_NOT_NEGOTIATED;
    }

    gst_caps_unref (caps);
  }

  GST_LOG_OBJECT (interappsrc, "Pushing %u bytes",
      (unsigned int) gst_buffer_get_size (buffer));

  *buf = buffer;

  return GST_FLOW_OK;
}
