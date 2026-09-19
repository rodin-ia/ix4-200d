#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ix4lcd.h"

void page_device(void)
{
    char hostname[64];
    int len;
    int width;
    int x;

    if (gethostname(hostname, sizeof(hostname) - 1) != 0)
        snprintf(hostname, sizeof(hostname), "ix4-200d");

    hostname[sizeof(hostname) - 1] = '\0';

    menu_title("DEVICE NAME");

    len = strlen(hostname);

    if (len > 21)
        len = 21;

    width = len * 6;
    x = (LCD_W - width) / 2;

    if (x < 1)
        x = 1;

    lcd_text(x, 3, hostname);
}
