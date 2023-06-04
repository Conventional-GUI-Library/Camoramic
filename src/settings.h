#ifndef SETTINGSH
#define SETTINGSH

typedef enum
{
    CAMORAMIC_CODEC_THEORA,
    CAMORAMIC_CODEC_VP8,
    CAMORAMIC_CODEC_VP9,
    CAMORAMIC_CODEC_AV1,
    CAMORAMIC_CODEC_H264,
    CAMORAMIC_CODEC_H265
}camoramic_codec;

typedef enum
{
    CAMORAMIC_CONTAINER_OGG,
    CAMORAMIC_CONTAINER_WEBM,
	CAMORAMIC_CONTAINER_MKV,
	CAMORAMIC_CONTAINER_MP4
}camoramic_container;

typedef enum
{
    CAMORAMIC_AUDIO_CODEC_VORBIS,
    CAMORAMIC_AUDIO_CODEC_OPUS,
    CAMORAMIC_AUDIO_CODEC_SPEEX,
	CAMORAMIC_AUDIO_CODEC_AAC,
	CAMORAMIC_AUDIO_CODEC_AC3	
}camoramic_audio_codec;
#endif

extern camoramic_codec current_video_codec;
extern camoramic_audio_codec current_audio_codec;
extern camoramic_container current_video_container;
extern int current_video_bitrate;
extern int current_device;
extern int current_device_caps;
extern gboolean photo_use_jpeg;
extern int photo_jpeg_amount;
extern char* photo_path;
extern char* video_path;
void camoramic_settings_init();
int camoramic_settings_get_int(const char* key);
gboolean camoramic_settings_get_bool(const char* key);
void camoramic_settings_set_int(const char* key, int value);
void camoramic_settings_set_bool(const char* key, gboolean value);
void camoramic_settings_save_settings();
void camoramic_settings_save_past_media();
void camoramic_settings_load_past_media();
extern gboolean draw_seperators;
extern gboolean draw_statusbar_margins;
extern gboolean record_audio;
extern GList* past_media;
