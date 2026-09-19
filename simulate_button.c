#include <stdio.h>
#include <string.h>

#include "ix4lcd.h"

#define PAGE_FILE "/tmp/ix4lcd-page"
#define PAGE_MIN  0
#define PAGE_MAX  3

int page_get(void)
{
    FILE *f;
    int page;

    f = fopen(PAGE_FILE, "r");

    if (!f)
        return 0;

    if (fscanf(f, "%d", &page) != 1) {
        fclose(f);
        return 0;
    }

    fclose(f);

    if (page < PAGE_MIN || page > PAGE_MAX)
        return 0;

    return page;
}

void page_set(int page)
{
    FILE *f;

    if (page < PAGE_MIN)
        page = PAGE_MIN;

    if (page > PAGE_MAX)
        page = PAGE_MAX;

    f = fopen(PAGE_FILE, "w");

    if (!f)
        return;

    fprintf(f, "%d\n", page);
    fclose(f);
}

void page_next(void)
{
    int page = page_get();

    page++;

    if (page > PAGE_MAX)
        page = PAGE_MIN;

    page_set(page);
    menu_page(page);
}

void page_prev(void)
{
    int page = page_get();

    page--;

    if (page < PAGE_MIN)
        page = PAGE_MAX;

    page_set(page);
    menu_page(page);
}

void simulate_button(const char *button)
{
    lcd_log("button=%s", button);

    if (strcmp(button, "next") == 0) {
        page_next();
        return;
    }

    if (strcmp(button, "prev") == 0) {
        page_prev();
        return;
    }

    fprintf(stderr,
            "Unknown button: %s\n"
            "Available: next, prev\n",
            button);
}
