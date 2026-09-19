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

static int parse_int_field(const char *line, const char *field)
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

static int find_md_device(char *device, size_t size)
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

static int get_raid_info(const char *device, struct raid_info *info)
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
        }

        else if (strstr(line, "State")) {
            parse_string_field(
                line,
                "State",
                info->state,
                sizeof(info->state)
            );
        }

        else if (strstr(line, "Rebuild Status")) {
            parse_string_field(
                line,
                "Rebuild Status",
                info->rebuild,
                sizeof(info->rebuild)
            );
        }

        else if (strstr(line, "Raid Devices")) {
            info->raid_devices =
                parse_int_field(line, "Raid Devices");
        }

        else if (strstr(line, "Total Devices")) {
            info->total_devices =
                parse_int_field(line, "Total Devices");
        }

        else if (strstr(line, "Active Devices")) {
            info->active_devices =
                parse_int_field(line, "Active Devices");
        }

        else if (strstr(line, "Working Devices")) {
            info->working_devices =
                parse_int_field(line, "Working Devices");
        }

        else if (strstr(line, "Failed Devices")) {
            info->failed_devices =
                parse_int_field(line, "Failed Devices");
        }

        else if (strstr(line, "Spare Devices")) {
            info->spare_devices =
                parse_int_field(line, "Spare Devices");
        }
    }

    pclose(f);

    if (info->raid_devices <= 0)
        return -1;

    return 0;
}

static int get_rebuild_percent(const char *rebuild)
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

void page_space(void)
{
    struct raid_info info;
    char device[32];
    char line[256];
    const char *short_device;
    int rebuild;

    menu_title("ARRAY");

    if (find_md_device(device, sizeof(device)) != 0) {
        menu_line(3, "ARRAY: N/A");
        menu_line(4, "STATE: OFFLINE");
        menu_line(5, "HEALTH: UNKNOWN");
        menu_line(6, "DEVICES: N/A");
        return;
    }

    if (get_raid_info(device, &info) != 0) {
        menu_line(3, "ARRAY: N/A");
        menu_line(4, "STATE: UNKNOWN");
        menu_line(5, "HEALTH: UNKNOWN");
        menu_line(6, "DEVICES: N/A");
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
    menu_line(3, line);

    snprintf(
        line,
        sizeof(line),
        "LEVEL: %s",
        info.level
    );
    menu_line(4, line);

    snprintf(
        line,
        sizeof(line),
        "STATE: %s",
        info.state
    );
    menu_line(5, line);

    rebuild = get_rebuild_percent(info.rebuild);

    if (rebuild >= 0) {
        snprintf(
            line,
            sizeof(line),
            "REBUILD: %d%%",
            rebuild
        );
        menu_line(6, line);
    }
    else if (strcmp(info.state, "clean") == 0) {
        menu_line(6, "HEALTH: OK");
    }
    else if (strstr(info.state, "degraded")) {
        menu_line(6, "HEALTH: DEGRADED");
    }
    else {
        menu_line(6, "HEALTH: UNKNOWN");
    }
}
