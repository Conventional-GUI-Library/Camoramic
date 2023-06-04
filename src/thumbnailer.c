#include <gst/gst.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdlib.h>
#include <string.h>
#include "pipeline.h"

void camoramic_thumbnailer_generate(char* filename, GdkPixbuf** pixbuf)
{
  GstElement * playbin;
  GstElement * sink;

  playbin = gst_element_factory_make("playbin", "source");
  camoramic_gst_check_element(playbin, "playbin");
  char* fn = malloc(strlen("file://")+strlen(filename)+1);
  strcpy(fn, "file://");
  strcat(fn, filename);
  g_object_set(playbin, "uri", fn, NULL);
  free(fn);
  
  sink = gst_element_factory_make("gdkpixbufsink", NULL);
  camoramic_gst_check_element(sink, "sink");
  g_object_set(sink, "post-messages", FALSE, NULL);
  g_object_set(playbin, "video-sink", sink, NULL);

  gst_element_set_state(playbin, GST_STATE_PLAYING);

  g_object_get(sink, "last-pixbuf",pixbuf, NULL);
  while (*pixbuf == NULL)
  {
      g_object_get(sink, "last-pixbuf", pixbuf, NULL);
  }
  gst_element_set_state(playbin, GST_STATE_NULL);
}
