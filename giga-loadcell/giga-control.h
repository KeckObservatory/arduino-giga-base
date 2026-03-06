/*
 * giga-control.h: implementation of a control interface 
 *
 */

#ifndef GIGA_CONTROL_H_
#define GIGA_CONTROL_H_

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <Arduino.h>

#include "timing.h"
#include "giga-config.h"

// Define the messaging start/stop characters for the control interface
#define CONTROL_MSG_STX  0x02
#define CONTROL_MSG_ETX  0x03

class GigaControl {

  private:
  	// Hold a reference to the config subsystem
	  GigaConfig& config;

  public:

    enum commands : uint8_t {
      CONTROL_CMD_ECHO = 0,  // Echo back the sent message 
      CONTROL_CMD_RESET      // Restart the Arduino
    };

    enum rc : uint8_t {
        CONTROL_NO_ERROR = 0,
        CONTROL_INVALID_COMMAND,
    };

    GigaControl(GigaConfig& the_config) : config(the_config) {}

    GigaControl::rc setup();
    void loop();

};

#endif