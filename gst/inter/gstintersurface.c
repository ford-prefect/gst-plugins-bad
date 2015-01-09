/* GStreamer
 * Copyright (C) 2011 David A. Schleef <ds@schleef.org>
 *               2015 Arun Raghavan <mail@arunraghavan.net>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "gstintersurface.h"

static GList *list;
static GMutex mutex;

#define DEFAULT_BUFFER_MODE GST_DEFERRED_CLIENT_BUFFER_MODE_LATEST_KEYFRAME
#define DEFAULT_MAX_BUFFERS -1

/* Call with client mutex held */
static void
gst_deferred_client_free_queue (GQueue * queue)
{
  GstBuffer *buf;

  while ((buf = GST_BUFFER_CAST (g_queue_pop_head (queue)))) {
    gst_buffer_unref (buf);
  }
}

void
gst_deferred_client_init (GstDeferredClient * client)
{
  g_mutex_init (&client->mutex);

  g_cond_init (&client->caps_cond);
  g_cond_init (&client->buffer_cond);

  client->buffer_mode = DEFAULT_BUFFER_MODE;
  client->max_buffers = DEFAULT_MAX_BUFFERS;
  client->started = FALSE;

  g_queue_init (&client->buffers);
  g_queue_init (&client->headers);
}

void
gst_deferred_client_reset (GstDeferredClient * client)
{
  g_mutex_lock (&client->mutex);

  if (client->caps)
    gst_caps_unref (client->caps);

  gst_deferred_client_free_queue (&client->headers);
  gst_deferred_client_free_queue (&client->buffers);

  client->started = FALSE;

  g_mutex_unlock (&client->mutex);
}

void
gst_deferred_client_free (GstDeferredClient * client)
{
  gst_deferred_client_reset (client);

  g_mutex_clear (&client->mutex);

  g_cond_clear (&client->caps_cond);
  g_cond_clear (&client->buffer_cond);
}

/* Call with client mutex held */
static void
gst_deferred_client_get_stream_headers (GstDeferredClient * client,
    GstCaps * old_caps, GstCaps * new_caps)
{
  gboolean send_streamheader = FALSE;
  GstStructure *s;

  if (!old_caps) {
    GST_DEBUG ("no previous caps for this client, send streamheader");
    send_streamheader = TRUE;
  } else {
    /* there were previous caps recorded, so compare */
    if (!gst_caps_is_equal (old_caps, new_caps)) {
      const GValue *sh1, *sh2;

      /* caps are not equal, but could still have the same streamheader */
      s = gst_caps_get_structure (new_caps, 0);
      if (!gst_structure_has_field (s, "streamheader")) {
        /* no new streamheader, so nothing new to send */
        GST_DEBUG ("new caps do not have streamheader, not sending");
      } else {
        /* there is a new streamheader */
        s = gst_caps_get_structure (old_caps, 0);
        if (!gst_structure_has_field (s, "streamheader")) {
          /* no previous streamheader, so send the new one */
          GST_DEBUG ("previous caps did not have streamheader, sending");
          send_streamheader = TRUE;
        } else {
          /* both old and new caps have streamheader set */
          sh1 = gst_structure_get_value (s, "streamheader");
          s = gst_caps_get_structure (new_caps, 0);
          sh2 = gst_structure_get_value (s, "streamheader");
          if (gst_value_compare (sh1, sh2) != GST_VALUE_EQUAL) {
            GST_DEBUG ("new streamheader different from old, sending");
            send_streamheader = TRUE;
          }
        }
      }
    }
  }

  if (G_UNLIKELY (send_streamheader)) {
    const GValue *sh;
    GArray *buffers;
    int i;

    /* Free any existing headers */
    gst_deferred_client_free_queue (&client->headers);

    GST_LOG ("sending streamheader from caps %" GST_PTR_FORMAT, new_caps);
    s = gst_caps_get_structure (new_caps, 0);

    if (!gst_structure_has_field (s, "streamheader")) {
      GST_DEBUG ("no new streamheader, so nothing to send");
    } else {
      sh = gst_structure_get_value (s, "streamheader");
      g_assert (G_VALUE_TYPE (sh) == GST_TYPE_ARRAY);

      buffers = g_value_peek_pointer (sh);
      GST_DEBUG ("%d streamheader buffers", buffers->len);

      for (i = 0; i < buffers->len; ++i) {
        GValue *bufval;
        GstBuffer *buffer;

        bufval = &g_array_index (buffers, GValue, i);
        g_assert (G_VALUE_TYPE (bufval) == GST_TYPE_BUFFER);
        buffer = g_value_peek_pointer (bufval);
        GST_DEBUG ("queueing streamheader buffer of length %" G_GSIZE_FORMAT,
            gst_buffer_get_size (buffer));

        g_queue_push_tail (&client->headers, gst_buffer_ref (buffer));
      }
    }
  }
}

void
gst_deferred_client_set_caps (GstDeferredClient * client, GstCaps * caps)
{
  g_mutex_lock (&client->mutex);

  /* See if we have new stream headers in caps to pass to the client */
  gst_deferred_client_get_stream_headers (client, client->caps, caps);

  if (client->caps) {
    /* Drop queued buffer if caps changed */
    if (!gst_caps_is_equal (client->caps, caps))
      gst_deferred_client_free_queue (&client->buffers);

    gst_caps_unref (client->caps);
  }

  client->caps = gst_caps_ref (caps);
  client->caps_changed = TRUE;

  g_cond_signal (&client->caps_cond);

  g_mutex_unlock (&client->mutex);
}

GstCaps *
gst_deferred_client_get_caps (GstDeferredClient * client, gboolean * changed,
    gboolean wait)
{
  GstCaps *ret;

  g_mutex_lock (&client->mutex);

  if (client->caps != NULL) {
    /* We have some caps, good to go */
    ret = gst_caps_ref (client->caps);

    if (changed)
      *changed = client->caps_changed;
    client->caps_changed = FALSE;

  } else if (wait) {
    /* We don't have caps, and want to wait till we have some */
    GST_LOG ("Waiting for caps");

    g_cond_wait (&client->caps_cond, &client->mutex);
    g_assert (client->caps);

    ret = gst_caps_ref (client->caps);

    if (changed)
      *changed = TRUE;
    /* changed caps are being returned already */
    client->caps_changed = FALSE;

  } else {
    /* No caps, don't want to wait */
    ret = NULL;
  }

  g_mutex_unlock (&client->mutex);

  return ret;
}

static gboolean
is_sync_frame (GstBuffer * buffer)
{
  if (GST_BUFFER_FLAG_IS_SET (buffer, GST_BUFFER_FLAG_DELTA_UNIT))
    return FALSE;
  else
    return TRUE;
}

void
gst_deferred_client_push_buffer (GstDeferredClient * client, GstBuffer * buf)
{
  GstBuffer *tmp;
  gboolean new_buffer = FALSE;

  g_mutex_lock (&client->mutex);

  switch (client->buffer_mode) {
    case GST_DEFERRED_CLIENT_BUFFER_MODE_LATEST_KEYFRAME:
      if (!client->started) {
        if (is_sync_frame (buf)) {
          GST_DEBUG ("Got new keyframe, dropping previous GOP (if any)");
          gst_deferred_client_free_queue (&client->buffers);
        }

        if (client->max_buffers == -1 ||
            g_queue_get_length (&client->headers) < client->max_buffers) {
          /* We have room in the queue */
          if (is_sync_frame (buf) || !g_queue_is_empty (&client->buffers)) {
            /* Either this is a sync frame, or we already have one queued */
            g_queue_push_tail (&client->buffers, gst_buffer_ref (buf));
            new_buffer = TRUE;
          } else {
            GST_DEBUG ("Ignoring non-keyframe until we see a keyframe");
          }
        } else {
          GST_DEBUG ("Queue is full, emptying and waiting for a new keyframe");
          gst_deferred_client_free_queue (&client->buffers);
        }

        break;
      }
      /* the else case is a fall through */

    case GST_DEFERRED_CLIENT_BUFFER_MODE_LATEST:
      /* Store one buffer at any given time */
      if (!g_queue_is_empty (&client->buffers)) {
        GST_DEBUG ("Replacing unconsumed buffer");
        tmp = GST_BUFFER_CAST (g_queue_pop_head (&client->buffers));
        gst_buffer_unref (tmp);
      }

      g_queue_push_tail (&client->buffers, gst_buffer_ref (buf));
      new_buffer = TRUE;
      break;


    default:
      g_assert_not_reached ();
  }

  if (new_buffer)
    g_cond_signal (&client->buffer_cond);

  g_mutex_unlock (&client->mutex);
}

GstBuffer *
gst_deferred_client_get_buffer (GstDeferredClient * client)
{
  GstBuffer *buf;

  g_mutex_lock (&client->mutex);

  if (!g_queue_is_empty (&client->headers)) {
    buf = g_queue_pop_head (&client->headers);
    goto done;
  }

  if (g_queue_is_empty (&client->buffers)) {
    GST_LOG ("Waiting for a buffer");
    g_cond_wait (&client->buffer_cond, &client->mutex);
  }

  buf = GST_BUFFER_CAST (g_queue_pop_head (&client->buffers));

  client->started = TRUE;

done:
  g_mutex_unlock (&client->mutex);

  return buf;
}

GstInterSurface *
gst_inter_surface_get (const char *name)
{
  GList *g;
  GstInterSurface *surface;

  g_mutex_lock (&mutex);
  for (g = list; g; g = g_list_next (g)) {
    surface = g->data;
    if (strcmp (name, surface->name) == 0) {
      surface->ref_count++;
      g_mutex_unlock (&mutex);
      return surface;
    }
  }

  surface = g_malloc0 (sizeof (GstInterSurface));
  surface->ref_count = 1;
  surface->name = g_strdup (name);
  g_mutex_init (&surface->mutex);
  surface->audio_adapter = gst_adapter_new ();
  surface->audio_buffer_time = DEFAULT_AUDIO_BUFFER_TIME;
  surface->audio_latency_time = DEFAULT_AUDIO_LATENCY_TIME;
  surface->audio_period_time = DEFAULT_AUDIO_PERIOD_TIME;

  gst_deferred_client_init (&surface->app_client);

  list = g_list_append (list, surface);
  g_mutex_unlock (&mutex);

  return surface;
}

void
gst_inter_surface_unref (GstInterSurface * surface)
{
  /* Mutex needed here, otherwise refcount might become 0
   * and someone else requests the same surface again before
   * we remove it from the list */
  g_mutex_lock (&mutex);
  if ((--surface->ref_count) == 0) {
    GList *g;

    for (g = list; g; g = g_list_next (g)) {
      GstInterSurface *tmp = g->data;
      if (strcmp (tmp->name, surface->name) == 0) {
        list = g_list_delete_link (list, g);
        break;
      }
    }

    g_mutex_clear (&surface->mutex);
    gst_buffer_replace (&surface->video_buffer, NULL);
    gst_buffer_replace (&surface->sub_buffer, NULL);
    gst_object_unref (surface->audio_adapter);
    gst_deferred_client_free (&surface->app_client);
    g_free (surface->name);
    g_free (surface);
  }
  g_mutex_unlock (&mutex);
}
