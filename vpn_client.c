/* VPN Client IMPL */
/*
 Author:    Mayo Saar
 Created:   29/9/23
 Modified:  2/10/23
*/

#define _POSIX_C_SOURCE 200809L

#include <unistd.h>     /* close                                            */
#include <stdlib.h>     /* system, exit                                     */
#include <string.h>     /* memset, strcpy, strcspn                          */
#include <stdio.h>      /* printf, perror, snprintf, popen, pclose, fgets   */
#include <netinet/in.h> /* sockaddr_in, htons                               */
#include <arpa/inet.h>  /* inet_addr                                        */
#include <sys/socket.h> /* socket, connect                                  */

#include "vpn_functions.h"

#define REMOTE_ADDRESS "10.100.102.7"
#define REMOTE_PORT 55555
#define LOCAL_ADDRESS
#define TRUE 1
#define FALSE 0
#define ERROR -1
#define SUCCESS 0


int ConnectUDPServer(int tun_fd, char *server_ip);
int Config(char *interface, char *server_ip);
void ReConfig(char *interface);


int main(int argc, char *argv[])
{
    int tun_fd = 0;
    int status = 0;
    char interface[32] = {0};
    char *server_ip = NULL;

    if (2 != argc)
    {
        printf("No server IP provided, using default: %s\n", REMOTE_ADDRESS);
        server_ip = REMOTE_ADDRESS;
    }
    else
    {
        server_ip = argv[1];
    }
    printf("Connecting to VPN server at: %s\n", server_ip);

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

    if (SUCCESS != Config(interface, server_ip))
    {
        close(tun_fd);
        ReConfig(interface);
        exit(EXIT_FAILURE);
    }

    status = ConnectUDPServer(tun_fd, server_ip);
    ReConfig(interface);
    close(tun_fd);

    return status;
}


int ConnectUDPServer(int tun_fd, char *server_ip)
{
    int status = 0;
    int udp_socket_fd = 0;
    struct sockaddr_in server_addr = {0};

    if (0 > (udp_socket_fd = socket(AF_INET, SOCK_DGRAM, 0)))
    {
        perror("UDP socket open failed");
        return ERROR;
    }
    printf("UDP socket created\n");

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(REMOTE_PORT);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    if(0 != connect(udp_socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
    {
        close(udp_socket_fd);
        perror("connection failed");
        return ERROR;
    }

    status = HandlePackets(udp_socket_fd, tun_fd, server_addr);

    close(udp_socket_fd);

    return status;
}


/* Config system */
int Config(char *interface, char *server_ip)
{
    int ret_val = 0;
    char command[256] = {0};
    char gateway[32] = {0};
    FILE *fp = NULL;

    printf("\nConfiguring VPN with interface: %s\n", interface);

    /* Get the gateway for the interface */
    snprintf(command, sizeof(command), 
            "ip route | grep 'default' | grep '%s' | awk '{print $3}'", interface);
    fp = popen(command, "r");
    if (NULL != fp && NULL != fgets(gateway, sizeof(gateway), fp))
    {
        gateway[strcspn(gateway, "\n")] = 0;  /* Remove newline */
        pclose(fp);
    }
    else
    {
        if (fp)
        {
            pclose(fp);
        }
        printf("WARNING: Could not detect gateway, using default 10.100.102.1\n");
        strcpy(gateway, "10.100.102.1");
    }

    printf("Using gateway: %s\n", gateway);


    /* Enable IP forwarding */
    ret_val += run("sysctl -w net.ipv4.ip_forward=1");
    /* Configure TUN device with limited MTU */
    ret_val += run("ifconfig tun0 10.8.0.2/24 mtu 1400 up");

    /* Add specific route to VPN server through physical interface */ 
    snprintf(command, sizeof(command), "ip route add %s via %s dev %s", server_ip, gateway, interface);
    ret_val += run(command);

    /* Split routing to override default gateway */
    ret_val += run("ip route add 0.0.0.0/1 via 10.8.0.2 dev tun0");
    ret_val += run("ip route add 128.0.0.0/1 via 10.8.0.2 dev tun0");

    /* NAT/Masquerading rules - masquerade on physical interface */
    snprintf(command, sizeof(command), "iptables -t nat -A POSTROUTING -o %s -j MASQUERADE", interface);
    ret_val += run(command);

    /* Forward rules (allow traffic through the tunnel) */
    snprintf(command, sizeof(command), "iptables -A FORWARD -i tun0 -o %s -j ACCEPT", interface);
    ret_val += run(command);
    snprintf(command, sizeof(command),
            "iptables -A FORWARD -i %s -o tun0 -m state --state RELATED,ESTABLISHED -j ACCEPT", interface);
    ret_val += run(command);

    return ret_val;
}

void ReConfig(char *interface)
{
    char command[256] = {0};

    printf("Cleaning up VPN configuration\n");

    /* Disable IP forwarding */
    run("sysctl -w net.ipv4.ip_forward=0");

    /* Flush iptables rules */
    run("iptables -F");
    run("iptables -F -t nat");

    /* Remove split routes */
    run("ip route del 0.0.0.0/1 via 10.8.0.2 dev tun0");
    run("ip route del 128.0.0.0/1 via 10.8.0.2 dev tun0");
    
    /* Rmove VPN server route */
    snprintf(command, sizeof(command), "ip route del 10.100.102.7 via 10.100.102.1 dev %s", interface);
    run(command);

    /* Delete TUN interface */
    run("ip link delete tun0");
}