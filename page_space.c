#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ix4lcd.h"

struct raid_info {
    char device[32];
    char level[32];

    int raid_disks;
    int degraded;
    int active;

    char sync_action[32];
    char sync_completed[64];
    char sync_speed[64];
};

static int read_sysfs(const char *device,
                      const char *file,
                      char *buf,
                      size_t size)
{
    char path[256];
    FILE *f;

    snprintf(path,
             sizeof(path),
             "/sys/class/block/%s/md/%s",
             device,
             file);

    f = fopen(path, "r");

    if (!f)
        return -1;

    if (!fgets(buf, size, f)) {
        fclose(f);
        return -1;
    }

    fclose(f);

    buf[strcspn(buf, "\r\n")] = '\0';

    return 0;
}

static int read_sysfs_int(const char *device,
                          const char *file)
{
    char buf[64];

    if (read_sysfs(device,
        file,
        buf,
        sizeof(buf)) != 0)
        return -1;

    return atoi(buf);
}

static void uppercase(char *str)
{
    for (int i = 0; str[i]; i++) {
        if (str[i] >= 'a' &&
            str[i] <= 'z')
            str[i] -= 'a' - 'A';
    }
}

static int find_raid(struct raid_info *raid)
{
    FILE *f;
    char line[512];

    memset(raid, 0, sizeof(*raid));

    f = fopen("/proc/mdstat", "r");

    if (!f)
        return -1;

    /*
     * Find first md array.
     *
     * Example:
     *
     * md0 : active raid10 sda1[0] ...
     */

    while (fgets(line, sizeof(line), f)) {

        char md[32];
        char state[32];

        if (sscanf(line,
            "%31s : %31s",
            md,
            state) != 2)
            continue;

        if (strncmp(md, "md", 2) != 0)
            continue;

        snprintf(raid->device,
                 sizeof(raid->device),
                 "%s",
                 md);

        if (strcmp(state, "active") == 0)
            raid->active = 1;

        break;
    }

    fclose(f);

    if (raid->device[0] == '\0')
        return -1;

    /*
     * RAID level
     */

    if (read_sysfs(raid->device,
        "level",
        raid->level,
        sizeof(raid->level)) != 0) {

        snprintf(raid->level,
                 sizeof(raid->level),
                 "UNKNOWN");
        }

        uppercase(raid->level);

    /*
     * Number of RAID disks
     */

    raid->raid_disks =
    read_sysfs_int(raid->device,
                   "raid_disks");

    /*
     * Number of failed/degraded disks
     */

    raid->degraded =
    read_sysfs_int(raid->device,
                   "degraded");

    /*
     * Current synchronization operation.
     *
     * Possible values:
     *
     * idle
     * resync
     * recovery
     * reshape
     * check
     * repair
     */

    if (read_sysfs(raid->device,
        "sync_action",
        raid->sync_action,
        sizeof(raid->sync_action)) != 0) {

        snprintf(raid->sync_action,
                 sizeof(raid->sync_action),
                 "idle");
        }

        uppercase(raid->sync_action);

    /*
     * Progress.
     *
     * Example:
     *
     * 42.37
     */

    if (read_sysfs(raid->device,
        "sync_completed",
        raid->sync_completed,
        sizeof(raid->sync_completed)) != 0) {

        raid->sync_completed[0] = '\0';
        }

        /*
         * Speed.
         */

        if (read_sysfs(raid->device,
            "sync_speed",
            raid->sync_speed,
            sizeof(raid->sync_speed)) != 0) {

            raid->sync_speed[0] = '\0';
            }

            return 0;
}

static int sync_percent(const char *completed)
{
    double done;

    if (!completed || !completed[0])
        return -1;

    if (sscanf(completed, "%lf", &done) != 1)
        return -1;

    if (done < 0.0)
        done = 0.0;

    if (done > 100.0)
        done = 100.0;

    return (int)(done + 0.5);
}

void page_space(void)
{
    struct raid_info raid;
    char line[64];

    menu_title("ARRAY");

    /*
     * No RAID array
     */

    if (find_raid(&raid) != 0) {

        menu_line(2, "ARRAY: N/A");
        menu_line(4, "STATE: OFFLINE");
        menu_line(5, "HEALTH: UNKNOWN");

        return;
    }

    /*
     * ARRAY: md0
     */

    snprintf(line,
             sizeof(line),
             "ARRAY: %s",
             raid.device);

    menu_line(2, line);

    /*
     * LEVEL: RAID10
     */

    snprintf(line,
             sizeof(line),
             "LEVEL: %s",
             raid.level);

    menu_line(3, line);

    /*
     * STATE: ACTIVE
     */

    if (raid.active)
        menu_line(4, "STATE: ACTIVE");
    else
        menu_line(4, "STATE: INACTIVE");

    /*
     * Synchronization / health
     */

    if (strcmp(raid.sync_action, "IDLE") != 0 &&
        raid.sync_action[0] != '\0') {

        int percent =
        sync_percent(raid.sync_completed);

    if (percent >= 0) {

        snprintf(line,
                 sizeof(line),
                 "%s: %d%%",
                 raid.sync_action,
                 percent);

        menu_line(5, line);

    } else {

        snprintf(line,
                 sizeof(line),
                 "%s",
                 raid.sync_action);

        menu_line(5, line);
    }

        } else if (raid.degraded > 0) {

            menu_line(5, "HEALTH: DEGRADED");

        } else if (raid.degraded == 0) {

            menu_line(5, "HEALTH: OK");

        } else {

            menu_line(5, "HEALTH: UNKNOWN");
        }

        /*
         * DISKS: 4/4
         */

        if (raid.raid_disks > 0) {

            int online = raid.raid_disks - raid.degraded;

            if (online < 0)
                online = 0;

            snprintf(line,
                     sizeof(line),
                     "DISKS: %d/%d",
                     online,
                     raid.raid_disks);

            menu_line(6, line);
        } else {
            menu_line(6, "DISKS: UNKNOWN");
        }
}
