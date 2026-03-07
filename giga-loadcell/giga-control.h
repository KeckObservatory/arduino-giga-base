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

#define REBOOT_COUNTDOWN 1000 // ms

class GigaControl {

  private:
  	// Hold a reference to the config subsystem
	  GigaConfig& config;

    bool response_ready;
    uint8_t response_client_id;

    bool reboot_flag;
    Timer rebootTimer;

  public:

    enum commands : uint8_t {
      CONTROL_CMD_ECHO = 0,  // Echo back the sent message 
      CONTROL_CMD_RESET      // Restart the Arduino
    };

    enum rc : uint8_t {
        CONTROL_NO_ERROR = 0,
        CONTROL_INVALID_COMMAND,
        CONTROL_RESPONSE_NONE,
        CONTROL_RESPONSE_READY,
    };

    GigaControl(GigaConfig& the_config) : config(the_config), rebootTimer(REBOOT_COUNTDOWN) {}

    GigaControl::rc setup();
    void loop();
    void command(uint8_t client_id, char char_received);
    GigaControl::rc get_response(uint8_t* client_id, char* buf);

};

#endif