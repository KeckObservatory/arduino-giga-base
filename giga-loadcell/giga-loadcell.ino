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

GigaLED led;
Loadcell loadcell;

int Delay = 100;

void setup() {

  // Setup RGB LED subsystem
  led.setup();

  // Setup the load cell interface
  loadcell.setup();


  // You can use Ethernet.init(pin) to configure the CS pin
  Ethernet.init(10);  // 10 is the slave select pin

  // initialize the Ethernet device
  Ethernet.begin(mac, ip, myDns, gateway, subnet);


  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
  //  Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    while (true) {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }
//  if (Ethernet.linkStatus() == LinkOFF) {
//    Serial.println("Ethernet cable is not connected.");
//  }

  // start listening for clients
  server.begin();

//  Serial.print("Chat server address:");
//  Serial.println(Ethernet.localIP());

  //TimerSet(&heartbeat_timer, 500);
  //TimerStart(&heartbeat_timer);
}



// A char to put incoming data from the ethernet that is ignored 
volatile char dummy;

void loop() {

  led.heartbeat();


  // Check for any new client connecting
  EthernetClient newClient = server.accept();
  if (newClient) {
    for (byte i = 0; i < 8; i++) {
      if (!clients[i]) {
        newClient.print("Hello, client number: ");
        newClient.println(i);

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