#include <stdio.h>
#include <string.h>
#include <time.h>

#include "ix4lcd.h"

void page_datetime(void)
{
    time_t now;
    struct tm tm;
    char buf[64];
    int len;
    int width;
    int x;

    menu_title("DATE / TIME");

    now = time(NULL);
    localtime_r(&now, &tm);

    strftime(buf, sizeof(buf), "%d %b %H:%M:%S", &tm);

    len = strlen(buf);

    if (len > 21)
        len = 21;

    width = len * 6;
    x = (LCD_W - width) / 2;

    if (x < 1)
        x = 1;

    lcd_text(x, 3, buf);
}
