/*
 Chat Server

 A simple server that distributes any incoming messages to all
 connected clients.  To use, telnet to your device's IP address and type.
 You can see the client's input in the serial monitor as well.
 Using an Arduino WIZnet Ethernet shield.

 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13

 created 18 Dec 2009
 by David A. Mellis
 modified 9 Apr 2012
 by Tom Igoe

 */

#include <SPI.h>
#include <Ethernet.h>

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network.
// gateway and subnet are optional:
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEE };
IPAddress ip(10, 77, 0, 210);
IPAddress myDns(8, 8, 8, 8);
IPAddress gateway(10, 77, 0, 1);
IPAddress subnet(255, 255, 0, 0);


// telnet defaults to port 23
EthernetServer server(23);
EthernetClient clients[8];
bool alreadyConnected = false; // whether or not the client was connected previously

int Delay = 100;

void setup() {

  pinMode(86, OUTPUT);
  pinMode(87, OUTPUT);
  pinMode(88, OUTPUT);

  // You can use Ethernet.init(pin) to configure the CS pin
  Ethernet.init(10);  // Most Arduino shields
  //Ethernet.init(5);   // MKR ETH Shield
  //Ethernet.init(0);   // Teensy 2.0
  //Ethernet.init(20);  // Teensy++ 2.0
  //Ethernet.init(15);  // ESP8266 with Adafruit FeatherWing Ethernet
  //Ethernet.init(33);  // ESP32 with Adafruit FeatherWing Ethernet

  // initialize the Ethernet device
  Ethernet.begin(mac, ip, myDns, gateway, subnet);

  // Open serial communications and wait for port to open:
  //Serial.begin(9600);
  // while (!Serial) {
  //  ; // wait for serial port to connect. Needed for native USB port only
  //}


  // Open serial communications and wait for port to open:
  Serial1.begin(4800);
   while (!Serial1) {
    ; // wait for RS485 serial port to connect
  }




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
}

void led_loop() {
    
  (GPIOI->ODR) = 0x0000;
  delay( Delay );

  (GPIOI->ODR) = 0x1000;
  delay( Delay );

  //********************
  //********************

  (GPIOJ->ODR) = 0x0000;
  delay( Delay);

  (GPIOJ->ODR) = 0x2000;
  delay( Delay );

  //********************
  //********************

  (GPIOE->ODR) = 0x0000;
  delay( Delay );

  (GPIOE->ODR) = 0x0008;
  delay( Delay );

}

void loop() {
  int temp;
  char strBuf[50];

  // wait for a new client:
  EthernetClient client = server.available();

  // when the client sends the first byte, say hello:
  if (client) {

    //led_loop();

    if (!alreadyConnected) {
      // clear out the input buffer:
      client.flush();
//      Serial.println("We have a new client");
      client.println("Hello, client!");
      alreadyConnected = true;
    }

    if (client.available() > 0) {

    if(Serial1.available())
      {
        temp=Serial1.read();
        if(temp == 0xAA){
          client.print("\n");
        } 

      sprintf(strBuf, "%02X ", temp);
      client.print(strBuf);
      }

      // read the bytes incoming from the client:
      //char thisChar = client.read();
      // echo the bytes back to the client:
      //server.write(thisChar);
      // echo the bytes to the server as well:
//      Serial.write(thisChar);
    } else {
      alreadyConnected = false;
    }
  }
}




void loop2() {
  // check for any new client connecting, and say hello (before any incoming data)
  EthernetClient newClient = server.accept();
  if (newClient) {
    for (byte i=0; i < 8; i++) {
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



  // stop any clients which disconnect
  for (byte i=0; i < 8; i++) {
    if (clients[i] && !clients[i].connected()) {
      clients[i].stop();
    } 
    
    if (clients[i] && clients[i].connected()) {
      //led_loop();
    }
  }
}
