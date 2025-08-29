/**
 * giga-ntp.cpp: implementation of a network time protocol client
 *
 * Adapted from https://github.com/arduino-libraries/NTPClient
 *
 *
 * The MIT License (MIT)
 * Copyright (c) 2015 by Fabrice Weinberg
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#define GIGA_NTP_C_

#include "giga-ntp.h"

GigaNTPClient::GigaNTPClient(UDP& udp) {
  _udp            = &udp;
}

GigaNTPClient::GigaNTPClient(UDP& udp, int32_t timeOffset) {
  _udp            = &udp;
  _timeOffset     = timeOffset;
}

GigaNTPClient::GigaNTPClient(UDP& udp, const char* poolServerName) {
  _udp            = &udp;
  _poolServerName = poolServerName;
}

GigaNTPClient::GigaNTPClient(UDP& udp, IPAddress poolServerIP) {
  _udp            = &udp;
  _poolServerIP   = poolServerIP;
  _poolServerName = NULL;
}

GigaNTPClient::GigaNTPClient(UDP& udp, const char* poolServerName, int32_t timeOffset) {
  _udp            = &udp;
  _timeOffset     = timeOffset;
  _poolServerName = poolServerName;
}

GigaNTPClient::GigaNTPClient(UDP& udp, IPAddress poolServerIP, int32_t timeOffset){
  _udp            = &udp;
  _timeOffset     = timeOffset;
  _poolServerIP   = poolServerIP;
  _poolServerName = NULL;
}

GigaNTPClient::GigaNTPClient(UDP& udp, const char* poolServerName, long timeOffset, uint32_t updateInterval) {
  _udp            = &udp;
  _timeOffset     = timeOffset;
  _poolServerName = poolServerName;
  _updateInterval = updateInterval;
}

GigaNTPClient::GigaNTPClient(UDP& udp, IPAddress poolServerIP, int32_t timeOffset, uint32_t updateInterval) {
  _udp            = &udp;
  _timeOffset     = timeOffset;
  _poolServerIP   = poolServerIP;
  _poolServerName = NULL;
  _updateInterval = updateInterval;
}

void GigaNTPClient::begin() {
  begin(NTP_DEFAULT_LOCAL_PORT);
}

void GigaNTPClient::begin(uint16_t port) {
  _port = port;

  _udp->begin(_port);

  _udpSetup = true;
}


bool GigaNTPClient::update() {

  char message[128];

  switch (_state) {

    case State::uninitialized: {
      _udp->begin(_port);
      _state = State::idle;

      // fall through -- we're all initialized now
    }

    case State::idle: {
      if ((millis() - _lastUpdate < _updateInterval)     // Update after _updateInterval
          && _lastUpdate != 0)                           // Update if there was no update yet.
        return false;

      _state = State::send_request;

      // fall through -- ready to send request now
    }

    case State::send_request: {
#ifdef DEBUG_NTP
      SerialUSB.println(F("Sending NTP request"));
#endif

      // flush any existing packets
      while(_udp->parsePacket() != 0)
        _udp->flush();

      sendNTPPacket();

      _lastRequest = millis();
      _state = State::wait_response;

      // fall through -- if we're lucky we may already receive a response

    case State::wait_response:
      if (!_udp->parsePacket()) {
        // no reply yet
        if (millis() - _lastRequest >= 1000) {
          // time out
#ifdef DEBUG_NTP
          SerialUSB.println(F("NTP reply timeout"));
#endif
          _state = State::idle;
        }
        return false;
      }

#ifdef DEBUG_NTP
      SerialUSB.println(F("NTP reply received"));
#endif

      // Got a reply
      _lastUpdate = _lastRequest;
      _udp->read(_packetBuffer, NTP_PACKET_SIZE);

      // Remember when we got this value
      _lastNTPMicros = micros();

      // How long did it take to make the request?
      //uint32_t ntp_request_time = ((millis() - _lastRequest) * 1000);

      // Bytes 40-43 are NTP time, seconds since Jan 1 1900  
      uint32_t ntp_seconds = (_packetBuffer[40] << 24) | (_packetBuffer[41] << 16) | (_packetBuffer[42] << 8) | _packetBuffer[43];

      // Add our time offset
      ntp_seconds += _timeOffset;

      // Convert to microseconds and store
      _lastNTPEpoch = ntp_seconds * 1e6;

      // Add the request trip time
      //_lastNTPEpoch += ntp_request_time;   // is this the right calculation?

      // Bytes 44-47 the fractional seconds
      uint32_t ntp_frac_seconds = (_packetBuffer[44] << 24) | (_packetBuffer[45] << 16) | (_packetBuffer[46] << 8) | _packetBuffer[47];

      // Convert fractional seconds to integer microseconds and add to the epoch value
      _lastNTPEpoch += trunc((double(ntp_frac_seconds) / pow(2,32)) * 1e6);

#ifdef NTP_RECV_XMIT_CALC
      // Calculate how much time was spent at the NTP server processing our request
      // Bytes 32-35, 36-39 are the NTP receive timestamp whole and fractional seconds
      uint32_t ntp_recv_seconds = (_packetBuffer[32] << 24) | (_packetBuffer[33] << 16) | (_packetBuffer[34] << 8) | _packetBuffer[35];
      ntp_recv_seconds += _timeOffset;  // Offset by our TZ so it can be compared
      uint64_t ntp_recv_usec = ntp_recv_seconds * 1e6;
      uint32_t ntp_recv_frac_seconds = (_packetBuffer[36] << 24) | (_packetBuffer[37] << 16) | (_packetBuffer[38] << 8) | _packetBuffer[39];
      ntp_recv_usec += trunc((double(ntp_recv_frac_seconds) / pow(2,32)) * 1e6);

      double recv_xmit_time = double(_lastNTPEpoch - ntp_recv_usec) / 1e6;
      sprintf(message, "recv_xmit_time: %0.6f", recv_xmit_time);
      SerialUSB.println(message);
#endif

      // Offset the Unix epoch to get to the more typical time standard
      _lastNTPEpoch -= (UNIX_EPOCH_OFFSET * 1e6);

#ifdef DEBUG_NTP
      //sprintf(message, "ntp_seconds: %lu  ntp_frac_seconds: %lu   _lastNTPEpoch=%llu", ntp_seconds, ntp_frac_seconds, _lastNTPEpoch);
      //SerialUSB.println(message);
#endif

      // Back to idle
      _state = State::idle;

      return true;  // return true after successful update
    }

    default: {
      _state = State::uninitialized;
    }

  }

  return false;
}

bool GigaNTPClient::forceUpdate() {
  // In contrast to NTPClient::update(), this function always sends a NTP
  // request and only returns when the whole operation completes (no matter
  // if it's a success or a failure because of a timeout).  In other words
  // this function is fully synchronous.  It will block until the whole
  // NTP operation completes.
  //
  // We could only make this function switch the state to State::send_request
  // to ensure a NTP request would happen with the next call to
  // NTPClient::update().  However, this would be an API change, users could
  // expect synchronous behaviour and even skip the calls to NTPClient::update()
  // completely relying only on this function for time updates.

  // ensure we're initialized
  if (_state == State::uninitialized) {
    _udp->begin(_port);
  }

  // At this point we can be in any state except for State::uninitialized.
  // Let's ignore that and switch right to State::send_request to send a
  // fresh NTP request.
  _state = State::send_request;

  while (true) {
    if (update()) {
      // time updated
      return true;
    } else if (_state != State::idle) {
      // still waiting for response
      delay(10);
    } else {
      // failure
      return false;
    }
  }
}

bool GigaNTPClient::isTimeSet() const {
  return (_lastUpdate != 0); // returns true if the time has been set, else false
}


double GigaNTPClient::getEpochTimeF() const {

  // How much time has elapsed since the last NTP sync?
  int64_t elapsed = micros() - _lastNTPMicros;

  // micros() will roll over back to 0 in ~70 hours, adjust it back by adding 2^32
  if (elapsed < 0) {
    elapsed += 0x100000000;
  }

  // Add that to what time it was at sync
  uint64_t now = _lastNTPEpoch + elapsed;

  // Convert back to seconds as a double precision float
  double now_f = double(now) / 1e6;

  return now_f;
}

int GigaNTPClient::getDay() const {
  uint32_t t = trunc(getEpochTimeF());
  return (((t / 86400L) + 4 ) % 7); //0 is Sunday
}
int GigaNTPClient::getHours() const {
  uint32_t t = trunc(getEpochTimeF());
  return ((t % 86400L) / 3600);
}
int GigaNTPClient::getMinutes() const {
  uint32_t t = trunc(getEpochTimeF());
  return ((t % 3600) / 60);
}
int GigaNTPClient::getSeconds() const {
  uint32_t t = trunc(getEpochTimeF());
  return (t % 60);
}



void GigaNTPClient::printFormattedTime() {
  char temp_str[32];
  static double rawTimePrev = 0;

  double rawTime = getEpochTimeF();
  
  double rawTimePrevDelta = rawTime - rawTimePrev;
  rawTimePrev = rawTime;

  uint32_t rawTimeT = trunc(rawTime);
  double rawTimeFrac = rawTime - rawTimeT;

  uint32_t hours = (rawTimeT % 86400L) / 3600;
  uint32_t minutes = (rawTimeT % 3600) / 60;
  uint32_t seconds = rawTimeT % 60;
  
  if (seconds < 10) {
    sprintf(temp_str, "%02lu:%02lu:0%.6f  +%0.6f", hours, minutes, seconds + rawTimeFrac, rawTimePrevDelta);
  } else {
    sprintf(temp_str, "%02lu:%02lu:%.6f  +%0.6f", hours, minutes, seconds + rawTimeFrac, rawTimePrevDelta);
  }
  
  SerialUSB.println(temp_str);
}
 

String GigaNTPClient::getFormattedTime() const {
  char temp_str[32];

  float rawTime = getEpochTimeF();
  uint32_t rawTimeT = trunc(rawTime);
  uint32_t rawTimeS = trunc(rawTime);

  float integral_part;
  float rawTimeFrac = modf(rawTime, &integral_part);

  uint32_t hours = (rawTimeT % 86400L) / 3600;
  uint32_t minutes = (rawTimeT % 3600) / 60;
  uint32_t seconds = rawTimeT % 60;
  
  if (seconds < 10) {
    sprintf(temp_str, "%02lu:%02lu:0%.6f", hours, minutes, seconds + rawTimeFrac);
  } else {
    sprintf(temp_str, "%02lu:%02lu:%.6f", hours, minutes, seconds + rawTimeFrac);
  }

  // Encapsulate as a String and return
  String time_str = temp_str;
  return time_str;
}

void GigaNTPClient::end() {
  _udp->stop();

  _udpSetup = false;
}

void GigaNTPClient::setTimeOffset(int16_t timeOffset) {
  _timeOffset     = timeOffset;
}

void GigaNTPClient::setUpdateInterval(uint32_t updateInterval) {
  _updateInterval = updateInterval;
}

void GigaNTPClient::setPoolServerName(const char* poolServerName) {
    _poolServerName = poolServerName;
}

void GigaNTPClient::sendNTPPacket() {

  // set all bytes in the buffer to 0
  memset(_packetBuffer, 0, NTP_PACKET_SIZE);

  // Initialize values needed to form NTP request
  _packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  _packetBuffer[1] = 0;     // Stratum, or type of clock
  _packetBuffer[2] = 6;     // Polling Interval
  _packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  _packetBuffer[12]  = 49;
  _packetBuffer[13]  = 0x4E;
  _packetBuffer[14]  = 49;
  _packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  if  (_poolServerName) {
    _udp->beginPacket(_poolServerName, 123);
  } else {
    _udp->beginPacket(_poolServerIP, 123);
  }
  _udp->write(_packetBuffer, NTP_PACKET_SIZE);
  _udp->endPacket();
}

void GigaNTPClient::setRandomPort(uint16_t minValue, uint16_t maxValue) {
  randomSeed(analogRead(0));
  _port = random(minValue, maxValue);
}