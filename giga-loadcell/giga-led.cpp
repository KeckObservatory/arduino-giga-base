/*
 * giga-led.cpp: implementation of RGB LED control for the Giga R1 board
 *
 */

#define GIGA_LED_CPP_

#include "giga-led.h"

/******************************************************************************************************************************
 * @brief Initialize the GPIO digital outputs for use as LED drivers.
 ******************************************************************************************************************************/
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

/******************************************************************************************************************************
 * @brief Turn off all LED outputs.
 ******************************************************************************************************************************/
void GigaLED::clear() {
  (GPIOI->ODR) |= (1 << 12);
  (GPIOJ->ODR) |= (1 << 13);
  (GPIOE->ODR) |= (1 << 3);
}

/******************************************************************************************************************************
 * @brief Set the red LED output.
 *
 * @param[in]  on                  Turn the LED off or on.
 ******************************************************************************************************************************/
void GigaLED::r(bool on) {

  if (on) {
    (GPIOI->ODR) &= ~(1 << 12);
  } else {
    (GPIOI->ODR) |=  (1 << 12);
  }
}

/******************************************************************************************************************************
 * @brief Set the green LED output.
 *
 * @param[in]  on                  Turn the LED off or on.
 ******************************************************************************************************************************/
void GigaLED::g(bool on) {

  if (on) {
    (GPIOJ->ODR) &= ~(1 << 13);
  } else {
    (GPIOJ->ODR) |=  (1 << 13);
  }
}

/******************************************************************************************************************************
 * @brief Set the blue LED output.
 *
 * @param[in]  on                  Turn the LED off or on.
 ******************************************************************************************************************************/
void GigaLED::b(bool on) {

  if (on) {
    (GPIOE->ODR) &= ~(1 << 3);
  } else {
    (GPIOE->ODR) |=  (1 << 3);
  }
}

/******************************************************************************************************************************
 * @brief Set the red led output while turning off the other two.
 *
 * @param[in]  on                  Turn the red LED off or on.
 ******************************************************************************************************************************/
void GigaLED::red(bool on) {

  r(on);
  g(false);
  b(false);
}

/******************************************************************************************************************************
 * @brief Set the green led output while turning off the other two.
 *
 * @param[in]  on                  Turn the green LED off or on.
 ******************************************************************************************************************************/
void GigaLED::green(bool on) {

  r(false);
  g(on);
  b(false);
}

/******************************************************************************************************************************
 * @brief Set the blue led output while turning off the other two.
 *
 * @param[in]  on                  Turn the green LED off or on.
 ******************************************************************************************************************************/
void GigaLED::blue(bool on) {

  r(false);
  g(false);
  b(on);
}

/******************************************************************************************************************************
 * @brief Set the red+green (amber) led outputs while turning off the blue.
 *
 * @param[in]  on                  Turn the red and green LEDs off or on.
 ******************************************************************************************************************************/
void GigaLED::amber(bool on) {

  r(on);
  g(on);
  b(false);
}

/******************************************************************************************************************************
 * @brief Set the red+blue (magenta) led outputs while turning off the blue.
 *
 * @param[in]  on                  Turn the red and blue LEDs off or on.
 ******************************************************************************************************************************/
void GigaLED::magenta(bool on) {

  r(on);
  g(false);
  b(on);
}

/******************************************************************************************************************************
 * @brief Wrapper around the heartbeat method to use only green.
 ******************************************************************************************************************************/
void GigaLED::heartbeat() {

  // Call the heartbeat feature, bypassing the warning flag
  heartbeat(false);
}

/******************************************************************************************************************************
 * @brief Use the LEDs as a heartbeat feature with a timer to dictate on/off cycle.
 *
 * @param[in]  warning             True = heartbeat with an amber light, false = heartbeat with a green light.
 ******************************************************************************************************************************/
void GigaLED::heartbeat(bool warning) {

  // Use the green part of the RGB LED as a heartbeat 
  if (heartbeatTimer.done()) {
    heartbeatTimer.resume();

    heartbeatToggle = !heartbeatToggle;

    if (warning) {
      amber(heartbeatToggle);
    } else { 
      green(heartbeatToggle);
    }
  }

}

/******************************************************************************************************************************
 * @brief Use the red LED as a panic/failure indicator.
 ******************************************************************************************************************************/
void GigaLED::panic() {

  // Use the red part of the RGB LED as a panic indicator 
  if (panicTimer.done()) {
    panicTimer.resume();

    panicToggle = !panicToggle;

    red(panicToggle);
  }
}

/******************************************************************************************************************************
 * @brief Display all the LED colors in sequence.
 ******************************************************************************************************************************/
void GigaLED::test_loop() {

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
