#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <alsa/asoundlib.h>
#include <glib/gi18n.h>
#include "alsautility.h"
#include "misc.h"

camoramic_alsautil_device *alsa_devices;
camoramic_alsautil_device current_alsa_device;
int no_alsa_devices = 0;
int alsadevicecount_cache;

int camoramic_alsautil_get_device_count() {
	int error;
	int count = 0;
    char** hints;
    char** iter;
	
	error = snd_device_name_hint(-1, "pcm", (void***)&hints);
	
    iter = hints;
    while (*iter != NULL) {
		char* name = snd_device_name_get_hint(*iter, "NAME");
		char* ioid = snd_device_name_get_hint(*iter, "IOID");
   
   		if ((ioid != NULL) && (strcmp("Input", ioid) == 0) && (camoramic_string_startswith(name, "dsnoop"))){
			count++;
		}
		
		if (name && strcmp("null", name)) {
			 free(name);
		}
		
		if (ioid && strcmp("null", ioid)) {
			 free(ioid);			
		}
		
		iter++;
	}
	if (count == 0) {
		no_alsa_devices = 0;
	}
	snd_device_name_free_hint((void**)hints);
	return count;
}

void camoramic_set_current_device(int card, int device) {
	for (int i = 0; i < camoramic_alsautil_get_device_count(); i++)
	{
		if ((alsa_devices[i].card == card) && (alsa_devices[i].device == device)) {
			current_alsa_device = alsa_devices[i];
		}
	}
}

void camoramic_alsautil_init() {
	if (camoramic_alsautil_get_device_count() == 0) {
		no_alsa_devices = 1;
		return;
	}
	if (alsa_devices != NULL) {
		free(alsa_devices);
	}
	alsa_devices = calloc(camoramic_alsautil_get_device_count(), sizeof(camoramic_alsautil_device));
	int error;
	int count = 0;
    char** hints;
    char** iter;
	
	error = snd_device_name_hint(-1, "pcm", (void***)&hints);
	
    iter = hints;
    while (*iter != NULL) {
		char* name = snd_device_name_get_hint(*iter, "NAME");
		char* ioid = snd_device_name_get_hint(*iter, "IOID");

   		if ((ioid != NULL) && (strcmp("Input", ioid) == 0) && (camoramic_string_startswith(name, "dsnoop"))){
			camoramic_alsautil_device dev;
			char* card_name_dup = strdup(name);
			char* card_name = strstr(strtok(card_name_dup, ","), "=");
			card_name++;
			dev.card = snd_card_get_index(card_name);
			char* device_name_dup = strdup(name);
			char* device_name = strstr(device_name_dup,",DEV=");
			device_name = device_name + 5;
			dev.device = atoi(device_name);
			char *ccard_name;
			snd_card_get_name(dev.card, &ccard_name);
			char *friendly_name = malloc(strlen(_("Device %d on %s"))+strlen(ccard_name)+512);
			snprintf(friendly_name, strlen(_("Device %d on %s"))+strlen(ccard_name)+512, _("Device %d on %s"), dev.device+1, ccard_name);
			free(ccard_name);
			dev.friendly_name = friendly_name;
			alsa_devices[count] = dev;
			count++;
			device_name = device_name - 5;
			free(device_name_dup);
			free(card_name_dup);
		}
		
		if (name && strcmp("null", name)) {
			 free(name);
		}
		
		if (ioid && strcmp("null", ioid)) {
			 free(ioid);			
		}	
		iter++;
	}
	snd_device_name_free_hint((void**)hints);
}
