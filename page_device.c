#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "ix4lcd.h"

#define MAX_IPS 2

static int get_ipv4_addresses(
    const char *interface,
    char ips[][INET_ADDRSTRLEN],
    int max_ips)
{
    struct ifaddrs *ifaddr;
    struct ifaddrs *ifa;
    int count = 0;

    if (getifaddrs(&ifaddr) != 0)
        return 0;

    for (ifa = ifaddr;
         ifa != NULL && count < max_ips;
         ifa = ifa->ifa_next) {

        struct sockaddr_in *addr;

        if (!ifa->ifa_name)
            continue;

        if (strcmp(ifa->ifa_name, interface) != 0)
            continue;

        if (!ifa->ifa_addr)
            continue;

        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;

        addr = (struct sockaddr_in *)ifa->ifa_addr;

        if (!inet_ntop(
                AF_INET,
                &addr->sin_addr,
                ips[count],
                INET_ADDRSTRLEN)) {
            continue;
        }

        count++;
    }

    freeifaddrs(ifaddr);

    return count;
}

static void center_line(int row, const char *text)
{
    int len;
    int width;
    int x;

    len = strlen(text);

    if (len > 21)
        len = 21;

    width = len * 6;
    x = (LCD_W - width) / 2;

    if (x < 1)
        x = 1;

    lcd_text(x, row, text);
}

void page_device(void)
{
    char hostname[64];
    char datetime[64];
    char ips[MAX_IPS][INET_ADDRSTRLEN];
    char line[22];

    time_t now;
    struct tm tm;

    int ip_count;
    int i;
    int row;

    if (gethostname(hostname, sizeof(hostname) - 1) != 0)
        snprintf(hostname, sizeof(hostname), "ix4-200d");

    hostname[sizeof(hostname) - 1] = '\0';

    now = time(NULL);
    localtime_r(&now, &tm);

    strftime(
        datetime,
        sizeof(datetime),
        "%d %b %H:%M:%S",
        &tm
    );

    center_line(1, hostname);
    center_line(3, datetime);

    ip_count = get_ipv4_addresses(
        "br-lan",
        ips,
        MAX_IPS
    );

    row = 5;

    for (i = 0; i < ip_count && row <= 6; i++, row++) {
        snprintf(
            line,
            sizeof(line),
            "IP: %.17s",
            ips[i]
        );

        menu_line(row, line);
    }

    if (ip_count == 0)
        menu_line(5, "IP: N/A");
}
