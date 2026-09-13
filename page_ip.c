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


static void get_ipv4(const char *name,
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

        struct sockaddr_in *addr =
        (struct sockaddr_in *)ifa->ifa_addr;

        if (!inet_ntop(AF_INET,
            &addr->sin_addr,
            buf,
            size)) {
            snprintf(buf, size, "N/A");
            }

            break;
    }

    freeifaddrs(ifaddr);
}


void page_ip(void)
{
    char ip[INET_ADDRSTRLEN];

    char eth0[16];
    char eth1[16];
    char line[64];

    /*
     * IP is assigned to br-lan on OpenWrt.
     */
    get_ipv4("br-lan", ip, sizeof(ip));

    if (interface_up("eth0"))
        snprintf(eth0, sizeof(eth0), "eth0: UP");
    else
        snprintf(eth0, sizeof(eth0), "eth0: DOWN");

    if (interface_up("eth1"))
        snprintf(eth1, sizeof(eth1), "eth1: UP");
    else
        snprintf(eth1, sizeof(eth1), "eth1: DOWN");


    snprintf(line, sizeof(line), "IP: %s", ip);
    menu_title("ADDRESS");

    menu_line(3, line);
    menu_line(5, eth0);
    menu_line(6, eth1);
}
