#include <stdio.h>
#include <string.h>

#include "ix4lcd.h"

#define PAGE_FILE "/tmp/ix4lcd-page"
#define DETAIL_FILE "/tmp/ix4lcd-detail"

#define PAGE_MIN  0
#define PAGE_MAX  4

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
    int page;

    page = page_get();

    page++;

    if (page > PAGE_MAX)
        page = PAGE_MIN;

    /*
     * Changing the main page always leaves detail mode.
     */
    detail_set(0);

    page_set(page);
    menu_page(page);
}

int detail_get(void)
{
    FILE *f;
    int detail;

    f = fopen(DETAIL_FILE, "r");

    if (!f)
        return 0;

    if (fscanf(f, "%d", &detail) != 1) {
        fclose(f);
        return 0;
    }

    fclose(f);

    if (detail != 0 && detail != 1)
        return 0;

    return detail;
}

void detail_set(int detail)
{
    FILE *f;

    detail = detail ? 1 : 0;

    f = fopen(DETAIL_FILE, "w");

    if (!f)
        return;

    fprintf(f, "%d\n", detail);
    fclose(f);
}

void detail_toggle(void)
{
    int page;
    int detail;

    page = page_get();
    detail = detail_get();

    /*
     * Only pages with a detail view can enter detail mode.
     */
    if (page != 1)
        return;

    detail = !detail;

    detail_set(detail);
    menu_page(page);
}

void simulate_button(const char *button)
{
    lcd_log("button=%s", button);

    if (strcmp(button, "next") == 0) {
        page_next();
        return;
    }

    if (strcmp(button, "detail") == 0) {
        detail_toggle();
        return;
    }

    fprintf(stderr,
            "Unknown button: %s\n"
            "Available: next, detail\n",
            button);
}#include <stdio.h>
#include <string.h>

#include "ix4lcd.h"

#define PAGE_FILE "/tmp/ix4lcd-page"
#define PAGE_MIN  0
#define PAGE_MAX  4

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
