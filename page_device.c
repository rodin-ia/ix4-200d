#include <stdio.h>
#include <unistd.h>
#include "ix4lcd.h"

void page_device(void)
{
    char hostname[64];

    if (gethostname(hostname, sizeof(hostname) - 1) != 0)
        snprintf(hostname, sizeof(hostname), "ix4-200d");

    hostname[sizeof(hostname) - 1] = '\0';

    menu_title("DEVICE NAME");

    menu_line(3, hostname);
}
