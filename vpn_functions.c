/* VPN Lib IMPL */
/*
 Author:    Mayo Saar
 Created:   29/9/23
 Modified:  2/10/23
*/

#define _XOPEN_SOURCE 700

#include <signal.h>     /* sigaction                                        */
#include <string.h>     /* strncpy, strcpy, strlen, strcspn                 */
#include <stdio.h>      /* printf, perror, FILE, fgets, popen, pclose       */
#include <unistd.h>     /* read, write, close                               */
#include <stdlib.h>     /* system, exit                                     */
#include <errno.h>      /* errno, EINTR                                     */
#include <linux/if.h>   /* struct ifreq, IFNAMSIZ                           */
#include <linux/if_tun.h> /* IFF_TUN, IFF_NO_PI, TUNSETIFF                  */
#include <netinet/in.h> /* struct sockaddr_in                               */
#include <sys/socket.h> /* sendto, recvfrom, socklen_t                      */
#include <sys/types.h>  /* ssize_t, size_t                                  */
#include <sys/ioctl.h>  /* ioctl                                            */
#include <sys/stat.h>   /* (file permissions)                               */
#include <sys/select.h> /* select, fd_set, FD_SET, FD_ZERO, FD_ISSET        */
#include <fcntl.h>      /* open, O_RDWR                                     */

#include "vpn_functions.h"

/* Get the maximum between two values */
#define MAX(a, b) a > b ? a : b

/* Maximum Transmission Unit - packet size for VPN tunnel */
#define MTU 1400

#define TRUE 1
#define FALSE 0
#define ERROR -1
#define SUCCESS 0

/* Global flag controlling packet handling loop - set to FALSE by signal handler */
int g_run = TRUE;


/******************************************************************************/

/* Allocates and configures a TUN device */
int TunAlloc(char *dev, int flags)
{
    struct ifreq ifr = {0};
    int fd = 0;
    char *clonedev = "/dev/net/tun";

    if ((fd = open(clonedev, O_RDWR)) < 0)
    {
        perror("Opening /dev/net/tun");
        return ERROR;
    }

    ifr.ifr_flags = flags;

    if (*dev)
    {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }

    if (0 > ioctl(fd, TUNSETIFF, (void *) &ifr))
    {
        close(fd);
        return ERROR;
    }

    strcpy(dev, ifr.ifr_name);

    return fd;
}

/* Creates TUN interface named "tun0" */
int CreateTunInterface(void)
{
    char tun_name[IFNAMSIZ] = {0};
    int tun_fd = 0;

    strcpy(tun_name, "tun0");
    tun_fd = TunAlloc(tun_name, IFF_TUN | IFF_NO_PI);
    if (0 > tun_fd)
    {
        return ERROR;
    }

    printf("tun_name = %s\n", tun_name);
    printf("tun_fd = %d\n", tun_fd);
    return tun_fd;
}

/* Read and write data to the relevant interface */
int HandlePackets(int udp_fd, int tun_fd, struct sockaddr_in addr)
{
    int ret = 0;
    int read_bytes = 0;
    int max_fd = MAX(udp_fd, tun_fd);
    char buffer[MTU + 1] = {0};
    socklen_t addr_len = sizeof(addr);

    while (g_run)
    {
        fd_set rd_set = {0};
        FD_ZERO(&rd_set);
        FD_SET(tun_fd, &rd_set);
        FD_SET(udp_fd, &rd_set);
        
        ret = select(max_fd + 1, &rd_set, NULL, NULL, NULL);
        memset(&buffer, 0, MTU);

        if (ret < 0 && errno == EINTR)
        {
            continue;
        }

        if (ret < 0)
        {
            perror("select");
            return ERROR;
        }

        if(FD_ISSET(tun_fd, &rd_set))
        {
            read_bytes = read(tun_fd, buffer, MTU);
            if (0 > read_bytes)
            {
                perror("tun read");
                continue;
            }

            /* encrypt */


            if (0 > sendto(udp_fd, buffer, read_bytes, 0, (const struct sockaddr *)&addr, addr_len))
            {
                perror("send to");
            }
        }

        if(FD_ISSET(udp_fd, &rd_set))
        {

            read_bytes = recvfrom(udp_fd, buffer, MTU, 0, (struct sockaddr *)&addr, &addr_len);
            if (0 > read_bytes)
            {
                perror("recieve from");
                continue;
            }

            /* decrypt */


            if (0 > write(tun_fd, buffer, read_bytes))
            {
                perror("tun write");
            }
        }
    }

    return 0;
}

/* Get the interface name for the default route */
int GetDefaultInterface(char *interface, size_t len)
{
    FILE *fp = NULL;
    char iface[32] = {0};

    fp = popen("ip route | grep '^default' | head -n1 | awk '{print $5}'", "r");
    if (NULL == fp)
    {
        perror("Failed to get default interface");
        return ERROR;
    }

    if (NULL != fgets(iface, sizeof(iface), fp))
    {
        /* Remove newline */
        iface[strcspn(iface, "\n")] = 0;
        
        if (0 < strlen(iface) && len > strlen(iface))
        {
            strcpy(interface, iface);
            pclose(fp);
            printf("Detected network interface: %s\n", interface);
            return SUCCESS;
        }
    }

    pclose(fp);
    printf("ERROR: Could not detect network interface\n");
    return ERROR;
}

/* Run a system command and check for erros */
int run(char *cmd)
{
    int result = 0;
    printf("Running \'%s\'\n", cmd);
    result = system(cmd);
    if (0 != result)
    {
        printf("ERROR: Command failed with exit code %d\n", result);
        perror(cmd);

        return ERROR;
    }

    return SUCCESS;
}

/* Handle SIGINT signal */
void ExitHandler(int signum)
{
    g_run = FALSE;
    (void)signum;
}

/* Config exit function */
int CtrlCHandler(void)
{
    struct sigaction sigint_act = {0};
    sigint_act.sa_handler = &ExitHandler;
    if (0 != sigaction(SIGINT, &sigint_act, NULL))
    {
        perror("sigaction");
        return ERROR;
    }

    return SUCCESS;
}

