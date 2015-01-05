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

void
gst_deferred_client_init (GstDeferredClient * client)
{
  g_mutex_init (&client->mutex);

  g_cond_init (&client->caps_cond);
  g_cond_init (&client->buffer_cond);

  client->buffer = NULL;
}

void
gst_deferred_client_reset (GstDeferredClient * client)
{
  g_mutex_lock (&client->mutex);

  if (client->caps)
    gst_caps_unref (client->caps);

  if (client->buffer)
    client->buffer = NULL;

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

void
gst_deferred_client_set_caps (GstDeferredClient * client, GstCaps * caps)
{
  g_mutex_lock (&client->mutex);

  if (client->caps) {
    /* Drop queued buffer if caps changed */
    if (!gst_caps_is_equal (client->caps, caps) && client->buffer) {
      gst_buffer_unref (client->buffer);
      client->buffer = NULL;
    }

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

void
gst_deferred_client_push_buffer (GstDeferredClient * client, GstBuffer * buf)
{
  g_mutex_lock (&client->mutex);

  if (client->buffer) {
    GST_DEBUG ("Replacing unconsumed buffer");
    gst_buffer_unref (client->buffer);
  }

  client->buffer = gst_buffer_ref (buf);
  g_cond_signal (&client->buffer_cond);

  g_mutex_unlock (&client->mutex);
}

GstBuffer *
gst_deferred_client_get_buffer (GstDeferredClient * client)
{
  GstBuffer *buf;

  g_mutex_lock (&client->mutex);

  if (!client->buffer) {
    GST_LOG ("Waiting for a buffer");
    g_cond_wait (&client->buffer_cond, &client->mutex);
  }

  buf = client->buffer;
  client->buffer = NULL;

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
