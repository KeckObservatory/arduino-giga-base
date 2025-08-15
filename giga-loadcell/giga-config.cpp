/*
 * giga-config.c: implementation of a configuration module for the Giga R1
 *                supports reading a settings file from a USB filesystem 
 */

#define GIGA_CONFIG_C_

#include "giga-config.h"

// Assign values to the const strings in the class
const char GigaConfig::config_ini_filename[] = CONFIG_INI_FILENAME;

void GigaConfig::setup() {

  KVString s1 = registry_net_ip;
  KVString s2 = "10.77.0.210";
  auto s1s2 = etl::make_pair(s1, s2);

  KVString s3 = registry_net_netmask;
  KVString s4 = "255.255.0.0";
  auto s3s4 = etl::make_pair(s3, s4);

  registry.insert(s1s2);
  registry.insert(s3s4);

  for (const std::pair<KVString, KVString>& n : registry) {
    Serial.println(n.first.c_str());
    Serial.println(n.second.c_str());
  }

}

// Parse an INI file that is already loaded into a buffer in memory
GigaConfig::rc GigaConfig::ini_parse() {

  // Verify an INI is loaded into memory
  SerialUSB.println("[CFG] Parsing configuration file.");

  using StringView = etl::string_view;
  using Token      = etl::optional<StringView>;

  // Connect a string to an external buffer, using the length of the file read from disk
  etl::string_ext ini_text(config_ini_buffer, config_ini_buffer, config_ini_buffer_length);
  KVVector<MAX_SIZE_REGISTRY> tokens;
  Token token; 

  // Iterate each line (separated by a newline character)
  while (token = etl::get_token(ini_text, "\n", token, true)) {

    // Convert the token into an etl::string so we can work on it
    etl::string<MAX_LENGTH_INI_LINE> line(token.value().begin(), token.value().end()); 

    // Remove whitespace from both ends of line
    etl::trim_whitespace(line);

    // Ignore comment lines
    if (line.starts_with(';')) {
      continue;
    }

    // Ignore section lines (present in INI files, not useful here [yet])
    if (line.starts_with('[')) {
      continue;
    }

    // Create a vector of size 2 for a key/value pair
    etl::vector<etl::string_view, 2> pair;

    // Split the line on the '=' to get the key/value pair
    etl::get_token_list(line, pair, "=", true, 2);

    if (pair.size() == 2) {

      // Extract the key and value then trim
      etl::string<MAX_LENGTH_INI_LINE> key(pair.at(0).begin(), pair.at(0).end()); 
      etl::string<MAX_LENGTH_INI_LINE> val(pair.at(1).begin(), pair.at(1).end()); 
      etl::trim_whitespace(key);
      etl::trim_whitespace(val);

      SerialUSB.print("[CFG]   ");
      SerialUSB.print(key.c_str());
      SerialUSB.print(" -> ");
      SerialUSB.println(val.c_str());

      // Convert into a pair then insert into the registry 
      auto kv_pair = etl::make_pair(key, val);
      registry.insert(kv_pair);

    } else {
      SerialUSB.print("[CFG] Unparsable line: '");
      SerialUSB.print(line.c_str());
      SerialUSB.println("'");
    }

  }

  SerialUSB.println("[CFG] Configuration file loaded.");
  return GigaConfig::rc::NO_ERROR;
}

// Load the registry values from USB flash drive (if one is attached) and other keys from the
// on-board flash.  This allows, for example, a user to insert a USB flash drive to update
// load cell calibrations and retain the existing TCP/IP settings, without knowing the TCP/IP
// addresses beforehand.
GigaConfig::rc GigaConfig::registry_load() {

  // Check that the flash is formatted for use.  If not, format it now.
  GigaStorage::rc_flash err = storage.flash_test();
  if ((err == GigaStorage::rc_flash::FLASH_UNFORMATTED) || KVSTORE_FORCE_REFORMAT) {
    SerialUSB.println("[CFG] Flash is unreadable or unformatted! Auto initializing...");
    storage.flash_format();
  }

  // Load the INI file into the buffer in this GigaConfig instance
  GigaStorage::rc_usb rc_usb = storage.usb_file_load(config_ini_buffer, &config_ini_buffer_length, config_ini_filename);

  switch (rc_usb) {

    case GigaStorage::rc_usb::NO_DEVICE: {
      SerialUSB.println("[CFG] No USB flash device present to load an INI file from.");
      break;
    }

    case GigaStorage::rc_usb::NOT_MOUNTABLE:
    case GigaStorage::rc_usb::NO_FILE:
    case GigaStorage::rc_usb::FILE_TOO_LARGE:
    case GigaStorage::rc_usb::FILE_NOT_READ: {
      SerialUSB.println("[CFG] Failure to read USB flash device or config.ini file.");
      break;
    }

    case GigaStorage::rc_usb::USB_NO_ERROR: {
      SerialUSB.println("[CFG] Processing INI file on USB flash device.");
    
      // Parse the INI file and store the values in flash
      GigaConfig::rc rc_ini = ini_parse();

      if (rc_ini != GigaConfig::rc::NO_ERROR) {
        SerialUSB.println("[CFG] Failed to load INI file!");
      }
      break;
    }

    default: {
      SerialUSB.print("[CFG] Unknown return code from storage.usb_file_load() = ");
      SerialUSB.println(rc_usb);
      break;
    }

  }

  // 


  return GigaConfig::rc::NO_ERROR;
}


// Retrieve a registry value that corresponds to a desired key (string)
KVStringRC GigaConfig::registry_get(const char key[]) {
  
  SerialUSB.print("[CFG] Locating registry key: ");
  SerialUSB.println(key);

  auto key_wrapper = KVString(key);

  if (registry.contains(key_wrapper)) {

    auto it = registry.find(key_wrapper);
    if (it != registry.end()) {
        SerialUSB.print("[CFG] Found kv_pair: ");
        SerialUSB.print(it->first.c_str());
        SerialUSB.print(" = ");
        SerialUSB.println(it->second.c_str());

        KVStringRC get_rc(NO_ERROR, it->second);
        return(get_rc);
    } 
  } 

  // Key was not found in the registry
  SerialUSB.println("[CFG] Not found!");
  KVStringRC get_rc(REGISTRY_KEY_NOT_FOUND, NULL);
  return(get_rc);
}

// Synchronize the registry in memory with the copy in flash
GigaConfig::rc GigaConfig::registry_flash_sync() {

  // If there are no keys 


}




