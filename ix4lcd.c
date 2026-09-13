#define _GNU_SOURCE

#include "ix4lcd.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <stdarg.h>

#define LCD_W 128
#define LCD_H 64
#define LCD_COLS 21
#define LCD_LINES 8

#define GPIO_RS 15
#define GPIO_A0 34
#define GPIO_CS 35
#define GPIO_RW 44
#define GPIO_E  45

static const int data_gpio[8] = {
    36, 37, 38, 39, 40, 41, 42, 43
};

static int gpio_fd[46];

static volatile sig_atomic_t running = 1;

static char lcd_log_buffer[LCD_LINES][LCD_COLS + 1];

static int log_enabled = 0;


void lcd_log(const char *fmt, ...)
{
    if (!log_enabled)
        return;

    va_list ap;

    va_start(ap, fmt);

    fprintf(stderr, "[ix4lcd] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");

    va_end(ap);
}

void lcd_log_reset(void)
{
    for (int i = 0; i < LCD_LINES; i++) {
        memset(lcd_log_buffer[i], ' ', LCD_COLS);
        lcd_log_buffer[i][LCD_COLS] = '\0';
    }
}


void lcd_log_page(int page)
{
    if (!log_enabled)
        return;

    if (page >= 0)
        fprintf(stderr, "[ix4lcd] PAGE %d\n", page);
    else
        fprintf(stderr, "[ix4lcd] DISPLAY\n");

    for (int i = 0; i < LCD_LINES; i++)
        fprintf(stderr, "[ix4lcd] |%-21s|\n",
                lcd_log_buffer[i]);
}

/* =========================================================
 * GPIO
 * ========================================================= */

static void gpio_export(int gpio)
{
    char path[64];
    char buf[16];

    snprintf(path, sizeof(path),
             "/sys/class/gpio/gpio%d", gpio);

    if (access(path, F_OK) == 0)
        return;

    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) {
        perror("gpio export");
        exit(1);
    }

    snprintf(buf, sizeof(buf), "%d", gpio);

    if (write(fd, buf, strlen(buf)) < 0 && errno != EBUSY) {
        perror("gpio export");
        close(fd);
        exit(1);
    }

    close(fd);

    usleep(10000);
}


static void gpio_init(int gpio)
{
    char path[64];

    gpio_export(gpio);

    snprintf(path, sizeof(path),
             "/sys/class/gpio/gpio%d/direction", gpio);

    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        perror("gpio direction");
        exit(1);
    }

    if (write(fd, "out", 3) != 3) {
        perror("gpio direction");
        close(fd);
        exit(1);
    }

    close(fd);

    snprintf(path, sizeof(path),
             "/sys/class/gpio/gpio%d/value", gpio);

    gpio_fd[gpio] = open(path, O_WRONLY);

    if (gpio_fd[gpio] < 0) {
        perror("gpio value");
        exit(1);
    }
}


static void gpio_set(int gpio, int value)
{
    char c = value ? '1' : '0';

    if (lseek(gpio_fd[gpio], 0, SEEK_SET) < 0)
        return;

    if (write(gpio_fd[gpio], &c, 1) != 1)
        return;
}


static void gpio_close_all(void)
{
    int i;

    for (i = 0; i < 46; i++) {
        if (gpio_fd[i] >= 0)
            close(gpio_fd[i]);
    }
}


/* =========================================================
 * LCD low level
 * ========================================================= */

static void lcd_write(uint8_t a0, uint8_t value)
{
    int i;

    gpio_set(GPIO_E, 0);

    /*
     * data_gpio[] = D0..D7
     */
    for (i = 0; i < 8; i++)
        gpio_set(data_gpio[i], (value >> i) & 1);

    gpio_set(GPIO_A0, a0);
    gpio_set(GPIO_RW, 0);
    gpio_set(GPIO_CS, 0);

    gpio_set(GPIO_E, 1);
    gpio_set(GPIO_E, 0);

    gpio_set(GPIO_CS, 1);
}


static void lcd_command(uint8_t cmd)
{
    lcd_write(0, cmd);
}


static void lcd_data(uint8_t data)
{
    lcd_write(1, data);
}


/* =========================================================
 * LCD init
 * ========================================================= */

static void lcd_init(void)
{
    static const uint8_t init_seq[] = {
        0xae,
        0xa2,
        0xa0,
        0xc8,
        0xa6,
        0x40,
        0x22,
        0x81,
        0x3f,
        0xf8,
        0x00,
        0xa4,
        0x2c
    };

    size_t i;

    gpio_set(GPIO_RS, 1);

    for (i = 0; i < sizeof(init_seq); i++)
        lcd_command(init_seq[i]);

    lcd_command(0x2e);
    lcd_command(0x2f);

    lcd_command(0xaf);
}


static void lcd_page(int page)
{
    lcd_command(0xb0 + page);
    lcd_command(0x10);
    lcd_command(0x00);
}


void lcd_clear(void)
{
    int page;
    int x;

    lcd_command(0xae);

    for (page = 0; page < 8; page++) {
        lcd_page(page);

        for (x = 0; x < 128; x++)
            lcd_data(0x00);
    }

    lcd_command(0xaf);
}


/* =========================================================
 * Font 5x7
 * ========================================================= */

static const uint8_t font_digits[10][5] = {
    {0x3e,0x51,0x49,0x45,0x3e},
    {0x00,0x42,0x7f,0x40,0x00},
    {0x42,0x61,0x51,0x49,0x46},
    {0x21,0x41,0x45,0x4b,0x31},
    {0x18,0x14,0x12,0x7f,0x10},
    {0x27,0x45,0x45,0x45,0x39},
    {0x3c,0x4a,0x49,0x49,0x30},
    {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1e}
};


static const uint8_t font_upper[26][5] = {
    {0x7e,0x11,0x11,0x11,0x7e},
    {0x7f,0x49,0x49,0x49,0x36},
    {0x3e,0x41,0x41,0x41,0x22},
    {0x7f,0x41,0x41,0x22,0x1c},
    {0x7f,0x49,0x49,0x49,0x41},
    {0x7f,0x09,0x09,0x09,0x01},
    {0x3e,0x41,0x49,0x49,0x7a},
    {0x7f,0x08,0x08,0x08,0x7f},
    {0x00,0x41,0x7f,0x41,0x00},
    {0x20,0x40,0x41,0x3f,0x01},
    {0x7f,0x08,0x14,0x22,0x41},
    {0x7f,0x40,0x40,0x40,0x40},
    {0x7f,0x02,0x0c,0x02,0x7f},
    {0x7f,0x04,0x08,0x10,0x7f},
    {0x3e,0x41,0x41,0x41,0x3e},
    {0x7f,0x09,0x09,0x09,0x06},
    {0x3e,0x41,0x51,0x21,0x5e},
    {0x7f,0x09,0x19,0x29,0x46},
    {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7f,0x01,0x01},
    {0x3f,0x40,0x40,0x40,0x3f},
    {0x1f,0x20,0x40,0x20,0x1f},
    {0x3f,0x40,0x38,0x40,0x3f},
    {0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},
    {0x61,0x51,0x49,0x45,0x43}
};


static void font_get(char c, uint8_t out[5])
{
    memset(out, 0, 5);

    if (c >= '0' && c <= '9') {
        memcpy(out, font_digits[c - '0'], 5);
        return;
    }

    if (c >= 'a' && c <= 'z')
        c -= 32;

    if (c >= 'A' && c <= 'Z') {
        memcpy(out, font_upper[c - 'A'], 5);
        return;
    }

    switch (c) {

    case '.':
        out[2] = 0x40;
        break;

    case ':':
        out[2] = 0x24;
        break;

    case '-':
        out[1] = 0x08;
        out[2] = 0x08;
        out[3] = 0x08;
        break;

    case '_':
        out[0] = 0x40;
        out[1] = 0x40;
        out[2] = 0x40;
        out[3] = 0x40;
        out[4] = 0x40;
        break;

    case '/':
        out[4] = 0x01;
        out[3] = 0x02;
        out[2] = 0x04;
        out[1] = 0x08;
        out[0] = 0x10;
        break;

    case '[':
        out[0] = 0x7f;
        out[1] = 0x41;
        out[2] = 0x41;
        break;

    case ']':
        out[2] = 0x41;
        out[3] = 0x41;
        out[4] = 0x7f;
        break;

    case '(':
        out[1] = 0x1c;
        out[0] = 0x22;
        break;

    case ')':
        out[3] = 0x22;
        out[4] = 0x1c;
        break;

    case '=':
        out[1] = 0x14;
        out[2] = 0x14;
        out[3] = 0x14;
        break;

    case '+':
        out[2] = 0x08;
        out[1] = 0x1c;
        out[2] = 0x08;
        out[3] = 0x1c;
        break;

    case '*':
        out[1] = 0x14;
        out[2] = 0x08;
        out[3] = 0x14;
        break;

    case '%':
        out[0] = 0x62;
        out[2] = 0x08;
        out[4] = 0x23;
        break;

    case '#':
        out[0] = 0x14;
        out[1] = 0x7f;
        out[2] = 0x14;
        out[3] = 0x7f;
        out[4] = 0x14;
        break;

    case '|':
        out[2] = 0x7f;
        break;

    case '!':
        out[2] = 0x5f;
        break;

    case '?':
        out[0] = 0x02;
        out[1] = 0x01;
        out[2] = 0x51;
        out[3] = 0x09;
        out[4] = 0x06;
        break;

    case ',':
        out[1] = 0x40;
        break;

    default:
        break;
    }
}

static void lcd_column(int x)
{
    int col = x & 0x7f;

    lcd_command(0x10 | ((col >> 4) & 0x0f));
    lcd_command(0x00 | (col & 0x0f));
}

/* =========================================================
 * Text
 * ========================================================= */

void lcd_text(int x, int page, const char *text)
{
    const unsigned char *p =
    (const unsigned char *)text;

    if (page >= 0 && page < LCD_LINES) {
        int col = x / 6;

        if (col < 0)
            col = 0;

        for (int i = 0;
             text[i] != '\0' &&
             col < LCD_COLS;
        i++, col++) {
            lcd_log_buffer[page][col] = text[i];
        }

        lcd_log_buffer[page][LCD_COLS] = '\0';
    }

    lcd_page(page);
    lcd_column(x);

    while (*p && x < LCD_W) {
        uint8_t glyph[5];

        font_get(*p, glyph);

        for (int i = 0; i < 5 && x < LCD_W; i++)
            lcd_data(glyph[i]);

        lcd_data(0x00);
        x += 6;
        p++;
    }
}
/* =========================================================
 * Buffer handling
 * ========================================================= */

static void display_buffer(const char *buffer)
{
    char line[256];
    const char *p = buffer;

    lcd_log_reset();

    lcd_clear();

    for (int row = 0; row < LCD_LINES && *p; row++) {

        int len = 0;

        while (*p && *p != '\n' && len < LCD_COLS) {
            line[len++] = *p++;
        }

        line[len] = '\0';

        if (*p == '\n')
            p++;

        int width = len * 6;
        int x = 0;

        if (width < LCD_W)
            x = (LCD_W - width) / 2;

        lcd_text(x, row, line);
    }

    lcd_log_page(-1);
}

/* =========================================================
 * stdin
 * ========================================================= */

static char *read_stdin(void)
{
    size_t size = 1024;
    size_t len = 0;

    char *buf = malloc(size);

    if (!buf)
        return NULL;

    int c;

    while ((c = getchar()) != EOF) {

        if (len + 1 >= size) {
            size *= 2;

            char *tmp = realloc(buf, size);

            if (!tmp) {
                free(buf);
                return NULL;
            }

            buf = tmp;
        }

        buf[len++] = c;

        /*
         * 8 lines is all the LCD can show.
         * Keep some extra data harmlessly.
         */
        if (len > 8192)
            break;
    }

    buf[len] = '\0';

    return buf;
}


/* =========================================================
 * File
 * ========================================================= */

static char *read_file(const char *filename)
{
    FILE *f;
    long size;
    char *buf;

    f = fopen(filename, "r");

    if (!f) {
        perror(filename);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    size = ftell(f);

    if (size < 0) {
        fclose(f);
        return NULL;
    }

    rewind(f);

    buf = malloc(size + 1);

    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t n = fread(buf, 1, size, f);

    fclose(f);

    buf[n] = '\0';

    return buf;
}


/* =========================================================
 * Signals
 * ========================================================= */

static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}


/* =========================================================
 * Usage
 * ========================================================= */

static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s \"line1\" \"line2\" ...\n"
        "  %s --clear\n"
        "  %s --page 0|1|2|3\n"
        "  %s --watch SECONDS FILE\n"
        "  echo text | %s\n"
        "  cat /proc/mdstat | %s\n",
        prog, prog, prog, prog, prog, prog);
}


/* =========================================================
 * Main
 * ========================================================= */

int main(int argc, char **argv)
{
    memset(gpio_fd, -1, sizeof(gpio_fd));

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /*
     * GPIO
     */

    gpio_init(GPIO_RS);
    gpio_init(GPIO_A0);
    gpio_init(GPIO_CS);
    gpio_init(GPIO_RW);
    gpio_init(GPIO_E);

    for (int i = 0; i < 8; i++)
        gpio_init(data_gpio[i]);

    gpio_set(GPIO_RS, 1);
    gpio_set(GPIO_CS, 1);
    gpio_set(GPIO_RW, 0);
    gpio_set(GPIO_E, 0);

    /*
     * LCD
     */

    lcd_init();

    /*
     * Global options
     */

    int arg = 1;

    if (arg < argc && strcmp(argv[arg], "--log") == 0) {
        log_enabled = 1;
        arg++;
    }

    /*
     * --page N
     * --page next
     * --page prev
     */

    if (arg < argc && strcmp(argv[arg], "--page") == 0) {

        if (arg + 1 >= argc) {
            fprintf(stderr, "missing page\n");
            gpio_close_all();
            return 1;
        }

        const char *page_arg = argv[arg + 1];

        if (strcmp(page_arg, "next") == 0) {

            simulate_button("next");

            gpio_close_all();
            return 0;
        }

        if (strcmp(page_arg, "prev") == 0) {

            simulate_button("prev");

            gpio_close_all();
            return 0;
        }

        char *end;
        long page = strtol(page_arg, &end, 10);

        if (*end != '\0' || page < 0 || page > 4) {
            fprintf(stderr, "invalid page: %s\n", page_arg);
            gpio_close_all();
            return 1;
        }

        menu_page((int)page);

        gpio_close_all();
        return 0;
    }

    /*
     * --clear
     */

    if (arg < argc && strcmp(argv[arg], "--clear") == 0) {

        lcd_clear();

        gpio_close_all();
        return 0;
    }

    /*
     * --watch SECONDS FILE
     */

    if (arg < argc && strcmp(argv[arg], "--watch") == 0) {

        if (arg + 2 >= argc) {
            fprintf(stderr, "usage: %s [--log] --watch SECONDS FILE\n",
                    argv[0]);
            gpio_close_all();
            return 1;
        }

        int seconds = atoi(argv[arg + 1]);

        if (seconds < 1)
            seconds = 1;

        const char *filename = argv[arg + 2];

        while (running) {

            char *buf = read_file(filename);

            if (buf) {
                display_buffer(buf);
                free(buf);
            }

            for (int i = 0;
                 i < seconds && running;
            i++)
                 sleep(1);
        }

        lcd_clear();

        gpio_close_all();
        return 0;
    }

    /*
     * --button BUTTON
     */

    if (arg < argc && strcmp(argv[arg], "--button") == 0) {

        if (arg + 1 >= argc) {
            fprintf(stderr, "missing button\n");
            gpio_close_all();
            return 1;
        }

        simulate_button(argv[arg + 1]);

        gpio_close_all();
        return 0;
    }

    /*
     * Text arguments:
     *
     * ix4lcd "IP" "192.168.1.1"
     */

    if (arg < argc) {

        char buffer[8192];
        size_t pos = 0;

        buffer[0] = '\0';

        for (int i = arg; i < argc; i++) {

            size_t n = strlen(argv[i]);

            if (pos + n + 2 >= sizeof(buffer))
                break;

            if (pos)
                buffer[pos++] = '\n';

            memcpy(buffer + pos, argv[i], n);

            pos += n;
            buffer[pos] = '\0';
        }

        display_buffer(buffer);

        gpio_close_all();
        return 0;
    }

    /*
     * No arguments:
     * read stdin.
     */

    if (!isatty(STDIN_FILENO)) {

        char *buf = read_stdin();

        if (!buf) {
            gpio_close_all();
            return 1;
        }

        display_buffer(buf);

        free(buf);

        gpio_close_all();
        return 0;
    }

    usage(argv[0]);

    gpio_close_all();
    return 1;
}
