/* VPN Server IMPL */
/*
 Author:    Mayo Saar
 Created:   29/9/23
 Modified:  2/10/23
*/

#define _POSIX_C_SOURCE 200809L

#include <unistd.h>     /* close                                            */
#include <stdlib.h>     /* system, exit                                     */
#include <string.h>     /* memset                                           */
#include <stdio.h>      /* printf, perror, snprintf                         */
#include <sys/socket.h> /* socket, bind, setsockopt, SOL_SOCKET, SO_REUSEADDR */
#include <netinet/in.h> /* sockaddr_in, htons, htonl, INADDR_ANY            */
#include <sys/select.h> /* (passed to HandlePackets)                        */

#include "vpn_functions.h"

#define PORT 55555
#define ERROR -1
#define SUCCESS 0

int UDPListener(int tun_fd);
int Config(char *interface);
void ReConfig(void);


int main()
{
    int tun_fd = 0;
    int status = 0;
    char interface[32] = {0};

    if (ERROR == CtrlCHandler())
    {
        exit(EXIT_FAILURE);
    }

    if (SUCCESS != GetDefaultInterface(interface, sizeof(interface)))
    {
        exit(EXIT_FAILURE);
    }

    tun_fd = CreateTunInterface();
    if (ERROR == tun_fd)
    {
        exit(EXIT_FAILURE);
    }

    if (0 != Config(interface))
    {
        close(tun_fd);
        ReConfig();
        exit(EXIT_FAILURE);
    }

    status = UDPListener(tun_fd);
    ReConfig();
    close(tun_fd);

    return status;
}


/* Listen to incoming client requests */
int UDPListener(int tun_fd)
{
    int optval = 1;
    int udp_socket_fd = 0;
    int status = 0;
    struct sockaddr_in server_addr = {0};

    if (0 > (udp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0)))
    {
        perror("UDP socket open failed");
        return ERROR;
    }
    printf("UDP socket created\n");

    if(0 > setsockopt(udp_socket_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval)))
    {
        close(udp_socket_fd);
        perror("setsockopt()");
        return ERROR;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (0 != (bind(udp_socket_fd, (struct sockaddr *)&server_addr,
                                            (sizeof(server_addr)))))
    {
        close(udp_socket_fd);

        perror("udp socket bind failed");
        return ERROR;
    }

    printf("Listening to UDP clients\n");
    status = HandlePackets(udp_socket_fd, tun_fd, server_addr);

    close(udp_socket_fd);

    return status;
}

/* Config system */
int Config(char *interface)
{
    int ret_val = 0;
    char command[256] = {0};

    printf("Configuring VPN with interface: %s\n", interface);

    /* Enable IP forwarding */
    ret_val += run("sysctl -w net.ipv4.ip_forward=1");

    /* Configure TUN interface with limited MTU */
    ret_val += run("ifconfig tun0 10.8.0.1/24 mtu 1400 up");

    /* NAT/Masquerading for VPN clients going to internet */
    snprintf(command, sizeof(command),
            "iptables -t nat -A POSTROUTING -o %s -s 10.8.0.0/24 -j MASQUERADE", interface);
    ret_val += run(command);

    /* Forward rules (allow VPN traffic through) */
    snprintf(command, sizeof(command), "iptables -A FORWARD -i tun0 -o %s -j ACCEPT", interface);
    ret_val += run(command);
    snprintf(command, sizeof(command), 
            "iptables -A FORWARD -i %s -o tun0 -m state --state RELATED,ESTABLISHED -j ACCEPT", interface);
    ret_val += run(command);

    return ret_val;
}

void ReConfig(void)
{
    printf("\nCleaning up VPN configuration\n");

    /* Disable IP forwarding */
    run("sysctl -w net.ipv4.ip_forward=0");

    /* Delete TUN interface */
    run("ip link delete tun0");

    /* Flush iptables rules */
    run("iptables -F");
    run("iptables -F -t nat");
}
