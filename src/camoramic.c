#include <gst/gst.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <locale.h>
#include "pipeline.h"
#include "settings.h"
#include "ui.h"
#include "v4l2utility.h"
#include "alsautility.h"
#include "misc.h"


int camoramic_gst_init(int *argc, char ***argv)
{
    GError *error;
    if (gst_init_check(argc, argv, &error) == FALSE)
    {
       camoramic_set_error(error->message);
    }
}

int main(int argc, char *argv[])
{
	setlocale(LC_ALL, "");
	bindtextdomain("camoramic", "/usr/share/locale/");
	textdomain("camoramic");
  
    gtk_init(&argc, &argv);
    g_thread_init (NULL);
	gdk_threads_init ();
	gdk_threads_enter ();

    camoramic_gst_init(&argc, &argv);
	camoramic_gst_init_fx();

	camoramic_alsautil_init();
	camoramic_settings_init();
    camoramic_v4l2util_init();
    camoramic_sfx_init();

	devicecount_cache = camoramic_v4l2util_get_device_count();
	alsadevicecount_cache = camoramic_alsautil_get_device_count();

    if (camoramic_v4l2util_get_device_count() == 0)
    {
       camoramic_set_error(_("Unable to find any video devices"));
    } else {
		int invalidate_device = 1;
		for (int i = 0; i < camoramic_v4l2util_get_device_count(); i++)
		{
			if (current_device == devices[i]) {
				invalidate_device = 0;
			}
		}
		if (invalidate_device == 1) {
			current_device = devices[0];
		}
		camoramic_gst_create_pipeline();	
	}

    camoramic_ui_create();

	if (camoramic_v4l2util_get_device_count() != 0) {
		char *device_name = camoramic_v4l2util_get_friendly_name(current_device);
		char *statusbar_text = malloc(strlen(_("Current device: %s")) + strlen(device_name) + 1);
		sprintf(statusbar_text, _("Current device: %s"), device_name);
		camoramic_ui_update_statusbar(statusbar_text);
		free(statusbar_text);
		free(device_name);

		gst_element_set_state(pipeline, GST_STATE_PLAYING);
	}
	
    gtk_main();
	gdk_threads_leave ();
    camoramic_sfx_destroy();
    return 0;
}
