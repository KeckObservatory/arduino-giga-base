/**
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
      _lastEpochKnown = micros();

      // Combine four bytes to a uint32, this is NTP time (seconds since Jan 1 1900):  
      uint32_t ntp_seconds = (_packetBuffer[40] << 24) | (_packetBuffer[41] << 16) | (_packetBuffer[42] << 8) | _packetBuffer[43];

      // Convert NTP to Unix epoch
      _lastEpochSeconds = ntp_seconds - EPOCH_OFFSET;

      // The next 4 bytes are the fractional seconds
      uint32_t ntp_frac_seconds = (_packetBuffer[44] << 24) | (_packetBuffer[45] << 16) | (_packetBuffer[46] << 8) | _packetBuffer[47];

      // Convert fractional seconds to integer microseconds
      _lastEpochMicros = trunc((float(ntp_frac_seconds) / pow(2,32)) * 10e6);

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

unsigned long GigaNTPClient::getEpochTime() const {
  return _timeOffset +                      // User offset
         _lastEpochSeconds +                // Epoch returned by the NTP server
         ((millis() - _lastUpdate) / 1000); // Time since last update
}

int GigaNTPClient::getDay() const {
  return (((getEpochTime()  / 86400L) + 4 ) % 7); //0 is Sunday
}
int GigaNTPClient::getHours() const {
  return ((getEpochTime()  % 86400L) / 3600);
}
int GigaNTPClient::getMinutes() const {
  return ((getEpochTime() % 3600) / 60);
}
int GigaNTPClient::getSeconds() const {
  return (getEpochTime() % 60);
}
uint32_t GigaNTPClient::getEpochMicros() const {
  // Compute how many microseconds since the last whole second
  uint32_t t = (micros() - _lastEpochKnown) % 60000;

  return _lastEpochMicros + t;  
}

 

String GigaNTPClient::getFormattedTime() const {
  char temp_str[32];

  uint32_t rawTime = getEpochTime();
  uint32_t hours = (rawTime % 86400L) / 3600;
  uint32_t minutes = (rawTime % 3600) / 60;
  uint32_t seconds = rawTime % 60;
  uint32_t rawEpochMicros = getEpochMicros();
  float epochMicros =  rawEpochMicros / 10e6;

  if (seconds < 10) {
    sprintf(temp_str, "%02lu:%02lu:0%.6f", hours, minutes, seconds + epochMicros);
  } else {
    sprintf(temp_str, "%02lu:%02lu:%.6f", hours, minutes, seconds + epochMicros);
  }

  // Encapsulate as a String and return
  String time_str = temp_str;
  return time_str;
}

void GigaNTPClient::printFormattedTime() {
  char temp_str[32];

  uint32_t rawTime = getEpochTime();
  uint32_t hours = (rawTime % 86400L) / 3600;
  uint32_t minutes = (rawTime % 3600) / 60;
  uint32_t seconds = rawTime % 60;
  float epochMicros = getEpochMicros() / 10e6;

  if (seconds < 10) {
    sprintf(temp_str, "%02lu:%02lu:0%.6f", hours, minutes, seconds + epochMicros);
  } else {
    sprintf(temp_str, "%02lu:%02lu:%.6f", hours, minutes, seconds + epochMicros);
  }
  
  SerialUSB.println(temp_str);
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