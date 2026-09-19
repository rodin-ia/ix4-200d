#include <stdio.h>
#include <string.h>

#include "ix4lcd.h"

#define PAGE_FILE   "/tmp/ix4lcd-page"
#define DETAIL_FILE "/tmp/ix4lcd-detail"

#define PAGE_MIN 0
#define PAGE_MAX 4

#define DETAIL_MIN 0
#define DETAIL_MAX 2

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
     * Changing the main page always resets detail mode.
     */
    detail_set(DETAIL_MIN);

    page_set(page);
    menu_page(page);
}

int detail_get(void)
{
    FILE *f;
    int detail;

    f = fopen(DETAIL_FILE, "r");

    if (!f)
        return DETAIL_MIN;

    if (fscanf(f, "%d", &detail) != 1) {
        fclose(f);
        return DETAIL_MIN;
    }

    fclose(f);

    if (detail < DETAIL_MIN || detail > DETAIL_MAX)
        return DETAIL_MIN;

    return detail;
}

void detail_set(int detail)
{
    FILE *f;

    if (detail < DETAIL_MIN)
        detail = DETAIL_MIN;

    if (detail > DETAIL_MAX)
        detail = DETAIL_MAX;

    f = fopen(DETAIL_FILE, "w");

    if (!f)
        return;

    fprintf(f, "%d\n", detail);
    fclose(f);
}

void detail_next(void)
{
    int page;
    int detail;

    page = page_get();
    detail = detail_get();

    /*
     * Currently only ARRAY has a detail menu.
     */
    if (page != 1)
        return;

    detail++;

    if (detail > DETAIL_MAX)
        detail = DETAIL_MIN;

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
        detail_next();
        return;
    }

    fprintf(stderr,
            "Unknown button: %s\n"
            "Available: next, detail\n",
            button);
}
