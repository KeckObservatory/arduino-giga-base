/*
 * timing.h - implementation of a timer and stopwatch class
 *
 */

#ifndef TIMING_H_
#define TIMING_H_

#include <stdint.h>
#include <stdbool.h>

class Timer {

  private:
    uint64_t startTime;
    uint64_t interval;

  public:
    Timer(uint64_t theInterval);

    void set(uint64_t theInterval);
    void set(uint64_t minutes, uint64_t seconds, uint64_t milliseconds) { set((minutes * 60000) + (seconds * 1000) + milliseconds); }
    void start();
    bool done();
    void resume();
    void expire();
    uint64_t remaining();
    uint64_t elapsed();
    bool wasInitialized();

};

class Stopwatch {

  private:
    bool running;
    uint64_t startTime;
    uint64_t residualTime; /* Accumulated time on the stopwatch prior to the current timing cycle */

  public:
    void clear();
    void start();
    void resume();
    void stop();
    uint64_t elapsed();
    float elapsedSeconds() { return elapsed() / 1000; }
};

#endif
