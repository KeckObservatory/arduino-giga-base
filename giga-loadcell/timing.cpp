/*
 * timing.h - implementation of a timer and stopwatch class
 *
 */

#define TIMING_CPP_

#include <Arduino.h>
#include "timing.h"

/* ************************************************************************** */
/* TIMER                                                                      */
/* ************************************************************************** */

/* Instantiate a timer, and start it */
Timer::Timer(uint64_t theInterval) {

  set(theInterval);
}

/* (Re)initialize a timer, and start it */
void Timer::set(uint64_t theInterval) {

  interval = theInterval;
  start();
}

/* Start a timer */
void Timer::start() {

  startTime = millis();
}

/* Returns true if the timer has expired */
bool Timer::done() {

  return (millis() >= (startTime + interval));
}

/* Resume (continue) a timer, retaining the base time */
void Timer::resume() {

  startTime += interval;
}

/* Expire a timer */
void Timer::expire() {

  startTime = 0;
}

/* Reports back the amount of time remaining before the timer is considered Done, in milliseconds */
uint64_t Timer::remaining() {

  uint64_t base = millis();

  if (base >= (startTime + interval))
    return 0;
  else
    return (startTime + interval) - base;
}

/* Reports back the amount of time elapsed on the timer, in milliseconds */
uint64_t Timer::elapsed() {

  return (millis() - startTime);
}

/* Returns true if this timer has been set at least once, ready to time */
bool Timer::wasInitialized() {

  return (interval != 0);
}


/* ************************************************************************** */
/* STOPWATCH                                                                  */
/* ************************************************************************** */

/* Clear a stop watch, making it ready to use */
void Stopwatch::clear() {

  running      = false;
  residualTime = 0;
}

/* Start new timing using a stop watch. Accumulated time is cleared */
void Stopwatch::start() {

  startTime    = millis();
  residualTime = 0;
  running      = true;
}

/* Continue a current timing using a stop watch. Accumulated time is not cleared */
void Stopwatch::resume() {

  /* If we are already running, this is nothing to do. */
  if (running)
    return;

  startTime = millis();
  running   = true;
}

/* Stop a running stop watch */
void Stopwatch::stop() {

  /* If we are running, grab the elapsed time currently on the watch, and add it
     to the residual time to keep it accumulated properly. */
  if (running)
    residualTime += (millis() - startTime);

  running = false;
}

/* Returns the total elapsed time on a stop watch, in milliseconds */
uint64_t Stopwatch::elapsed() {

  uint64_t et;

  if (running)
    et = residualTime + (millis() - startTime);
  else
    et = residualTime;

  return et;
}

