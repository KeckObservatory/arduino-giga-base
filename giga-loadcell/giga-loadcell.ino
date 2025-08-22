/*
 giga-loadcell: implements an interface to a Sole Digital DRC-xT rope clamp load cell
 
 Hardware components:
    Arduino Giga R1 
    DFRobot Ethernet shield
    DFRobot RS-485 shield

 July 2025, Paul Richards - W. M. Keck Observatory
 */

#include <stdint.h>
#include <arduino.h>
#include <SPI.h>
#include <Ethernet.h>

#include "giga-types.h"
#include "giga-led.h"  
#include "giga-ethernet.h"  
#include "giga-storage.h"
#include "giga-config.h"
#include "loadcell.h"

// Instances of the classes needed to run the LED, USB, registry, network, and load cell 
GigaLED led;
GigaStorage storage;
GigaConfig config(storage);
GigaEthernet ethernet(config);
Loadcell loadcell;
Timer client_message_timer(100);  // 100ms between outbound messages (10Hz)

// The processor universal ID
uint8_t uid[12];

void setup() {

  // Hold off on setup for two seconds to allow the USB port to connect to the PC, if one is present
  delay(2000);
  SerialUSB.begin(115200);
  SerialUSB.println("");
  SerialUSB.println("--------------------------------------------------------------------------------");
  SerialUSB.println(">>> Load cell device initialization start.");

  auto ver = __cplusplus;
  SerialUSB.print(">>> Built with C++ version: ");  
  SerialUSB.println(ver);

  // Print the UID, for eventual correlation with calibrations
  char message_text[128] = {0};
  ethernet.GetUID(uid);
  sprintf(message_text, ">>> Processor UID = %02X%02X%02X%02X-%02X%02X%02X%02X-%02X%02X%02X%02X", 
    uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7], uid[8], uid[9], uid[10], uid[11]);
  SerialUSB.println(message_text);

  // Setup RGB LED subsystem
  led.setup();

  // Setup the storage interface (USB device)
  SerialUSB.println(">>> Init: storage.");
  storage.setup();

  // Setup the configuration subsystem
  SerialUSB.println(">>> Init: registry.");
  config.setup();
  GigaConfig::rc rc_registry = config.registry_load();

  // If the registry failed to load we cannot continue.
  if (rc_registry != GigaConfig::rc::NO_ERROR) {
    SerialUSB.println(">>> HALTING FOR FAILURE <<<");
    while (1) led.panic();  
  }

  // Setup the ethernet interface
  SerialUSB.println(">>> Init: ethernet.");
  GigaEthernet::rc_ethernet rc_ethernet = ethernet.setup();

  // If the ethernet hardware is missing or cable unplugged, we cannot continue.
  if (rc_ethernet != GigaEthernet::rc_ethernet::ETHER_NO_ERROR) {
    SerialUSB.println(">>> HALTING FOR FAILURE <<<");
    while (1) led.panic();  
  }

  // Setup the load cell interface 
  // CRITICAL NOTE: This must be done _after_ the Ethernet device setup due to some not-yet-understood
  // conflict between the devices!
  SerialUSB.println(">>> Init: load cell.");
  loadcell.setup();

  // Start the timer for emitting messages back to the client(s)
  client_message_timer.start();

  SerialUSB.println(">>> Initialization complete.");
}



char client_buffer[128] = {0};
uint32_t loop_count = 0;

void loop() {

  // Use the heartbeat green flashing to indicate the load cell is connected, and amber for disconnected
  led.heartbeat(!loadcell.connected);

  // Process the load cell incoming serial messages
  loadcell.loop();

  // Perform the ethernet connection management
  ethernet.loop();

  // Once a second emit the device status
  if (client_message_timer.done()) {

    client_message_timer.resume();
    loop_count++;

    // Build the outbound message.  Send the load as both hex values and base 10.
    sprintf(client_buffer, "%08lX;%d;%0lX;%li\n", loop_count, loadcell.connected, loadcell.load, loadcell.load);
  
    // Send the outbound message to all clients
    ethernet.send_all(client_buffer);
  }

}