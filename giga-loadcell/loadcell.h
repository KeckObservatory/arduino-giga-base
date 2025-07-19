/*
 * loadcell.h 
 *
 */

#ifndef LOADCELL_H_
#define LOADCELL_H_

#include <stdint.h>
#include <stdbool.h>

#define LOADCELL_BAUD_RATE 4800

class Loadcell {

  public:
    void setup(void);

};

#endif