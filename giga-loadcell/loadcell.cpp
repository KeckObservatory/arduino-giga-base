/*
 * loadcell.cpp - implementation of a Sole Digital DRC-xT rope clamp load cell interface 
 *
 */

#define LOADCELL_CPP_

#include "loadcell.h"
#include <etl/to_arithmetic.h>

void Loadcell::setup() {
  int led = 13;

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

  // Setup the enable pin for the DFRobot 485 shield and turn it on
  pinMode(RS485_SHIELD_ENABLE, OUTPUT);
  set_485_shield_enable(true);

  // Flush the buffer before first use  
  Serial1.flush();

  // Init the state machine
  lc_state = LC_STATE_IDLE;

  // Start the timeout timer
  timeout.start();
  connected = false;

  char_count = 0;

  // Get the registry values for use with this load cell
  KVStringRC registry_sn = config.registry_get(registry_sensor_sn);
  KVStringRC registry_slope = config.registry_get(registry_cal_slope);
  KVStringRC registry_const = config.registry_get(registry_cal_const);
 
  // Convert from strings to numeric types
  sn = etl::to_arithmetic<uint32_t>(registry_sn.second, 10);
  cal_slope = etl::to_arithmetic<float>(registry_slope.second);
  cal_const = etl::to_arithmetic<float>(registry_const.second);
}

void Loadcell::loop() {

  char message_text[128];
  uint8_t i;
  int8_t count;
  uint8_t checksum;
  static uint8_t chars_since_good = 0;
  static uint32_t fault_count = 0;

  // Run the communications to the load cell device

  // All messages from the load cell arrive in a predictable format, 5 bytes at a time:
  //    0xAA        - start of message
  //    0x??        - bits 23-16 of load value
  //    0x??        - bits 15-8 of load value
  //    0x??        - bits 7-0 of load value
  //    0x??        - sum (checksum) of previous three bytes

  switch (lc_state) {

    case LC_STATE_IDLE:

      // Start off not connected
      connected = false;

      // Start the messaging timer
      timeout.start();

      // fall immediately into run state, no break needed
      lc_state = LC_STATE_RUN;

    case LC_STATE_RUN:
          
      // Accumulate characters into a rolling buffer
      count = Serial1.available();
      char_count += count;

      if (count > 0) {

        // Count the number of characters since we had a good value
        chars_since_good++;

        for (i = 0; i < count; i++) {

          uint8_t temp = Serial1.read();
        
          // Shift each char in the buffer to the left
          buffer[0] = buffer[1];
          buffer[1] = buffer[2];
          buffer[2] = buffer[3];
          buffer[3] = buffer[4];
          buffer[4] = temp;
      
          // Test the 0th byte, is it the start of message?
          //
          // And, have enough bytes come through since the last good value that we won't 
          // be fooled by a 0xAA appearing in the data randomly?
          if ((buffer[0] == LOADCELL_PREFIX_CHAR) && (chars_since_good > 4)) {

            // Verify the checksum, which will now be in position 4
            checksum = (buffer[1] + buffer[2] + buffer[3]) & 0x0000FF;
            if (checksum == buffer[4]) {

              // Load the values into the top 3 bytes of a 32 bit signed integer.  Then, shift it
              // to the right by 8 bits, to cause sign extension and get back our 24 bit signed int.
              load = ((buffer[1] << 24) + (buffer[2] << 16) + (buffer[3] << 8)) >> 8;

              // Convert the ADC value to kilograms
              kg = (cal_slope*load) + cal_const;
              
              // When a message is valid, we are connected, restart the timer!
              connected = true;
              timeout.start();

              // Reset the counter now that we have a good value
              chars_since_good = 0;
            }
          }
        }
      }

      // Look for timeout conditions
      if (timeout.done()) {
  
        // The load cell has timed out. It is possible that the RS485 shield is faulted. Toggle the enable to it
        // to try to clear the fault.
        lc_state = LC_STATE_FAULT;
      }

      break;

    case LC_STATE_FAULT:

      fault_count++;

      sprintf(message_text, "[LC] Load cell faulted! (%ld)", fault_count);
      SerialUSB.println(message_text);

      // Faulted, so we are not connected
      connected = false;

      // Start the reset process
      set_485_shield_enable(false);
      shield_reset_timer.start();      

      // Reset the shield
      lc_state = LC_STATE_RESET;
      break;

    case LC_STATE_RESET:

      // When reset is complete, turn the 485 enable back on
      if (shield_reset_timer.done()) {

        sprintf(message_text, "[LC] RS485 shield reset complete.");
        SerialUSB.println(message_text);

        set_485_shield_enable(true);

        // Back to run to start over again
        lc_state = LC_STATE_IDLE;

      }
      break;

  }
}

void Loadcell::set_485_shield_enable(bool enable) {

  // Turn on or off the 485 shield EN signal on I/O 2.  Signal is active LOW.
  // This enable only works when the shield is physically switched to "MANU"!
  if (enable) {
    digitalWrite(RS485_SHIELD_ENABLE, LOW);
  } else {
    digitalWrite(RS485_SHIELD_ENABLE, HIGH);
  }
}




