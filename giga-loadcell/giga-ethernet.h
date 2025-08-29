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
#include "giga-ntp.h"

// Use standard telnet port 23 since the protocol is human readable
#define ETHERNET_LISTEN_PORT 23

// Use the Wiznet OUI since it's a Wiznet W5500 device on the board, see https://standards-oui.ieee.org/
#define WIZNET_OUI_0 0x00
#define WIZNET_OUI_1 0x08
#define WIZNET_OUI_2 0xDC

class GigaEthernet {

  private:
  	// Hold a reference to the config subsystem
	  GigaConfig& config;

    // One server and multiple possible clients, even if only one is expected to be used at a time
    EthernetServer server;
    EthernetClient clients[8];

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

  public:
    EthernetUDP udp;

    enum rc_ethernet : uint8_t {
        ETHER_NO_ERROR = 0,
        ETHER_NO_HARDWARE,
        ETHER_NO_CABLE,
    };

    GigaEthernet(GigaConfig& the_config) : config(the_config), server(ETHERNET_LISTEN_PORT), udp() {}

    GigaEthernet::rc_ethernet setup();
    void loop();
    void send_all(char *buf);

    // Convert the STM32 unique ID to a MAC address
    uint32_t GetUIDtoMAC();

    // Get the raw UID to a 12 byte buffer
    void GetUID(uint8_t *buf);

};

#endif