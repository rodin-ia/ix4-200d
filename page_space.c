#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "ix4lcd.h"

struct raid_info {
    char device[32];
    char level[32];
    char state[128];
    char rebuild[64];

    int raid_devices;
    int total_devices;
    int active_devices;
    int working_devices;
    int failed_devices;
    int spare_devices;
};

static void trim(char *s)
{
    char *start;
    char *end;

    start = s;

    while (*start && isspace((unsigned char)*start))
        start++;

    if (start != s)
        memmove(s, start, strlen(start) + 1);

    end = s + strlen(s);

    while (end > s && isspace((unsigned char)*(end - 1)))
        end--;

    *end = '\0';
}

static int parse_int_field(
    const char *line,
    const char *field)
{
    const char *p;

    p = strstr(line, field);

    if (!p)
        return -1;

    p += strlen(field);

    while (*p && (*p == ':' || isspace((unsigned char)*p)))
        p++;

    return atoi(p);
}

static int parse_string_field(
    const char *line,
    const char *field,
    char *out,
    size_t out_size)
{
    const char *p;

    p = strstr(line, field);

    if (!p)
        return -1;

    p += strlen(field);

    while (*p && isspace((unsigned char)*p))
        p++;

    if (*p != ':')
        return -1;

    p++;

    while (*p && isspace((unsigned char)*p))
        p++;

    snprintf(out, out_size, "%s", p);
    trim(out);

    return 0;
}

static int find_md_device(
    char *device,
    size_t size)
{
    FILE *f;
    char line[256];
    char md[32];

    f = fopen("/proc/mdstat", "r");

    if (!f)
        return -1;

    while (fgets(line, sizeof(line), f)) {

        if (sscanf(line, "%31s", md) != 1)
            continue;

        if (strncmp(md, "md", 2) != 0)
            continue;

        if (!isdigit((unsigned char)md[2]))
            continue;

        snprintf(
            device,
            size,
            "/dev/%.*s",
            (int)(size - 6),
            md
        );

        fclose(f);
        return 0;
    }

    fclose(f);

    return -1;
}

static int get_raid_info(
    const char *device,
    struct raid_info *info)
{
    FILE *f;
    char command[128];
    char line[256];

    memset(info, 0, sizeof(*info));

    snprintf(
        info->device,
        sizeof(info->device),
        "%s",
        device
    );

    snprintf(
        command,
        sizeof(command),
        "mdadm --detail %s 2>/dev/null",
        device
    );

    f = popen(command, "r");

    if (!f)
        return -1;

    while (fgets(line, sizeof(line), f)) {

        if (strstr(line, "Raid Level")) {

            parse_string_field(
                line,
                "Raid Level",
                info->level,
                sizeof(info->level)
            );

        } else if (strstr(line, "State")) {

            parse_string_field(
                line,
                "State",
                info->state,
                sizeof(info->state)
            );

        } else if (strstr(line, "Rebuild Status")) {

            parse_string_field(
                line,
                "Rebuild Status",
                info->rebuild,
                sizeof(info->rebuild)
            );

        } else if (strstr(line, "Raid Devices")) {

            info->raid_devices =
                parse_int_field(line, "Raid Devices");

        } else if (strstr(line, "Total Devices")) {

            info->total_devices =
                parse_int_field(line, "Total Devices");

        } else if (strstr(line, "Active Devices")) {

            info->active_devices =
                parse_int_field(line, "Active Devices");

        } else if (strstr(line, "Working Devices")) {

            info->working_devices =
                parse_int_field(line, "Working Devices");

        } else if (strstr(line, "Failed Devices")) {

            info->failed_devices =
                parse_int_field(line, "Failed Devices");

        } else if (strstr(line, "Spare Devices")) {

            info->spare_devices =
                parse_int_field(line, "Spare Devices");
        }
    }

    pclose(f);

    if (info->raid_devices <= 0)
        return -1;

    return 0;
}

static int get_rebuild_percent(
    const char *rebuild)
{
    const char *p;

    if (!rebuild || !*rebuild)
        return -1;

    p = rebuild;

    while (*p && !isdigit((unsigned char)*p))
        p++;

    if (!*p)
        return -1;

    return atoi(p);
}

static int get_rebuild_time(
    char *buf,
    size_t size)
{
    FILE *f;
    char line[256];
    char *p;
    char *end;
    double minutes;

    snprintf(buf, size, "N/A");

    f = fopen("/proc/mdstat", "r");

    if (!f)
        return -1;

    while (fgets(line, sizeof(line), f)) {

        if (!strstr(line, "recovery =") &&
            !strstr(line, "resync ="))
            continue;

        p = strstr(line, "finish=");

        if (!p)
            continue;

        p += strlen("finish=");

        minutes = strtod(p, &end);

        if (end == p)
            continue;

        if (strstr(end, "min")) {

            int total_minutes;
            int hours;
            int mins;

            total_minutes = (int)(minutes + 0.5);

            hours = total_minutes / 60;
            mins = total_minutes % 60;

            if (hours > 0) {

                snprintf(
                    buf,
                    size,
                    "%dh%02dm",
                    hours,
                    mins
                );

            } else {

                snprintf(
                    buf,
                    size,
                    "%dm",
                    mins
                );
            }

        } else if (strstr(end, "sec")) {

            int seconds;

            seconds = (int)(minutes + 0.5);

            snprintf(
                buf,
                size,
                "%ds",
                seconds
            );
        }

        fclose(f);
        return 0;
    }

    fclose(f);

    return -1;
}

static void display_state(
    const char *state)
{
    char first[18];
    char second[22];
    char line[22];

    size_t len;
    size_t first_len;
    size_t second_len;
    size_t i;

    const char *p;
    const char *last_space;

    if (!state || !*state) {
        menu_line(4, "ST:");
        return;
    }

    len = strlen(state);

    if (len <= 17) {

        memcpy(first, state, len);
        first[len] = '\0';

        snprintf(
            line,
            sizeof(line),
            "ST: %s",
            first
        );

        menu_line(4, line);

        return;
    }

    first_len = 17;
    last_space = NULL;

    for (i = 0; i < first_len; i++) {

        if (state[i] == ' ')
            last_space = state + i;
    }

    if (last_space) {

        first_len = (size_t)(last_space - state);
        p = last_space + 1;

    } else {

        p = state + first_len;
    }

    memcpy(first, state, first_len);
    first[first_len] = '\0';

    snprintf(
        line,
        sizeof(line),
        "ST: %s",
        first
    );

    menu_line(4, line);

    second_len = strlen(p);

    if (second_len > 21)
        second_len = 21;

    memcpy(second, p, second_len);
    second[second_len] = '\0';

    menu_line(5, second);
}

static void display_state_detail(
    const char *state)
{
    char first[22];
    char second[22];

    size_t len;
    size_t first_len;
    size_t i;

    const char *p;
    const char *last_space;

    if (!state || !*state) {
        menu_line(5, "STATE: N/A");
        menu_line(6, "");
        return;
    }

    len = strlen(state);

    if (len <= 14) {

        char line[22];

        snprintf(
            line,
            sizeof(line),
            "STATE: %s",
            state
        );

        menu_line(5, line);
        menu_line(6, "");

        return;
    }

    /*
     * "STATE: " uses 7 characters, leaving 14 characters
     * on the first line.
     */
    first_len = 14;
    last_space = NULL;

    for (i = 0; i < first_len; i++) {

        if (state[i] == ' ')
            last_space = state + i;
    }

    if (last_space) {

        first_len = (size_t)(last_space - state);
        p = last_space + 1;

    } else {

        p = state + first_len;
    }

    memcpy(first, state, first_len);
    first[first_len] = '\0';

    snprintf(
        second,
        sizeof(second),
        "STATE: %s",
        first
    );

    menu_line(5, second);

    snprintf(
        second,
        sizeof(second),
        "%s",
        p
    );

    menu_line(6, second);
}

void page_space(void)
{
    struct raid_info info;

    char device[32];
    char line[256];

    const char *short_device;

    int rebuild;

    menu_title("ARRAY");

    if (find_md_device(
            device,
            sizeof(device)) != 0) {

        menu_line(2, "ARRAY: N/A");
        menu_line(3, "LEVEL: N/A");
        menu_line(4, "ST: OFFLINE");
        menu_line(5, "");
        menu_line(6, "REBUILD: N/A");

        return;
    }

    if (get_raid_info(
            device,
            &info) != 0) {

        menu_line(2, "ARRAY: N/A");
        menu_line(3, "LEVEL: N/A");
        menu_line(4, "ST: UNKNOWN");
        menu_line(5, "");
        menu_line(6, "REBUILD: N/A");

        return;
    }

    short_device = device;

    if (strncmp(device, "/dev/", 5) == 0)
        short_device = device + 5;

    snprintf(
        line,
        sizeof(line),
        "ARRAY: %s",
        short_device
    );

    menu_line(2, line);

    snprintf(
        line,
        sizeof(line),
        "LEVEL: %s",
        info.level
    );

    menu_line(3, line);

    display_state(info.state);

    rebuild = get_rebuild_percent(info.rebuild);

    if (rebuild >= 0) {

        snprintf(
            line,
            sizeof(line),
            "REBUILD: %d%%",
            rebuild
        );

        menu_line(6, line);

    } else {

        menu_line(6, "REBUILD: N/A");
    }
}

void page_space_detail(void)
{
    struct raid_info info;

    char device[32];
    char line[64];
    char rebuild_time[32];

    const char *short_device;

    int rebuild;
    int detail;

    detail = detail_get();

    if (find_md_device(
            device,
            sizeof(device)) != 0) {

        menu_title("ARRAY");

        menu_line(2, "ARRAY: N/A");
        menu_line(3, "REBUILD: N/A");
        menu_line(4, "LEFT: N/A");
        menu_line(5, "ACTIVE: N/A");
        menu_line(6, "WORKING: N/A");

        return;
    }

    if (get_raid_info(
            device,
            &info) != 0) {

        menu_title("ARRAY");

        menu_line(2, "ARRAY: N/A");
        menu_line(3, "REBUILD: N/A");
        menu_line(4, "LEFT: N/A");
        menu_line(5, "ACTIVE: N/A");
        menu_line(6, "WORKING: N/A");

        return;
    }

    short_device = device;

    if (strncmp(device, "/dev/", 5) == 0)
        short_device = device + 5;

    menu_title("ARRAY");

    if (detail == 1) {

        /*
         * Detail screen 1:
         *
         * ARRAY
         * REBUILD
         * LEFT
         * ACTIVE
         * WORKING
         * FAILED
         */

        snprintf(
            line,
            sizeof(line),
            "ARRAY: %s",
            short_device
        );

        menu_line(2, line);

        rebuild = get_rebuild_percent(info.rebuild);

        if (rebuild >= 0) {

            snprintf(
                line,
                sizeof(line),
                "REBUILD: %d%%",
                rebuild
            );

            menu_line(3, line);

            if (get_rebuild_time(
                    rebuild_time,
                    sizeof(rebuild_time)) == 0) {

                snprintf(
                    line,
                    sizeof(line),
                    "LEFT: %s",
                    rebuild_time
                );

                menu_line(4, line);

            } else {

                menu_line(4, "LEFT: N/A");
            }

        } else {

            menu_line(3, "REBUILD: NONE");
            menu_line(4, "LEFT: N/A");
        }

        snprintf(
            line,
            sizeof(line),
            "ACTIVE: %d/%d",
            info.active_devices,
            info.raid_devices
        );

        menu_line(5, line);

        snprintf(
            line,
            sizeof(line),
            "WORKING: %d/%d",
            info.working_devices,
            info.total_devices
        );

        menu_line(6, line);

        /*
         * FAILED is shown in the logical log because the
         * first detail screen has only five content rows.
         */
        lcd_log(
            "FAILED: %d",
            info.failed_devices
        );

        return;
    }

    /*
     * Detail screen 2:
     *
     * ARRAY
     * SPARE
     * RAID DEVICES
     * TOTAL DEVICES
     * STATE
     * state continuation
     */

    snprintf(
        line,
        sizeof(line),
        "ARRAY: %s",
        short_device
    );

    menu_line(2, line);

    snprintf(
        line,
        sizeof(line),
        "SPARE: %d",
        info.spare_devices
    );

    menu_line(3, line);

    snprintf(
        line,
        sizeof(line),
        "RAID DEV: %d",
        info.raid_devices
    );

    menu_line(4, line);

    snprintf(
        line,
        sizeof(line),
        "TOTAL DEV: %d",
        info.total_devices
    );

    menu_line(5, line);

    display_state_detail(info.state);
}
