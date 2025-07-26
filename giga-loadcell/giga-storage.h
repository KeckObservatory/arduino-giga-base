/*
 * giga-storage.h: implementation of USB and flash storage for the Giga R1 board
 *
 */

#ifndef GIGA_STORAGE_H_
#define GIGA_STORAGE_H_

#include <stdint.h>
#include <stdbool.h>
#include "timing.h"

class GigaStorage {

  private:

  public:
    GigaStorage() {}

    void setup();
    void clear();

};

#endif