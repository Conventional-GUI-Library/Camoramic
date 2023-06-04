typedef struct camoramic_alsautil_device_s {
  int card;
  int device;
  char* friendly_name;
} camoramic_alsautil_device;

extern camoramic_alsautil_device *alsa_devices;
extern camoramic_alsautil_device current_alsa_device;
extern int alsadevicecount_cache;
extern int no_alsa_devices;
int camoramic_alsautil_get_device_count();
void camoramic_alsautil_init();
void camoramic_set_current_device(int card, int device);
