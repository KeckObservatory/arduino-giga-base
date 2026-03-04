/*
 * giga-ethernet.cpp: implementation of an ethernet interface for the Giga R1 board
 *
 */

#define GIGA_ETHERNET_CPP_

#include "giga-ethernet.h"

/******************************************************************************************************************************
 * @brief Setup the ethernet subsystem.
 ******************************************************************************************************************************/
GigaEthernet::rc_ethernet GigaEthernet::setup() {

  char message_text[128];

  // Ethernet.init(pin) configures the CS pin, which in this case is 10
  Ethernet.init(10);

  // To start the Ethernet interface, we need a MAC address as well as the usual TCP/IP 
  // components such as the IP address, netmask, and gateway.  DNS is not strictly needed
  // here (because we are not making outbound connections) but the API calls for some value
  // so we will use a reasonable default that ought to always work: Google at 8.8.8.8

  // Build a unique MAC address.  Get the low 3 bytes hashed from the STM32 unique identifier.
  uint8_t mac[6] = {WIZNET_OUI_0, WIZNET_OUI_1, WIZNET_OUI_2, 0, 0, 1};

  uint32_t low_mac = GetUIDtoMAC();
  mac[3] = (low_mac >> 2) & 0xFF;
  mac[4] = (low_mac >> 1) & 0xFF;
  mac[5] = (low_mac     ) & 0xFF;

  sprintf(message_text, "[ETH] MAC address %02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  SerialUSB.println(message_text);
 
  // The registry entries for these 4 settings are guaranteed to exist because they are listed in the
  // REGISTRY_REQUIRED_KEYS define in giga-config.cpp.
  KVStringRC registry_ip = config.registry_get(registry_net_ip);
  KVStringRC registry_netmask = config.registry_get(registry_net_netmask);
  KVStringRC registry_gateway = config.registry_get(registry_net_gateway);
  KVStringRC registry_dns = config.registry_get(registry_net_dns);

  // Convert to IP address instances
  IPAddress ip(registry_ip.second.c_str());
  IPAddress netmask(registry_netmask.second.c_str());
  IPAddress gateway(registry_gateway.second.c_str());
  IPAddress dns(registry_dns.second.c_str());

  sprintf(message_text, "[ETH] Configuring IP address %s (netmask %s, gateway %s, dns %s)", ip.toString().c_str(), netmask.toString().c_str(), gateway.toString().c_str(), dns.toString().c_str());
  SerialUSB.println(message_text);

  // Initialize the Ethernet device
  Ethernet.begin(mac, ip, dns, gateway, netmask);

  // Check for a missing ethernet shield
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    SerialUSB.println("[ETH] Ethernet hardware is not present!");
    return GigaEthernet::rc_ethernet::ETHER_NO_HARDWARE;
  }

  // Check for a missing ethernet cable
  if (Ethernet.linkStatus() == LinkOFF) {
    SerialUSB.println("[ETH] Ethernet cable is not connected.");
    return GigaEthernet::rc_ethernet::ETHER_NO_CABLE;
  }
  
  // Start listening for clients
  SerialUSB.println("[ETH] Starting TCP/IP server(s).");
  ioc_server.begin();
  
  // 2026-03-03 prichards: control server feature not ready for deployment yet
  //control_server.begin();

  // Return success
  return GigaEthernet::rc_ethernet::ETHER_NO_ERROR;
}

/******************************************************************************************************************************
 * @brief Run the ethernet communications loop.
 ******************************************************************************************************************************/
void GigaEthernet::loop() {

  char client_buffer[128] = {0};

  // This value is never actually used but we need it to consume any data coming from the clients,
  // which are ignored for the load cell implementation.
  volatile char __attribute__((unused)) dummy;

  // Check for any new client connecting to the IOC server
  EthernetClient new_client = ioc_server.accept();
  if (new_client) {
    for (byte i = 0; i < MAX_CLIENTS; i++) {
      if (!ioc_clients[i]) {

        // Once we "accept", the client is no longer tracked by EthernetServer
        // so we must store it into our list of clients
        ioc_clients[i] = new_client;
        break;
      }
    }
  }

  // Check for any new client connecting to the control server
  // 2026-03-03 prichards: control server not ready yet
#ifdef CONTROL_SERVER
  EthernetClient new_control_client = control_server.accept();
  if (new_control_client) {
    for (byte i = 0; i < MAX_CLIENTS; i++) {
      if (!control_clients[i]) {

        // Once we "accept", the client is no longer tracked by EthernetServer
        // so we must store it into our list of clients
        control_clients[i] = new_control_client;
        break;
      }
    }
  }
#endif



  // Check for incoming data from all clients and throw it away, as it is not needed
  for (byte i = 0; i < MAX_CLIENTS; i++) {
    while (ioc_clients[i] && ioc_clients[i].available() > 0) {

      // read incoming data from the client into a variable but do nothing with it
      dummy = ioc_clients[i].read();
    }
  }

  // stop any clients which disconnect
  for (byte i = 0; i < MAX_CLIENTS; i++) {
    if (ioc_clients[i] && !ioc_clients[i].connected()) {
      ioc_clients[i].stop();
    }

    // 2026-03-03 prichards: control server not ready yet
#ifdef CONTROL_SERVER    
    if (control_clients[i] && !control_clients[i].connected()) {
      control_clients[i].stop();
    }
#endif

  }

}


/******************************************************************************************************************************
 * @brief Send to every connected IOC client the contents of a buffer.
 *
 * @param[in]  buf                  The (string) contents to send.
 ******************************************************************************************************************************/
void GigaEthernet::ioc_send_all(char *buf) {

  for (uint8_t i = 0; i < MAX_CLIENTS; i++) {
    if (ioc_clients[i] && ioc_clients[i].connected()) {
      // Send to every connected client
      ioc_clients[i].print(buf);
    }
  }

}


/******************************************************************************************************************************
 * @brief Send to a particular connected control client the contents of a buffer.
 *
 * @param[in]  buf                  The (string) contents to send.
 ******************************************************************************************************************************/
void GigaEthernet::control_send(uint8_t client_index, char *buf) {

// 2026-03-03 prichards: control server not ready yet
#ifdef CONTROL_SERVER
  if (control_clients[client_index] && control_clients[client_index].connected()) {

    // Send to connected control client
    control_clients[client_index].print(buf);
  }
#endif
}


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

void GigaEthernet::GetUID(uint8_t *buf) {

  // Arrange 12 bytes of UID into buf[]
  uint32_t uid = HAL_GetUIDw0();
  memcpy (&buf[8], &uid, 4);

  uid = HAL_GetUIDw1();
  memcpy (&buf[4], &uid, 4);

  uid = HAL_GetUIDw2();
  memcpy (&buf[0], &uid, 4);
}







