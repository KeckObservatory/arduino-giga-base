#include "etl/string.h"
#include "etl/unordered_map.h"
/*
 * giga-config.c: implementation of a configuration module for the Giga R1
 *                supports reading a settings file from a USB filesystem 
 */

#define GIGA_CONFIG_C_

#include "giga-config.h"

GigaConfig::~GigaConfig() {
}

void GigaConfig::setup() {

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


}