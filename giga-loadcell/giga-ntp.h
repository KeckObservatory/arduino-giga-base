#pragma once

#include <Arduino.h>

#include <Embedded_Template_Library.h>
#include <etl/string.h>
#include <etl/string_utilities.h>
#include <etl/to_string.h>

#include <Udp.h>

// Define this to 1 for debug messaging
#define DEBUG_NTP 1

// (70years * 365days/year + 17leapyears) * 86400secs/day
#define UNIX_EPOCH_OFFSET            2208988800UL
#define NTP_PACKET_SIZE              48
#define NTP_DEFAULT_LOCAL_PORT       1337
#define NTP_DEFAULT_UPDATE_INTERVAL  60000
#define NTP_DEFAULT_SERVER           "pool.ntp.org"

class GigaNTPClient {
private:
   UDP* _udp;
   bool _udpSetup = false;

   const char* _poolServerName     = NTP_DEFAULT_SERVER;  // Default time server
   IPAddress _poolServerIP;
   uint16_t _port                  = NTP_DEFAULT_LOCAL_PORT;
   int32_t _timeOffset             = 0;

   uint32_t _updateInterval        = NTP_DEFAULT_UPDATE_INTERVAL;  // In ms

   //uint32_t _currentSeconds        = 0; 
   //uint32_t _currentFracSecondsD   = 0;
   //uint32_t _currentFracOrigin     = 0;
   //float _currentFracSeconds       = 0.0;

   //uint32_t _lastEpochKnown        = 0;  // Overflows in 70 minutes of micros()
   //uint32_t _lastEpochSeconds      = 0;  // In s
   //uint32_t _lastEpochMicros       = 0;  // In usec
   
   uint32_t _lastNTPMicros         = 0;  // Time last epoch was acquired from NTP server
   uint64_t _lastNTPEpoch          = 0;  // Time since 1/1/1900 in usec

   uint32_t _lastUpdate            = 0;  // In ms
   uint32_t _lastRequest           = 0;  // In ms

   enum class State {
        uninitialized,
        idle,
        send_request,
        wait_response,
    } _state = State::uninitialized;

   byte _packetBuffer[NTP_PACKET_SIZE];

   void sendNTPPacket();

public:
   GigaNTPClient(UDP& udp);
   GigaNTPClient(UDP& udp, int32_t timeOffset);
   GigaNTPClient(UDP& udp, const char* poolServerName);
   GigaNTPClient(UDP& udp, const char* poolServerName, int32_t timeOffset);
   GigaNTPClient(UDP& udp, const char* poolServerName, int32_t timeOffset, uint32_t updateInterval);
   GigaNTPClient(UDP& udp, IPAddress poolServerIP);
   GigaNTPClient(UDP& udp, IPAddress poolServerIP, int32_t timeOffset);
   GigaNTPClient(UDP& udp, IPAddress poolServerIP, int32_t timeOffset, uint32_t updateInterval);

   /**
    * Set time server name
    *
    * @param poolServerName
    */
   void setPoolServerName(const char* poolServerName);

   /**
    * Set random local port
    */
   void setRandomPort(uint16_t minValue = 49152, uint16_t maxValue = 65535);

   /**
    * Starts the underlying UDP client with the default local port
    */
   void begin();

   /**
    * Starts the underlying UDP client with the specified local port
    */
   void begin(uint16_t port);

   /**
    * This should be called in the main loop of your application. By default an update from the NTP Server is only
    * made every 60 seconds. This can be configured in the GigaNTPClient constructor.
    *
    * @return true on success, false on failure
    */
   bool update();

   /**
    * This will force the update from the NTP Server.
    *
    * @return true on success, false on failure
    */
   bool forceUpdate();

   /**
    * This allows to check if the GigaNTPClient successfully received a NTP packet and set the time.
    *
    * @return true if time has been set, else false
    */
   bool isTimeSet() const;

   int getDay() const;
   int getHours() const;
   int getMinutes() const;
   int getSeconds() const;
   uint32_t getEpochMicros() const;

   /**
    * Changes the time offset. Useful for changing timezones dynamically
    */
   void setTimeOffset(int16_t timeOffset);

   /**
    * Set the update interval to another frequency. E.g. useful when the
    * timeOffset should not be set in the constructor
    */
   void setUpdateInterval(uint32_t updateInterval);

   /**
    * @return time formatted like `hh:mm:ss`
    */
   String getFormattedTime() const;

   void printFormattedTime();

   /**
    * @return time in seconds since Jan. 1, 1970
    */
   //unsigned long getEpochTime() const;

   /**
    * @return time in microseconds since Jan. 1, 1900
    */
   double getEpochTimeF() const;

   /**
    * Stops the underlying UDP client
    */
   void end();
};