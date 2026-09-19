#ifndef IX4LCD_H
#define IX4LCD_H

#include <stddef.h>

/* LCD */

#define LCD_W 128
#define LCD_H 64

/* Page handlers */

void page_device(void);
void page_space(void);
void page_space_detail(void);
void page_ip(void);
void page_storage(void);
void page_bays(void);

/* Common LCD functions */

void lcd_clear(void);
void lcd_text(int x, int page, const char *text);

/* Common helpers */

void menu_line(int row, const char *text);
void menu_page(int page);

void lcd_log(const char *fmt, ...);
void lcd_log_page(int page);
void lcd_log_reset(void);

void simulate_button(const char *button);

/* Main page navigation */

int page_get(void);
void page_set(int page);

void page_next(void);

/* Detail mode */

int detail_get(void);
void detail_set(int detail);
void detail_toggle(void);

void menu_title(const char *text);

#endif
