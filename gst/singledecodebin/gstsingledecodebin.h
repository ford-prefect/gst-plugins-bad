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

#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct {
  GstBin parent;

  gboolean connected;
  gboolean parse_only;
} GstSingleDecodeBin;

typedef struct {
  GstBinClass parent;
} GstSingleDecodeBinClass;

#define GST_TYPE_SINGLE_DECODE_BIN \
  (gst_single_decode_bin_get_type ())
#define GST_SINGLE_DECODE_BIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_SINGLE_DECODE_BIN, \
                               GstSingleDecodeBin))
#define GST_SINGLE_DECODE_BIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_SINGLE_DECODE_BIN, \
                            GstSingleDecodeBinClass))
#define GST_IS_SINGLE_DECODE_BIN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_SINGLE_DECODE_BIN))
#define GST_IS_SINGLE_DECODE_BIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_SINGLE_DECODE_BIN))

GType gst_single_decode_bin_get_type (void);

G_END_DECLS
