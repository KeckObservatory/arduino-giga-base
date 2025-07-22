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
#define PANIC_INTERVAL     100 // ms

class GigaLED {

  private:
    bool heartbeatToggle;
    Timer heartbeatTimer;

    bool panicToggle;
    Timer panicTimer;

  public:
    GigaLED() : heartbeatTimer(HEARTBEAT_INTERVAL), panicTimer(PANIC_INTERVAL) {}

    void setup();

    void clear();

    void r(bool on);
    void g(bool on);
    void b(bool on);

    void red(bool on);
    void green(bool on);
    void blue(bool on);
    void amber(bool on);
    void magenta(bool on);

    void heartbeat();
    void heartbeat(bool warning);
    void panic();
    void test_loop();

};

#endif