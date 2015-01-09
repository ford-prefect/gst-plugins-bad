/* GStreamer
 * Copyright (C) 2011 David A. Schleef <ds@schleef.org>
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

#ifndef _GST_INTER_SURFACE_H_
#define _GST_INTER_SURFACE_H_

#include <gst/base/gstadapter.h>
#include <gst/audio/audio.h>
#include <gst/video/video.h>

G_BEGIN_DECLS

typedef struct _GstDeferredClient GstDeferredClient;

typedef enum {
  GST_DEFERRED_CLIENT_BUFFER_MODE_LATEST,
  GST_DEFERRED_CLIENT_BUFFER_MODE_LATEST_KEYFRAME,
} GstDeferredClientBufferMode;

struct _GstDeferredClient
{
  GMutex mutex;

  GstCaps *caps;
  gboolean caps_changed;

  GstDeferredClientBufferMode buffer_mode;
  gint max_buffers;
  gboolean started; /* TRUE if the client has started consuming buffers */

  GQueue buffers;
  GQueue headers;

  GCond caps_cond;
  GCond buffer_cond;
};

void gst_deferred_client_init (GstDeferredClient * client);
void gst_deferred_client_reset (GstDeferredClient * client);
void gst_deferred_client_free (GstDeferredClient * client);
void gst_deferred_client_set_caps (GstDeferredClient * client, GstCaps * caps);
GstCaps * gst_deferred_client_get_caps (GstDeferredClient * client,
    gboolean * changed, gboolean wait);
void gst_deferred_client_push_buffer (GstDeferredClient * client,
    GstBuffer * buf);
GstBuffer * gst_deferred_client_get_buffer (GstDeferredClient * client);

typedef struct _GstInterSurface GstInterSurface;

struct _GstInterSurface
{
  GMutex mutex;
  gint ref_count;

  char *name;

  /* video */
  GstVideoInfo video_info;
  int video_buffer_count;

  /* audio */
  GstAudioInfo audio_info;
  guint64 audio_buffer_time;
  guint64 audio_latency_time;
  guint64 audio_period_time;

  /* app */
  GstDeferredClient app_client;

  GstBuffer *video_buffer;
  GstBuffer *sub_buffer;
  GstAdapter *audio_adapter;
};

#define DEFAULT_AUDIO_BUFFER_TIME  (GST_SECOND)
#define DEFAULT_AUDIO_LATENCY_TIME (100 * GST_MSECOND)
#define DEFAULT_AUDIO_PERIOD_TIME  (25 * GST_MSECOND)


GstInterSurface * gst_inter_surface_get (const char *name);
void gst_inter_surface_unref (GstInterSurface *surface);


G_END_DECLS

#endif
