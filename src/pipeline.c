#include <gst/gst.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pipeline.h"
#include "alsautility.h"
#include "misc.h"
#include "settings.h"
#include "ui.h"
#include "v4l2utility.h"
#include "fxpreviews/dice.h"
#include "fxpreviews/edge.h"
#include "fxpreviews/hflip.h"
#include "fxpreviews/hulk.h"
#include "fxpreviews/mauve.h"
#include "fxpreviews/noir.h"
#include "fxpreviews/saturation.h"
#include "fxpreviews/shagadelic.h"
#include "fxpreviews/vertigo.h"
#include "fxpreviews/vflip.h"
#include "fxpreviews/warp.h"

GstElement *pipeline;
GstElement *src;
GstElement *capsfilter;
GstElement *videoconvert;
GstElement *decoder;
GstElement *frameratefilter;
GstElement *tee;
GstElement *preview_queue;
GstElement *preview_videoconvert;
GstElement *preview_overlay;
GstElement *preview_sink;
GstElement *pixbuf_queue;
GstElement *pixbuf_sink;
GstElement *fx;
GstElement *fx_videoconvert;
GstElement *record_queue;
GstElement *record_videorate;
GstElement *record_encoder;
GstElement *record_videoconvert;
GstElement *record_muxer;
GstElement *record_filesink;
GstElement *record_alsasrc;
GstElement *record_audioconvert;
GstElement *record_audioqueue;
GstElement *record_audioencoder;
GdkPixbuf *videopreview;
char* ccomplete_filename;

int video_recording = 0;

camoramic_gst_fx *fx_list;
int fx_count = 0;
int fx_current = 0;
int fx_prev_set = 0;

double overlay_opacity = 0;

gboolean camoramic_gst_take_photo_fx_thread(gpointer user_data)
{
	if (overlay_opacity <= 0.01) {	
		overlay_opacity = 0;
		//g_usleep(500000);
		char *device_name = camoramic_v4l2util_get_friendly_name(current_device);
		char *statusbar_text = malloc(strlen(_("Current device: %s")) + strlen(device_name) + 1);
		sprintf(statusbar_text, _("Current device: %s"), device_name);
		camoramic_ui_update_statusbar(statusbar_text);
		free(statusbar_text);
		free(device_name);
		return FALSE;
	}
	overlay_opacity = overlay_opacity - 0.01;
	return TRUE;
}

void camoramic_gst_take_photo()
{
    if (overlay_opacity != 0)
    {
        return;
    }
    char *complete_filename;
    char *filename = malloc(512);

    time_t time_object = time(NULL);
    struct tm *time_struct = localtime(&time_object);

    if (photo_use_jpeg == TRUE)
    {
        strftime(filename, 512, "photo-%d-%m-%Y-%H-%M-%S.jpg", time_struct);
    }
    else
    {
        strftime(filename, 512, "photo-%d-%m-%Y-%H-%M-%S.png", time_struct);
    }
    char *statusbar_text = malloc(strlen(_("Photo '%s' taken")) + strlen(filename) + 1);
    sprintf(statusbar_text, _("Photo '%s' taken"), filename);
    camoramic_ui_update_statusbar(statusbar_text);
    free(statusbar_text);

    complete_filename = malloc(strlen(photo_path) + strlen("/") + strlen(filename) + 1);
    strcpy(complete_filename, photo_path);
    strcat(complete_filename, "/");
    strcat(complete_filename, filename);

    GdkPixbuf *photo;
    g_object_get(pixbuf_sink, "last-pixbuf", &photo, NULL);
    while (photo == NULL)
    {
        g_object_get(pixbuf_sink, "last-pixbuf", &photo, NULL);
    }

    if (photo_use_jpeg == TRUE)
    {
        char *quality = malloc(512);
        snprintf(quality, 512, "%d", photo_jpeg_amount);
        gdk_pixbuf_save(photo, complete_filename, "jpeg", NULL, "quality", quality, NULL);
        free(quality);
    }
    else
    {
        gdk_pixbuf_save(photo, complete_filename, "png", NULL, NULL);
    }

	overlay_opacity = 1;
    camoramic_sfx_play("camoramic-shutter-event", "camera-shutter");
    
    double width = (double)gdk_pixbuf_get_width(photo);
    double height = (double)gdk_pixbuf_get_height(photo);
	double width_c;
    int gcd = camoramic_get_gcd(width, height);
    width = width/gcd;
    height = height/gcd;
	width_c = round(55/height);
	width = width*width_c;
	height = 55;
	
	GdkPixbuf* preview = gdk_pixbuf_scale_simple(photo, width, height, GDK_INTERP_BILINEAR);
    gtk_list_store_prepend(pastphotos_store, &pastphotos_iter);
    gtk_list_store_set(pastphotos_store, &pastphotos_iter, 0, preview, -1);
    gdk_pixbuf_unref(preview);
    
	past_media = g_list_prepend(past_media, complete_filename);
	camoramic_settings_save_past_media();
    g_timeout_add(5, camoramic_gst_take_photo_fx_thread, NULL);
    
	//free(complete_filename);
    free(filename);
    gdk_pixbuf_unref(photo);
}

int camoramic_gst_check_if_element_exists(char *name)
{
    if (gst_element_factory_make(name, NULL) == NULL)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

void camoramic_gst_check_element(GstElement *elem, char *name)
{
    if (elem == NULL)
    {
        char *message = malloc(strlen(name) + strlen(_("Unable to create element: %s")) + 1);
        sprintf(message, _("Unable to create element: %s"), name);
        camoramic_set_error(message);
    }
}

void camoramic_gst_draw_preview_overlay(GstElement *overlay, cairo_t *cr, guint64 timestamp, guint64 duration, gpointer user_data)
{
	//printf("test%f\n", fabs(overlay_opacity));
	cairo_set_source_rgba(cr, 1, 1, 1, fabs(overlay_opacity));
	cairo_paint(cr);		
}

void camoramic_gst_rebuild_pipeline_v4lsrc()
{
    gst_element_set_state(pipeline, GST_STATE_NULL);

    gst_element_unlink_many(src, capsfilter, decoder, videoconvert, NULL);
    gst_bin_remove_many(GST_BIN(pipeline), src, decoder, NULL);

    src = gst_element_factory_make("v4l2src", NULL);
    camoramic_gst_check_element(src, "v4l2src");
    char *devicestr = camoramic_v4l2util_device_to_string(current_device);
    g_object_set(src, "device", devicestr, NULL);
    free(devicestr);

    GstCaps *caps = camoramic_v4l2util_get_caps(current_device, current_device_caps);
    g_object_set(capsfilter, "caps", caps, NULL);

    char *format = camoramic_v4l2util_get_format(current_device, current_device_caps);
    if (strcmp(format, "image/jpeg") == 0)
    {
        decoder = gst_element_factory_make("jpegdec", NULL);
    }
    else if (strcmp(format, "video/x-raw") == 0)
    {
        decoder = gst_element_factory_make("funnel", NULL);
    }
    else if (strcmp(format, "video/x-dv") == 0)
    {
        decoder = gst_element_factory_make("dvdec", NULL);
    }
    else if (strcmp(format, "video/x-bayer") == 0)
    {
        decoder = gst_element_factory_make("bayer2rgb", NULL);
    }
    else
    {
        decoder = gst_element_factory_make("decodebin", NULL);
    }
    free(format);

    camoramic_gst_check_element(decoder, "decoder");
    if ((camoramic_v4l2util_get_caps_framerate(current_device, current_device_caps) <= 4) ||
        (camoramic_v4l2util_get_caps_framerate(current_device, current_device_caps) > 75))
    {
        GstCaps *ffcaps = gst_caps_new_simple("video/x-raw", "framerate", GST_TYPE_FRACTION, 24, 1, NULL);
        g_object_set(frameratefilter, "caps", ffcaps, NULL);
    }
    else
    {
        g_object_set(frameratefilter, "caps", NULL, NULL);
    }

    gst_bin_add_many(GST_BIN(pipeline), src, decoder, NULL);
    gst_element_link_many(src, capsfilter, decoder, videoconvert, NULL);
    gst_element_set_state(pipeline, GST_STATE_PLAYING);
}

void camoramic_gst_record_start()
{
    char *complete_filename;
    char *filename = malloc(512);

    time_t time_object = time(NULL);
    struct tm *time_struct = localtime(&time_object);

    switch (current_video_container)
    {
    case CAMORAMIC_CONTAINER_WEBM:
        strftime(filename, 512, "video-%d-%m-%Y-%H-%M-%S.webm", time_struct);
        break;
    case CAMORAMIC_CONTAINER_MKV:
        strftime(filename, 512, "video-%d-%m-%Y-%H-%M-%S.mkv", time_struct);
        break;
    case CAMORAMIC_CONTAINER_MP4:
        strftime(filename, 512, "video-%d-%m-%Y-%H-%M-%S.mp4", time_struct);
        break;
    default:
        strftime(filename, 512, "video-%d-%m-%Y-%H-%M-%S.ogv", time_struct);
    }

    char *statusbar_text = malloc(strlen(_("Recording video '%s'")) + strlen(filename) + 1);
    sprintf(statusbar_text, _("Recording video '%s'"), filename);
    camoramic_ui_update_statusbar(statusbar_text);
    free(statusbar_text);

    complete_filename = malloc(strlen(video_path) + strlen("/") + strlen(filename) + 1);
    strcpy(complete_filename, video_path);
    strcat(complete_filename, "/");
    strcat(complete_filename, filename);
    gst_element_set_state(pipeline, GST_STATE_PAUSED);

    if (record_audio == TRUE)
    {
        record_alsasrc = gst_element_factory_make("alsasrc", NULL);
        camoramic_gst_check_element(record_alsasrc, "record_alsasrc");
        char *devicestr = malloc(512);
        snprintf(devicestr, 512, "dsnoop:%d,%d", current_alsa_device.card, current_alsa_device.device);
        g_object_set(record_alsasrc, "device", devicestr, NULL);
        free(devicestr);

        record_audioqueue = gst_element_factory_make("queue", NULL);
        camoramic_gst_check_element(record_audioqueue, "record_audioqueue");
        g_object_set(record_audioqueue, "max-size-buffers", 0, NULL);
        g_object_set(record_audioqueue, "max-size-bytes", 0, NULL);
        g_object_set(record_audioqueue, "max-size-time", 20000000000, NULL);

        record_audioconvert = gst_element_factory_make("audioconvert", NULL);
        camoramic_gst_check_element(record_audioconvert, "record_audioconvert");

        switch (current_audio_codec)
        {
        case CAMORAMIC_AUDIO_CODEC_OPUS:
            record_audioencoder = gst_element_factory_make("opusenc", NULL);
            break;
        case CAMORAMIC_AUDIO_CODEC_SPEEX:
            record_audioencoder = gst_element_factory_make("speexenc", NULL);
            break;
        case CAMORAMIC_AUDIO_CODEC_AAC:
            record_audioencoder = gst_element_factory_make("voaacenc", NULL);
            break;
        case CAMORAMIC_AUDIO_CODEC_AC3:
            record_audioencoder = gst_element_factory_make("avenc_ac3", NULL);
            break;
        default:
            record_audioencoder = gst_element_factory_make("vorbisenc", NULL);
        }
        camoramic_gst_check_element(record_audioencoder, "record_audioencoder");
    }

    record_queue = gst_element_factory_make("queue", NULL);
    camoramic_gst_check_element(record_queue, "record_queue");
    g_object_set(record_queue, "max-size-buffers", 0, NULL);
    g_object_set(record_queue, "max-size-bytes", 0, NULL);
    g_object_set(record_queue, "max-size-time", 20000000000, NULL);

    record_videoconvert = gst_element_factory_make("videoconvert", NULL);
    camoramic_gst_check_element(record_videoconvert, "record_videoconvert");

    record_videorate = gst_element_factory_make("videorate", NULL);
    camoramic_gst_check_element(record_videorate, "record_videorate");

    switch (current_video_codec)
    {
    case CAMORAMIC_CODEC_VP8:
        record_encoder = gst_element_factory_make("vp8enc", NULL);
        g_object_set(record_encoder, "target-bitrate", current_video_bitrate * 1000, NULL);
        break;
    case CAMORAMIC_CODEC_VP9:
        record_encoder = gst_element_factory_make("vp9enc", NULL);
        g_object_set(record_encoder, "target-bitrate", current_video_bitrate * 1000, NULL);
        break;
    case CAMORAMIC_CODEC_AV1:
        record_encoder = gst_element_factory_make("av1enc", NULL);
        g_object_set(record_encoder, "usage-profile", 1, NULL);
        g_object_set(record_encoder, "target-bitrate", current_video_bitrate, NULL);
        break;
    case CAMORAMIC_CODEC_H264:
        record_encoder = gst_element_factory_make("x264enc", NULL);
        g_object_set(record_encoder, "tune", 0x00000004, NULL);
        g_object_set(record_encoder, "bitrate", current_video_bitrate, NULL);
        break;
    case CAMORAMIC_CODEC_H265:
        record_encoder = gst_element_factory_make("x265enc", NULL);
        g_object_set(record_encoder, "tune", 4, NULL);
        g_object_set(record_encoder, "bitrate", current_video_bitrate, NULL);
        break;
    default:
        record_encoder = gst_element_factory_make("theoraenc", NULL);
        g_object_set(record_encoder, "bitrate", current_video_bitrate, NULL);
    }
    camoramic_gst_check_element(record_encoder, "record_encoder");

    switch (current_video_container)
    {
    case CAMORAMIC_CONTAINER_WEBM:
        record_muxer = gst_element_factory_make("webmmux", NULL);
        break;
    case CAMORAMIC_CONTAINER_MKV:
        record_muxer = gst_element_factory_make("matroskamux", NULL);
        break;
    case CAMORAMIC_CONTAINER_MP4:
        record_muxer = gst_element_factory_make("mp4mux", NULL);
        break;
    default:
        record_muxer = gst_element_factory_make("oggmux", NULL);
    }
    camoramic_gst_check_element(record_muxer, "record_muxer");

    record_filesink = gst_element_factory_make("filesink", NULL);
    camoramic_gst_check_element(record_filesink, "record_filesink");
    g_object_set(record_filesink, "location", complete_filename, NULL);

    gst_bin_add_many(GST_BIN(pipeline), record_queue, record_videoconvert, record_videorate, record_encoder,record_muxer, record_filesink, NULL);
    gst_element_link_many(tee, record_queue, record_videoconvert, record_videorate, record_encoder, record_muxer,record_filesink, NULL);
    if (record_audio == TRUE)
    {
        gst_bin_add_many(GST_BIN(pipeline), record_alsasrc, record_audioqueue, record_audioconvert, record_audioencoder,NULL);
        gst_element_link_many(record_alsasrc, record_audioqueue, record_audioconvert, record_audioencoder, record_muxer,NULL);
    }
    gst_element_set_state(pipeline, GST_STATE_PLAYING);


	ccomplete_filename = strdup(complete_filename);
    free(complete_filename);
    free(filename);
    video_recording = 1;
    g_object_get(pixbuf_sink, "last-pixbuf", &videopreview, NULL);
    while (videopreview == NULL)
    {
        g_object_get(pixbuf_sink, "last-pixbuf", &videopreview, NULL);
    }
}

gboolean camoramic_gst_record_stop_bus_call(GstBus *bus, GstMessage *msg, gpointer user_data)
{
    switch (GST_MESSAGE_TYPE(msg))
    {
    case GST_MESSAGE_EOS: {
		gst_element_set_state (pipeline, GST_STATE_PAUSED);
		gst_element_set_state (pipeline, GST_STATE_READY);
		gst_element_set_state (pipeline, GST_STATE_NULL);
        char *device_name = camoramic_v4l2util_get_friendly_name(current_device);
        char *statusbar_text = malloc(strlen(_("Current device: %s")) + strlen(device_name) + 1);
        sprintf(statusbar_text, _("Current device: %s"), device_name);
        camoramic_ui_update_statusbar(statusbar_text);
        free(statusbar_text);
        free(device_name);
		gst_element_unlink_many(tee, record_queue, record_videoconvert, record_videorate, record_encoder, record_muxer,record_filesink, NULL);
		if (record_audio == TRUE)
		{
			gst_element_unlink_many(record_alsasrc, record_audioqueue, record_audioconvert, record_audioencoder, record_muxer, NULL);
			gst_bin_remove_many(GST_BIN(pipeline), record_alsasrc, record_audioqueue, record_audioconvert, record_audioencoder, NULL);		
		}
		gst_bin_remove_many(GST_BIN(pipeline), record_queue, record_videoconvert, record_videorate, record_encoder,record_muxer, record_filesink, NULL);
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
        video_recording = 0;
        gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(recordvideobutton), GTK_STOCK_MEDIA_RECORD);
        gtk_tool_button_set_label(GTK_TOOL_BUTTON(recordvideobutton), _("Record Video"));
        gtk_widget_set_sensitive(GTK_WIDGET(prefbutton), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(recordvideobutton), TRUE);
        gtk_widget_set_sensitive(GTK_WIDGET(fxbutton), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(takephotobutton), TRUE);
		
		double width = (double)gdk_pixbuf_get_width(videopreview);
		double height = (double)gdk_pixbuf_get_height(videopreview);
		double width_c;
		int gcd = camoramic_get_gcd(width, height);
		width = width/gcd;
		height = height/gcd;
		width_c = round(55/height);
		width = width*width_c;
		height = 55;
		GdkPixbuf* resized = gdk_pixbuf_scale_simple(videopreview, width, height, GDK_INTERP_BILINEAR);
		GdkPixbuf* videooverlayimage_resized = gdk_pixbuf_scale_simple(videooverlayimage, width, height, GDK_INTERP_BILINEAR);
		gdk_pixbuf_composite(videooverlayimage_resized, resized, 0, 0, width, height, 0, 0, 1, 1, GDK_INTERP_BILINEAR, 255);
		gtk_list_store_prepend(pastphotos_store, &pastphotos_iter);
		gtk_list_store_set(pastphotos_store, &pastphotos_iter, 0, resized, -1);	
		gdk_pixbuf_unref(videopreview);
		gdk_pixbuf_unref(videooverlayimage_resized);
		past_media = g_list_prepend(past_media, ccomplete_filename);
		camoramic_settings_save_past_media();
        return FALSE;
    }
    }
    return TRUE;
}

void camoramic_gst_record_stop()
{
    gtk_widget_set_sensitive(GTK_WIDGET(takephotobutton), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(recordvideobutton), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(fxbutton), FALSE);
    camoramic_ui_update_statusbar(_("Finalizing recording..."));
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    uint bus_watch_id = gst_bus_add_watch(bus, camoramic_gst_record_stop_bus_call, NULL);
    gst_object_unref(bus);
    gst_element_send_event(pipeline, gst_event_new_eos());
}

void camoramic_gst_init_fx()
{
    fx_count = 11;
    fx_list = calloc(fx_count, sizeof(camoramic_gst_fx));

    camoramic_gst_fx mauvefx;
    mauvefx.name = "videobalance";
    mauvefx.friendly_name = _("Mauve");
    mauvefx.param_name = "saturation";
    mauvefx.param2_name = "hue";
    mauvefx.param = 1.5;
    mauvefx.param2 = +0.5;
    mauvefx.param_is_int = 0;
    mauvefx.preview = gdk_pixbuf_new_from_inline(-1, mauve, FALSE, NULL);
    fx_list[0] = mauvefx;

    camoramic_gst_fx noirfx;
    noirfx.name = "videobalance";
    noirfx.friendly_name = _("Monochrome");
    noirfx.param_name = "saturation";
    noirfx.param2_name = "";
    noirfx.param = 0;
    noirfx.param2 = 0;
    noirfx.param_is_int = 0;
    noirfx.preview = gdk_pixbuf_new_from_inline(-1, noir, FALSE, NULL);
    fx_list[1] = noirfx;

    camoramic_gst_fx saturationfx;
    saturationfx.name = "videobalance";
    saturationfx.friendly_name = _("Saturation");
    saturationfx.param_name = "saturation";
    saturationfx.param2_name = "";
    saturationfx.param = 2;
    saturationfx.param2 = 0;
    saturationfx.param_is_int = 0;
    saturationfx.preview = gdk_pixbuf_new_from_inline(-1, saturation, FALSE, NULL);
    fx_list[2] = saturationfx;

    camoramic_gst_fx hulkfx;
    hulkfx.name = "videobalance";
    hulkfx.friendly_name = _("Hulk");
    hulkfx.param_name = "saturation";
    hulkfx.param2_name = "hue";
    hulkfx.param = 1.5;
    hulkfx.param2 = -0.5;
    hulkfx.param_is_int = 0;
    hulkfx.preview = gdk_pixbuf_new_from_inline(-1, hulk, FALSE, NULL);
    fx_list[3] = hulkfx;

    camoramic_gst_fx vflipfx;
    vflipfx.name = "videoflip";
    vflipfx.friendly_name = _("Vertical Flip");
    vflipfx.param_name = "method";
    vflipfx.param2_name = "";
    vflipfx.param = 5;
    vflipfx.param2 = 0;
    vflipfx.param_is_int = 1;
    vflipfx.preview = gdk_pixbuf_new_from_inline(-1, vflip, FALSE, NULL);
    fx_list[4] = vflipfx;

    camoramic_gst_fx hflipfx;
    hflipfx.name = "videoflip";
    hflipfx.friendly_name = _("Horizontal Flip");
    hflipfx.param_name = "method";
    hflipfx.param2_name = "";
    hflipfx.param = 4;
    hflipfx.param2 = 0;
    hflipfx.param_is_int = 1;
    hflipfx.preview = gdk_pixbuf_new_from_inline(-1, hflip, FALSE, NULL);
    fx_list[5] = hflipfx;

    camoramic_gst_fx shagadelicfx;
    shagadelicfx.name = "shagadelictv";
    shagadelicfx.friendly_name = _("Shagadelic");
    shagadelicfx.param_name = "";
    shagadelicfx.param2_name = "";
    shagadelicfx.param = 0;
    shagadelicfx.param2 = 0;
    shagadelicfx.param_is_int = 0;
    shagadelicfx.preview = gdk_pixbuf_new_from_inline(-1, shagadelic, FALSE, NULL);
    fx_list[6] = shagadelicfx;

    camoramic_gst_fx vertigofx;
    vertigofx.name = "vertigotv";
    vertigofx.friendly_name = _("Vertigo");
    vertigofx.param_name = "";
    vertigofx.param2_name = "";
    vertigofx.param = 0;
    vertigofx.param2 = 0;
    vertigofx.param_is_int = 0;
    vertigofx.preview = gdk_pixbuf_new_from_inline(-1, vertigo, FALSE, NULL);
    fx_list[7] = vertigofx;

    camoramic_gst_fx edgefx;
    edgefx.name = "edgetv";
    edgefx.friendly_name = _("Edge");
    edgefx.param_name = "";
    edgefx.param2_name = "";
    edgefx.param = 0;
    edgefx.param2 = 0;
    edgefx.param_is_int = 0;
    edgefx.preview = gdk_pixbuf_new_from_inline(-1, edge, FALSE, NULL);
    fx_list[8] = edgefx;

    camoramic_gst_fx dicefx;
    dicefx.name = "dicetv";
    dicefx.friendly_name = _("Dice");
    dicefx.param_name = "";
    dicefx.param2_name = "";
    dicefx.param = 0;
    dicefx.param2 = 0;
    dicefx.param_is_int = 0;
    dicefx.preview = gdk_pixbuf_new_from_inline(-1, dice, FALSE, NULL);
    fx_list[9] = dicefx;

    camoramic_gst_fx warptv;
    warptv.name = "warptv";
    warptv.friendly_name = _("Warp");
    warptv.param_name = "";
    warptv.param2_name = "";
    warptv.param = 0;
    warptv.param2 = 0;
    warptv.param_is_int = 0;
    warptv.preview = gdk_pixbuf_new_from_inline(-1, warp, FALSE, NULL);
    fx_list[10] = warptv;
}
void camoramic_gst_set_fx(int fxno)
{
    camoramic_gst_fx fxp = fx_list[fxno - 1];

    if (fxno != 0)
    {
        gst_element_set_state(pipeline, GST_STATE_PAUSED);
        if (fx_current == 0)
        {
            gst_element_unlink(frameratefilter, tee);
        }
        else
        {
            gst_element_unlink_many(frameratefilter, fx, fx_videoconvert, tee, NULL);
            gst_bin_remove_many(GST_BIN(pipeline), fx, fx_videoconvert, NULL);
        }
        fx_current = fxno;

        fx = gst_element_factory_make(fxp.name, NULL);
        camoramic_gst_check_element(tee, fxp.name);
        if (strcmp(fxp.param_name, "") != 0)
        {
            if (fxp.param_is_int == 0)
            {
                g_object_set(fx, fxp.param_name, fxp.param, NULL);
            }
            else
            {
                g_object_set(fx, fxp.param_name, (int)fxp.param, NULL);
            }
        }
        if (strcmp(fxp.param2_name, "") != 0)
        {
            g_object_set(fx, fxp.param2_name, fxp.param2, NULL);
        }

        fx_videoconvert = gst_element_factory_make("videoconvert", NULL);
        camoramic_gst_check_element(fx_videoconvert, "videoconvert");
        fx_prev_set = 1;

        gst_bin_add_many(GST_BIN(pipeline), fx, fx_videoconvert, NULL);
        gst_element_link_many(frameratefilter, fx, fx_videoconvert, tee, NULL);
        gst_element_set_state(pipeline, GST_STATE_PLAYING);
    }
    else
    {
        if (fx_current != 0)
        {
            if (video_recording == 0)
            {
				gst_element_set_state (pipeline, GST_STATE_NULL);
            }
            else
            {
                gst_element_set_state(pipeline, GST_STATE_PAUSED);
            }

            gst_element_unlink_many(frameratefilter, fx, fx_videoconvert, tee, NULL);
            gst_bin_remove_many(GST_BIN(pipeline), fx, fx_videoconvert, NULL);            
            gst_element_link(frameratefilter, tee);
            gst_element_set_state(pipeline, GST_STATE_PLAYING);
            fx_current = 0;
            fx_prev_set = 0;
        }
    }
}

void camoramic_gst_create_pipeline()
{
    pipeline = gst_pipeline_new("camoramic");

    src = gst_element_factory_make("v4l2src", NULL);
    camoramic_gst_check_element(src, "v4l2src");
    char *devicestr = camoramic_v4l2util_device_to_string(current_device);
    g_object_set(src, "device", devicestr, NULL);
    free(devicestr);

    capsfilter = gst_element_factory_make("capsfilter", NULL);
    camoramic_gst_check_element(capsfilter, "capsfilter");
    GstCaps *caps = camoramic_v4l2util_get_caps(current_device, current_device_caps);
    g_object_set(capsfilter, "caps", caps, NULL);

    char *format = camoramic_v4l2util_get_format(current_device, current_device_caps);
    if (strcmp(format, "image/jpeg") == 0)
    {
        decoder = gst_element_factory_make("jpegdec", NULL);
        camoramic_gst_check_element(decoder, "jpegdec");
    }
    else if (strcmp(format, "video/x-raw") == 0)
    {
        decoder = gst_element_factory_make("funnel", NULL);
        camoramic_gst_check_element(decoder, "funnel");
    }
    else if (strcmp(format, "video/x-dv") == 0)
    {
        decoder = gst_element_factory_make("dvdec", NULL);
        camoramic_gst_check_element(decoder, "dvdec");
    }
    else if (strcmp(format, "video/x-bayer") == 0)
    {
        decoder = gst_element_factory_make("bayer2rgb", NULL);
        camoramic_gst_check_element(decoder, "bayer2rgb");
    }
    else
    {
        decoder = gst_element_factory_make("decodebin", NULL);
        camoramic_gst_check_element(decoder, "decodebin");
    }
    free(format);

    videoconvert = gst_element_factory_make("videoconvert", NULL);
    camoramic_gst_check_element(videoconvert, "videoconvert");

    tee = gst_element_factory_make("tee", NULL);
    camoramic_gst_check_element(tee, "tee");

    frameratefilter = gst_element_factory_make("capsfilter", NULL);
    camoramic_gst_check_element(frameratefilter, "capsfilter");
    if ((camoramic_v4l2util_get_caps_framerate(current_device, current_device_caps) <= 4) ||
        (camoramic_v4l2util_get_caps_framerate(current_device, current_device_caps) > 75))
    {
        GstCaps *ffcaps = gst_caps_new_simple("video/x-raw", "framerate", GST_TYPE_FRACTION, 24, 1, NULL);
        g_object_set(frameratefilter, "caps", ffcaps, NULL);
    }
    else
    {
        g_object_set(frameratefilter, "caps", NULL, NULL);
    }

    preview_queue = gst_element_factory_make("queue", NULL);
    camoramic_gst_check_element(preview_queue, "preview_queue");
    g_object_set(preview_queue, "max-size-buffers", 0, NULL);
    g_object_set(preview_queue, "max-size-bytes", 0, NULL);
    g_object_set(preview_queue, "max-size-time", 20000000000, NULL);

    preview_videoconvert = gst_element_factory_make("videoconvert", NULL);
    camoramic_gst_check_element(preview_queue, "preview_queue");

    preview_overlay = gst_element_factory_make("cairooverlay", NULL);
    camoramic_gst_check_element(preview_overlay, "preview_overlay");
    g_signal_connect(preview_overlay, "draw", G_CALLBACK(camoramic_gst_draw_preview_overlay), NULL);

    preview_sink = gst_element_factory_make("gtksink", NULL);
    camoramic_gst_check_element(preview_sink, "preview_sink");

    pixbuf_sink = gst_element_factory_make("gdkpixbufsink", NULL);
    camoramic_gst_check_element(pixbuf_sink, "pixbuf_sink");
    g_object_set(pixbuf_sink, "post-messages", FALSE, NULL);

    pixbuf_queue = gst_element_factory_make("queue", NULL);
    camoramic_gst_check_element(pixbuf_queue, "pixbuf_queue");
    g_object_set(pixbuf_queue, "max-size-buffers", 0, NULL);
    g_object_set(pixbuf_queue, "max-size-bytes", 0, NULL);
    g_object_set(pixbuf_queue, "max-size-time", 20000000000, NULL);

    gst_bin_add_many(GST_BIN(pipeline), src, capsfilter, decoder, videoconvert, frameratefilter, tee, pixbuf_queue,
                     pixbuf_sink, preview_queue, preview_videoconvert, preview_overlay, preview_sink, NULL);
    gst_element_link_many(src, capsfilter, decoder, videoconvert, frameratefilter, tee, NULL);

    gst_element_link_many(tee, pixbuf_queue, pixbuf_sink, NULL);
    gst_element_link_many(tee, preview_queue, preview_videoconvert, preview_overlay, preview_sink, NULL);
}
