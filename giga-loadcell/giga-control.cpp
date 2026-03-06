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

  sprintf(message_text, "[CMD] Starting control processor.");
  SerialUSB.println(message_text);
 
  // Return success
  return GigaControl::rc::CONTROL_NO_ERROR;
}

/******************************************************************************************************************************
 * @brief Run the control processing loop.
 ******************************************************************************************************************************/
void GigaControl::loop() {

  char client_buffer[128] = {0};


}







