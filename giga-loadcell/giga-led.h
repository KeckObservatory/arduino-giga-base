/*
 * giga-led.h: implementation of RGB LED control for the Giga R1 board 
 *
 */

#ifndef GIGA_LED_H_
#define GIGA_LED_H_

#include <stdint.h>
#include <stdbool.h>
#include "timing.h"

#define HEARTBEAT_INTERVAL 500 // ms

class GigaLED {

  private:
    bool heartbeatToggle;
    Timer heartbeatTimer;

  public:
    GigaLED() : heartbeatTimer(HEARTBEAT_INTERVAL) {}

    void setup();

    void clear(void);
    void red(bool on);
    void green(bool on);
    void blue(bool on);
    void amber(bool on);
    void magenta(bool on);

    void heartbeat(void);
    void test_loop(void);

};

#endif