#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

// Generally, try to enable features needed and disable unused ones to save space
#define NO_SYS                      0 // We are using pico_lwip_sys_threadsafe_background or FreeRTOS
#define LWIP_SOCKET                 0 // Sockets API not used for basic httpd
#if PICO_CYW43_ARCH_POLL
#define MEMP_NUM_SYS_TIMEOUT        (LWIP_NUM_SYS_TIMEOUT_INTERNAL + 1)
#endif

// --- Memory options ---
// Allow overriding memory options for a memory constrained system
#ifndef MEM_LIBC_MALLOC
#define MEM_LIBC_MALLOC             0
#endif
#ifndef MEMP_MEM_MALLOC
#define MEMP_MEM_MALLOC             0
#endif

// MEM_SIZE: the size of the heap memory. allocations are done from here.
#define MEM_SIZE                    (12 * 1024) // Reduced for Pico without external RAM
#define MEMP_NUM_PBUF               8
#define MEMP_NUM_TCP_SEG            16 // Increased for httpd
#define PBUF_POOL_SIZE              8

// --- TCP options ---
#define LWIP_TCP                    1
#define TCP_TTL                     255
#define TCP_MSS                     (1500 - 40) // Path MTU discovery not enabled
#define TCP_SND_BUF                 (2 * TCP_MSS)
#define TCP_WND                     (2 * TCP_MSS)

// --- HTTPD options ---
#define LWIP_HTTPD                  1
#define LWIP_HTTPD_CGI              1 // Enable CGI
#define LWIP_HTTPD_SSI              0 // SSI not strictly needed for these CGIs
#define LWIP_HTTPD_CGI_SSI          0 // If SSI is 0, this must be 0. Set to 1 if SSI also handles CGI.
#define LWIP_HTTPD_FS_READ_DELAY    0
#define HTTPD_USE_CUSTOM_FSDATA     1 // Using custom fsdata (embedded string for now)
#define HTTPD_FSDATA_FILE           "fsdata_custom.c" // Dummy name, content will be in web_server.c
#define HTTPD_MAX_CGI_PARAMETERS    4 // Max number of parameters to parse in a CGI request

// --- DHCP options ---
#define LWIP_DHCP                   1 // Enable DHCP for RNDIS

// --- DNS options ---
#define LWIP_DNS                    1

// --- Checksum options ---
#define CHECKSUM_GEN_IP             1
#define CHECKSUM_GEN_UDP            1
#define CHECKSUM_GEN_TCP            1
#define CHECKSUM_CHECK_IP           1
#define CHECKSUM_CHECK_UDP          1
#define CHECKSUM_CHECK_TCP          1
#define CHECKSUM_GEN_ICMP           1

// --- Debugging options ---
// #define LWIP_DEBUG                  1
// #define HTTPD_DEBUG                 LWIP_DBG_ON

#endif /* _LWIPOPTS_H */
