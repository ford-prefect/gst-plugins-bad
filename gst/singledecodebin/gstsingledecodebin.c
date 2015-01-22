/* GStreamer
 * Copyright (C) 2015 Centricular Ltd.
 * Author: Arun Raghavan <arun@centricular.com>
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
 * SECTION:element-gstsingledecodebin
 *
 * The singledecodebin element takes a given stream and just decodes it. As a
 * result, it provides exactly one sink and source pad.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstsingledecodebin.h"

#define parent_class gst_single_decode_bin_parent_class
G_DEFINE_TYPE (GstSingleDecodeBin, gst_single_decode_bin, GST_TYPE_BIN);

GST_DEBUG_CATEGORY_STATIC (gst_single_decode_bin_debug_category);
#define GST_CAT_DEFAULT gst_single_decode_bin_debug_category

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

static void
dbin_pad_added_cb (GstElement * dbin, GstPad * pad,
    gpointer user_data G_GNUC_UNUSED)
{
  GstPad *srcpad;
  GstSingleDecodeBin *sdbin =
      GST_SINGLE_DECODE_BIN (gst_element_get_parent (dbin));

  if (sdbin->connected) {
    GST_WARNING_OBJECT (sdbin, "Ignoring new decodebin pad");
    return;
  }

  srcpad = gst_element_get_static_pad (GST_ELEMENT (sdbin), "src");
  gst_ghost_pad_set_target (GST_GHOST_PAD (srcpad), pad);
  gst_object_unref (srcpad);

  sdbin->connected = TRUE;
}

static void
gst_single_decode_bin_init (GstSingleDecodeBin * sdbin)
{
  GstElement *decodebin;
  GstPad *sinkpad;

  decodebin = gst_element_factory_make ("decodebin", "singledecodebin-dbin");
  g_return_if_fail (decodebin != NULL);

  g_signal_connect (decodebin, "pad-added", G_CALLBACK (dbin_pad_added_cb),
      NULL);

  gst_bin_add (GST_BIN (sdbin), decodebin);

  sinkpad = gst_element_get_static_pad (decodebin, "sink");
  gst_element_add_pad (GST_ELEMENT (sdbin),
      gst_ghost_pad_new ("sink", sinkpad));
  gst_object_unref (sinkpad);

  gst_element_add_pad (GST_ELEMENT (sdbin),
      gst_ghost_pad_new_no_target ("src", GST_PAD_SRC));

  sdbin->connected = FALSE;
}

static void
gst_single_decode_bin_class_init (GstSingleDecodeBinClass * sdbin_class)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (sdbin_class);

  gst_element_class_set_static_metadata (element_class, "Single Decode Bin",
      "Decoder/Bin", "Decode a single stream",
      "Arun Raghavan <arun@centricular.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));

  GST_DEBUG_CATEGORY_INIT (gst_single_decode_bin_debug_category,
      "singledecodebin", 0, "debug category for the single decodebin element");
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "singledecodebin", GST_RANK_NONE,
      GST_TYPE_SINGLE_DECODE_BIN);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    singledecodebin,
    "Decodes a single stream",
    plugin_init, VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN);
