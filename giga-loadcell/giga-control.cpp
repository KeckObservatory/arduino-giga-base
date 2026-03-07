/*
 * giga-control.cpp: implementation of a control interface
 *
 */

#define GIGA_CONTROL_CPP_

#include "giga-control.h"

/******************************************************************************************************************************
 * @brief Setup the ethernet subsystem.
 ******************************************************************************************************************************/
GigaControl::rc GigaControl::setup() {

  char message_text[128];

  sprintf(message_text, "[CTL] Starting control processor.");
  SerialUSB.println(message_text);
 
  response_ready = false;
  reboot_flag = false;

  // Return success
  return GigaControl::rc::CONTROL_NO_ERROR;
}

/******************************************************************************************************************************
 * @brief Run the control processing loop.
 ******************************************************************************************************************************/
void GigaControl::loop() {

  char message_text[128];
  //char client_buffer[128] = {0};

  if (reboot_flag && rebootTimer.done()) {
    reboot_flag = false;

    // Get a reference to the watchdog timer
    mbed::Watchdog &watchdog = mbed::Watchdog::get_instance();

    sprintf(message_text, "[CTL] Rebooting now!");
    SerialUSB.println(message_text);

    watchdog.start(100); // Force watchdog to timeout in 100 ms from now
  }

}



void GigaControl::command(uint8_t client_id, char char_received) {

  char message_text[128];

  if (char_received == 'r') {
    response_ready = true;
    response_client_id = client_id;

    reboot_flag = true;
    rebootTimer.start();

    sprintf(message_text, "[CTL] Reboot requested.");
    SerialUSB.println(message_text);
  }

}


GigaControl::rc GigaControl::get_response(uint8_t* client_id, char* buf) {

  if (response_ready) {
    bcopy("ACK\n", buf, 4);

    *client_id = response_client_id;
    response_ready = false;

    return GigaControl::rc::CONTROL_RESPONSE_READY;
  }
  else {
    return GigaControl::rc::CONTROL_RESPONSE_NONE;
  }
}








