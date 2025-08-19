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

#include "giga-types.h"
#include "giga-storage.h"

// Max sizes for various elements
#define MAX_LENGTH_INI_LINE 128
#define MAX_SIZE_REGISTRY   64
#define MAX_SIZE_CONFIG_INI 16384

// Define the configuration filenames
#define CONFIG_INI_FILENAME       "/usb/config.ini"

// Define the known registry keys that can be present in config.ini
const char registry_net_ip[]          = "net.ip";
const char registry_net_netmask[]     = "net.netmask";
const char registry_net_gateway[]     = "net.gateway";
const char registry_net_dns[]         = "net.dns";
const char registry_cal_placeholder[] = "cal.placeholder";

// Define which keys _must_ exist in the registry (and therefore flash) for normal operation
#define REGISTRY_REQUIRED_KEYS { registry_net_ip, registry_net_netmask, registry_net_gateway, registry_net_dns }

//const auto registry_keys = { registry_net_ip, registry_net_netmask, registry_net_gateway, registry_net_dns };
//const std::initializer_list<const char *> registry_keys2 = { registry_net_ip, registry_net_netmask, registry_net_gateway, registry_net_dns };

class GigaConfig {
private:
	
	// Hold a reference to the storage subsystem
	GigaStorage& storage;

  // The system registry
	GigaRegistry<MAX_SIZE_REGISTRY> registry;
	GigaRegistryRequiredKeys registry_required_keys;

	// The keys for use with the registry
	GigaKeys<MAX_SIZE_REGISTRY> keys;

public:
	static const char config_ini_filename[];
	static const char config_ini_saved_filename[];

	char config_ini_buffer[MAX_SIZE_CONFIG_INI];
	uint32_t config_ini_buffer_length;

	enum rc : uint8_t {
		NO_ERROR = 0,
		CONFIG_FILE_NOT_FOUND,
    CONFIG_FILE_TOO_LARGE,
		UNKNOWN_ERROR,
		REGISTRY_KEY_NOT_FOUND,
		FLASH_STORAGE_FAILURE
	};
	
	GigaConfig(GigaStorage& the_storage) : storage(the_storage), 
	                                       registry(), 																				 
																				 registry_required_keys(REGISTRY_REQUIRED_KEYS),
																				 keys() { 																					
		bzero(config_ini_buffer, sizeof(config_ini_buffer)); 
		config_ini_buffer_length = MAX_SIZE_CONFIG_INI; 
	};

	~GigaConfig() {};

  void setup();
	GigaConfig::rc ini_parse();
	GigaConfig::rc registry_load();
	KVStringRC registry_get(const char key[]);
	GigaConfig::rc registry_flash_sync();

};

#endif
