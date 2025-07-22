/*
 * loadcell.c - implementation of a Sole Digital DRC-xT rope clamp load cell interface 
 *
 */

#define LOADCELL_CPP_

#include <arduino.h>
#include <stdint.h>
#include <stdbool.h>
#include "loadcell.h"

void Loadcell::setup() {
  int led = 13;
  int EN = 2;

  // Zero out the incoming message buffer
  bzero(buffer, LOADCELL_MAX_BUFFER);

  // Initial "load" is unloaded
  load = LOADCELL_UNLOADED;

  // Open serial communications and wait for port to open
  Serial1.begin(LOADCELL_BAUD_RATE);

  while (!Serial1) {
    ; // wait for RS485 serial port to connect
  }

  pinMode(led, OUTPUT);
  digitalWrite(led, 1);

  pinMode(EN, OUTPUT);
  digitalWrite(EN, HIGH);

  // Flush the buffer before first use  
  Serial1.flush();


  // Start the timeout timer
  timeout.start();
  connected = false;

  char_count = 0;
}

void Loadcell::loop() {
  uint8_t i;
  int8_t count;
  uint8_t checksum;

  // Run the communications to the load cell device

  // All messages from the load cell arrive in a predictable format, 5 bytes at a time:
  //    0xAA        - start of message
  //    0x??        - bits 23-16 of load value
  //    0x??        - bits 15-8 of load value
  //    0x??        - bits 7-0 of load value
  //    0x??        - sum (checksum) of previous three bytes

  // Accumulate characters into a rolling buffer
  count = Serial1.available();
  char_count += count;

  if (count > 0) {

    for (i = 0; i < count; i++) {

      uint8_t temp = Serial1.read();
    
      // Shift each char in the buffer to the left
      buffer[0] = buffer[1];
      buffer[1] = buffer[2];
      buffer[2] = buffer[3];
      buffer[3] = buffer[4];
      buffer[4] = temp;
  
      // Test the 0th byte, is it the start of message?
      if (buffer[0] == LOADCELL_PREFIX_CHAR) {

        // Verify the checksum, which will now be in position 4
        checksum = (buffer[1] + buffer[2] + buffer[3]) & 0x0000FF;
        if (checksum == buffer[4]) {

          // Load the values into the top 3 bytes of a 32 bit signed integer.  Then, shift it
          // to the right by 8 bits, to cause sign extension and get back our 24 bit signed int.
          load = ((buffer[1] << 24) + (buffer[2] << 16) + (buffer[3] << 8)) >> 8;
          
          // When a message is valid, we are connected
          connected = true;
          timeout.start();
        }

      }

    }

  }

  // Look for timeout conditions
  if (timeout.done()) {

    connected = false;
  }


}




