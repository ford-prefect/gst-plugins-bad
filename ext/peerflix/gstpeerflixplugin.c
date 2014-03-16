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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "peerflixsrc.h"

GST_DEBUG_CATEGORY (peerflix_debug);

static gboolean
peerflix_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (peerflix_debug, "peerflix", 0, "peerflix");

  if (!gst_peerflix_src_plugin_init (plugin))
    return FALSE;

  return TRUE;
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    peerflix,
    "Peerflix plugin",
    peerflix_init, VERSION, "LGPL", PACKAGE_NAME, "http://www.gstreamer.org/")
