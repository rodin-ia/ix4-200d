#include <stdio.h>
#include <string.h>

#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/socket.h>

#include "ix4lcd.h"

static int interface_up(const char *name)
{
    struct ifaddrs *ifaddr;
    struct ifaddrs *ifa;
    int up = 0;

    if (getifaddrs(&ifaddr) != 0)
        return 0;

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {

        if (!ifa->ifa_name)
            continue;

        if (strcmp(ifa->ifa_name, name) != 0)
            continue;

        if ((ifa->ifa_flags & IFF_UP) &&
            (ifa->ifa_flags & IFF_RUNNING))
            up = 1;

        break;
    }

    freeifaddrs(ifaddr);

    return up;
}

static void get_ipv4(
    const char *name,
    char *buf,
    size_t size)
{
    struct ifaddrs *ifaddr;
    struct ifaddrs *ifa;

    snprintf(buf, size, "N/A");

    if (getifaddrs(&ifaddr) != 0)
        return;

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {

        if (!ifa->ifa_name)
            continue;

        if (strcmp(ifa->ifa_name, name) != 0)
            continue;

        if (!ifa->ifa_addr)
            continue;

        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;

        {
            struct sockaddr_in *addr =
                (struct sockaddr_in *)ifa->ifa_addr;

            if (!inet_ntop(
                    AF_INET,
                    &addr->sin_addr,
                    buf,
                    size)) {
                snprintf(buf, size, "N/A");
            }
        }

        break;
    }

    freeifaddrs(ifaddr);
}

static void get_netmask(
    const char *name,
    char *buf,
    size_t size)
{
    struct ifaddrs *ifaddr;
    struct ifaddrs *ifa;

    snprintf(buf, size, "N/A");

    if (getifaddrs(&ifaddr) != 0)
        return;

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {

        struct sockaddr_in *mask;

        if (!ifa->ifa_name)
            continue;

        if (strcmp(ifa->ifa_name, name) != 0)
            continue;

        if (!ifa->ifa_netmask)
            continue;

        if (ifa->ifa_netmask->sa_family != AF_INET)
            continue;

        mask = (struct sockaddr_in *)ifa->ifa_netmask;

        if (!inet_ntop(
                AF_INET,
                &mask->sin_addr,
                buf,
                size)) {
            snprintf(buf, size, "N/A");
        }

        break;
    }

    freeifaddrs(ifaddr);
}

static void get_gateway(
    char *buf,
    size_t size)
{
    FILE *f;
    char line[128];
    char gateway[INET_ADDRSTRLEN];

    snprintf(buf, size, "N/A");

    f = popen(
        "ip -4 route show default 2>/dev/null",
        "r"
    );

    if (!f)
        return;

    while (fgets(line, sizeof(line), f)) {

        if (sscanf(
                line,
                "default via %15s",
                gateway) == 1) {

            snprintf(
                buf,
                size,
                "%s",
                gateway
            );

            break;
        }
    }

    pclose(f);
}

static void get_mode(
    char *buf,
    size_t size)
{
    FILE *f;
    char line[32];

    snprintf(buf, size, "N/A");

    f = popen(
        "uci -q get network.lan.proto 2>/dev/null",
        "r"
    );

    if (!f)
        return;

    if (fgets(line, sizeof(line), f)) {

        size_t len;

        len = strlen(line);

        while (len > 0 &&
               (line[len - 1] == '\n' ||
                line[len - 1] == '\r' ||
                line[len - 1] == ' ' ||
                line[len - 1] == '\t')) {
            line[--len] = '\0';
        }

        if (strcmp(line, "dhcp") == 0)
            snprintf(buf, size, "DHCP");
        else if (strcmp(line, "static") == 0)
            snprintf(buf, size, "STATIC");
        else
            snprintf(buf, size, "%.15s", line);
    }

    pclose(f);
}

void page_ip(void)
{
    char ip[INET_ADDRSTRLEN];
    char mask[INET_ADDRSTRLEN];
    char gateway[INET_ADDRSTRLEN];
    char mode[16];

    char eth0[16];
    char eth1[16];

    char line[64];

    /*
     * IP is assigned to br-lan on OpenWrt.
     */
    get_ipv4(
        "br-lan",
        ip,
        sizeof(ip)
    );

    get_netmask(
        "br-lan",
        mask,
        sizeof(mask)
    );

    get_gateway(
        gateway,
        sizeof(gateway)
    );

    get_mode(
        mode,
        sizeof(mode)
    );

    if (interface_up("eth0"))
        snprintf(
            eth0,
            sizeof(eth0),
            "eth0: UP"
        );
    else
        snprintf(
            eth0,
            sizeof(eth0),
            "eth0: DOWN"
        );

    if (interface_up("eth1"))
        snprintf(
            eth1,
            sizeof(eth1),
            "eth1: UP"
        );
    else
        snprintf(
            eth1,
            sizeof(eth1),
            "eth1: DOWN"
        );

    menu_title("ADDRESS");

    snprintf(
        line,
        sizeof(line),
        "IP: %s",
        ip
    );
    menu_line(2, line);

    snprintf(
        line,
        sizeof(line),
        "MSK: %s",
        mask
    );
    menu_line(3, line);

    snprintf(
        line,
        sizeof(line),
        "GW: %s",
        gateway
    );
    menu_line(4, line);

    snprintf(
        line,
        sizeof(line),
        "MODE: %s",
        mode
    );
    menu_line(5, line);

    menu_line(6, eth0);
}
