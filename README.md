# TUN/TAP VPN Implementation

A lightweight VPN implementation in C using TUN virtual interfaces and UDP sockets for secure network tunneling.

## Overview

This project implements a client-server VPN system that creates an encrypted tunnel between two machines using Linux TUN interfaces. The VPN uses split routing techniques and UDP for packet transmission, providing a functional virtual private network solution.

## Features

- **Client-Server Architecture** - UDP-based communication
- **TUN Virtual Interface** - Layer 3 (IP) packet handling
- **Split Routing** - Routes all traffic through VPN using 0.0.0.0/1 and 128.0.0.0/1
- **Dynamic Interface Detection** - Automatically detects default network interface
- **NAT Masquerading** - Configured iptables rules for proper packet forwarding
- **Stateful Firewall** - Connection tracking with RELATED/ESTABLISHED states
- **Graceful Shutdown** - Ctrl+C handler with proper cleanup
- **Custom MTU** - 1400 bytes to prevent fragmentation issues

## Project Structure

```
.
├── vpn_client.c        # VPN client implementation
├── vpn_server.c        # VPN server implementation
├── vpn_functions.c     # Shared VPN functionality
├── vpn_functions.h     # API header file
└── README.md          # This file
```

## Requirements

- **Operating System:** Linux (tested on Ubuntu/Debian)
- **Compiler:** GCC
- **Privileges:** Root/sudo access (required for network configuration)
- **Kernel Support:** TUN/TAP module (usually built-in)
- **Tools:** iptables, iproute2, net-tools

## Installation

### Check TUN/TAP Support
```bash
# Verify TUN module is available
lsmod | grep tun

# If not loaded, load it
sudo modprobe tun
```

### Clone Repository
```bash
git clone https://github.com/SaarMayo/C-VPN-Tunnel.git
cd C-VPN-Tunnel
```

## Compilation

### Compile Client
```bash
gcc -ansi -pedantic-errors -Wall -Wextra -DNDEBUG -O3 vpn_client.c vpn_functions.c -o vpn_client
```

### Compile Server
```bash
gcc -ansi -pedantic-errors -Wall -Wextra -DNDEBUG -O3 vpn_server.c vpn_functions.c -o vpn_server
```

### Compile Both
```bash
gcc -ansi -pedantic-errors -Wall -Wextra -DNDEBUG -O3 vpn_client.c vpn_functions.c -o vpn_client && \
gcc -ansi -pedantic-errors -Wall -Wextra -DNDEBUG -O3 vpn_server.c vpn_functions.c -o vpn_server
```

### Compiler Flags Explained
- `-ansi` - Enforce ANSI C standard compliance
- `-pedantic-errors` - Strict ISO C compliance, errors for non-standard code
- `-Wall -Wextra` - Enable all warnings for code quality
- `-DNDEBUG` - Disable assert() statements for production
- `-O3` - Maximum optimization for performance

## Usage

### 1. Start the Server
On the server machine:
```bash
sudo ./vpn_server
```

Expected output:
```
Detected network interface: eth0
tun_name = tun0
tun_fd = 3
Configuring VPN with interface: eth0
UDP socket created
Listening to UDP clients
```

### 2. Start the Client
On the client machine:

**Using default server IP (10.100.102.7):**
```bash
sudo ./vpn_client
```

**Connecting to specific server:**
```bash
sudo ./vpn_client <server_ip>
```

Example:
```bash
sudo ./vpn_client 192.168.1.100
```

Expected output:
```
Connecting to VPN server at: 192.168.1.100
Detected network interface: wlan0
tun_name = tun0
tun_fd = 3
Configuring VPN with interface: wlan0
UDP socket created
```

### 3. Shutdown
Press **Ctrl+C** on either client or server to gracefully shut down and clean up all configurations.

## Configuration

### Network Settings
- **Server Port:** 55555 (UDP)
- **VPN Subnet:** 10.8.0.0/24
- **Server IP:** 10.8.0.1
- **Client IP:** 10.8.0.2
- **MTU:** 1400 bytes

### Default Server
Change the default server IP in `vpn_client.c`:
```c
#define REMOTE_ADDRESS "10.100.102.7"  // Change this
```

### Port Configuration
Change the port in `vpn_server.c`:
```c
#define PORT 55555  // Change this
```

## How It Works

### Client Side
1. Creates TUN interface (tun0)
2. Detects default network interface dynamically
3. Configures split routing (0.0.0.0/1 and 128.0.0.0/1)
4. Adds specific route to VPN server through physical interface
5. Sets up NAT masquerading and iptables forwarding rules
6. Establishes UDP connection to server
7. Forwards packets bidirectionally between TUN and UDP socket

### Server Side
1. Creates TUN interface (tun0)
2. Binds UDP socket to port 55555
3. Configures NAT for VPN subnet (10.8.0.0/24)
4. Sets up forwarding rules for VPN traffic
5. Listens for client connections
6. Forwards packets between clients and internet

### Packet Flow
```
Client Application → TUN Interface → VPN Client → UDP → VPN Server → Internet
Internet → VPN Server → UDP → VPN Client → TUN Interface → Client Application
```

## Troubleshooting

### Permission Denied
```bash
# VPN requires root privileges
sudo ./vpn_client
```

### TUN Device Not Found
```bash
# Load TUN module
sudo modprobe tun

# Verify
ls -l /dev/net/tun
```

### Connection Refused
- Verify server is running
- Check firewall rules allow UDP port 55555
- Ensure correct server IP address

### Route Already Exists
```bash
# Clean up existing routes
sudo ip route del 0.0.0.0/1 2>/dev/null
sudo ip route del 128.0.0.0/1 2>/dev/null
sudo ip link delete tun0 2>/dev/null
```

### Network Interface Not Detected
The program automatically detects your default interface. If detection fails, it will show an error message.

## Security Considerations

⚠️ **This is an educational implementation. For production use, consider:**

- Implement encryption (currently packets are sent in plaintext)
- Add authentication mechanism
- Implement key exchange protocol
- Add connection state management
- Implement rate limiting
- Add logging and monitoring
- Use proper certificate validation

## Technical Details

### Technologies Used
- **TUN/TAP Driver** - Virtual network kernel device
- **UDP Sockets** - Unreliable but fast transport
- **iptables** - Linux firewall configuration
- **iproute2** - Advanced routing configuration
- **POSIX Signals** - Graceful shutdown handling

### Key Functions
- `CreateTunInterface()` - Allocates TUN device
- `HandlePackets()` - Bidirectional packet forwarding loop
- `GetDefaultInterface()` - Dynamic interface detection
- `Config()` - System network configuration
- `ReConfig()` - Cleanup and restoration

## Development

### Author
Mayo Saar - ILRD CR5

### Date
Created: September 29, 2023

### Contributing
This is an educational project. Feel free to fork and experiment!

## License

MIT License - See LICENSE file for details

## Acknowledgments

Built as part of the ILRD CR5 systems programming course, demonstrating practical networking concepts including:
- Virtual network interfaces
- Packet encapsulation
- Network routing
- Firewall configuration
- System programming in C

---

**Note:** This VPN implementation is for educational purposes. Always use established VPN solutions for production environments.
