/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define GLIB_DISABLE_DEPRECATION_WARNINGS

#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <pthread.h>

#include <netinet/in.h>

#include <bluetooth/bluetooth.h>

#include <gst/rtp/gstrtpbuffer.h>

#include <dbus/dbus.h>

#include "rtp.h"
#include "a2dp-codecs.h"

#include "gstpragma.h"
#include "gstavdtpsink.h"

GST_DEBUG_CATEGORY_STATIC (avdtp_sink_debug);
#define GST_CAT_DEFAULT avdtp_sink_debug

#define CRC_PROTECTED 1
#define CRC_UNPROTECTED 0

#define DEFAULT_AUTOCONNECT TRUE

#define GST_AVDTP_SINK_MUTEX_LOCK(s) G_STMT_START {	\
		g_mutex_lock(s->sink_lock);		\
	} G_STMT_END

#define GST_AVDTP_SINK_MUTEX_UNLOCK(s) G_STMT_START {	\
		g_mutex_unlock(s->sink_lock);		\
	} G_STMT_END

#define IS_SBC(n) (strcmp((n), "audio/x-sbc") == 0)
#define IS_MPEG_AUDIO(n) (strcmp((n), "audio/mpeg") == 0)

enum
{
  PROP_0,
  PROP_DEVICE,
  PROP_AUTOCONNECT,
  PROP_TRANSPORT
};

GST_BOILERPLATE (GstAvdtpSink, gst_avdtp_sink, GstBaseSink, GST_TYPE_BASE_SINK);

static const GstElementDetails avdtp_sink_details =
GST_ELEMENT_DETAILS ("Bluetooth AVDTP sink",
    "Sink/Audio",
    "Plays audio to an A2DP device",
    "Marcel Holtmann <marcel@holtmann.org>");

static GstStaticPadTemplate avdtp_sink_factory =
    GST_STATIC_PAD_TEMPLATE ("sink", GST_PAD_SINK, GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp, "
        "media = (string) \"audio\","
        "payload = (int) "
        GST_RTP_PAYLOAD_DYNAMIC_STRING ", "
        "clock-rate = (int) { 16000, 32000, "
        "44100, 48000 }, "
        "encoding-name = (string) \"SBC\"; "
        "application/x-rtp, "
        "media = (string) \"audio\", "
        "payload = (int) "
        GST_RTP_PAYLOAD_MPA_STRING ", "
        "clock-rate = (int) 90000; "
        "application/x-rtp, "
        "media = (string) \"audio\", "
        "payload = (int) "
        GST_RTP_PAYLOAD_DYNAMIC_STRING ", "
        "clock-rate = (int) 90000, " "encoding-name = (string) \"MPA\""));

static gboolean
gst_avdtp_sink_stop (GstBaseSink * basesink)
{
  GstAvdtpSink *self = GST_AVDTP_SINK (basesink);

  GST_INFO_OBJECT (self, "stop");

  if (self->watch_id != 0) {
    g_source_remove (self->watch_id);
    self->watch_id = 0;
  }

  gst_avdtp_connection_release (&self->conn);

  if (self->stream_caps) {
    gst_caps_unref (self->stream_caps);
    self->stream_caps = NULL;
  }

  if (self->dev_caps) {
    gst_caps_unref (self->dev_caps);
    self->dev_caps = NULL;
  }

  return TRUE;
}

static void
gst_avdtp_sink_finalize (GObject * object)
{
  GstAvdtpSink *self = GST_AVDTP_SINK (object);

  gst_avdtp_sink_stop (GST_BASE_SINK (self));

  gst_avdtp_connection_reset (&self->conn);

  g_mutex_free (self->sink_lock);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_avdtp_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstAvdtpSink *sink = GST_AVDTP_SINK (object);

  switch (prop_id) {
    case PROP_DEVICE:
      gst_avdtp_connection_set_device (&sink->conn, g_value_get_string (value));
      break;

    case PROP_AUTOCONNECT:
      sink->autoconnect = g_value_get_boolean (value);
      break;

    case PROP_TRANSPORT:
      gst_avdtp_connection_set_transport (&sink->conn,
          g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_avdtp_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstAvdtpSink *sink = GST_AVDTP_SINK (object);

  switch (prop_id) {
    case PROP_DEVICE:
      g_value_set_string (value, sink->conn.device);
      break;

    case PROP_AUTOCONNECT:
      g_value_set_boolean (value, sink->autoconnect);
      break;

    case PROP_TRANSPORT:
      g_value_set_string (value, sink->conn.transport);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gint
gst_avdtp_sink_get_channel_mode (const gchar * mode)
{
  if (strcmp (mode, "stereo") == 0)
    return SBC_CHANNEL_MODE_STEREO;
  else if (strcmp (mode, "joint-stereo") == 0)
    return SBC_CHANNEL_MODE_JOINT_STEREO;
  else if (strcmp (mode, "dual-channel") == 0)
    return SBC_CHANNEL_MODE_DUAL_CHANNEL;
  else if (strcmp (mode, "mono") == 0)
    return SBC_CHANNEL_MODE_MONO;
  else
    return -1;
}

static void
gst_avdtp_sink_tag (const GstTagList * taglist,
    const gchar * tag, gpointer user_data)
{
  gboolean crc;
  gchar *channel_mode = NULL;
  GstAvdtpSink *self = GST_AVDTP_SINK (user_data);

  if (strcmp (tag, "has-crc") == 0) {

    if (!gst_tag_list_get_boolean (taglist, tag, &crc)) {
      GST_WARNING_OBJECT (self, "failed to get crc tag");
      return;
    }

    gst_avdtp_sink_set_crc (self, crc);

  } else if (strcmp (tag, "channel-mode") == 0) {

    if (!gst_tag_list_get_string (taglist, tag, &channel_mode)) {
      GST_WARNING_OBJECT (self, "failed to get channel-mode tag");
      return;
    }

    self->channel_mode = gst_avdtp_sink_get_channel_mode (channel_mode);
    if (self->channel_mode == -1)
      GST_WARNING_OBJECT (self, "Received invalid channel "
          "mode: %s", channel_mode);
    g_free (channel_mode);

  } else
    GST_DEBUG_OBJECT (self, "received unused tag: %s", tag);
}

static gboolean
gst_avdtp_sink_event (GstBaseSink * basesink, GstEvent * event)
{
  GstAvdtpSink *self = GST_AVDTP_SINK (basesink);
  GstTagList *taglist = NULL;

  if (GST_EVENT_TYPE (event) == GST_EVENT_TAG) {
    /* we check the tags, mp3 has tags that are importants and
     * are outside caps */
    gst_event_parse_tag (event, &taglist);
    gst_tag_list_foreach (taglist, gst_avdtp_sink_tag, self);
  }

  return TRUE;
}

static gboolean
gst_avdtp_sink_start (GstBaseSink * basesink)
{
  GstAvdtpSink *self = GST_AVDTP_SINK (basesink);

  GST_INFO_OBJECT (self, "start");

  self->stream_caps = NULL;
  self->mp3_using_crc = -1;
  self->channel_mode = -1;

  if (self->conn.transport == NULL)
    return FALSE;

  if (!gst_avdtp_connection_acquire (&self->conn)) {
    GST_ERROR_OBJECT (self, "Failed to acquire connection");
    return FALSE;
  }

  if (!gst_avdtp_connection_get_properties (&self->conn)) {
    GST_ERROR_OBJECT (self, "Failed to get transport properties");
    return FALSE;
  }

  if (self->dev_caps)
    gst_caps_unref (self->dev_caps);

  self->dev_caps = gst_avdtp_connection_get_caps (&self->conn);

  if (!self->dev_caps) {
    GST_ERROR_OBJECT (self, "Failed to get device caps");
    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "Got connection caps: " GST_PTR_FORMAT,
      self->dev_caps);

  return TRUE;
}

static GstFlowReturn
gst_avdtp_sink_preroll (GstBaseSink * basesink, GstBuffer * buffer)
{
  GstAvdtpSink *sink = GST_AVDTP_SINK (basesink);
  gboolean ret;

  GST_AVDTP_SINK_MUTEX_LOCK (sink);

  ret = gst_avdtp_connection_conf_recv_stream_fd (&sink->conn);

  GST_AVDTP_SINK_MUTEX_UNLOCK (sink);

  if (!ret)
    return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

static GstFlowReturn
gst_avdtp_sink_render (GstBaseSink * basesink, GstBuffer * buffer)
{
  GstAvdtpSink *self = GST_AVDTP_SINK (basesink);
  ssize_t ret;
  int fd;

  /* FIXME: temporary sanity check */
  g_assert (!(g_io_channel_get_flags (self->conn.stream) & G_IO_FLAG_NONBLOCK));

  /* FIXME: why not use g_io_channel_write_chars() instead? */
  fd = g_io_channel_unix_get_fd (self->conn.stream);
  ret = write (fd, GST_BUFFER_DATA (buffer), GST_BUFFER_SIZE (buffer));
  if (ret < 0) {
    GST_ERROR_OBJECT (self, "Error while writting to socket: %s",
        strerror (errno));
    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static gboolean
gst_avdtp_sink_unlock (GstBaseSink * basesink)
{
  GstAvdtpSink *self = GST_AVDTP_SINK (basesink);

  if (self->conn.stream != NULL)
    g_io_channel_flush (self->conn.stream, NULL);

  return TRUE;
}

static GstFlowReturn
gst_avdtp_sink_buffer_alloc (GstBaseSink * basesink,
    guint64 offset, guint size, GstCaps * caps, GstBuffer ** buf)
{
  GstAvdtpSink *self = GST_AVDTP_SINK (basesink);

  *buf = gst_buffer_new_and_alloc (size);
  if (!(*buf)) {
    GST_ERROR_OBJECT (self, "buffer allocation failed");
    return GST_FLOW_ERROR;
  }

  gst_buffer_set_caps (*buf, caps);

  GST_BUFFER_OFFSET (*buf) = offset;

  return GST_FLOW_OK;
}

static void
gst_avdtp_sink_class_init (GstAvdtpSinkClass * klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GstBaseSinkClass *basesink_class = GST_BASE_SINK_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = GST_DEBUG_FUNCPTR (gst_avdtp_sink_finalize);
  object_class->set_property = GST_DEBUG_FUNCPTR (gst_avdtp_sink_set_property);
  object_class->get_property = GST_DEBUG_FUNCPTR (gst_avdtp_sink_get_property);

  basesink_class->start = GST_DEBUG_FUNCPTR (gst_avdtp_sink_start);
  basesink_class->stop = GST_DEBUG_FUNCPTR (gst_avdtp_sink_stop);
  basesink_class->render = GST_DEBUG_FUNCPTR (gst_avdtp_sink_render);
  basesink_class->preroll = GST_DEBUG_FUNCPTR (gst_avdtp_sink_preroll);
  basesink_class->unlock = GST_DEBUG_FUNCPTR (gst_avdtp_sink_unlock);
  basesink_class->event = GST_DEBUG_FUNCPTR (gst_avdtp_sink_event);

  basesink_class->buffer_alloc =
      GST_DEBUG_FUNCPTR (gst_avdtp_sink_buffer_alloc);

  g_object_class_install_property (object_class, PROP_DEVICE,
      g_param_spec_string ("device", "Device",
          "Bluetooth remote device address", NULL, G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_AUTOCONNECT,
      g_param_spec_boolean ("auto-connect",
          "Auto-connect",
          "Automatically attempt to connect "
          "to device", DEFAULT_AUTOCONNECT, G_PARAM_READWRITE));

  g_object_class_install_property (object_class, PROP_TRANSPORT,
      g_param_spec_string ("transport",
          "Transport", "Use configured transport", NULL, G_PARAM_READWRITE));

  GST_DEBUG_CATEGORY_INIT (avdtp_sink_debug, "avdtpsink", 0,
      "A2DP headset sink element");
}

static void
gst_avdtp_sink_init (GstAvdtpSink * self, GstAvdtpSinkClass * klass)
{
  self->conn.device = NULL;
  self->conn.transport = NULL;

  self->conn.stream = NULL;

  self->dev_caps = NULL;

  self->autoconnect = DEFAULT_AUTOCONNECT;

  self->sink_lock = g_mutex_new ();

  /* FIXME this is for not synchronizing with clock, should be tested
   * with devices to see the behaviour
   gst_base_sink_set_sync(GST_BASE_SINK(self), FALSE);
   */
}

gboolean
gst_avdtp_sink_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "avdtpsink", GST_RANK_NONE,
      GST_TYPE_AVDTP_SINK);
}


/* public functions */
GstCaps *
gst_avdtp_sink_get_device_caps (GstAvdtpSink * sink)
{
  if (sink->dev_caps == NULL)
    return NULL;

  return gst_caps_copy (sink->dev_caps);
}

gboolean
gst_avdtp_sink_set_device_caps (GstAvdtpSink * self, GstCaps * caps)
{
  GST_DEBUG_OBJECT (self, "setting device caps");
  GST_AVDTP_SINK_MUTEX_LOCK (self);

  if (self->stream_caps)
    gst_caps_unref (self->stream_caps);
  self->stream_caps = gst_caps_ref (caps);

  GST_AVDTP_SINK_MUTEX_UNLOCK (self);

  return TRUE;
}

guint
gst_avdtp_sink_get_link_mtu (GstAvdtpSink * sink)
{
  return sink->conn.data.link_mtu;
}

void
gst_avdtp_sink_set_device (GstAvdtpSink * self, const gchar * dev)
{
  if (self->conn.device != NULL)
    g_free (self->conn.device);

  GST_LOG_OBJECT (self, "Setting device: %s", dev);
  self->conn.device = g_strdup (dev);
}

void
gst_avdtp_sink_set_transport (GstAvdtpSink * self, const gchar * trans)
{
  if (self->conn.transport != NULL)
    g_free (self->conn.transport);

  GST_LOG_OBJECT (self, "Setting transport: %s", trans);
  self->conn.transport = g_strdup (trans);
}

gchar *
gst_avdtp_sink_get_device (GstAvdtpSink * self)
{
  return g_strdup (self->conn.device);
}

gchar *
gst_avdtp_sink_get_transport (GstAvdtpSink * self)
{
  return g_strdup (self->conn.transport);
}

void
gst_avdtp_sink_set_crc (GstAvdtpSink * self, gboolean crc)
{
  gint new_crc;

  new_crc = crc ? CRC_PROTECTED : CRC_UNPROTECTED;

  /* test if we already received a different crc */
  if (self->mp3_using_crc != -1 && new_crc != self->mp3_using_crc) {
    GST_WARNING_OBJECT (self, "crc changed during stream");
    return;
  }
  self->mp3_using_crc = new_crc;

}

void
gst_avdtp_sink_set_channel_mode (GstAvdtpSink * self, const gchar * mode)
{
  gint new_mode;

  new_mode = gst_avdtp_sink_get_channel_mode (mode);

  if (self->channel_mode != -1 && new_mode != self->channel_mode) {
    GST_WARNING_OBJECT (self, "channel mode changed during stream");
    return;
  }

  self->channel_mode = new_mode;
  if (self->channel_mode == -1)
    GST_WARNING_OBJECT (self, "Received invalid channel " "mode: %s", mode);
}
