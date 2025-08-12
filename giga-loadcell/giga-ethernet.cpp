/*
 * giga-ethernet.cpp: implementation of an ethernet interface for the Giga R1 board
 *
 */

#define GIGA_ETHERNET_CPP_

#include "giga-ethernet.h"

void GigaEthernet::setup() {


  // Ethernet.init(pin) configures the CS pin, which in this case is 10
  Ethernet.init(10);

  // Build a unique MAC address.  Get the low 3 bytes hashed from the STM32 unique identifier.
  // The high 3 bytes of the MAC are ASCII 'W' 'M' and 'K' which is 57:4D:4B, not assigned to any vendor as of July 2025.
  uint32_t low_mac = GetUIDtoMAC();
  uint8_t mac[6] = {WIZNET_OUI_0, WIZNET_OUI_1, WIZNET_OUI_2, 0, 0, 1};

  mac[3] = (low_mac >> 2) & 0xFF;
  mac[4] = (low_mac >> 1) & 0xFF;
  mac[5] = (low_mac     ) & 0xFF;

  // Get the IP address out of flash memory.  
  IPAddress ip(10, 77, 0, 210);
  IPAddress myDns(8, 8, 8, 8);
  IPAddress gateway(10, 77, 0, 1);
  IPAddress subnet(255, 255, 0, 0);

  // initialize the Ethernet device
  Ethernet.begin(mac, ip, myDns, gateway, subnet);

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    SerialUSB.println("[ETH] Ethernet hardware is not present!");
    return;

    //while (true) {
    //  led.panic();
    //}
  }

  if (Ethernet.linkStatus() == LinkOFF) {
    SerialUSB.println("[ETH] Ethernet cable is not connected.");
  }

  
  // start listening for clients
  SerialUSB.println("[ETH] Starting TCP/IP server.");
  server.begin();


}

void GigaEthernet::clear() {


}

void GigaEthernet::loop() {
  volatile char dummy;

  // Check for any new client connecting
  EthernetClient newClient = server.accept();
  if (newClient) {
    for (byte i = 0; i < 8; i++) {
      if (!clients[i]) {

        // TESTING: print a line indicating which client has connected, not used in production
        //newClient.print("This is client number ");
        //newClient.println(i);

        // Once we "accept", the client is no longer tracked by EthernetServer
        // so we must store it into our list of clients
        clients[i] = newClient;
        break;
      }
    }
  }

  // Check for incoming data from all clients and throw it away, as it is not needed
  for (byte i = 0; i < 8; i++) {
    while (clients[i] && clients[i].available() > 0) {
      // read incoming data from the client into a variable but do nothing with it
      dummy = clients[i].read();
    }
  }

  // stop any clients which disconnect
  for (byte i = 0; i < 8; i++) {
    if (clients[i] && !clients[i].connected()) {
      clients[i].stop();
    }
  }




}


void GigaEthernet::send_all(char *buf) {

  for (byte i = 0; i < 8; i++) {
    if (clients[i] && clients[i].connected()) {
      // Send every connected client the latest load value
      clients[i].print(buf);
    }
  }

}


#ifdef zero

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE };
IPAddress ip(10, 77, 0, 210);
IPAddress myDns(8, 8, 8, 8);
IPAddress gateway(10, 77, 0, 1);
IPAddress subnet(255, 255, 0, 0);


// telnet defaults to port 23
EthernetServer server(23);
EthernetClient clients[8];
bool alreadyConnected = false; // whether or not the client was connected previously


#endif



/* ************************************************************************** */
/* MAC ADDRESS                                                                */
/* ************************************************************************** */

// The following algorithm was derived from this article https://pcbartists.com/firmware/stm32-firmware/generating-32-bit-stm32-unique-id/ 
// which also is stored as a PDF in the doc directory of this Github repository.
uint32_t GigaEthernet::UNALIGNED_LOAD32(const char *p) {
  uint32_t result;
  memcpy(&result, p, sizeof(result));
  return result;
}

uint32_t GigaEthernet::Fetch32(const char *p) {
  return UNALIGNED_LOAD32(p);
}

uint32_t GigaEthernet::Rotate32(uint32_t val, int shift) {
  // Avoid shifting by 32: doing so yields an undefined result.
  return shift == 0 ? val : ((val >> shift) | (val << (32 - shift)));
}

// A 32-bit to 32-bit integer hash copied from Murmur3.
uint32_t GigaEthernet::fmix(uint32_t h)
{
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;
  return h;
}

uint32_t GigaEthernet::Mur(uint32_t a, uint32_t h) {
  // Helper from Murmur3 for combining two 32-bit values.
  a *= c1;
  a = Rotate32(a, 17);
  a *= c2;
  h ^= a;
  h = Rotate32(h, 19);
  return h * 5 + 0xe6546b64;
}

uint32_t GigaEthernet::Hash32Len5to12(const char *s, size_t len) {
  uint32_t a = (uint32_t)len, b = a * 5, c = 9, d = b;
  a += Fetch32(s);
  b += Fetch32(s + len - 4);
  c += Fetch32(s + ((len >> 1) & 4));
  return fmix(Mur(c, Mur(b, Mur(a, d))));
}

uint32_t GigaEthernet::GetUIDtoMAC() {

  char uidstr[12];

  // Arrange 12 bytes of UID into uidstr[]
  uint32_t uid = HAL_GetUIDw0();
  memcpy (&uidstr[8], &uid, 4);

  uid = HAL_GetUIDw1();
  memcpy (&uidstr[4], &uid, 4);

  uid = HAL_GetUIDw2();
  memcpy (&uidstr[0], &uid, 4);

  // Generate UID value from uidstr[]
  uid = Hash32Len5to12((const char *)uidstr, 12);

  return uid;

}







