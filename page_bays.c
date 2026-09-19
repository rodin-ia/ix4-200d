#include <stdio.h>
#include <string.h>

#include "ix4lcd.h"

#define BAY_COUNT 4

struct bay_info {
    const char *ata;
    char disk[8];
    char model[32];
    char state[32];
};

static const char *bay_ata[BAY_COUNT] = {
    "ata1",
    "ata2",
    "ata4",
    "ata5"
};

static int find_disk(
    const char *ata,
    char *disk,
    size_t size)
{
    char path[256];
    char command[256];
    FILE *f;
    char line[256];

    snprintf(
        command,
        sizeof(command),
        "for dev in /sys/class/block/sd?; do "
        "[ -e \"$dev\" ] || continue; "
        "d=${dev##*/}; "
        "path=$(readlink -f \"$dev\"); "
        "case \"$path\" in "
        "*/%s/*) echo \"$d\"; break ;; "
        "esac; "
        "done",
        ata
    );

    f = popen(command, "r");
    if (!f)
        return -1;

    if (!fgets(line, sizeof(line), f)) {
        pclose(f);
        return -1;
    }

    pclose(f);

    line[strcspn(line, "\r\n")] = '\0';

    if (!line[0])
        return -1;

    snprintf(
        path,
        sizeof(path),
        "%s",
        line
    );

    snprintf(
        disk,
        size,
        "%s",
        path
    );

    return 0;
}

static void get_model(
    const char *disk,
    char *model,
    size_t size)
{
    char path[128];
    FILE *f;

    snprintf(
        path,
        sizeof(path),
        "/sys/block/%s/device/model",
        disk
    );

    f = fopen(path, "r");

    if (!f) {
        snprintf(model, size, "UNKNOWN");
        return;
    }

    if (!fgets(model, size, f)) {
        fclose(f);
        snprintf(model, size, "UNKNOWN");
        return;
    }

    fclose(f);

    model[strcspn(model, "\r\n")] = '\0';

    while (*model) {
        size_t len = strlen(model);

        if (model[len - 1] != ' ' &&
            model[len - 1] != '\t')
            break;

        model[len - 1] = '\0';
    }
}

static void get_raid_state(
    const char *disk,
    char *state,
    size_t size)
{
    char path[128];
    FILE *f;

    snprintf(
        path,
        sizeof(path),
        "/sys/block/md0/md/dev-%s1/state",
        disk
    );

    f = fopen(path, "r");

    if (!f) {
        snprintf(state, size, "N/M");
        return;
    }

    if (!fgets(state, size, f)) {
        fclose(f);
        snprintf(state, size, "N/M");
        return;
    }

    fclose(f);

    state[strcspn(state, "\r\n")] = '\0';

    if (strcmp(state, "in_sync") == 0)
        snprintf(state, size, "OK");
    else if (strcmp(state, "faulty") == 0)
        snprintf(state, size, "FAIL");
    else if (strcmp(state, "spare") == 0)
        snprintf(state, size, "SPARE");
    else if (strcmp(state, "removed") == 0)
        snprintf(state, size, "REM");
    else if (strcmp(state, "rebuilding") == 0)
        snprintf(state, size, "REBUILD");
}

static void get_raid_status(
    char *status,
    size_t size)
{
    FILE *f;
    char line[256];

    snprintf(status, size, "N/A");

    f = fopen("/proc/mdstat", "r");
    if (!f)
        return;

    while (fgets(line, sizeof(line), f)) {

        char md[32];
        char level[32];
        char bitmap[32];

        if (sscanf(
                line,
                "%31s : %31s %31s",
                md,
                level,
                bitmap) != 3)
            continue;

        if (strcmp(md, "md0") != 0)
            continue;

        /*
         * mdstat contains the RAID bitmap in the form:
         *
         * [UUUU]
         * [_UUU]
         * [UU_U]
         */
        {
            char *p;

            p = strchr(line, '[');

            if (p) {
                char *end;

                end = strchr(p, ']');

                if (end) {
                    size_t len;

                    len = (size_t)(end - p + 1);

                    if (len >= size)
                        len = size - 1;

                    memcpy(status, p, len);
                    status[len] = '\0';
                }
            }
        }

        break;
    }

    fclose(f);
}

static void format_bay_line(
    int bay,
    const struct bay_info *info,
    char *line,
    size_t size)
{
    char model[9];
    char state[7];

    memset(model, 0, sizeof(model));
    memset(state, 0, sizeof(state));

    if (info->disk[0]) {
        snprintf(
            model,
            sizeof(model),
            "%.8s",
            info->model
        );

        snprintf(
            state,
            sizeof(state),
            "%.6s",
            info->state
        );

        snprintf(
            line,
            size,
            "B%d %-3s %-8s %s",
            bay,
            info->disk,
            model,
            state
        );
    } else {
        snprintf(
            line,
            size,
            "B%d --- EMPTY",
            bay
        );
    }
}

void page_bays(void)
{
    struct bay_info bays[BAY_COUNT];

    char line[22];
    char raid_status[16];

    int i;

    memset(bays, 0, sizeof(bays));

    for (i = 0; i < BAY_COUNT; i++) {

        bays[i].ata = bay_ata[i];

        if (find_disk(
                bays[i].ata,
                bays[i].disk,
                sizeof(bays[i].disk)) != 0) {

            snprintf(
                bays[i].model,
                sizeof(bays[i].model),
                "EMPTY"
            );

            snprintf(
                bays[i].state,
                sizeof(bays[i].state),
                "removed"
            );

            continue;
        }

        get_model(
            bays[i].disk,
            bays[i].model,
            sizeof(bays[i].model)
        );

        get_raid_state(
            bays[i].disk,
            bays[i].state,
            sizeof(bays[i].state)
        );
    }

    for (i = 0; i < BAY_COUNT; i++) {

        format_bay_line(
            i + 1,
            &bays[i],
            line,
            sizeof(line)
        );

        menu_line(
            i + 1,
            line
        );
    }

    /*
     * Leave one empty line between bay information
     * and RAID status.
     */
    menu_line(5, "");

    get_raid_status(
        raid_status,
        sizeof(raid_status)
    );

    snprintf(
        line,
        sizeof(line),
        "RAID: md0 %s",
        raid_status
    );

    menu_line(6, line);
}
