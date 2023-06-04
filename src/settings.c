#include <glib.h>
#include <gio/gio.h>
#include "alsautility.h"
#include "settings.h"

camoramic_codec current_video_codec;
camoramic_audio_codec current_audio_codec;
camoramic_container current_video_container;
int current_video_bitrate;
int current_device;
int current_device_caps;
gboolean photo_use_jpeg;
int photo_jpeg_amount;
char* photo_path;
char* video_path;
gboolean draw_seperators;
gboolean draw_statusbar_margins;
gboolean record_audio;
GList* past_media = NULL;
char** cpast_media;
int citer;

GSettings* settings;

int camoramic_settings_get_int(const char* key) {
	return g_settings_get_int(settings, key);
}

gboolean camoramic_settings_get_bool(const char* key) {
	return g_settings_get_boolean(settings, key);
}

char* camoramic_settings_get_string(const char* key) {
	return g_settings_get_string(settings, key);
}

void camoramic_settings_set_int(const char* key, int value) {
	g_settings_set_int(settings, key, value);
}

void camoramic_settings_set_bool(const char* key, gboolean value) {
	g_settings_set_boolean(settings, key, value);
}

void camoramic_settings_set_string(const char* key, const char* value) {
	 g_settings_set_string(settings, key, value);
}


void camoramic_settings_save_past_media_fe(gpointer item, gpointer data)
{
	cpast_media[citer] = (char*)item;
	citer++;
}

void camoramic_settings_save_past_media()
{
	cpast_media = calloc(g_list_length(past_media)+1, sizeof(char*));
	citer = 0;
    g_list_foreach(past_media, camoramic_settings_save_past_media_fe, NULL);
    cpast_media[citer] = NULL;
	g_settings_set_strv(settings, "past-media", (const gchar * const*)cpast_media);
	free(cpast_media);
}


void camoramic_settings_save_settings() {
	 camoramic_settings_set_int("video-device", current_device);
	 camoramic_settings_set_int("video-mode", current_device_caps);
	 camoramic_settings_set_int("audio-card", current_alsa_device.card);
	 camoramic_settings_set_int("audio-device", current_alsa_device.device);
	 camoramic_settings_set_int("photo-jpeg-amount", photo_jpeg_amount);
	 camoramic_settings_set_string("photo-path", photo_path);
	 camoramic_settings_set_bool("photo-jpeg", photo_use_jpeg);
	 camoramic_settings_set_bool("record-audio", record_audio);
 	 camoramic_settings_set_int("video-container", current_video_container);
	 camoramic_settings_set_int("video-codec", current_video_codec);
	 camoramic_settings_set_string("video-path", video_path);
	 camoramic_settings_set_int("video-bitrate", current_video_bitrate);
	 camoramic_settings_set_int("video-audio-codec", current_audio_codec);
	 camoramic_settings_save_past_media();
}

void camoramic_settings_load_past_media() {	
	cpast_media = g_settings_get_strv(settings, "past-media");
	
	for (citer = 0; cpast_media[citer] != NULL; ++citer)
	{
		past_media = g_list_append(past_media, cpast_media[citer]);
	}
	free(cpast_media);
}


void camoramic_settings_load_settings() {
	current_device = camoramic_settings_get_int("video-device");
	current_device_caps = camoramic_settings_get_int("video-mode");
	photo_jpeg_amount = camoramic_settings_get_int("photo-jpeg-amount");
	photo_use_jpeg = camoramic_settings_get_bool("photo-jpeg");
	photo_path = camoramic_settings_get_string("photo-path");
	current_video_container = camoramic_settings_get_int("video-container");
	current_video_codec = camoramic_settings_get_int("video-codec");
	current_audio_codec = camoramic_settings_get_int("video-audio-codec");
	current_video_bitrate	= camoramic_settings_get_int("video-bitrate");
	video_path = camoramic_settings_get_string("video-path");
	draw_seperators = camoramic_settings_get_bool("ui-seperators");
	draw_statusbar_margins = camoramic_settings_get_bool("ui-statusbar-margins");
	camoramic_settings_load_past_media();
	
	if (no_alsa_devices == 0) {
			record_audio = camoramic_settings_get_bool("record-audio");
	} else {
			record_audio = FALSE;
	}
	if ((strcmp(photo_path, "default") == 0) || (g_file_test(photo_path, G_FILE_TEST_IS_DIR) == FALSE)) { 
		free(photo_path);
		photo_path = strdup(g_get_user_special_dir(G_USER_DIRECTORY_PICTURES));
	}
	
	if ((strcmp(video_path, "default") == 0) || (g_file_test(video_path, G_FILE_TEST_IS_DIR) == FALSE)) { 
		free(video_path);
		video_path = strdup(g_get_user_special_dir(G_USER_DIRECTORY_VIDEOS));
	}
		
	if (no_alsa_devices == 0) {
		int invalidate_alsa_device = 1;
		if (no_alsa_devices	== 0) {
		for (int i = 0; i < camoramic_alsautil_get_device_count(); i++)
			{
				if ((alsa_devices[i].card == camoramic_settings_get_int("audio-card")) && (alsa_devices[i].device == camoramic_settings_get_int("audio-device"))) {
					invalidate_alsa_device = 0;
				}
			}		
		}
		if (invalidate_alsa_device == 0) {
			camoramic_set_current_device(camoramic_settings_get_int("audio-card"),camoramic_settings_get_int("audio-device"));
		} else {
			current_alsa_device = alsa_devices[0];
		}
	}
}

void camoramic_settings_init() {
	settings = g_settings_new("org.tga.camoramic");
	camoramic_settings_load_settings();
}
