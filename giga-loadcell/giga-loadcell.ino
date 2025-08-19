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

Timer client_message_timer(100);

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

  // Setup RGB LED subsystem
  led.setup();

  // Setup the storage interface (USB device)
  SerialUSB.println(">>> Init: storage.");
  storage.setup();

  // Setup the configuration subsystem
  SerialUSB.println(">>> Init: registry.");
  config.setup();
  config.registry_load();

  // You can use Ethernet.init(pin) to configure the CS pin
  SerialUSB.println(">>> Init: ethernet.");
  ethernet.setup();

  // Setup the load cell interface 
  // CRITICAL NOTE: This must be done _after_ the Ethernet device setup due to some not-yet-understood
  // conflict between the devices!
  SerialUSB.println(">>> Init: load cell.");
  loadcell.setup();

  // Start the timer for emitting messages back to the client(s)
  client_message_timer.start();

  SerialUSB.println(">>> Initialization complete.");
}



char client_buffer[64];
uint32_t loop_count = 0;

void loop() {

  led.heartbeat(!loadcell.connected);
  loadcell.loop();
  ethernet.loop();

  // Once a second emit the device status
  if (client_message_timer.done()) {

    client_message_timer.resume();
    loop_count++;

    // Build the outbound message
    sprintf(client_buffer, "%08lX;%d;%0lX;%li\n", loop_count, loadcell.connected, loadcell.load, loadcell.load);
  
    ethernet.send_all(client_buffer);
  }

}