/*
 * giga-led.cpp: implementation of RGB LED control for the Giga R1 board
 *
 */

#include <Arduino.h>
#include "giga-led.h"

void GigaLED::setup() {

  heartbeatToggle = false;

  // Derived from RM0399 rev 4 page 587 regarding GPIO registers
  // Tutorial: https://controllerstech.com/stm32-gpio-output-config-using-registers/
  // rm0399-stm32h745755-and-stm32h747757-advanced-armbased-32bit-mcus-stmicroelectronics.pdf

  // Red LED on PI_12
  GPIOI->MODER   |=   (1<<24);             // pin PI12 (bits 25:24) as Output (01)
  GPIOI->OTYPER  &=  ~(1<<12);             // bit 12=0 --> Output push pull
  GPIOI->OSPEEDR |=   (1<<25);             // pin PI12 (bits 25:24) as Fast Speed (1:0)
  GPIOI->PUPDR   &= ~((1<<25) | (1<<24));  // pin PI12 (bits 25:24) are 0:0 --> no pull up or pulldown

  // Green LED on PJ_13
  GPIOJ->MODER   |=   (1<<26);             // pin PJ13 (bits 27:26) as Output (01)
  GPIOJ->OTYPER  &=  ~(1<<13);             // bit 13=0 --> Output push pull
  GPIOJ->OSPEEDR |=   (1<<27);             // pin PJ13 (bits 27:26) as Fast Speed (1:0)
  GPIOJ->PUPDR   &= ~((1<<27) | (1<<26));  // pin PJ13 (bits 27:26) are 0:0 --> no pull up or pulldown

  // Blue LED on PE_3
  GPIOE->MODER   |=   (1<<6);              // pin PE3 (bits 7:6) as Output (01)
  GPIOE->OTYPER  &=  ~(1<<3);              // bit 3=0 --> Output push pull
  GPIOE->OSPEEDR |=   (1<<7);              // pin PE3 (bits 7:6) as Fast Speed (1:0)
  GPIOE->PUPDR   &= ~((1<<7) | (1<<6));    // pin PE3 (bits 7:6) are 0:0 --> no pull up or pulldown

  // Start with LEDs off
  clear();
}

void GigaLED::clear() {
  (GPIOI->ODR) |= (1 << 12);
  (GPIOJ->ODR) |= (1 << 13);
  (GPIOE->ODR) |= (1 << 3);
}


void GigaLED::red(bool on) {

  if (on) {
    (GPIOI->ODR) &= ~(1 << 12);
  } else {
    (GPIOI->ODR) |=  (1 << 12);
  }
}

void GigaLED::green(bool on) {

  if (on) {
    (GPIOJ->ODR) &= ~(1 << 13);
  } else {
    (GPIOJ->ODR) |=  (1 << 13);
  }
}

void GigaLED::blue(bool on) {

  if (on) {
    (GPIOE->ODR) &= ~(1 << 3);
  } else {
    (GPIOE->ODR) |=  (1 << 3);
  }
}

void GigaLED::amber(bool on) {

  red(on);
  green(on);
}

void GigaLED::magenta(bool on) {

  blue(on);
  red(on);
}

void GigaLED::heartbeat(void) {

  // Use the green part of the RGB LED as a heartbeat 
  if (heartbeatTimer.done()) {
    heartbeatTimer.resume();

    heartbeatToggle = !heartbeatToggle;
    green(heartbeatToggle);
  }

}

void GigaLED::test_loop(void) {

  uint32_t led_delay = 500;

  red(true);
  delay( led_delay );

  red(false);
  delay( led_delay );

  //********************
  amber(true);
  delay( led_delay );

  amber(false);
  delay( led_delay );

  //********************
  magenta(true);
  delay( led_delay );

  magenta(false);
  delay( led_delay );
}









