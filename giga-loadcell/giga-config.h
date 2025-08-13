/*
 * giga-config.h: implementation of a configuration module for the Giga R1
 *                supports reading a settings file from a USB filesystem 
 *
 */

#ifndef GIGA_CONFIG_H_
#define GIGA_CONFIG_H_

#include <stdint.h>
#include <string.h>
#include <Arduino_USBHostMbed5.h>
#include <IPAddress.h>
#include <Embedded_Template_Library.h>
#include <etl/unordered_map.h>
#include <etl/string.h>
#include <etl/string_utilities.h>
#include <etl/optional.h>

#include "giga-storage.h"

// Max sizes for various elements
#define MAX_LENGTH_KEY_VAL  64
#define MAX_LENGTH_INI_LINE 128
#define MAX_SIZE_REGISTRY   64
#define MAX_SIZE_CONFIG_INI 16384

// Define the configuration filenames
#define CONFIG_INI_FILENAME       "/usb/config.ini"

// Define the registry keys that ought to be present in config.ini
const char registry_net_ip[]          = "net.ip";
const char registry_net_netmask[]     = "net.netmask";
const char registry_net_gateway[]     = "net.gateway";
const char registry_net_dns[]         = "net.dns";
const char registry_cal_placeholder[] = "cal.placeholder";

class GigaConfig {
private:
	
	// Hold a reference to the storage subsystem
	GigaStorage& storage;

  etl::unordered_map<etl::string<MAX_LENGTH_KEY_VAL>, etl::string<MAX_LENGTH_KEY_VAL>, MAX_SIZE_REGISTRY> registry;

public:
	static const char config_ini_filename[];
	static const char config_ini_saved_filename[];

	char config_ini_buffer[MAX_SIZE_CONFIG_INI];
	uint32_t config_ini_buffer_length;

	enum rc {
		NO_ERROR = 0,
		CONFIG_FILE_NOT_FOUND,
    CONFIG_FILE_TOO_LARGE,
		UNKNOWN_ERROR,
		REGISTRY_KEY_NOT_FOUND
	};
	
	GigaConfig(GigaStorage& the_storage) : storage(the_storage), registry() { bzero(config_ini_buffer, sizeof(config_ini_buffer)); config_ini_buffer_length = MAX_SIZE_CONFIG_INI; };
	~GigaConfig() {};

  void setup();
  const char * get_error_text(uint8_t error);
	GigaConfig::rc ini_parse();
	GigaConfig::rc registry_load();

	std::pair<GigaConfig::rc, etl::string<MAX_LENGTH_KEY_VAL>> registry_get(const char key[]);
	//etl::string<MAX_LENGTH_KEY_VAL> registry_get(const char key[]);

};

#endif
