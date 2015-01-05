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
 * SECTION:element-gstinterappsink
 *
 * The interappsink element is a sink element used in connection with a
 * interappsrc element in a different pipeline.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v ... ! interappsink
 * ]|
 * 
 * The interappsink element cannot be used effectively with gst-launch,
 * as it requires a second pipeline in the application to send audio.
 * See the gstintertest.c example in the gst-plugins-bad source code for
 * more details.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>
#include "gstinterappsink.h"

GST_DEBUG_CATEGORY_STATIC (gst_inter_app_sink_debug_category);
#define GST_CAT_DEFAULT gst_inter_app_sink_debug_category

/* prototypes */
static void gst_inter_app_sink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_inter_app_sink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_inter_app_sink_finalize (GObject * object);

static gboolean gst_inter_app_sink_start (GstBaseSink * sink);
static gboolean gst_inter_app_sink_stop (GstBaseSink * sink);
static gboolean
gst_inter_app_sink_set_caps (GstBaseSink * sink, GstCaps * caps);
static GstFlowReturn
gst_inter_app_sink_render (GstBaseSink * sink, GstBuffer * buffer);

enum
{
  PROP_0,
  PROP_CHANNEL
};

/* pad templates */
static GstStaticPadTemplate gst_inter_app_sink_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );


/* class initialization */
#define parent_class gst_inter_app_sink_parent_class
G_DEFINE_TYPE (GstInterAppSink, gst_inter_app_sink, GST_TYPE_BASE_SINK);

static void
gst_inter_app_sink_class_init (GstInterAppSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSinkClass *base_sink_class = GST_BASE_SINK_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  GST_DEBUG_CATEGORY_INIT (gst_inter_app_sink_debug_category, "interappsink", 0,
      "debug category for interappsink element");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_inter_app_sink_sink_template));

  gst_element_class_set_static_metadata (element_class,
      "Internal communication sink",
      "Sink",
      "Virtual sink for internal process communication",
      "Arun Raghavan <mail@arunraghavan.net>");

  gobject_class->set_property = gst_inter_app_sink_set_property;
  gobject_class->get_property = gst_inter_app_sink_get_property;
  gobject_class->finalize = gst_inter_app_sink_finalize;
  base_sink_class->start = GST_DEBUG_FUNCPTR (gst_inter_app_sink_start);
  base_sink_class->stop = GST_DEBUG_FUNCPTR (gst_inter_app_sink_stop);
  base_sink_class->set_caps = GST_DEBUG_FUNCPTR (gst_inter_app_sink_set_caps);
  base_sink_class->render = GST_DEBUG_FUNCPTR (gst_inter_app_sink_render);

  g_object_class_install_property (gobject_class, PROP_CHANNEL,
      g_param_spec_string ("channel", "Channel",
          "Channel name to match inter src and sink elements",
          "default", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
gst_inter_app_sink_init (GstInterAppSink * interappsink)
{
  interappsink->channel = g_strdup ("default");
}

void
gst_inter_app_sink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstInterAppSink *interappsink = GST_INTER_APP_SINK (object);

  switch (property_id) {
    case PROP_CHANNEL:
      g_free (interappsink->channel);
      interappsink->channel = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_inter_app_sink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstInterAppSink *interappsink = GST_INTER_APP_SINK (object);

  switch (property_id) {
    case PROP_CHANNEL:
      g_value_set_string (value, interappsink->channel);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
gst_inter_app_sink_finalize (GObject * object)
{
  GstInterAppSink *interappsink = GST_INTER_APP_SINK (object);

  g_free (interappsink->channel);
  interappsink->channel = NULL;

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_inter_app_sink_start (GstBaseSink * sink)
{
  GstInterAppSink *interappsink = GST_INTER_APP_SINK (sink);

  GST_LOG_OBJECT (interappsink, "start");

  interappsink->surface = gst_inter_surface_get (interappsink->channel);

  return TRUE;
}

static gboolean
gst_inter_app_sink_stop (GstBaseSink * sink)
{
  GstInterAppSink *interappsink = GST_INTER_APP_SINK (sink);

  GST_LOG_OBJECT (interappsink, "stop");

  gst_deferred_client_reset (&interappsink->surface->app_client);

  gst_inter_surface_unref (interappsink->surface);
  interappsink->surface = NULL;

  return TRUE;
}

static gboolean
gst_inter_app_sink_set_caps (GstBaseSink * sink, GstCaps * caps)
{
  GstInterAppSink *interappsink = GST_INTER_APP_SINK (sink);

  GST_LOG_OBJECT (interappsink, "set caps");

  gst_deferred_client_set_caps (&interappsink->surface->app_client, caps);

  return TRUE;
}

static GstFlowReturn
gst_inter_app_sink_render (GstBaseSink * sink, GstBuffer * buffer)
{
  GstInterAppSink *interappsink = GST_INTER_APP_SINK (sink);

  GST_LOG_OBJECT (interappsink, "render");

  gst_deferred_client_push_buffer (&interappsink->surface->app_client, buffer);

  return GST_FLOW_OK;
}
