#include "etl/string.h"
#include "etl/unordered_map.h"
/*
 * giga-config.c: implementation of a configuration module for the Giga R1
 *                supports reading a settings file from a USB filesystem 
 */

#define GIGA_CONFIG_C_

#include "giga-config.h"

const char GigaConfig::config_ini_filename[] = CONFIG_INI_FILENAME;


GigaConfig::~GigaConfig() {}

void GigaConfig::setup() {

  // Allocate a buffer on the stack for containing the entire config file contents for processing
  char buf[MAX_SIZE_CONFIG_INI];
  bzero(buf, MAX_SIZE_CONFIG_INI);

  etl::string<MAX_LENGTH_KEY> s1 = registry_network_ip;
  etl::string<MAX_LENGTH_VAL> s2 = "10.77.0.210";
  auto s1s2 = etl::make_pair(s1, s2);

  etl::string<MAX_LENGTH_KEY> s3 = registry_network_netmask;
  etl::string<MAX_LENGTH_VAL> s4 = "255.255.0.0";
  auto s3s4 = etl::make_pair(s3, s4);

  registry.insert(s1s2);
  registry.insert(s3s4);

  for (const std::pair<etl::string<MAX_LENGTH_KEY>, etl::string<MAX_LENGTH_VAL>>& n : registry) {
    Serial.println(n.first.c_str());
    Serial.println(n.second.c_str());
  }

  // Check for the INI file on the USB disk
  // Open the 




}