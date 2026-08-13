/*
 * giga-ethernet.h: implementation of an ethernet interface for the Giga R1 board
 *
 */

#ifndef GIGA_ETHERNET_H_
#define GIGA_ETHERNET_H_

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>

#include "timing.h"
#include "giga-config.h"
#include "giga-control.h"

// Use standard telnet port 23 for the IOC, since the protocol is human readable
#define ETHERNET_IOC_PORT 23

// A second port for interactive control of the device
#define ETHERNET_CONTROL_PORT 24

// Use the Wiznet OUI since it's a Wiznet W5500 device on the board, see https://standards-oui.ieee.org/
#define WIZNET_OUI_0 0x00
#define WIZNET_OUI_1 0x08
#define WIZNET_OUI_2 0xDC

/* ****************************************************************************************************************************
 * SOCKET BUDGET
 *
 * The W5500 has 8 hardware sockets and they are a single global pool shared by every server, client and UDP endpoint on the
 * board.  Each EthernetServer permanently occupies one of them for its listening socket, so with two servers running only
 * MAX_SOCK_NUM - 2 == 6 sockets are available for actual connections.
 *
 * This matters more than it looks.  EthernetServer::accept() *detaches* the listening socket the instant a client connects and
 * then calls begin() to open a replacement listener.  begin() -> socketBegin() needs a socket in the CLOSED state; if there
 * isn't one it silently returns MAX_SOCK_NUM and no new listener is created.  The port then has nothing in LISTEN, so the
 * W5500 answers incoming SYNs with RST and clients see ECONNREFUSED -- while the chip keeps answering ping in hardware, so the
 * device looks perfectly healthy from the network.  That is the failure mode this file is written to avoid.
 *
 * Keep MAX_IOC_CLIENTS + MAX_CONTROL_CLIENTS strictly below (MAX_SOCK_NUM - 2) so there is always headroom for begin().
 * ****************************************************************************************************************************/
#define MAX_IOC_CLIENTS      3
#define MAX_CONTROL_CLIENTS  2

#if (MAX_IOC_CLIENTS + MAX_CONTROL_CLIENTS) > (MAX_SOCK_NUM - 2)
#error "Client budget exceeds the available W5500 socket pool (need 2 sockets for the listeners)."
#endif

#define CLIENT_WELCOME "[load cell command processor]\n"

/* ****************************************************************************************************************************
 * W5x00 Sn_SR (socket status) values.
 *
 * These are duplicated here rather than pulled in from utility/w5100.h: the SnSR:: enum lives in a header the Ethernet library
 * treats as private, and its include path has moved between library versions.  The register values themselves are fixed by the
 * Wiznet silicon and are not going to change.
 * ****************************************************************************************************************************/
#define W5X00_SOCK_CLOSED       0x00
#define W5X00_SOCK_INIT         0x13
#define W5X00_SOCK_LISTEN       0x14
#define W5X00_SOCK_SYNSENT      0x15
#define W5X00_SOCK_SYNRECV      0x16
#define W5X00_SOCK_ESTABLISHED  0x17
#define W5X00_SOCK_FIN_WAIT     0x18
#define W5X00_SOCK_CLOSING      0x1A
#define W5X00_SOCK_TIME_WAIT    0x1B
#define W5X00_SOCK_CLOSE_WAIT   0x1C
#define W5X00_SOCK_LAST_ACK     0x1D

/* ****************************************************************************************************************************
 * TIMEOUTS
 * ****************************************************************************************************************************/

// EthernetClient::stop() polls for the socket to reach CLOSED and only then gives up and forces the close.  The library
// default is 1000 ms, which is a full second of blocked loop() -- long enough to overrun the RS485 receive buffer.  100 ms is
// plenty for a LAN peer and bounds the damage when we have to close a socket that will never answer.
#define CLIENT_STOP_TIMEOUT_MS      100

// Idle timeout for IOC clients.  Disabled (0): the device pushes a status line to every IOC client at 10 Hz, so a peer that
// has vanished silently will trip the W5500 retransmit timeout within a couple of seconds and the socket drops to CLOSED on
// its own.  IOC connections are reaped by that path, not by idleness.
#define IOC_IDLE_TIMEOUT_MS         0UL

// Idle timeout for control clients.  This one is load bearing.  Nothing is ever written to a control client after the welcome
// banner, so there is no traffic to trip a retransmit timeout: a control connection whose peer disappears without sending a
// FIN (terminal window closed, laptop suspended, VPN dropped, client SIGKILLed) sits in ESTABLISHED forever and permanently
// consumes one of the six available sockets.  Ten minutes of silence on an interactive port means the operator is gone.
#define CONTROL_IDLE_TIMEOUT_MS     600000UL

// How long a socket may sit in a connected state, owned by nobody, before we force it closed.  Any socket we accept is placed
// into a slot in the same loop() iteration, so anything unowned for this long is genuinely leaked.
#define ORPHAN_GRACE_MS             5000UL

// How long a server may go without a socket in LISTEN before we start clawing sockets back, and before we give up and let the
// watchdog restart the board.
#define LISTEN_RECOVERY_MS          30000UL
#define LISTEN_PANIC_MS             120000UL

// Minimum spacing between reclaim attempts, so recovery drops one connection at a time rather than all of them at once.
#define RECLAIM_INTERVAL_MS         2000UL

// How often the deep audit (ownership revalidation, orphan sweep, listener supervision) runs.
#define NET_AUDIT_INTERVAL_MS       1000UL


/* ****************************************************************************************************************************
 * @brief One accepted connection, plus everything needed to prove the socket underneath it is still ours.
 *
 * EthernetClient is a one byte object: it holds a hardware socket index and nothing else.  The Ethernet library is free to
 * close that socket and hand the same index straight back out as a new connection or as a server listening socket, and the
 * stale EthernetClient we are still holding has no way to know.  Acting on it then closes somebody else's socket.  Recording
 * the socket index and the peer identity at accept time lets us detect that and walk away from the slot instead.
 * ****************************************************************************************************************************/
struct ClientSlot {

  EthernetClient client;

  // Socket index and peer identity captured at accept time, used to prove ownership later
  uint8_t   sockindex;
  IPAddress remote_ip;
  uint16_t  remote_port;

  // millis() of the last byte read from, or successfully written to, this client
  uint32_t  last_activity_ms;

  // millis() at accept
  uint32_t  connected_ms;

  bool      in_use;

  void reset() {
    client = EthernetClient(MAX_SOCK_NUM);
    sockindex = MAX_SOCK_NUM;
    remote_ip = IPAddress(0, 0, 0, 0);
    remote_port = 0;
    last_activity_ms = 0;
    connected_ms = 0;
    in_use = false;
  }
};


class GigaEthernet {

  private:
  	// Hold references to subsystems
	  GigaConfig& config;
    GigaControl& control;

    // One IOC server and multiple possible clients, even if only one is expected to be used at a time
    EthernetServer ioc_server;
    ClientSlot ioc_clients[MAX_IOC_CLIENTS];

    // A control server and clients
    EthernetServer control_server;
    ClientSlot control_clients[MAX_CONTROL_CLIENTS];

    // Listener supervision state.  Zero means "healthy"; otherwise it is the millis() at which the listener went missing.
    uint32_t ioc_listen_lost_ms;
    uint32_t control_listen_lost_ms;
    uint32_t last_reclaim_ms;
    uint32_t last_audit_ms;

    // millis() at which each hardware socket was first seen connected but unowned; zero means "not a candidate"
    uint32_t orphan_since_ms[MAX_SOCK_NUM];

    // Magic numbers for 32-bit hashing, used in the MAC address routines below
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    // MAC address calculation related functions
    uint32_t UNALIGNED_LOAD32(const char *p);
    uint32_t Fetch32(const char *p);
    uint32_t Rotate32(uint32_t val, int shift);
    uint32_t fmix(uint32_t h);
    uint32_t Mur(uint32_t a, uint32_t h);
    uint32_t Hash32Len5to12(const char *s, size_t len);

    // -- socket and slot management ------------------------------------------------------------------------------------
    static uint8_t socket_state(uint8_t sockindex);
    static bool    state_is_connection(uint8_t state);
    static bool    state_is_connected(uint8_t state, bool data_pending);

    bool    slot_owns_socket(ClientSlot& slot, uint8_t state, bool deep);
    void    slot_abandon(ClientSlot& slot, const char *label, uint8_t index);
    void    slot_close(ClientSlot& slot, const char *label, uint8_t index, const char *reason);
    int8_t  slot_store(ClientSlot *slots, uint8_t count, EthernetClient& client);
    void    slot_service(ClientSlot *slots, uint8_t count, uint32_t idle_timeout_ms, const char *label, bool deep);

    bool    socket_is_owned(uint8_t sockindex);
    bool    server_is_listening(uint16_t port);
    void    supervise_listener(uint16_t port, uint32_t& lost_since_ms, const char *label);
    bool    reclaim_socket();
    void    reap_orphan_sockets();
    void    audit();

  public:
    EthernetUDP udp;

    enum rc_ethernet : uint8_t {
        ETHER_NO_ERROR = 0,
        ETHER_NO_HARDWARE,
        ETHER_NO_CABLE,
    };

    GigaEthernet(GigaConfig& the_config,GigaControl& the_control) :
                config(the_config),
                control(the_control),
                ioc_server(ETHERNET_IOC_PORT),
                control_server(ETHERNET_CONTROL_PORT),
                ioc_listen_lost_ms(0),
                control_listen_lost_ms(0),
                last_reclaim_ms(0),
                last_audit_ms(0),
                udp() {}

    GigaEthernet::rc_ethernet setup();
    void loop();
    void ioc_send_all(const char *buf);
    void control_send(uint8_t client_index, const char *buf);

    // Dump the hardware socket table to the USB console.  Call this from the control interface when a device is misbehaving:
    // a healthy board shows one socket in 0x14 (LISTEN) per server; an exhausted one shows none.
    void report_sockets(const char *why);

    // Convert the STM32 unique ID to a MAC address
    uint32_t GetUIDtoMAC();

    // Get the raw UID to a 12 byte buffer
    void GetUID(uint8_t *buf);

};

#endif
