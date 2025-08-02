/*
 * giga-config.h: implementation of a configuration module for the Giga R1
 *                supports reading a settings file from a USB filesystem 
 *
 */

#ifndef GIGA_CONFIG_H_
#define GIGA_CONFIG_H_

#include <stdint.h>
#include <string.h>
#include <Embedded_Template_Library.h>
#include <etl/unordered_map.h>
#include <etl/string.h>
#include <Arduino_USBHostMbed5.h>
#include <IPAddress.h>

// Max sizes for various elements
#define MAX_LENGTH_KEY 64
#define MAX_LENGTH_VAL 64
#define MAX_SIZE_MAP   64

// Define the configuration filenames
const char config_ini_filename[] = "config.ini";
const char config_ini_saved_filename[] = "config.ini.S";

// Define the network registry keys that ought to be present in config.ini
const char registry_network_ip[] = "network.ip";
const char registry_network_netmask[] = "network.netmask";
const char registry_network_gateway[] = "network.gateway";
const char registry_network_dns[] = "network.dns";



class GigaConfig {
private:
  etl::unordered_map<etl::string<MAX_LENGTH_KEY>, etl::string<MAX_LENGTH_VAL>, MAX_SIZE_MAP> registry;

public:

	enum error_t {
		NO_ERROR = 0,
		FILE_NOT_FOUND,
		BUFFER_TOO_SMALL,
		UNKNOWN_ERROR,
	};
	
	GigaConfig() : registry() {};
	~GigaConfig();

  void setup();

};

#endif
