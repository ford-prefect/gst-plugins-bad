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
#ifndef _GST_PEERFLIX_SRC_H_
#define _GST_PEERFLIX_SRC_H_

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_PEERFLIX_SRC (gst_peerflix_src_get_type())
#define GST_PEERFLIX_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PEERFLIX_SRC,GstPeerflixSrc))
#define GST_PEERFLIX_SRC_CAST(obj) ((GstPeerflixSrc *) obj)
#define GST_PEERFLIX_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PEERFLIX_SRC,GstPeerflixSrcClass))
#define GST_IS_PEERFLIX_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PEERFLIX_SRC))
#define GST_IS_PEERFLIX_SRC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PEERFLIX_SRC))

typedef struct _GstPeerflixSrc GstPeerflixSrc;
typedef struct _GstPeerflixSrcClass GstPeerflixSrcClass;

struct _GstPeerflixSrc
{
  GstBin bin;

  GstPad *ghostpad;
  GstElement *souphttpsrc;

  gboolean elements_created;

  gchar *location;
  gchar *peerflix_path;
  gint port;

  GPid child_pid;
  gboolean started;
};

struct _GstPeerflixSrcClass
{
  GstBinClass bin_class;
};

GType gst_peerflix_src_get_type (void);
gboolean gst_peerflix_src_plugin_init (GstPlugin * plugin);

G_END_DECLS

#endif
