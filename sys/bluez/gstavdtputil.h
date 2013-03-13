/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2004-2010  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2012  Collabora Ltd.
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

#ifndef __GST_AVDTP_UTIL_H
#define __GST_AVDTP_UTIL_H

#include <glib.h>

#include <dbus/dbus.h>

G_BEGIN_DECLS
#define DEFAULT_CODEC_BUFFER_SIZE 2048
#define TEMPLATE_MAX_BITPOOL_STR "64"
    struct bluetooth_data
{
  guint link_mtu;

  DBusConnection *conn;
  guint8 codec;                 /* Bluetooth transport configuration */
  gchar *uuid;
  guint8 *config;
  gint config_size;

  gchar buffer[DEFAULT_CODEC_BUFFER_SIZE];      /* Codec transfer buffer */
};

typedef struct _GstAvdtpConnection GstAvdtpConnection;

struct _GstAvdtpConnection
{
  gchar *device;
  gchar *transport;
  GIOChannel *stream;

  struct bluetooth_data data;
};

gboolean gst_avdtp_connection_acquire (GstAvdtpConnection * conn);
void gst_avdtp_connection_release (GstAvdtpConnection * conn);
gboolean gst_avdtp_connection_get_properties (GstAvdtpConnection * conn);
GstCaps *gst_avdtp_connection_get_caps (GstAvdtpConnection * conn);
void gst_avdtp_connection_set_device (GstAvdtpConnection * conn,
    const char *device);
void gst_avdtp_connection_set_transport (GstAvdtpConnection * conn,
    const char *transport);
gboolean gst_avdtp_connection_conf_recv_stream_fd (GstAvdtpConnection * conn);

G_END_DECLS
#endif
