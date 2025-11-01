/*
 * loadcell.h 
 *
 */

#ifndef LOADCELL_H_
#define LOADCELL_H_

#include <stdint.h>
#include <stdbool.h>

#include <Arduino.h>

#include "timing.h"
#include "giga-config.h"

#define LOADCELL_TIMEOUT     1000 // ms
#define LOADCELL_BAUD_RATE   4800
#define LOADCELL_PREFIX_CHAR 0xAA
#define LOADCELL_MAX_BUFFER  5
#define LOADCELL_UNLOADED    0

class Loadcell {

  private:
    // Hold a reference to the config subsystem
	  GigaConfig& config;

    Timer timeout;
    
    // Storage for last values recieved
    uint8_t buffer[LOADCELL_MAX_BUFFER];

    // Properties of the load cell calibration
    float cal_slope;
    float cal_const;

  public:
    bool connected;
    uint32_t sn;
    int32_t load;
    uint32_t char_count;
    float kg;

    Loadcell(GigaConfig& the_config) : config(the_config),timeout(LOADCELL_TIMEOUT), sn(0) {}
    void setup(void);
    void loop(void);

};

#endif