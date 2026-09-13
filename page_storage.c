#include <stdio.h>
#include <string.h>
#include <sys/statvfs.h>

#include "ix4lcd.h"

struct storage_info {
    char name[64];
    char mountpoint[256];

    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
};


static int is_system_fs(const char *fstype,
                        const char *mountpoint)
{
    if (strcmp(mountpoint, "/") == 0)
        return 1;

    if (strcmp(mountpoint, "/rom") == 0)
        return 1;

    if (strcmp(mountpoint, "/overlay") == 0)
        return 1;

    if (strcmp(mountpoint, "/tmp") == 0)
        return 1;

    if (strcmp(fstype, "squashfs") == 0)
        return 1;

    if (strcmp(fstype, "overlay") == 0)
        return 1;

    if (strcmp(fstype, "tmpfs") == 0)
        return 1;

    if (strcmp(fstype, "devtmpfs") == 0)
        return 1;

    if (strcmp(fstype, "proc") == 0)
        return 1;

    if (strcmp(fstype, "sysfs") == 0)
        return 1;

    if (strcmp(fstype, "cgroup2") == 0)
        return 1;

    if (strcmp(fstype, "debugfs") == 0)
        return 1;

    if (strcmp(fstype, "bpffs") == 0)
        return 1;

    if (strcmp(fstype, "devpts") == 0)
        return 1;

    return 0;
}


static void format_size(unsigned long long bytes,
                        char *buf,
                        size_t size)
{
    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {

        snprintf(buf,
                 size,
                 "%.1f GB",
                 (double)bytes /
                 (1024.0 * 1024.0 * 1024.0));

    } else if (bytes >= 1024ULL * 1024ULL) {

        snprintf(buf,
                 size,
                 "%.0f MB",
                 (double)bytes /
                 (1024.0 * 1024.0));

    } else {

        snprintf(buf,
                 size,
                 "%.0f KB",
                 (double)bytes / 1024.0);
    }
}


static void get_array_name(const char *device,
                           char *name,
                           size_t size)
{
    const char *p;

    if (!device || !name || size == 0)
        return;

    /*
     * /dev/md/data
     */
    if (strncmp(device, "/dev/md/", 8) == 0) {

        p = device + 8;

        snprintf(name, size, "%.*s",
                 (int)(size - 1), p);

        return;
    }

    /*
     * /dev/md0
     * /dev/md1
     * /dev/md127
     */
    if (strncmp(device, "/dev/md", 7) == 0) {

        p = device + 5;

        snprintf(name, size, "%.*s",
                 (int)(size - 1), p);

        return;
    }

    snprintf(name, size, "STORAGE");
}


static int find_storage(struct storage_info *info)
{
    FILE *f;

    char device[256];
    char mountpoint[256];
    char fstype[64];

    struct storage_info best;

    int found = 0;

    memset(&best, 0, sizeof(best));

    f = fopen("/proc/mounts", "r");

    if (!f)
        return -1;

    while (fscanf(f,
        "%255s %255s %63s %*s %*d %*d",
        device,
        mountpoint,
        fstype) == 3) {

        struct statvfs st;

    if (is_system_fs(fstype, mountpoint)) {
        continue;
    }

    /*
     * Only mounted md RAID filesystems.
     */
    if (strncmp(device, "/dev/md", 7) != 0) {
        continue;
    }

    if (statvfs(mountpoint, &st) != 0) {
        continue;
    }

    if (st.f_blocks == 0) {
        continue;
    }

    unsigned long long total =
    (unsigned long long)st.f_blocks *
    (unsigned long long)st.f_frsize;

    unsigned long long free =
    (unsigned long long)st.f_bavail *
    (unsigned long long)st.f_frsize;

    unsigned long long used =
    total - free;

    /*
     * If there are several md filesystems,
     * use the largest one.
     */
    if (!found || total > best.total) {

        memset(&best, 0, sizeof(best));

        get_array_name(device,
                       best.name,
                       sizeof(best.name));

        snprintf(best.mountpoint,
                 sizeof(best.mountpoint),
                 "%.*s",
                 (int)(sizeof(best.mountpoint) - 1),
                 mountpoint);

        best.total = total;
        best.free = free;
        best.used = used;

        found = 1;
    }
        }

        fclose(f);

        if (!found)
            return -1;

    *info = best;

    return 0;
}


void page_storage(void)
{
    struct storage_info storage;

    char total[32];
    char used[32];
    char free_space[32];

    char line[64];

    menu_title("STORAGE");

    if (find_storage(&storage) != 0) {

        menu_line(3, "NO RAID");
        return;
    }

    format_size(storage.total,
        total,
        sizeof(total));

    format_size(storage.used,
        used,
        sizeof(used));

    format_size(storage.free,
        free_space,
        sizeof(free_space));

    /*
     * Array name
     */
    menu_line(2, storage.name);

    /*
     * Used
     */
    snprintf(line,
             sizeof(line),
             "USED: %s",
             used);

    menu_line(4, line);

    /*
     * Free
     */
    snprintf(line,
             sizeof(line),
             "FREE: %s",
             free_space);

    menu_line(5, line);

    /*
     * Total
     */
    snprintf(line,
             sizeof(line),
             "TOTAL: %s",
             total);

    menu_line(6, line);
}
