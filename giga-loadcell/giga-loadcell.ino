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

#include "giga-led.h"  
#include "giga-ethernet.h"  
#include "giga-storage.h"
#include "giga-config.h"
#include "loadcell.h"

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network.
// gateway and subnet are optional:
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE };
IPAddress ip(10, 77, 0, 210);
IPAddress myDns(8, 8, 8, 8);
IPAddress gateway(10, 77, 0, 1);
IPAddress subnet(255, 255, 0, 0);


// telnet defaults to port 23
EthernetServer server(23);
EthernetClient clients[8];
bool alreadyConnected = false; // whether or not the client was connected previously

// Instances of the classes needed to run the LED, USB, and load cell 
GigaLED led;
GigaStorage storage;
GigaConfig config;
Loadcell loadcell;

Timer client_message_timer(100);


void setup() {

  // Hold off on setup for two seconds to allow the USB port to connect to the PC, if one is present
  delay(2000);
  SerialUSB.begin(115200);
  SerialUSB.println("");
  SerialUSB.println("--------------------------------------------------------------------------------");
  SerialUSB.println(">>> Load cell device initialization start.");

  // Setup RGB LED subsystem
  led.setup();

  // You can use Ethernet.init(pin) to configure the CS pin
  SerialUSB.println(">>> Init: ethernet.");
  Ethernet.init(10);  // 10 is the slave select pin

  // initialize the Ethernet device
  Ethernet.begin(mac, ip, myDns, gateway, subnet);

  // Setup the storage interface (USB device)
  SerialUSB.println(">>> Init: storage.");
  storage.setup();

  // Load the INI file into the buffer in the GigaConfig instance
  GigaStorage::rc rc = storage.load_file(config.config_ini_buffer, &config.config_ini_buffer_length, config.config_ini_filename);
  if (rc == GigaStorage::rc::NO_ERROR) {
    SerialUSB.println(">>> Init: Processing INI file.");

    // Parse the INI file and store the values in flash
    GigaConfig::rc rc_ini = config.load_ini();

    if (rc_ini != GigaConfig::rc::NO_ERROR) {
      SerialUSB.println(">>> Init: Failed to load INI file!");
    }

  }

  // Setup the configuration subsystem
  config.setup();

  // Setup the load cell interface 
  // CRITICAL NOTE: This must be done _after_ the Ethernet device setup due to some not-yet-understood
  // conflict between the devices!
  SerialUSB.println(">>> Init: load cell.");
  loadcell.setup();

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    while (true) {
      led.panic();
    }
  }

//  if (Ethernet.linkStatus() == LinkOFF) {
//    Serial.println("Ethernet cable is not connected.");
//  }

  // start listening for clients
  SerialUSB.println(">>> Init: TCP/IP server.");
  server.begin();

  // Start the timer for emitting messages back to the client(s)
  client_message_timer.start();

  SerialUSB.println(">>> Initialization complete.");
}



// A char to put incoming data from the ethernet that is ignored 
volatile char dummy;
char client_buffer[64];
uint32_t loop_count = 0;

void loop() {

  led.heartbeat(!loadcell.connected);
  loadcell.loop();

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

  // Once a second emit the device status
  if (client_message_timer.done()) {

    client_message_timer.resume();
    loop_count++;

    // Build the outbound message
    sprintf(client_buffer, "%08lX;%d;%0lX;%li\n", loop_count, loadcell.connected, loadcell.load, loadcell.load);
  
    for (byte i = 0; i < 8; i++) {
      if (clients[i] && clients[i].connected()) {
        // Send every connected client the latest load value
        clients[i].print(client_buffer);
      }
    }
  }

}