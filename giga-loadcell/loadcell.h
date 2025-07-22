/*
 * loadcell.h 
 *
 */

#ifndef LOADCELL_H_
#define LOADCELL_H_

#include <stdint.h>
#include <stdbool.h>
#include "timing.h"

#define LOADCELL_TIMEOUT     1000 // ms
#define LOADCELL_BAUD_RATE   4800
#define LOADCELL_PREFIX_CHAR 0xAA
#define LOADCELL_MAX_BUFFER  5
#define LOADCELL_UNLOADED    0

class Loadcell {

  private:
    Timer timeout;
    
    // Storage for last values recieved
    uint8_t buffer[LOADCELL_MAX_BUFFER];

  public:
    bool connected;
    int32_t load;
    uint32_t char_count;

    Loadcell() : timeout(LOADCELL_TIMEOUT) {}
    void setup(void);
    void loop(void);

};

#endif