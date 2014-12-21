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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_INTER_APP_SRC_H_
#define _GST_INTER_APP_SRC_H_

#include <gst/base/gstbasesrc.h>
#include "gstintersurface.h"

G_BEGIN_DECLS
#define GST_TYPE_INTER_APP_SRC   (gst_inter_app_src_get_type())
#define GST_INTER_APP_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_INTER_APP_SRC,GstInterAppSrc))
#define GST_INTER_APP_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_INTER_APP_SRC,GstInterAppSrcClass))
#define GST_IS_INTER_APP_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_INTER_APP_SRC))
#define GST_IS_INTER_APP_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_INTER_APP_SRC))
typedef struct _GstInterAppSrc GstInterAppSrc;
typedef struct _GstInterAppSrcClass GstInterAppSrcClass;

struct _GstInterAppSrc
{
  GstBaseSrc parent;

  GstInterSurface *surface;
  char *channel;

  gboolean got_caps;
};

struct _GstInterAppSrcClass
{
  GstBaseSrcClass parent_class;
};

GType gst_inter_app_src_get_type (void);

G_END_DECLS
#endif
