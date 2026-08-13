/*
 * giga-ethernet.cpp: implementation of an ethernet interface for the Giga R1 board
 *
 */

#define GIGA_ETHERNET_CPP_

#include <mbed.h>

#include "giga-ethernet.h"

/******************************************************************************************************************************
 * @brief Setup the ethernet subsystem.
 ******************************************************************************************************************************/
GigaEthernet::rc_ethernet GigaEthernet::setup() {

  char message_text[128];

  // Ethernet.init(pin) configures the CS pin, which in this case is 10
  Ethernet.init(10);

  // Start every client slot and orphan timer from a known state
  for (uint8_t i = 0; i < MAX_IOC_CLIENTS; i++) ioc_clients[i].reset();
  for (uint8_t i = 0; i < MAX_CONTROL_CLIENTS; i++) control_clients[i].reset();
  for (uint8_t s = 0; s < MAX_SOCK_NUM; s++) orphan_since_ms[s] = 0;

  ioc_listen_lost_ms = 0;
  control_listen_lost_ms = 0;
  last_reclaim_ms = 0;
  last_audit_ms = millis();

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
  control_server.begin();

  sprintf(message_text, "[ETH] Socket budget: %d hardware, 2 listeners, %d IOC + %d control clients.",
          MAX_SOCK_NUM, MAX_IOC_CLIENTS, MAX_CONTROL_CLIENTS);
  SerialUSB.println(message_text);

  report_sockets("startup");

  // Return success
  return GigaEthernet::rc_ethernet::ETHER_NO_ERROR;
}


/******************************************************************************************************************************
 * @brief Run the ethernet communications loop.
 *
 * Order matters here.  Slot servicing runs *before* accept(), because accept() calls EthernetServer::begin() to re-arm the
 * listening socket and begin() can only succeed if a hardware socket is already in the CLOSED state.  Reaping first means a
 * socket released this iteration is available to the listener in the same iteration.
 ******************************************************************************************************************************/
void GigaEthernet::loop() {

  char client_buffer[128] = {0};
  uint8_t client_id;

  // This value is never actually used but we need it to consume any data coming from the clients,
  // which are ignored for the load cell implementation.
  volatile char __attribute__((unused)) dummy;

  // Temporary storage for a byte coming from the control client
  volatile uint8_t temp;

  uint32_t now = millis();

  // The deep ownership check costs a few extra SPI reads per slot, so it runs on the audit interval rather than every
  // iteration.  The cheap checks (socket index, server_port ownership, socket state) run every time through.
  bool deep = (now - last_audit_ms) >= NET_AUDIT_INTERVAL_MS;

  // Release anything that has disconnected, gone idle, or had its socket taken away from us
  slot_service(ioc_clients, MAX_IOC_CLIENTS, IOC_IDLE_TIMEOUT_MS, "IOC", deep);
  slot_service(control_clients, MAX_CONTROL_CLIENTS, CONTROL_IDLE_TIMEOUT_MS, "CTL", deep);

  // Check for any new client connecting to the IOC server
  EthernetClient new_client = ioc_server.accept();
  if (new_client) {

    // Once we "accept", the client is no longer tracked by EthernetServer so we must store it into our list of clients.
    // If we cannot store it we must close it here: an accepted socket that nobody holds is leaked for good.
    if (slot_store(ioc_clients, MAX_IOC_CLIENTS, new_client) < 0) {
      SerialUSB.println("[ETH] IOC connection refused: no free client slot.");
      new_client.setConnectionTimeout(CLIENT_STOP_TIMEOUT_MS);
      new_client.stop();
    }
  }

  // Check for any new client connecting to the control server
  EthernetClient new_control_client = control_server.accept();
  if (new_control_client) {

    int8_t index = slot_store(control_clients, MAX_CONTROL_CLIENTS, new_control_client);
    if (index < 0) {
      SerialUSB.println("[ETH] Control connection refused: no free client slot.");
      new_control_client.setConnectionTimeout(CLIENT_STOP_TIMEOUT_MS);
      new_control_client.stop();
    }
    else {
      control_send((uint8_t)index, CLIENT_WELCOME);
    }
  }

  // Check for incoming data from all IOC clients and throw it away, as it is not needed
  for (uint8_t i = 0; i < MAX_IOC_CLIENTS; i++) {
    while (ioc_clients[i].in_use && ioc_clients[i].client.available() > 0) {

      // read incoming data from the client into a variable but do nothing with it
      dummy = ioc_clients[i].client.read();
      ioc_clients[i].last_activity_ms = now;
    }
  }

  // Check for incoming data from control client and do the initial command parsing
  for (uint8_t i = 0; i < MAX_CONTROL_CLIENTS; i++) {
    while (control_clients[i].in_use && control_clients[i].client.available() > 0) {

      // read incoming data from the client
      temp = control_clients[i].client.read();
      control_clients[i].last_activity_ms = now;

      // tell the control instance that a new command byte has arrived
      control.command(i, temp);
    }
  }

  // Check for command responses and send them back to the client
  if (control.get_response(&client_id, client_buffer) == GigaControl::rc::CONTROL_RESPONSE_READY) {

    control_send(client_id, client_buffer);
  }

  // Periodically supervise the listening sockets and sweep up leaked ones
  if (deep) {
    last_audit_ms = now;
    audit();
  }
}


/* ************************************************************************** */
/* SOCKET AND SLOT MANAGEMENT                                                 */
/* ************************************************************************** */

/******************************************************************************************************************************
 * @brief Read the Sn_SR status register of an arbitrary hardware socket.
 *
 * EthernetClient's single argument constructor and status() are both public and neither touches the socket, so this is a
 * read-only probe that does not depend on any private part of the Ethernet library.
 ******************************************************************************************************************************/
uint8_t GigaEthernet::socket_state(uint8_t sockindex) {

  if (sockindex >= MAX_SOCK_NUM) return W5X00_SOCK_CLOSED;

  EthernetClient probe(sockindex);
  return probe.status();
}


/******************************************************************************************************************************
 * @brief Is this socket state one that an accepted connection can legitimately be in?
 ******************************************************************************************************************************/
bool GigaEthernet::state_is_connection(uint8_t state) {

  return (state == W5X00_SOCK_ESTABLISHED) || (state == W5X00_SOCK_CLOSE_WAIT);
}


/******************************************************************************************************************************
 * @brief Reimplementation of EthernetClient::connected() against a status value we have already read, to avoid paying for a
 *        second SPI transaction per client per loop.
 ******************************************************************************************************************************/
bool GigaEthernet::state_is_connected(uint8_t state, bool data_pending) {

  return !(state == W5X00_SOCK_LISTEN ||
           state == W5X00_SOCK_CLOSED ||
           state == W5X00_SOCK_FIN_WAIT ||
           (state == W5X00_SOCK_CLOSE_WAIT && !data_pending));
}


/******************************************************************************************************************************
 * @brief Determine whether the hardware socket behind a slot is still the connection we accepted into it.
 *
 * An EthernetClient carries only a socket index.  Once the underlying socket reaches CLOSED, socketBegin() is free to reopen
 * that same index as a listening socket or as an entirely different connection, and our copy has no idea.  Calling stop() on
 * it at that point closes somebody else's socket -- including, quite often, the listener that was just created to replace the
 * one accept() consumed.
 *
 * The cheap checks below catch every re-purposing path, because socketBegin() must drive a socket through CLOSED -> INIT ->
 * LISTEN, and socketListen() records the port in server_port[], before that socket can carry a new connection.  Since this
 * runs every loop() iteration and a full recycle requires a TCP handshake with a remote peer, a slot cannot sleep through the
 * whole sequence.  The deep check closes the gap completely by comparing the peer address, which the W5500 rewrites in
 * Sn_DIPR/Sn_DPORT on every new connection.
 *
 * @param[in]  state    Sn_SR value already read for slot.sockindex.
 * @param[in]  deep     Also verify the peer identity (costs 6 more bytes over SPI).
 * @return              true if the slot may be acted upon.
 ******************************************************************************************************************************/
bool GigaEthernet::slot_owns_socket(ClientSlot& slot, uint8_t state, bool deep) {

  if (!slot.in_use) return false;
  if (slot.sockindex >= MAX_SOCK_NUM) return false;

  // stop() sets the client's index to MAX_SOCK_NUM, so a mismatch means this object has already been released
  if (slot.client.getSocketNumber() != slot.sockindex) return false;

  // A non-zero entry means an EthernetServer has claimed this socket as its listener
  if (EthernetServer::server_port[slot.sockindex] != 0) return false;

  // Anything that is not a live connection is not ours to close
  if (!state_is_connection(state)) return false;

  if (deep) {
    if (slot.client.remotePort() != slot.remote_port) return false;
    if (!(slot.client.remoteIP() == slot.remote_ip)) return false;
  }

  return true;
}


/******************************************************************************************************************************
 * @brief Give up a slot *without* touching the socket, because the socket is no longer ours.
 ******************************************************************************************************************************/
void GigaEthernet::slot_abandon(ClientSlot& slot, const char *label, uint8_t index) {

  char message_text[128];

  sprintf(message_text, "[ETH] %s client %u released socket %u (no longer owned, state 0x%02X).",
          label, (unsigned)index, (unsigned)slot.sockindex, (unsigned)socket_state(slot.sockindex));
  SerialUSB.println(message_text);

  slot.reset();
}


/******************************************************************************************************************************
 * @brief Close a connection we still own and free the slot.
 ******************************************************************************************************************************/
void GigaEthernet::slot_close(ClientSlot& slot, const char *label, uint8_t index, const char *reason) {

  char message_text[128];

  sprintf(message_text, "[ETH] %s client %u on socket %u closing: %s (up %lu s).",
          label, (unsigned)index, (unsigned)slot.sockindex, reason, (unsigned long)((millis() - slot.connected_ms) / 1000UL));
  SerialUSB.println(message_text);

  // Bound the time stop() will block waiting for a peer that may never answer
  slot.client.setConnectionTimeout(CLIENT_STOP_TIMEOUT_MS);
  slot.client.stop();

  slot.reset();
}


/******************************************************************************************************************************
 * @brief Record a freshly accepted client in the first free slot.
 *
 * @return  The slot index, or -1 if every slot is occupied.
 ******************************************************************************************************************************/
int8_t GigaEthernet::slot_store(ClientSlot *slots, uint8_t count, EthernetClient& client) {

  char message_text[128];

  for (uint8_t i = 0; i < count; i++) {

    if (slots[i].in_use) continue;

    slots[i].client = client;
    slots[i].client.setConnectionTimeout(CLIENT_STOP_TIMEOUT_MS);
    slots[i].sockindex = client.getSocketNumber();
    slots[i].remote_ip = client.remoteIP();
    slots[i].remote_port = client.remotePort();
    slots[i].last_activity_ms = millis();
    slots[i].connected_ms = slots[i].last_activity_ms;
    slots[i].in_use = true;

    sprintf(message_text, "[ETH] Accepted %s:%u into slot %u on socket %u.",
            slots[i].remote_ip.toString().c_str(), (unsigned)slots[i].remote_port, (unsigned)i, (unsigned)slots[i].sockindex);
    SerialUSB.println(message_text);

    return (int8_t)i;
  }

  return -1;
}


/******************************************************************************************************************************
 * @brief Validate, reap and time out every slot in an array.
 ******************************************************************************************************************************/
void GigaEthernet::slot_service(ClientSlot *slots, uint8_t count, uint32_t idle_timeout_ms, const char *label, bool deep) {

  uint32_t now = millis();

  for (uint8_t i = 0; i < count; i++) {

    ClientSlot& slot = slots[i];
    if (!slot.in_use) continue;

    uint8_t state = socket_state(slot.sockindex);

    // The socket has been recycled out from under us: drop the slot, but do not close the socket
    if (!slot_owns_socket(slot, state, deep)) {
      slot_abandon(slot, label, i);
      continue;
    }

    // available() is only needed to disambiguate CLOSE_WAIT, so skip the SPI read in the common case
    bool data_pending = (state == W5X00_SOCK_CLOSE_WAIT) ? (slot.client.available() > 0) : false;

    if (!state_is_connected(state, data_pending)) {
      slot_close(slot, label, i, "peer disconnected");
      continue;
    }

    // A connection whose peer vanished without a FIN stays in ESTABLISHED indefinitely, because the W5500 only times a socket
    // out when a transmission goes unacknowledged.  For a port we never write to, that never happens -- hence this.
    if (idle_timeout_ms && ((now - slot.last_activity_ms) >= idle_timeout_ms)) {
      slot_close(slot, label, i, "idle timeout");
      continue;
    }
  }
}


/******************************************************************************************************************************
 * @brief Is a hardware socket currently claimed by one of our client slots?
 ******************************************************************************************************************************/
bool GigaEthernet::socket_is_owned(uint8_t sockindex) {

  for (uint8_t i = 0; i < MAX_IOC_CLIENTS; i++) {
    if (ioc_clients[i].in_use && ioc_clients[i].sockindex == sockindex) return true;
  }

  for (uint8_t i = 0; i < MAX_CONTROL_CLIENTS; i++) {
    if (control_clients[i].in_use && control_clients[i].sockindex == sockindex) return true;
  }

  return false;
}


/******************************************************************************************************************************
 * @brief Does the given port currently have a hardware socket sitting in LISTEN?
 *
 * If it does not, the W5500 answers incoming SYNs on that port with RST and clients see ECONNREFUSED.
 ******************************************************************************************************************************/
bool GigaEthernet::server_is_listening(uint16_t port) {

  for (uint8_t s = 0; s < MAX_SOCK_NUM; s++) {
    if (EthernetServer::server_port[s] == port && socket_state(s) == W5X00_SOCK_LISTEN) return true;
  }

  return false;
}


/******************************************************************************************************************************
 * @brief Force closed any socket that is carrying a connection nobody owns.
 *
 * Every socket we accept is placed into a slot in the same loop() iteration, and a slot whose socket was recycled is dropped
 * without closing it, so a connected socket that stays unclaimed for ORPHAN_GRACE_MS is genuinely leaked.  Only ESTABLISHED
 * and CLOSE_WAIT are swept: the teardown states resolve on their own, and socketBegin() will force those closed if it ever
 * needs the socket.  UDP sockets are never in these states, so an endpoint on the udp member is not at risk.
 ******************************************************************************************************************************/
void GigaEthernet::reap_orphan_sockets() {

  char message_text[128];
  uint32_t now = millis();

  for (uint8_t s = 0; s < MAX_SOCK_NUM; s++) {

    uint8_t state = socket_state(s);

    bool candidate = (EthernetServer::server_port[s] == 0) &&
                     state_is_connection(state) &&
                     !socket_is_owned(s);

    if (!candidate) {
      orphan_since_ms[s] = 0;
      continue;
    }

    if (orphan_since_ms[s] == 0) {
      orphan_since_ms[s] = now;
      continue;
    }

    if ((now - orphan_since_ms[s]) >= ORPHAN_GRACE_MS) {

      sprintf(message_text, "[ETH] Reclaiming leaked socket %u (state 0x%02X, unowned for %lu ms).",
              (unsigned)s, (unsigned)state, (unsigned long)(now - orphan_since_ms[s]));
      SerialUSB.println(message_text);

      EthernetClient orphan(s);
      orphan.setConnectionTimeout(CLIENT_STOP_TIMEOUT_MS);
      orphan.stop();

      orphan_since_ms[s] = 0;
    }
  }
}


/******************************************************************************************************************************
 * @brief Free exactly one socket by closing the least recently active connection.
 *
 * @return  true if a connection was closed.
 ******************************************************************************************************************************/
bool GigaEthernet::reclaim_socket() {

  ClientSlot *victim = NULL;
  const char *label = NULL;
  uint8_t index = 0;
  uint32_t now = millis();
  uint32_t oldest = 0;

  for (uint8_t i = 0; i < MAX_CONTROL_CLIENTS; i++) {
    if (!control_clients[i].in_use) continue;
    uint32_t age = now - control_clients[i].last_activity_ms;
    if (victim == NULL || age > oldest) { victim = &control_clients[i]; label = "CTL"; index = i; oldest = age; }
  }

  for (uint8_t i = 0; i < MAX_IOC_CLIENTS; i++) {
    if (!ioc_clients[i].in_use) continue;
    uint32_t age = now - ioc_clients[i].last_activity_ms;
    if (victim == NULL || age > oldest) { victim = &ioc_clients[i]; label = "IOC"; index = i; oldest = age; }
  }

  if (victim == NULL) return false;

  slot_close(*victim, label, index, "reclaimed to restore a listening socket");
  return true;
}


/******************************************************************************************************************************
 * @brief Watch one server's listening socket and take escalating action if it stays missing.
 *
 * A missing listener is the observable form of socket exhaustion: the port refuses connections while the board keeps
 * answering ping, because the W5500 handles ICMP in hardware regardless of socket state.
 ******************************************************************************************************************************/
void GigaEthernet::supervise_listener(uint16_t port, uint32_t& lost_since_ms, const char *label) {

  char message_text[128];
  uint32_t now = millis();

  if (server_is_listening(port)) {
    if (lost_since_ms != 0) {
      sprintf(message_text, "[ETH] %s listener on port %u restored.", label, (unsigned)port);
      SerialUSB.println(message_text);
      lost_since_ms = 0;
    }
    return;
  }

  // First time we have noticed it missing.  This is normal for a single pass: accept() detaches the listener and re-arms it
  // on the next call, so only a persistent absence is a fault.
  if (lost_since_ms == 0) {
    lost_since_ms = now;
    return;
  }

  uint32_t elapsed = now - lost_since_ms;

  if (elapsed >= LISTEN_PANIC_MS) {

    sprintf(message_text, "[ETH] %s listener on port %u missing for %lu s and unrecoverable. Rebooting.",
            label, (unsigned)port, (unsigned long)(elapsed / 1000UL));
    SerialUSB.println(message_text);
    report_sockets("listener panic");

    // Same mechanism the control interface uses for a commanded restart
    mbed::Watchdog &watchdog = mbed::Watchdog::get_instance();
    watchdog.start(100);
    return;
  }

  if (elapsed >= LISTEN_RECOVERY_MS) {

    if ((last_reclaim_ms != 0) && ((now - last_reclaim_ms) < RECLAIM_INTERVAL_MS)) return;

    sprintf(message_text, "[ETH] %s listener on port %u missing for %lu s: reclaiming a socket.",
            label, (unsigned)port, (unsigned long)(elapsed / 1000UL));
    SerialUSB.println(message_text);
    report_sockets("listener starved");

    if (reclaim_socket()) {
      last_reclaim_ms = now;

      // Give the server an immediate chance to grab the socket we just freed
      if (port == ETHERNET_IOC_PORT) ioc_server.begin();
      else                           control_server.begin();
    }
  }
}


/******************************************************************************************************************************
 * @brief Periodic network health work.
 ******************************************************************************************************************************/
void GigaEthernet::audit() {

  reap_orphan_sockets();
  supervise_listener(ETHERNET_IOC_PORT, ioc_listen_lost_ms, "IOC");
  supervise_listener(ETHERNET_CONTROL_PORT, control_listen_lost_ms, "CTL");
}


/******************************************************************************************************************************
 * @brief Dump the hardware socket table to the USB console.
 *
 * Sn_SR values: 00 CLOSED, 13 INIT, 14 LISTEN, 15 SYNSENT, 16 SYNRECV, 17 ESTABLISHED,
 *               18 FIN_WAIT, 1A CLOSING, 1B TIME_WAIT, 1C CLOSE_WAIT, 1D LAST_ACK, 22 UDP
 ******************************************************************************************************************************/
void GigaEthernet::report_sockets(const char *why) {

  char message_text[128];

  sprintf(message_text, "[ETH] Socket table (%s):", why);
  SerialUSB.println(message_text);

  for (uint8_t s = 0; s < MAX_SOCK_NUM; s++) {

    uint8_t state = socket_state(s);
    EthernetClient probe(s);

    sprintf(message_text, "[ETH]   sock %u state 0x%02X server_port %-5u owner %-4s peer %s:%u",
            (unsigned)s, (unsigned)state, (unsigned)EthernetServer::server_port[s],
            socket_is_owned(s) ? "slot" : "-",
            probe.remoteIP().toString().c_str(), (unsigned)probe.remotePort());
    SerialUSB.println(message_text);
  }
}


/******************************************************************************************************************************
 * @brief Send to every connected IOC client the contents of a buffer.
 *
 * @param[in]  buf                  The (string) contents to send.
 ******************************************************************************************************************************/
void GigaEthernet::ioc_send_all(const char *buf) {

  size_t len = strlen(buf);

  for (uint8_t i = 0; i < MAX_IOC_CLIENTS; i++) {

    ClientSlot& slot = ioc_clients[i];
    if (!slot.in_use) continue;

    uint8_t state = socket_state(slot.sockindex);
    if (!slot_owns_socket(slot, state, false)) {
      slot_abandon(slot, "IOC", i);
      continue;
    }

    // A short write means the socket died mid-send; slot_service() will reap it on the next pass.  Only a full write counts
    // as activity.
    if (slot.client.write((const uint8_t *)buf, len) == len) {
      slot.last_activity_ms = millis();
    }
  }
}


/******************************************************************************************************************************
 * @brief Send to a particular connected control client the contents of a buffer.
 *
 * @param[in]  client_index         Which control slot to send to.
 * @param[in]  buf                  The (string) contents to send.
 ******************************************************************************************************************************/
void GigaEthernet::control_send(uint8_t client_index, const char *buf) {

  if (client_index >= MAX_CONTROL_CLIENTS) return;

  ClientSlot& slot = control_clients[client_index];
  if (!slot.in_use) return;

  uint8_t state = socket_state(slot.sockindex);
  if (!slot_owns_socket(slot, state, false)) {
    slot_abandon(slot, "CTL", client_index);
    return;
  }

  size_t len = strlen(buf);
  if (slot.client.write((const uint8_t *)buf, len) == len) {
    slot.last_activity_ms = millis();
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

void GigaEthernet::GetUID(uint8_t *buf) {

  // Arrange 12 bytes of UID into buf[]
  uint32_t uid = HAL_GetUIDw0();
  memcpy (&buf[8], &uid, 4);

  uid = HAL_GetUIDw1();
  memcpy (&buf[4], &uid, 4);

  uid = HAL_GetUIDw2();
  memcpy (&buf[0], &uid, 4);
}
