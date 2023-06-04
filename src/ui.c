#include <gst/gst.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>
#include <math.h>
#include <stdlib.h>
#include "ui.h"
#include "alsautility.h"
#include "fxpreviews/nofx.h"
#include "misc.h"
#include "pipeline.h"
#include "settings.h"
#include "v4l2utility.h"
#include "thumbnailer.h"
#include "videooverlay.h"
 
GtkWidget *window;
GtkWidget *box;
GtkWidget *pastphotos;
GtkWidget *statusbar;
GtkWidget *preview;
GtkListStore *pastphotos_store;
GtkTreeIter pastphotos_iter;

GtkWidget *settings_dialog;
GtkWidget *notebook;
GtkWidget *videodevice;
GtkWidget *videomode;
GtkWidget *audiodevice;
GtkWidget *photopathbutton;
GtkWidget *photoformat;
GtkWidget *photocompression;
GtkWidget *recordaudio;
GtkWidget *videocontainer;
GtkWidget *videocodec;
GtkWidget *videoaudiocodec;
GtkWidget *videopathbutton;
GtkWidget *audiocodec;
GtkWidget *videobitrate;

GtkWidget *fx_dialog;
GtkWidget *fx_iconview;
GdkPixbuf *nonepreview;
GdkPixbuf *videooverlayimage;
GtkTreeModel *fx_model;

GtkToolItem *takephotobutton;
GtkToolItem *recordvideobutton;
GtkToolItem *fxbutton;
GtkToolItem *prefbutton;
GtkAccelGroup *accel_group = NULL;

int ui_created = 0;
int audiodevice_activated = 0;
int current_selected_item; 
GtkTreeIter current_selected_item_iter; 

enum
{
    PCOL_PIXBUF,
    PNUM_COLS
};

enum
{
    COL_DISPLAY_NAME,
    COL_PIXBUF,
    NUM_COLS
};

void camoramic_ui_kill_widget(GtkWidget *widget, gpointer data)
{
    gtk_widget_destroy(GTK_WIDGET(data));
}


void camoramic_ui_show_error_dialog(char *message)
{
    GtkWidget *dialog;
    GtkWidget *dwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    dialog = gtk_message_dialog_new(GTK_WINDOW(dwindow), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, message);
    gtk_window_set_title(GTK_WINDOW(dialog), "Camoramic");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    gtk_widget_destroy(dwindow);
}



GtkTreeModel *camoramic_ui_fx_dialog_init_model()
{
    GtkListStore *list_store;
    GtkTreeIter iter;

    list_store = gtk_list_store_new(NUM_COLS, G_TYPE_STRING, GDK_TYPE_PIXBUF);

    gtk_list_store_append(list_store, &iter);
    gtk_list_store_set(list_store, &iter, COL_DISPLAY_NAME, _("None"), COL_PIXBUF, nonepreview, -1);

    for (int i = 0; i < fx_count; ++i)
    {
        gtk_list_store_append(list_store, &iter);
        gtk_list_store_set(list_store, &iter, COL_DISPLAY_NAME, fx_list[i].friendly_name, COL_PIXBUF, fx_list[i].preview, -1);
    }
    return GTK_TREE_MODEL(list_store);
}

void camoramic_ui_fx_dialog_killed(GtkWidget *widget, gpointer data)
{
    GList *selected_item_list = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(fx_iconview));
    gpointer selected_item = g_list_nth_data(selected_item_list, 0);
    gint *selected_item_list2 = gtk_tree_path_get_indices((GtkTreePath *)(selected_item));
    camoramic_gst_set_fx(selected_item_list2[0]);
    gtk_tree_path_free((GtkTreePath *)(selected_item));
    g_list_free(selected_item_list);
}

void camoramic_ui_show_fx_dialog(GtkWidget *widget, gpointer data)
{

    GtkWidget *content_area;
    GtkWidget *action_area;
    GtkWidget *button_alignment;
    GtkWidget *scrolled_window;
    GtkWidget *apply_button;

    fx_dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(fx_dialog), _("Effects"));
    gtk_window_set_modal(GTK_WINDOW(fx_dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(fx_dialog), GTK_WINDOW(window));
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(fx_dialog));
    action_area = gtk_dialog_get_action_area(GTK_DIALOG(fx_dialog));

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(content_area), scrolled_window);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scrolled_window, 380, 380);
	gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 5);

    fx_iconview = gtk_icon_view_new_with_model(fx_model);
    gtk_widget_set_margin_bottom(scrolled_window, 8);
    gtk_widget_set_margin_top(scrolled_window, 8);
    gtk_widget_set_margin_start(scrolled_window, 8);
    gtk_widget_set_margin_end(scrolled_window, 8);
    gtk_icon_view_set_text_column(GTK_ICON_VIEW(fx_iconview), COL_DISPLAY_NAME);
    gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(fx_iconview), COL_PIXBUF);
    gtk_container_add(GTK_CONTAINER(scrolled_window), fx_iconview);
    gtk_widget_set_size_request(fx_iconview, 380, 380);
    gtk_icon_view_set_selection_mode(GTK_ICON_VIEW(fx_iconview), GTK_SELECTION_BROWSE);
    gtk_icon_view_set_item_width(GTK_ICON_VIEW(fx_iconview), 100);
    
    GtkTreePath *path = gtk_tree_path_new();
    gtk_tree_path_append_index(path, fx_current);
    gtk_icon_view_select_path(GTK_ICON_VIEW(fx_iconview), path);
    gtk_tree_path_free(path);
    
    button_alignment = gtk_alignment_new(1, 1, 0, 0);
    gtk_container_add(GTK_CONTAINER(action_area), button_alignment);

    apply_button = gtk_button_new_from_stock(GTK_STOCK_APPLY);
    gtk_container_add(GTK_CONTAINER(button_alignment), apply_button);
    g_signal_connect(G_OBJECT(apply_button), "clicked", G_CALLBACK(camoramic_ui_kill_widget), fx_dialog);
    g_signal_connect(fx_dialog, "destroy", G_CALLBACK(camoramic_ui_fx_dialog_killed), NULL);

    gtk_widget_show_all(fx_dialog);
}

void camoramic_ui_video_device_switched(GtkComboBox *self, gpointer user_data)
{
    int combo_box_active = devices[gtk_combo_box_get_active(GTK_COMBO_BOX(videodevice))];
    gtk_combo_box_set_active(GTK_COMBO_BOX(videomode), -1);
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(videomode));
    for (int i = 0; i < camoramic_v4l2util_get_caps_count(combo_box_active); ++i)
    {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videomode), camoramic_v4l2util_get_human_string_caps(combo_box_active, i));
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(videomode), 0);
}

void camoramic_ui_settings_dialog_killed(GtkWidget *widget, gpointer data)
{
    // settings
    if (gtk_widget_get_sensitive(videodevice) == TRUE)
    {
        current_device = devices[gtk_combo_box_get_active(GTK_COMBO_BOX(videodevice))];
        int invalidate_device = 1;
        for (int i = 0; i < camoramic_v4l2util_get_device_count(); i++)
        {
            if (current_device == devices[i])
            {
                invalidate_device = 0;
            }
        }
        if (invalidate_device == 1)
        {
            gtk_combo_box_set_active(GTK_COMBO_BOX(videodevice), 0);
            current_device = devices[gtk_combo_box_get_active(GTK_COMBO_BOX(videodevice))];
        }
        if (gtk_combo_box_get_active(GTK_COMBO_BOX(videomode)) > -1)
        {
            current_device_caps = gtk_combo_box_get_active(GTK_COMBO_BOX(videomode));
        }
    }
    camoramic_alsautil_init();
    int invalidate_alsa_device = 1;
    if (no_alsa_devices == 0)
    {
        for (int i = 0; i < camoramic_alsautil_get_device_count(); i++)
        {
            if ((alsa_devices[i].card == camoramic_settings_get_int("audio-card")) &&
                (alsa_devices[i].device == camoramic_settings_get_int("audio-device")))
            {
                invalidate_alsa_device = 0;
            }
        }
        if (invalidate_alsa_device == 0)
        {
            current_alsa_device = alsa_devices[gtk_combo_box_get_active(GTK_COMBO_BOX(audiodevice))];
        }
        else
        {
            current_alsa_device = alsa_devices[0];
        }
    }
    free(photo_path);
    photo_path = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(photopathbutton));
    free(video_path);
    video_path = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(videopathbutton));
    photo_jpeg_amount = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(photocompression));
    record_audio = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(recordaudio));
    current_video_container = gtk_combo_box_get_active(GTK_COMBO_BOX(videocontainer));
    current_video_bitrate = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(videobitrate));
  
	switch(current_video_container) {
		case CAMORAMIC_CONTAINER_WEBM:
			switch(gtk_combo_box_get_active(GTK_COMBO_BOX(videocodec))) {
				case 1:
					current_video_codec = CAMORAMIC_CODEC_VP9;
					break;
				case 2:
					current_video_codec = CAMORAMIC_CODEC_AV1;
					break;
				default:
					current_video_codec = CAMORAMIC_CODEC_VP8;
			}
			break;
		case CAMORAMIC_CONTAINER_MKV:
			switch(gtk_combo_box_get_active(GTK_COMBO_BOX(videocodec))) {
				case 1:
					current_video_codec = CAMORAMIC_CODEC_VP8;
					break;
				case 2:
					current_video_codec = CAMORAMIC_CODEC_VP9;
					break;
				case 3:
					current_video_codec = CAMORAMIC_CODEC_AV1;
					break;
				case 4:
					current_video_codec = CAMORAMIC_CODEC_H264;
					break;
				case 5:
					current_video_codec = CAMORAMIC_CODEC_H265;
					break;
				default:
					current_video_codec = CAMORAMIC_CODEC_THEORA;
			}
			break;
		case CAMORAMIC_CONTAINER_MP4:
			switch(gtk_combo_box_get_active(GTK_COMBO_BOX(videocodec))) {
				case 1:
					current_video_codec = CAMORAMIC_CODEC_AV1;
					break;
				case 2:
					current_video_codec = CAMORAMIC_CODEC_H264;
					break;
				case 3:
					current_video_codec = CAMORAMIC_CODEC_H265;
					break;
				default:
					current_video_codec = CAMORAMIC_CODEC_VP9;
			}
			break;
		default:
			switch(gtk_combo_box_get_active(GTK_COMBO_BOX(videocodec))) {
				case 1:
					current_video_codec = CAMORAMIC_CODEC_VP8;
					break;
				default:
					current_video_codec = CAMORAMIC_CODEC_THEORA;
			}
	}	
	
	switch(gtk_combo_box_get_active(GTK_COMBO_BOX(videocontainer))) {
		case CAMORAMIC_CONTAINER_WEBM:
			switch(gtk_combo_box_get_active(GTK_COMBO_BOX(audiocodec))) {
				case 1:
					current_audio_codec = CAMORAMIC_AUDIO_CODEC_OPUS;
					break;
				default:
					current_audio_codec = CAMORAMIC_AUDIO_CODEC_VORBIS;
			}
			break;
		case CAMORAMIC_CONTAINER_MKV:
			switch(gtk_combo_box_get_active(GTK_COMBO_BOX(audiocodec))) {
				case 1:
					current_audio_codec = CAMORAMIC_AUDIO_CODEC_OPUS;
					break;
				case 2:
					current_audio_codec = CAMORAMIC_AUDIO_CODEC_SPEEX;
					break;
				case 3:
					current_audio_codec = CAMORAMIC_AUDIO_CODEC_AAC;
					break;
				case 4:
					current_audio_codec = CAMORAMIC_AUDIO_CODEC_AC3;
					break;
				default:
					current_audio_codec = CAMORAMIC_AUDIO_CODEC_VORBIS;
			}			
			break;
		case CAMORAMIC_CONTAINER_MP4:
			switch(gtk_combo_box_get_active(GTK_COMBO_BOX(audiocodec))) {
				case 1:
					current_audio_codec = CAMORAMIC_AUDIO_CODEC_AC3;
					break;
				default:
					current_audio_codec = CAMORAMIC_AUDIO_CODEC_AAC;
			}			
			break;
		default:
			switch(gtk_combo_box_get_active(GTK_COMBO_BOX(audiocodec))) {
				case 1:
					current_audio_codec = CAMORAMIC_AUDIO_CODEC_OPUS;
					break;
				case 2:
					current_audio_codec = CAMORAMIC_AUDIO_CODEC_SPEEX;
					break;
				default:
					current_audio_codec = CAMORAMIC_AUDIO_CODEC_VORBIS;
			}	
	}	
    camoramic_settings_save_settings();
	
    // update statusbar
    if (gtk_widget_get_sensitive(videodevice) == TRUE)
    {
        char *device_name = camoramic_v4l2util_get_friendly_name(current_device);
        char *statusbar_text = malloc(strlen(_("Current device: %s")) + strlen(device_name) + 1);
        sprintf(statusbar_text, _("Current device: %s"), device_name);
        camoramic_ui_update_statusbar(statusbar_text);
        free(statusbar_text);
        free(device_name);
        camoramic_gst_rebuild_pipeline_v4lsrc();
    }
}

void camoramic_ui_photo_format_switched(GtkComboBox *self, gpointer user_data)
{
    if (gtk_combo_box_get_active(GTK_COMBO_BOX(photoformat)) == 1)
    {
        photo_use_jpeg = TRUE;
    }
    else
    {
        photo_use_jpeg = FALSE;
    }
    gtk_widget_set_sensitive(GTK_WIDGET(photocompression), photo_use_jpeg);
}

void camoramic_ui_encoding_tab_fill_codec_combo() {
	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(videocodec));
	switch(gtk_combo_box_get_active(GTK_COMBO_BOX(videocontainer))) {
		case CAMORAMIC_CONTAINER_WEBM:
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocodec), "VP8");
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocodec), "VP9");
			if (camoramic_gst_check_if_element_exists("av1enc") == 1) {
				gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocodec), "AV1");
			}
			break;
		case CAMORAMIC_CONTAINER_MKV:
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocodec), "Theora");
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocodec), "VP8");
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocodec), "VP9");
			if (camoramic_gst_check_if_element_exists("av1enc") == 1) {
				gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocodec), "AV1");
			}
			if (camoramic_gst_check_if_element_exists("x264enc") == 1) {
				gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocodec), "H264");
			}
			if (camoramic_gst_check_if_element_exists("x265enc") == 1) {
				gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocodec), "H265");
			}
			break;
		case CAMORAMIC_CONTAINER_MP4:
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocodec), "VP9");
			if (camoramic_gst_check_if_element_exists("av1enc") == 1) {
				gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocodec), "AV1");
			}
			if (camoramic_gst_check_if_element_exists("x264enc") == 1) {
				gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocodec), "H264");
			}
			if (camoramic_gst_check_if_element_exists("x265enc") == 1) {
				gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocodec), "H265");
			}
			break;
		default:
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocodec), "Theora");
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocodec), "VP8");
	}	
}


void camoramic_ui_encoding_tab_fill_audiocodec_combo() {
	gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(audiocodec));
	switch(gtk_combo_box_get_active(GTK_COMBO_BOX(videocontainer))) {
		case CAMORAMIC_CONTAINER_WEBM:
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audiocodec), "Vorbis");
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audiocodec), "Opus");
			break;
		case CAMORAMIC_CONTAINER_MKV:
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audiocodec), "Vorbis");
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audiocodec), "Opus");
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audiocodec), "Speex");
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audiocodec), "AAC");
			if (camoramic_gst_check_if_element_exists("avenc_ac3") == 1) {
				gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audiocodec), "AC3");
			}
			break;
		case CAMORAMIC_CONTAINER_MP4:
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audiocodec), "AAC");
			if (camoramic_gst_check_if_element_exists("avenc_ac3") == 1) {
				gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audiocodec), "AC3");
			}
			break;
		default:
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audiocodec), "Vorbis");
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audiocodec), "Opus");
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audiocodec), "Speex");
	}	
}

void camoramic_ui_encoding_tab_select_codec_combo() {
	switch(gtk_combo_box_get_active(GTK_COMBO_BOX(videocontainer))) {
		case CAMORAMIC_CONTAINER_WEBM:
			switch(current_video_codec) {
				case CAMORAMIC_CODEC_VP9:
					gtk_combo_box_set_active(GTK_COMBO_BOX(videocodec), 1);
					break;
				case CAMORAMIC_CODEC_AV1:
					gtk_combo_box_set_active(GTK_COMBO_BOX(videocodec), 2);
					break;
				default:
					gtk_combo_box_set_active(GTK_COMBO_BOX(videocodec), 0);
			}
			break;
		case CAMORAMIC_CONTAINER_MKV:
			switch(current_video_codec) {
				case CAMORAMIC_CODEC_VP8:
					gtk_combo_box_set_active(GTK_COMBO_BOX(videocodec), 1);
					break;
				case CAMORAMIC_CODEC_VP9:
					gtk_combo_box_set_active(GTK_COMBO_BOX(videocodec), 2);
					break;
				case CAMORAMIC_CODEC_AV1:
					gtk_combo_box_set_active(GTK_COMBO_BOX(videocodec), 3);
					break;
				case CAMORAMIC_CODEC_H264:
					gtk_combo_box_set_active(GTK_COMBO_BOX(videocodec), 4);
					break;
				case CAMORAMIC_CODEC_H265:
					gtk_combo_box_set_active(GTK_COMBO_BOX(videocodec), 5);
					break;
				default:
					gtk_combo_box_set_active(GTK_COMBO_BOX(videocodec), 0);
			}
			break;
		case CAMORAMIC_CONTAINER_MP4:
			switch(current_video_codec) {
				case CAMORAMIC_CODEC_AV1:
					gtk_combo_box_set_active(GTK_COMBO_BOX(videocodec), 1);
					break;
				case CAMORAMIC_CODEC_H264:
					gtk_combo_box_set_active(GTK_COMBO_BOX(videocodec), 2);
					break;
				case CAMORAMIC_CODEC_H265:
					gtk_combo_box_set_active(GTK_COMBO_BOX(videocodec), 3);
					break;
				default:
					gtk_combo_box_set_active(GTK_COMBO_BOX(videocodec), 0);
			}
			break;
		default:
			switch(current_video_codec) {
				case CAMORAMIC_CODEC_VP8:
					gtk_combo_box_set_active(GTK_COMBO_BOX(videocodec), 1);
					break;
				default:
					gtk_combo_box_set_active(GTK_COMBO_BOX(videocodec), 0);
			}
	}	
}

void camoramic_ui_encoding_tab_select_audiocodec_combo() {
	switch(gtk_combo_box_get_active(GTK_COMBO_BOX(videocontainer))) {
		case CAMORAMIC_CONTAINER_WEBM:
			switch(current_audio_codec) {
				case CAMORAMIC_AUDIO_CODEC_OPUS:
					gtk_combo_box_set_active(GTK_COMBO_BOX(audiocodec), 1);
					break;
				default:
					gtk_combo_box_set_active(GTK_COMBO_BOX(audiocodec), 0);
			}
			break;
		case CAMORAMIC_CONTAINER_MKV:
			switch(current_audio_codec) {
				case CAMORAMIC_AUDIO_CODEC_OPUS:
					gtk_combo_box_set_active(GTK_COMBO_BOX(audiocodec), 1);
					break;
				case CAMORAMIC_AUDIO_CODEC_SPEEX:
					gtk_combo_box_set_active(GTK_COMBO_BOX(audiocodec), 2);
					break;
				case CAMORAMIC_AUDIO_CODEC_AAC:
					gtk_combo_box_set_active(GTK_COMBO_BOX(audiocodec), 3);
					break;
				case CAMORAMIC_AUDIO_CODEC_AC3:
					gtk_combo_box_set_active(GTK_COMBO_BOX(audiocodec), 4);
					break;
				default:
					gtk_combo_box_set_active(GTK_COMBO_BOX(audiocodec), 0);
			}			
			break;
		case CAMORAMIC_CONTAINER_MP4:
			switch(current_audio_codec) {
				case CAMORAMIC_AUDIO_CODEC_AC3:
					gtk_combo_box_set_active(GTK_COMBO_BOX(audiocodec), 1);
					break;
				default:
					gtk_combo_box_set_active(GTK_COMBO_BOX(audiocodec), 0);
			}			
			break;
		default:
			switch(current_audio_codec) {
				case CAMORAMIC_AUDIO_CODEC_OPUS:
					gtk_combo_box_set_active(GTK_COMBO_BOX(audiocodec), 1);
					break;
				case CAMORAMIC_AUDIO_CODEC_SPEEX:
					gtk_combo_box_set_active(GTK_COMBO_BOX(audiocodec), 2);
					break;
				default:
					gtk_combo_box_set_active(GTK_COMBO_BOX(audiocodec), 0);
			}	
	}	
}

void camoramic_ui_encoding_tab_container_combo_switched(GtkComboBox *self, gpointer user_data)
{
	camoramic_ui_encoding_tab_fill_codec_combo();
	camoramic_ui_encoding_tab_select_codec_combo();
	camoramic_ui_encoding_tab_fill_audiocodec_combo();
	camoramic_ui_encoding_tab_select_audiocodec_combo();
}

void camoramic_ui_create_settings_encoding_tab()
{
    GtkWidget *grid;
    GtkWidget *label;
    GtkWidget *photoformat_label;
    GtkWidget *photocompression_label;
	GtkWidget *videocontainer_label;
	GtkWidget *videocodec_label;
	GtkWidget *videoaudiocodec_label;
	GtkWidget *audiocodec_label;
	GtkWidget *videobitrate_label;
    GtkAdjustment *photocompression_adj;
    GtkAdjustment *videobitrate_adj;

    label = gtk_label_new(_("Encoding"));

    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    gtk_widget_set_margin_bottom(grid, 5);
    gtk_widget_set_margin_top(grid, 5);
    gtk_widget_set_margin_start(grid, 5);
    gtk_widget_set_margin_end(grid, 5);

    photoformat_label = gtk_label_new(_("Photo format"));
    gtk_label_set_xalign(GTK_LABEL(photoformat_label), 0.0);

    photoformat = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(photoformat), "PNG");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(photoformat), "JPEG");
    if (photo_use_jpeg == TRUE)
    {
        gtk_combo_box_set_active(GTK_COMBO_BOX(photoformat), 1);
    }
    else
    {
        gtk_combo_box_set_active(GTK_COMBO_BOX(photoformat), 0);
    }
    g_signal_connect(G_OBJECT(photoformat), "changed", G_CALLBACK(camoramic_ui_photo_format_switched), NULL);
    gtk_widget_set_hexpand(photoformat, TRUE);
    gtk_widget_set_margin_start(photoformat, 70);

    gtk_grid_attach(GTK_GRID(grid), photoformat_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), photoformat, 1, 0, 1, 1);

    photocompression_label = gtk_label_new(_("Photo compression level"));
    gtk_label_set_xalign(GTK_LABEL(photocompression_label), 0.0);

    photocompression_adj = gtk_adjustment_new(1, 0, 100, 1, 100.0, 0.0);
    photocompression = gtk_spin_button_new(photocompression_adj, 1.0, 0);
    gtk_widget_set_hexpand(photocompression, TRUE);
    gtk_widget_set_margin_start(photocompression, 70);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(photocompression), photo_jpeg_amount);
    gtk_widget_set_sensitive(GTK_WIDGET(photocompression), photo_use_jpeg);

    gtk_grid_attach(GTK_GRID(grid), photocompression_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), photocompression, 1, 1, 1, 1);

    videocontainer_label = gtk_label_new(_("Video container"));
    gtk_label_set_xalign(GTK_LABEL(videocontainer_label), 0.0);

    videocontainer = gtk_combo_box_text_new();
    gtk_widget_set_hexpand(videocontainer, TRUE);
    gtk_widget_set_margin_start(videocontainer, 70);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocontainer), "OGG");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocontainer), "WEBM");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocontainer), "MKV");
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videocontainer), "MP4");
    gtk_combo_box_set_active(GTK_COMBO_BOX(videocontainer), current_video_container);
    g_signal_connect(G_OBJECT(videocontainer), "changed", G_CALLBACK(camoramic_ui_encoding_tab_container_combo_switched), NULL);

    gtk_grid_attach(GTK_GRID(grid), videocontainer_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), videocontainer, 1, 2, 1, 1);

    videocodec_label = gtk_label_new(_("Video codec"));
    gtk_label_set_xalign(GTK_LABEL(videocodec_label), 0.0);

    videocodec = gtk_combo_box_text_new();
    gtk_widget_set_hexpand(videocodec, TRUE);
    gtk_widget_set_margin_start(videocodec, 70);
	camoramic_ui_encoding_tab_fill_codec_combo();
	camoramic_ui_encoding_tab_select_codec_combo();
	
    gtk_grid_attach(GTK_GRID(grid), videocodec_label, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), videocodec, 1, 3, 1, 1);
  
    videobitrate_label = gtk_label_new(_("Video bitrate (kbps)"));
    gtk_label_set_xalign(GTK_LABEL(videobitrate_label), 0.0);

    videobitrate_adj = gtk_adjustment_new(1, 0, 100000000000, 1, 100.0, 0.0);
    videobitrate = gtk_spin_button_new(videobitrate_adj, 1.0, 0);
    gtk_widget_set_hexpand(videobitrate, TRUE);
    gtk_widget_set_margin_start(videobitrate, 70);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(videobitrate), current_video_bitrate);

    gtk_grid_attach(GTK_GRID(grid), videobitrate_label, 0, 4, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), videobitrate, 1, 4, 1, 1);

  
    audiocodec_label = gtk_label_new(_("Audio codec"));
    gtk_label_set_xalign(GTK_LABEL(audiocodec_label), 0.0);

    audiocodec = gtk_combo_box_text_new();
    gtk_widget_set_hexpand(audiocodec, TRUE);
    gtk_widget_set_margin_start(audiocodec, 70);
	camoramic_ui_encoding_tab_fill_audiocodec_combo();
	camoramic_ui_encoding_tab_select_audiocodec_combo();

    gtk_grid_attach(GTK_GRID(grid), audiocodec_label, 0, 5, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), audiocodec, 1, 5, 1, 1);
  
      
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), grid, label);

}

void camoramic_ui_create_settings_output_tab()
{
    GtkWidget *grid;
    GtkWidget *label;
    GtkWidget *photopathbutton_label;
    GtkWidget *videopathbutton_label;

    label = gtk_label_new(_("Output"));

    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    gtk_widget_set_margin_bottom(grid, 5);
    gtk_widget_set_margin_top(grid, 5);
    gtk_widget_set_margin_start(grid, 5);
    gtk_widget_set_margin_end(grid, 5);

    photopathbutton_label = gtk_label_new(_("Photo folder"));
    gtk_label_set_xalign(GTK_LABEL(photopathbutton_label), 0.0);

    photopathbutton = gtk_file_chooser_button_new("Select a folder", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(photopathbutton), photo_path);
    gtk_widget_set_hexpand(photopathbutton, TRUE);
    gtk_widget_set_margin_start(photopathbutton, 70);

    gtk_grid_attach(GTK_GRID(grid), photopathbutton_label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), photopathbutton, 1, 0, 1, 1);

    videopathbutton_label = gtk_label_new(_("Video folder"));
    gtk_label_set_xalign(GTK_LABEL(videopathbutton_label), 0.0);

    videopathbutton = gtk_file_chooser_button_new(_("Select a folder"), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(videopathbutton), video_path);
    gtk_widget_set_hexpand(videopathbutton, TRUE);
    gtk_widget_set_margin_start(videopathbutton, 70);

    gtk_grid_attach(GTK_GRID(grid), videopathbutton_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), videopathbutton, 1, 1, 1, 1);
    
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), grid, label);
}

void camoramic_ui_record_audio_checked_switched(GtkToggleButton* self, gpointer user_data)
{
    if (audiodevice_activated == 1) {
		gtk_widget_set_sensitive(audiodevice, gtk_toggle_button_get_active(self));	
	}
}

void camoramic_ui_create_settings_dialog_input_tab()
{
    camoramic_alsautil_init();
    GtkWidget *grid;
    GtkWidget *label;
    GtkWidget *videodevice_label;
    GtkWidget *videomode_label;
    GtkWidget *audiodevice_label;

    label = gtk_label_new(_("Input"));

    camoramic_v4l2util_reload();

    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    gtk_widget_set_margin_bottom(grid, 5);
    gtk_widget_set_margin_top(grid, 5);
    gtk_widget_set_margin_start(grid, 5);
    gtk_widget_set_margin_end(grid, 5);

	recordaudio = gtk_check_button_new_with_label(_("Record audio"));
    gtk_grid_attach(GTK_GRID(grid), recordaudio, 0, 0, 1, 1);
    gtk_widget_set_sensitive(recordaudio, FALSE);
    if (no_alsa_devices == 0) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(recordaudio), record_audio);
	    gtk_widget_set_sensitive(recordaudio, TRUE);
	}
    g_signal_connect(G_OBJECT(recordaudio), "toggled", G_CALLBACK(camoramic_ui_record_audio_checked_switched), NULL);
	
    videodevice_label = gtk_label_new(_("Video device"));
    gtk_label_set_xalign(GTK_LABEL(videodevice_label), 0.0);

    videodevice = gtk_combo_box_text_new();
    gtk_widget_set_sensitive(videodevice, FALSE);
    if ((camoramic_v4l2util_get_device_count() > devicecount_cache) ||
        (camoramic_v4l2util_get_device_count() < devicecount_cache))
    {
        camoramic_v4l2util_reload();
        for (int i = 0; i < camoramic_v4l2util_get_device_count(); i++)
        {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videodevice),camoramic_v4l2util_get_friendly_name(devices[i]));
            gtk_widget_set_sensitive(videodevice, TRUE);
        }
        for (int i = 0; i < camoramic_v4l2util_get_device_count(); i++)
        {
            if (current_device == devices[i])
            {
                gtk_combo_box_set_active(GTK_COMBO_BOX(videodevice), i);
            }
        }
    }
    else
    {
        for (int i = 0; i < camoramic_v4l2util_get_device_count(); i++)
        {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videodevice),camoramic_v4l2util_get_friendly_name(devices[i]));
            gtk_widget_set_sensitive(videodevice, TRUE);
        }
        for (int i = 0; i < camoramic_v4l2util_get_device_count(); i++)
        {
            if (current_device == devices[i])
            {
                gtk_combo_box_set_active(GTK_COMBO_BOX(videodevice), i);
            }
        }
    }

    int invalidate_device = 1;
    for (int i = 0; i < camoramic_v4l2util_get_device_count(); i++)
    {
        if (current_device == devices[i])
        {
            invalidate_device = 0;
        }
    }
    if (invalidate_device == 1)
    {
        gtk_combo_box_set_active(GTK_COMBO_BOX(videodevice), 0);
    }
    gtk_widget_set_hexpand(videodevice, TRUE);
    gtk_widget_set_margin_start(videodevice, 100);
    g_signal_connect(G_OBJECT(videodevice), "changed", G_CALLBACK(camoramic_ui_video_device_switched), NULL);

    gtk_grid_attach(GTK_GRID(grid), videodevice_label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), videodevice, 1, 1, 1, 1);

    videomode_label = gtk_label_new(_("Video mode"));
    gtk_label_set_xalign(GTK_LABEL(videomode_label), 0.0);

    videomode = gtk_combo_box_text_new();
    gtk_widget_set_sensitive(videomode, FALSE);
    if (invalidate_device == 0)
    {
        for (int i = 0; i < camoramic_v4l2util_get_caps_count(current_device); ++i)
        {
            gtk_widget_set_sensitive(videomode, TRUE);
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videomode), camoramic_v4l2util_get_human_string_caps(current_device, i));
        }
    }
    else
    {
        for (int i = 0;
             i < camoramic_v4l2util_get_caps_count(devices[gtk_combo_box_get_active(GTK_COMBO_BOX(videodevice))]); ++i)
        {
            gtk_widget_set_sensitive(videomode, TRUE);
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(videomode),camoramic_v4l2util_get_human_string_caps(devices[gtk_combo_box_get_active(GTK_COMBO_BOX(videodevice))], i));
        }
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(videomode), current_device_caps);
    gtk_widget_set_hexpand(videomode, TRUE);
    gtk_widget_set_margin_start(videomode, 100);

    gtk_grid_attach(GTK_GRID(grid), videomode_label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), videomode, 1, 2, 1, 1);

    audiodevice_label = gtk_label_new(_("Audio device"));
    gtk_label_set_xalign(GTK_LABEL(audiodevice_label), 0.0);

    audiodevice = gtk_combo_box_text_new();
    gtk_widget_set_sensitive(audiodevice, FALSE);
    int invalidate_alsa_device = 1;
    if (no_alsa_devices == 0)
    {
        for (int i = 0; i < camoramic_alsautil_get_device_count(); i++)
        {
            if ((alsa_devices[i].card == camoramic_settings_get_int("audio-card")) && (alsa_devices[i].device == camoramic_settings_get_int("audio-device")))
            {
                invalidate_alsa_device = 0;
            }
        }
        if (invalidate_alsa_device == 1)
        {
            current_alsa_device = alsa_devices[0];
        }
        if (no_alsa_devices == 0)
        {
			audiodevice_activated = 1;
            gtk_widget_set_sensitive(audiodevice, TRUE);
            gtk_combo_box_set_active(GTK_COMBO_BOX(audiodevice), 0);
            for (int i = 0; i < camoramic_alsautil_get_device_count(); i++)
            {
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audiodevice), alsa_devices[i].friendly_name);
            }
            for (int i = 0; i < camoramic_alsautil_get_device_count(); i++)
            {
                if ((alsa_devices[i].card == current_alsa_device.card) &&
                    (alsa_devices[i].device == current_alsa_device.device))
                {
                    gtk_combo_box_set_active(GTK_COMBO_BOX(audiodevice), i);
                }
            }
        }
    }
    if (audiodevice_activated == 1) {
		gtk_widget_set_sensitive(audiodevice, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(recordaudio)));	
	}
	
    gtk_widget_set_hexpand(audiodevice, TRUE);
    gtk_widget_set_margin_start(audiodevice, 100);

    gtk_grid_attach(GTK_GRID(grid), audiodevice_label, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), audiodevice, 1, 3, 1, 1);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), grid, label);
}

void camoramic_ui_show_settings_dialog(GtkWidget *widget, gpointer data)
{
	if (video_recording == 1)  {
		return;
	}
    GtkWidget *content_area;
    GtkWidget *action_area;
    GtkWidget *button_alignment;
    GtkWidget *close_button;

    settings_dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(settings_dialog), _("Preferences"));
    gtk_window_set_modal(GTK_WINDOW(settings_dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(settings_dialog), GTK_WINDOW(window));
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(settings_dialog));
    action_area = gtk_dialog_get_action_area(GTK_DIALOG(settings_dialog));

    notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
    gtk_widget_set_margin_bottom(notebook, 8);
    gtk_widget_set_margin_top(notebook, 8);
    gtk_widget_set_margin_start(notebook, 8);
    gtk_widget_set_margin_end(notebook, 8);

    gtk_container_add(GTK_CONTAINER(content_area), notebook);

    camoramic_ui_create_settings_dialog_input_tab();
    camoramic_ui_create_settings_output_tab();
    camoramic_ui_create_settings_encoding_tab();

    button_alignment = gtk_alignment_new(1, 1, 0, 0);
    gtk_container_add(GTK_CONTAINER(action_area), button_alignment);

    close_button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
    gtk_container_add(GTK_CONTAINER(button_alignment), close_button);
    g_signal_connect(G_OBJECT(close_button), "clicked", G_CALLBACK(camoramic_ui_kill_widget), settings_dialog);
    g_signal_connect(settings_dialog, "destroy", G_CALLBACK(camoramic_ui_settings_dialog_killed), NULL);

    gtk_widget_show_all(settings_dialog);
}

void camoramic_ui_show_about_dialog(GtkWidget *widget, gpointer data)
{
    GtkWidget *dialog;
    GtkWidget *action_area;

    dialog = gtk_about_dialog_new();
    action_area = gtk_dialog_get_action_area(GTK_DIALOG(dialog));

    gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), "Camoramic");
    gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog), "(c) Jason Contoso");
    gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog), _("An utility for taking videos and photos from your webcam."));
    gtk_about_dialog_set_logo( GTK_ABOUT_DIALOG(dialog), gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), "camera-photo", 64, GTK_ICON_LOOKUP_USE_BUILTIN, NULL));

    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

void camoramic_ui_take_photo(GtkWidget *widget, gpointer data)
{
    camoramic_gst_take_photo();
}

void camoramic_ui_record_button_pressed(GtkWidget *widget, gpointer data)
{
    if (video_recording == 0)
    {
        camoramic_gst_record_start();
        gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(recordvideobutton), GTK_STOCK_MEDIA_STOP);
        gtk_tool_button_set_label(GTK_TOOL_BUTTON(recordvideobutton), _("Stop Recording"));
        gtk_widget_set_sensitive(GTK_WIDGET(prefbutton), FALSE);
    }
    else
    {
        camoramic_gst_record_stop();
    }
}

void camoramic_ui_create_toolbar()
{
    GtkWidget *toolbar;
    GtkToolItem *toolbarseperator;
    GtkToolItem *helpbutton;
    GtkToolItem *aboutbutton;

    toolbar = gtk_toolbar_new();
    gtk_style_context_add_class(gtk_widget_get_style_context(toolbar), GTK_STYLE_CLASS_PRIMARY_TOOLBAR);
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);
    gtk_box_pack_start(GTK_BOX(box), toolbar, FALSE, FALSE, 0);;
	
    takephotobutton = gtk_tool_button_new(gtk_image_new_from_icon_name("camera-photo", GTK_ICON_SIZE_LARGE_TOOLBAR), _("Take Photo"));
    gtk_tool_item_set_is_important(GTK_TOOL_ITEM(takephotobutton), TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), takephotobutton, -1);
    g_signal_connect(G_OBJECT(takephotobutton), "clicked", G_CALLBACK(camoramic_ui_take_photo), NULL);

    recordvideobutton = gtk_tool_button_new(NULL, _("Record Video"));
    gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(recordvideobutton), GTK_STOCK_MEDIA_RECORD);
    gtk_tool_item_set_is_important(GTK_TOOL_ITEM(recordvideobutton), TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), recordvideobutton, -1);
    g_signal_connect(G_OBJECT(recordvideobutton), "clicked", G_CALLBACK(camoramic_ui_record_button_pressed), NULL);

    fxbutton = gtk_tool_button_new(gtk_image_new_from_icon_name("applications-graphics", GTK_ICON_SIZE_LARGE_TOOLBAR),_("Effects"));
    gtk_tool_item_set_is_important(GTK_TOOL_ITEM(fxbutton), TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), fxbutton, -1);
    g_signal_connect(G_OBJECT(fxbutton), "clicked", G_CALLBACK(camoramic_ui_show_fx_dialog), NULL);

    toolbarseperator = gtk_separator_tool_item_new();
    gtk_separator_tool_item_set_draw(GTK_SEPARATOR_TOOL_ITEM(toolbarseperator), FALSE);
    gtk_tool_item_set_expand(GTK_TOOL_ITEM(toolbarseperator), TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), toolbarseperator, -1);

    prefbutton = gtk_tool_button_new_from_stock(GTK_STOCK_PREFERENCES);
    gtk_tool_item_set_is_important(GTK_TOOL_ITEM(prefbutton), TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), prefbutton, -1);
    g_signal_connect(G_OBJECT(prefbutton), "clicked", G_CALLBACK(camoramic_ui_show_settings_dialog), NULL);

    helpbutton = gtk_tool_button_new_from_stock(GTK_STOCK_HELP);
    gtk_tool_item_set_is_important(GTK_TOOL_ITEM(helpbutton), TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), helpbutton, -1);

    aboutbutton = gtk_tool_button_new_from_stock(GTK_STOCK_ABOUT);
    gtk_tool_item_set_is_important(GTK_TOOL_ITEM(aboutbutton), TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), aboutbutton, -1);
    g_signal_connect(G_OBJECT(aboutbutton), "clicked", G_CALLBACK(camoramic_ui_show_about_dialog), NULL);
}

void camoramic_ui_create_preview()
{
    GtkWidget *seperator;

    if (error == NULL)
    {
        preview = gtk_drawing_area_new();
        g_object_get(preview_sink, "widget", &preview, NULL);
    }
    else
    {
        GdkRGBA bg_color, color;
        bg_color.red = 0;
        bg_color.green = 0;
        bg_color.blue = 0;
        bg_color.alpha = 1;
        color.red = 1;
        color.green = 1;
        color.blue = 1;
        color.alpha = 1;
        preview = gtk_label_new(error);
        gtk_widget_override_background_color(preview, GTK_STATE_FLAG_NORMAL, &bg_color);
        gtk_widget_override_color(preview, GTK_STATE_FLAG_NORMAL, &color);
    }
    gtk_widget_set_size_request(preview, 300, 250);
    gtk_box_pack_start(GTK_BOX(box), preview, TRUE, TRUE, 0);
    if (draw_seperators == TRUE) {
		seperator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
		gtk_widget_set_size_request(seperator, 300, 1);
		gtk_box_pack_start(GTK_BOX(box), seperator, FALSE, FALSE, 0);
	}
}

void camoramic_ui_past_photos_init_model_fe(gpointer item, gpointer data) {
	GdkPixbuf *photo;
	if ((camoramic_string_endswith((char*)item, "png") == 1) ||  (camoramic_string_endswith((char*)item, "jpg") == 1)) {
		photo = gdk_pixbuf_new_from_file((char*)item, NULL);
	} else {
		camoramic_thumbnailer_generate((char*)item, &photo);
	}

    double width = (double)gdk_pixbuf_get_width(photo);
    double height = (double)gdk_pixbuf_get_height(photo);
	double width_c;
    int gcd = camoramic_get_gcd(width, height);
    width = width/gcd;
    height = height/gcd;
	width_c = round(55/height);
	width = width*width_c;
	height = 55;
	GdkPixbuf* resized = gdk_pixbuf_scale_simple(photo, width, height, GDK_INTERP_BILINEAR);
	if ((camoramic_string_endswith((char*)item, "png")== 0) && (camoramic_string_endswith((char*)item, "jpg") == 0)) {
		GdkPixbuf* videooverlayimage_resized = gdk_pixbuf_scale_simple(videooverlayimage, width, height, GDK_INTERP_BILINEAR);
		gdk_pixbuf_composite(videooverlayimage_resized, resized, 0, 0, width, height, 0, 0, 1, 1, GDK_INTERP_BILINEAR, 255);
		gdk_pixbuf_unref(videooverlayimage_resized);
	}
    gtk_list_store_append(pastphotos_store, &pastphotos_iter);
    gtk_list_store_set(pastphotos_store, &pastphotos_iter, 0, resized, -1);
    gdk_pixbuf_unref(resized);
    gdk_pixbuf_unref(photo);
}

void camoramic_ui_past_photos_init_model()
{
    pastphotos_store = gtk_list_store_new(PNUM_COLS, GDK_TYPE_PIXBUF);
    g_list_foreach(past_media, camoramic_ui_past_photos_init_model_fe, NULL);
}

void camoramic_ui_create_past_photos()
{
    GtkWidget *seperator;
    GtkWidget *scrolled_window;

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), GTK_SHADOW_ETCHED_IN);

    pastphotos = gtk_icon_view_new();
    gtk_icon_view_set_model(GTK_ICON_VIEW(pastphotos), GTK_TREE_MODEL(pastphotos_store));
    gtk_icon_view_set_text_column(GTK_ICON_VIEW(pastphotos), -1);
    gtk_icon_view_set_pixbuf_column(GTK_ICON_VIEW(pastphotos), PCOL_PIXBUF);
    gtk_widget_set_size_request(pastphotos, 300, 70);
    gtk_icon_view_set_item_width(GTK_ICON_VIEW(pastphotos), 55);
      
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), pastphotos);
    gtk_box_pack_start(GTK_BOX(box), scrolled_window, TRUE, TRUE, 0);
    gtk_widget_set_size_request(scrolled_window, 300, 90);
    if (draw_seperators == TRUE) {
		seperator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
		gtk_widget_set_size_request(seperator, 300, 1);
		gtk_box_pack_start(GTK_BOX(box), seperator, FALSE, FALSE, 0);
	}
}

void camoramic_ui_update_statusbar(char *text)
{
    gtk_statusbar_push(GTK_STATUSBAR(statusbar), gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar), text), text);
}

void camoramic_ui_create_statusbar()
{
    statusbar = gtk_statusbar_new();
    if (draw_statusbar_margins == FALSE) {
		gtk_widget_set_margin_bottom(statusbar, 0);
		gtk_widget_set_margin_left(statusbar, 0); 
		gtk_widget_set_margin_right(statusbar, 0); 
		gtk_widget_set_margin_top(statusbar, 0);
	}
    gtk_box_pack_end(GTK_BOX(box), statusbar, FALSE, FALSE, 0);
}

void camoramic_ui_open_currently_selected(GtkWidget *widget, gpointer data) {
	char* selected_item = g_list_nth_data(past_media, current_selected_item);
    char* cmd = malloc(strlen("xdg-open ")+strlen(selected_item)+1);
    strcpy(cmd, "xdg-open ");
	strcat(cmd, selected_item);
    system(cmd);	
    free(cmd);
}

void camoramic_ui_save_as_currently_selected(GtkWidget *widget, gpointer data) {
	char* selected_item = g_list_nth_data(past_media, current_selected_item);
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new (_("Save File"),GTK_WINDOW(window),GTK_FILE_CHOOSER_ACTION_SAVE,GTK_STOCK_CANCEL,GTK_RESPONSE_CANCEL,GTK_STOCK_SAVE,GTK_RESPONSE_ACCEPT,NULL);

	gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);

	gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), selected_item);

	int res = gtk_dialog_run(GTK_DIALOG(dialog));
	if (res == GTK_RESPONSE_ACCEPT)
	{
		char *fn;
		fn = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		GFile* dest = g_file_new_for_path(fn);
		GFile* src = g_file_new_for_path(selected_item);
		g_file_copy(src,dest,G_FILE_COPY_NONE, NULL, NULL, NULL, NULL);
		g_object_unref(dest);
		g_object_unref(src);
		free(fn);
	}

	gtk_widget_destroy (dialog);
}

void camoramic_ui_delete_currently_selected(GtkWidget *widget, gpointer data) {
    GList *selected_item_list = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(pastphotos));
    gpointer selected_itemz = g_list_nth_data(selected_item_list, 0);
    if (selected_itemz == NULL) {
		gtk_tree_path_free((GtkTreePath *)(selected_itemz));
		g_list_free(selected_item_list); 
		return;
	}

    gtk_tree_model_get_iter(GTK_TREE_MODEL(pastphotos_store), &current_selected_item_iter, (GtkTreePath *)(selected_itemz));	
	gtk_tree_path_free((GtkTreePath *)(selected_itemz));
	g_list_free(selected_item_list); 
		
	char* selected_item = g_list_nth_data(past_media, current_selected_item);
	past_media = g_list_remove(past_media, selected_item),
	gtk_list_store_remove(pastphotos_store, &current_selected_item_iter);
	remove(selected_item);
	camoramic_settings_save_past_media();
}

gboolean camoramic_ui_show_popup(GtkWidget *widget, GdkEvent *event) {
    GList *selected_item_list = gtk_icon_view_get_selected_items(GTK_ICON_VIEW(pastphotos));
    gpointer selected_item = g_list_nth_data(selected_item_list, 0);
    if (selected_item == NULL) {
		gtk_tree_path_free((GtkTreePath *)(selected_item));
		g_list_free(selected_item_list); 
		return FALSE;
	}

    gtk_tree_model_get_iter(GTK_TREE_MODEL(pastphotos_store), &current_selected_item_iter, (GtkTreePath *)(selected_item));	
    gint *selected_item_list2 = gtk_tree_path_get_indices((GtkTreePath *)(selected_item));

    current_selected_item = selected_item_list2[0];
  
	if (event->type == GDK_BUTTON_PRESS) {
		GdkEventButton *bevent = (GdkEventButton *)event;
		if (bevent->button == 3) {      
			gtk_menu_popup(GTK_MENU(widget), NULL, NULL, NULL, NULL, bevent->button, bevent->time);
		}
		gtk_tree_path_free((GtkTreePath *)(selected_item));
		g_list_free(selected_item_list); 
	} else if (event->type == GDK_2BUTTON_PRESS) {
		GdkEventButton *bevent = (GdkEventButton *)event;
		camoramic_ui_open_currently_selected(NULL, NULL);
		gtk_tree_path_free((GtkTreePath *)(selected_item));
		g_list_free(selected_item_list); 
	} 
	return FALSE;
}

void camoramic_ui_create()
{
	GtkWidget *pmenu;
	GtkWidget *pmenu_open;
	GtkWidget *pmenu_saveas;
	GtkWidget *pmenu_trash;
	GtkWidget *pmenu_del;
	
	ui_created = 1;
    nonepreview = gdk_pixbuf_new_from_inline(-1, nofx, FALSE, NULL);
	videooverlayimage = gdk_pixbuf_new_from_inline(-1, videooverlay, FALSE, NULL);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_window_set_default_size(GTK_WINDOW(window), 670, 350);
    gtk_widget_set_size_request(window, 670, 350);
    gtk_window_set_title(GTK_WINDOW(window), "Camoramic");
    gtk_window_set_icon_name(GTK_WINDOW(window), "camera-photo");
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), box);
    
	accel_group = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);


    camoramic_ui_create_toolbar();
    camoramic_ui_create_preview();
    camoramic_ui_past_photos_init_model();
    camoramic_ui_create_past_photos();
    camoramic_ui_create_statusbar();
    fx_model = camoramic_ui_fx_dialog_init_model();
    
    
	pmenu = gtk_menu_new();
  
	pmenu_open = gtk_image_menu_item_new_from_stock(GTK_STOCK_OPEN, accel_group);
	gtk_widget_show(pmenu_open);
	gtk_menu_shell_append(GTK_MENU_SHELL(pmenu), pmenu_open);
    g_signal_connect(G_OBJECT(pmenu_open), "activate", G_CALLBACK(camoramic_ui_open_currently_selected), NULL);

 	pmenu_saveas = gtk_image_menu_item_new_from_stock(GTK_STOCK_SAVE_AS, accel_group);
	gtk_widget_show(pmenu_saveas);
	gtk_menu_shell_append(GTK_MENU_SHELL(pmenu), pmenu_saveas);
	gtk_widget_add_accelerator(pmenu_saveas, "activate", accel_group, GDK_KEY_s, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);   
    g_signal_connect(G_OBJECT(pmenu_saveas), "activate", G_CALLBACK(camoramic_ui_save_as_currently_selected), NULL);
	 
  	pmenu_del = gtk_image_menu_item_new_from_stock(GTK_STOCK_DELETE, accel_group);
	gtk_widget_show(pmenu_del);
	gtk_menu_shell_append(GTK_MENU_SHELL(pmenu), pmenu_del);
    g_signal_connect(G_OBJECT(pmenu_del), "activate", G_CALLBACK(camoramic_ui_delete_currently_selected), NULL);
	gtk_widget_add_accelerator(pmenu_del, "activate", accel_group, GDK_KEY_Delete,(GdkModifierType)0, GTK_ACCEL_VISIBLE);   
  
	g_signal_connect_swapped(G_OBJECT(pastphotos), "button-press-event", G_CALLBACK(camoramic_ui_show_popup), pmenu);  
	      
    if (error != NULL)
    {
        camoramic_ui_update_statusbar(error);
        gtk_widget_set_sensitive(GTK_WIDGET(takephotobutton), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(recordvideobutton), FALSE);
        gtk_widget_set_sensitive(GTK_WIDGET(fxbutton), FALSE);
        camoramic_sfx_play("camoramic-error-event", "dialog-error");
    }
    g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
    gtk_widget_show_all(window);
}
