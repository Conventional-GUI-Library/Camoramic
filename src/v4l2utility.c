#include <gst/gst.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <math.h>
#include "v4l2utility.h"
#include "ui.h"
#include "settings.h"
#include "misc.h"

char *format;
char *device_friendly_name;
int* devices;
int devicecount = 0;
int devicecount_cache = 0;
int caps_struct_no;
int iter = 0;
GstStructure *caps_struct;
GstDeviceProvider *v4l2provider;

void camoramic_v4l2util_init_fe(gpointer item, gpointer data)
{
    GstDevice *device = item;
    GstStructure *deviceextrastruct = gst_device_get_properties(device);
    const char *path_string = gst_structure_get_string(deviceextrastruct, "device.path");
    devices[iter] = atoi(&path_string[strlen(path_string) - 1]);
    iter++;
}

void camoramic_v4l2util_reload()
{
	iter = 0;
	if (devices != NULL) {
		free(devices);
	}
	devices = calloc(camoramic_v4l2util_get_device_count(), sizeof(int));
    g_list_foreach(gst_device_provider_get_devices(v4l2provider), camoramic_v4l2util_init_fe, NULL);
}

void camoramic_v4l2util_init()
{
    v4l2provider = gst_device_provider_factory_get_by_name("v4l2deviceprovider");
    if (v4l2provider == NULL)
    {
        camoramic_set_error(_("Unable to create a V4L2 device provider."));
    }
    devices = calloc(camoramic_v4l2util_get_device_count(), sizeof(int));
    g_list_foreach(gst_device_provider_get_devices(v4l2provider), camoramic_v4l2util_init_fe, NULL);
}

void camoramic_v4l2util_get_friendly_name_fe(gpointer item, gpointer data)
{
    int deviceint = *(int *)data;
    GstDevice *device = item;
    GstStructure *deviceextrastruct = gst_device_get_properties(device);
    const char *path_string = gst_structure_get_string(deviceextrastruct, "device.path");
    if (atoi(&path_string[strlen(path_string) - 1]) == deviceint)
    {
        device_friendly_name = gst_device_get_display_name(device);
    }
    gst_structure_free(deviceextrastruct);
}

void camoramic_v4l2util_get_device_count_fe(gpointer item, gpointer data)
{
    devicecount++;
}

void camoramic_v4l2util_get_format_fe(gpointer item, gpointer data)
{
    int deviceint = *(int *)data;
    GstDevice *device = item;
    GstStructure *deviceextrastruct = gst_device_get_properties(device);
    const char *path_string = gst_structure_get_string(deviceextrastruct, "device.path");
    if (atoi(&path_string[strlen(path_string) - 1]) == deviceint)
    {
        GstCaps *caps = gst_caps_normalize(gst_device_get_caps(device));
        GstStructure *caps_struct = gst_caps_get_structure(caps, caps_struct_no);
		format = strdup(gst_structure_get_name(caps_struct));
        gst_caps_unref(caps);
    }
    gst_structure_free(deviceextrastruct);
}

char *camoramic_v4l2util_get_format(int device, int ccaps)
{
    caps_struct_no = ccaps;
    g_list_foreach(gst_device_provider_get_devices(v4l2provider), camoramic_v4l2util_get_format_fe, &device);
    return format;
}

void camoramic_v4l2util_get_caps_count_fe(gpointer item, gpointer data)
{
    int deviceint = *(int *)data;
    GstDevice *device = item;
    GstStructure *deviceextrastruct = gst_device_get_properties(device);
    const char *path_string = gst_structure_get_string(deviceextrastruct, "device.path");
    if (atoi(&path_string[strlen(path_string) - 1]) == deviceint)
    {
        GstCaps *caps = gst_caps_normalize(gst_device_get_caps(device));
        caps_struct_no = gst_caps_get_size(caps);
        gst_caps_unref(caps);
    }
    gst_structure_free(deviceextrastruct);
}

int camoramic_v4l2util_get_caps_count(int device)
{
    g_list_foreach(gst_device_provider_get_devices(v4l2provider), camoramic_v4l2util_get_caps_count_fe, &device);
    return caps_struct_no;
}

void camoramic_v4l2util_get_caps_fe(gpointer item, gpointer data)
{
    int deviceint = *(int *)data;
    GstDevice *device = item;
    GstStructure *deviceextrastruct = gst_device_get_properties(device);
    const char *path_string = gst_structure_get_string(deviceextrastruct, "device.path");
    if (atoi(&path_string[strlen(path_string) - 1]) == deviceint)
    {
        GstCaps *caps = gst_caps_normalize(gst_device_get_caps(device));
        caps_struct = gst_caps_get_structure(caps, caps_struct_no);
        //gst_caps_unref(caps);
    }
    gst_structure_free(deviceextrastruct);
}

GstCaps *camoramic_v4l2util_get_caps(int device, int ccaps)
{
    caps_struct_no = ccaps;
    GList *device_list = gst_device_provider_get_devices(v4l2provider);
    g_list_foreach(device_list, camoramic_v4l2util_get_caps_fe, &device);
	g_list_free_full(g_steal_pointer(&device_list), g_object_unref);    
	return gst_caps_new_full(gst_structure_copy(caps_struct), NULL);
}

char *camoramic_v4l2util_get_human_string_caps(int device, int ccaps)
{
	int width, height, framerate, framerate_denom;
	char* string = malloc(512);
    caps_struct_no = ccaps;
    GList *device_list = gst_device_provider_get_devices(v4l2provider);
    g_list_foreach(device_list, camoramic_v4l2util_get_caps_fe, &device);
	g_list_free_full(g_steal_pointer(&device_list), g_object_unref);
    gst_structure_get_int(caps_struct, "width", &width);
    gst_structure_get_int(caps_struct, "height", &height);
    gst_structure_get_fraction(caps_struct, "framerate", &framerate, &framerate_denom);
    framerate = (int)round(framerate/framerate_denom);
    if ((framerate <= 4) || (framerate > 75)) {
		snprintf(string, 512, "%dx%d", width, height);
	} else {
		snprintf(string, 512, "%dx%d (%d fps)", width, height, framerate);
	}
    return string;
}

int camoramic_v4l2util_get_caps_framerate(int device, int ccaps)
{
	int framerate, framerate_denom;
    caps_struct_no = ccaps;
    GList *device_list = gst_device_provider_get_devices(v4l2provider);
    g_list_foreach(device_list, camoramic_v4l2util_get_caps_fe, &device);
	g_list_free_full(g_steal_pointer(&device_list), g_object_unref);
    gst_structure_get_fraction(caps_struct, "framerate", &framerate, &framerate_denom);
    if (framerate_denom == 0) {
		return 0;
	}
    framerate = (int)round(framerate/framerate_denom);
    return framerate;
}

char *camoramic_v4l2util_device_to_string(int device)
{
    char *devicestr = malloc(256);
    snprintf(devicestr, 256, "/dev/video%d", device);
    return devicestr;
}

char *camoramic_v4l2util_get_friendly_name(int device)
{
    GList *device_list = gst_device_provider_get_devices(v4l2provider);
    g_list_foreach(device_list, camoramic_v4l2util_get_friendly_name_fe, &device);
	g_list_free_full(g_steal_pointer(&device_list), g_object_unref);
    return device_friendly_name;
}

int camoramic_v4l2util_get_device_count()
{
    devicecount = 0;
    GList *device_list = gst_device_provider_get_devices(v4l2provider);
    g_list_foreach(device_list, camoramic_v4l2util_get_device_count_fe, NULL);
	g_list_free_full(g_steal_pointer(&device_list), g_object_unref);
    return devicecount;
}
