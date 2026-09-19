#include <string.h>

#include "ix4lcd.h"

void menu_title(const char *text)
{
    int len;
    int width;
    int x;

    len = strlen(text);

    if (len > 21)
        len = 21;

    width = len * 6;
    x = (LCD_W - width) / 2;

    if (x < 1)
        x = 1;

    lcd_text(x, 1, text);
}

void menu_line(int row, const char *text)
{
    int len;
    char line[22];

    len = strlen(text);

    if (len > 21)
        len = 21;

    memcpy(line, text, len);
    line[len] = '\0';

    lcd_text(7, row, line);
}

void menu_page(int page)
{
    lcd_log_reset();
    lcd_clear();

    lcd_text(1, 0, "+-------------------+");
    lcd_text(1, 7, "+-------------------+");

    for (int row = 1; row < 7; row++) {
        lcd_text(1, row, "|");
        lcd_text(121, row, "|");
    }

    switch (page) {
    case 0:
        page_device();
        break;

    case 1:
        page_space();
        break;

    case 2:
        page_ip();
        break;

    case 3:
        page_storage();
        break;

    default:
        page_device();
        break;
    }

    lcd_log_page(page);
}
