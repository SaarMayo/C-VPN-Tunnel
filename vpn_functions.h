/*  API for VPN Implementation  */
/*
 Author:    Mayo Saar
 Created:   29/9/23
 Modified:  2/10/23
*/

#ifndef __VPN_H__
#define __VPN_H__

/******************************************************************************
 * Allocates and configures a TUN virtual network device.
 * Returns file descriptor for the TUN device on success, -1 on failure.
 * *dev: buffer containing desired device name (e.g., "tun0")
 *       On success, contains the actual device name assigned
 * flags: configuration flags for the device
 *        IFF_TUN: TUN device (layer 3, IP packets)
 *        IFF_TAP: TAP device (layer 2, Ethernet frames)
 *        IFF_NO_PI: No packet information (just raw packets)
 */
int TunAlloc(char *dev, int flags);

/******************************************************************************
 * Creates a TUN interface named "tun0" with standard VPN configuration.
 * Returns file descriptor for the TUN device on success, -1 on failure.
 * Wrapper around TunAlloc() that uses hardcoded name "tun0" and
 * sets flags to IFF_TUN | IFF_NO_PI (IP layer, no packet info).
 */
int CreateTunInterface(void);

/******************************************************************************
 * Main packet forwarding loop between TUN interface and UDP socket.
 * Returns 0 on normal exit, -1 on select() failure.
 * udp_fd: file descriptor for UDP socket (connection to VPN peer)
 * tun_fd: file descriptor for TUN interface (virtual network device)
 * addr: sockaddr_in structure containing peer's address and port
 * 
 * Implements bidirectional packet forwarding:
 * - TUN -> UDP: Read from TUN, send via UDP (encapsulation)
 * - UDP -> TUN: Receive from UDP, write to TUN (decapsulation)
 * Runs until global variable g_run becomes 0 (signal handler).
 */
int HandlePackets(int net_fd, int tun_fd, struct sockaddr_in addr);

/******************************************************************************
 * Automatically detects the network interface with the default route.
 * Returns 0 on success, -1 on failure.
 * *interface: buffer to store interface name
 * len: size of the interface buffer (at least 32 bytes)
 * 
 * Finds the interface used for internet access.
 */
int GetDefaultInterface(char *interface, size_t len);

/******************************************************************************
 * Executes a shell command and checks for errors.
 * Returns 0 if command succeeds, -1 otherwise.
 * *cmd: null-terminated string containing the command to execute
 * 
 * Prints the command being executed and any errors that occur.
 * Used for system configuration (ip, iptables, sysctl, ifconfig).
 */
int run(char *cmd);

/******************************************************************************
 * Signal handler for SIGINT (Ctrl+C).
 * signum: signal number (unused but required by signal handler signature)
 * 
 * Sets global flag g_run to 0, causing the main packet handling loop to exit.
 * Allows for graceful shutdown and proper cleanup.
 */
void ExitHandler(int signum);

/******************************************************************************
 * Installs signal handler for SIGINT (Ctrl+C).
 * Returns 0 on success, -1 on failure.
 * 
 * Sets up ExitHandler() to be called when user presses Ctrl+C.
 * Allows program to exit packet handling loop and run cleanup code.
 */
int CtrlCHandler(void);


#endif