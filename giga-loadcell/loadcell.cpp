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

  // Open serial communications and wait for port to open
  Serial1.begin(LOADCELL_BAUD_RATE);

  while (!Serial1) {
    ; // wait for RS485 serial port to connect
  }

}