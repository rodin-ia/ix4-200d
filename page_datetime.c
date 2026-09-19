#include <time.h>

#include "ix4lcd.h"

void page_datetime(void)
{
    time_t now;
    struct tm tm;
    char buf[64];

    menu_title("DATE / TIME");

    now = time(NULL);
    localtime_r(&now, &tm);

    strftime(buf, sizeof(buf), "%d %b %H:%M:%S", &tm);

    lcd_text(7, 3, buf);
}
