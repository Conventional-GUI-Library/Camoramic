#include <canberra.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "misc.h"
#include "ui.h"

char* error = NULL;
ca_context *sfx_ctx;
struct stat sb;

void camoramic_set_error(char* errorstring) {
	if (ui_created == 0) {
		if (error == NULL) {
			error = errorstring;
		}
	} else {
		camoramic_ui_show_error_dialog(errorstring);
		exit(1);
	}
}

void camoramic_sfx_init()
{
    if (ca_context_create(&sfx_ctx) != 0)
    {
        camoramic_set_error(_("Unable to create a canberra context."));
    }
}

void camoramic_sfx_play(char *event_desc, char *sound)
{
    ca_context_play(sfx_ctx, 0, CA_PROP_EVENT_ID, sound, CA_PROP_EVENT_DESCRIPTION, event_desc, NULL);
}

void camoramic_sfx_destroy()
{
    ca_context_destroy(sfx_ctx);
}

int camoramic_string_startswith(char *string1, char *string2)
{
   if(strncmp(string1, string2, strlen(string2)) == 0)  
   { 
	   return 1;
   } else {
	   return 0;
   }
}

int camoramic_string_endswith(char *string1, char *string2)
{
    if (!string1 || !string2) {
        return 0;
    }
    int lenstring1 = strlen(string1);
    int lenstring2 = strlen(string2);
    if (lenstring2 > lenstring1) {
        return 0;
    }
    return strncmp(string1 + lenstring1 - lenstring2, string2, lenstring2) == 0;
}


int camoramic_get_gcd(int n1, int n2) {
    int citer;
    int gcd;

    for(citer=1; citer <= n1 && citer <= n2; ++citer)
    {
        if(n1%citer==0 && n2%citer==0) {
            gcd = citer;
        }
    }
    
    return gcd;
}
