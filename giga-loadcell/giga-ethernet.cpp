/*
 * giga-ethernet.cpp: implementation of an ethernet interface for the Giga R1 board
 *
 */

#define GIGA_ETHERNET_CPP_

#include "giga-ethernet.h"

void GigaEthernet::setup() {

  char log_buf[96];

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

  // Get the IP address from the registry, which will either get it directly from flash, or from
  // a USB flash drive that is inserted.
  IPAddress ip("10.77.0.210");
  IPAddress dns("8.8.8.8");
  IPAddress gateway("10.77.0.1");
  IPAddress netmask("255.255.0.0");

  //KVString ip2("10.77.0.210");

  // Get the IP address out of the registry
  auto registry_ip = config.registry_get(registry_net_ip);
  if (registry_ip.first == GigaConfig::rc::NO_ERROR) {
    SerialUSB.println(">>>>> success finding registry_net_ip");
  }

  auto registry_cal = config.registry_get(registry_cal_placeholder);
  if (registry_cal.first == GigaConfig::rc::NO_ERROR) {
    SerialUSB.println("success found registry_cal_placeholder");
  } else {
    SerialUSB.println(">>>>> failed to find registry_cal_placeholder");
  }

  

#ifdef zero
  KVString registry_ip = config.registry_get(registry_net_ip);
  KVString registry_netmask = config.registry_get(registry_net_netmask);
  KVString registry_gateway = config.registry_get(registry_net_gateway);
  KVString registry_dns = config.registry_get(registry_net_dns);
  sprintf(log_buf, "[ETH] Configuring IP address %s (netmask %s, gateway %s, dns %s)", registry_ip.c_str(), registry_netmask.c_str(), registry_gateway.c_str(), registry_dns.c_str());
  SerialUSB.println(log_buf);
#endif

#ifdef zero
  // Convert to IP address instances
  IPAddress ip(registry_ip.c_str());
  IPAddress netmask(registry_netmask.c_str());
  IPAddress gateway(registry_gateway.c_str());
  IPAddress dns(registry_dns.c_str());
#endif 

  // initialize the Ethernet device
  Ethernet.begin(mac, ip, dns, gateway, netmask);

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

void GigaEthernet::loop() {

  // This value is never actually used but we need it to consume any data coming from the clients,
  // which are ignored for the load cell implementation.
  volatile char __attribute__((unused)) dummy;

  // Check for any new client connecting
  EthernetClient newClient = server.accept();
  if (newClient) {
    for (byte i = 0; i < 8; i++) {
      if (!clients[i]) {

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







