/*
 * giga-ethernet.h: implementation of an ethernet interface for the Giga R1 board 
 *
 */

#ifndef GIGA_ETHERNET_H_
#define GIGA_ETHERNET_H_

#include <stdint.h>
#include <stdbool.h>
#include "timing.h"

class GigaEthernet {

  private:

  public:
    GigaEthernet() {}

    void setup();
    void clear();

};

#endif