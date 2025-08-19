/*
 * giga-config.c: implementation of a configuration module for the Giga R1
 *                supports reading a settings file from a USB filesystem 
 */

#define GIGA_CONFIG_C_

#include "giga-config.h"

// Assign values to the const strings in the class
const char GigaConfig::config_ini_filename[] = CONFIG_INI_FILENAME;

void GigaConfig::setup() {

  int32_t mbed_err;
  char message_text[128];

  mbed::TDBStore::iterator_t iterator;
  uint16_t index = 0;
  char key[MAX_LENGTH_KV] = {0};
  char value[MAX_LENGTH_KV] = {0};
  size_t length;

  // Preload the registry with the values stored in flash, if there are any
  SerialUSB.println("[CFG] Preloading registry with values from flash.");
  bzero(key, MAX_LENGTH_KV);

  // Iterate the keys
  mbed_err = storage.registry_store.iterator_open(&iterator, NULL);
  while (storage.registry_store.iterator_next(iterator, key, MAX_LENGTH_KV) != MBED_ERROR_ITEM_NOT_FOUND) {

    // Get the value for this key
    bzero(value, MAX_LENGTH_KV);
    mbed_err = storage.registry_store.get(key, value, sizeof(value), &length);

    if (mbed_err == MBED_SUCCESS) {

      sprintf(message_text, "[CFG]  %s = '%s'", key, value);
      SerialUSB.println(message_text);

      // Add the key/value to the in-memory registry but skip the mbed reserved key
      if (strcmp(key, registry_tdb_reserved) != 0) {

        registry.insert(etl::make_pair(key, value));
      }

    } else {

      sprintf(message_text, "[CFG] storage.registry_store.get failed!");
      SerialUSB.println(message_text);
      storage.print_mbed_error(mbed_err);
      SerialUSB.println("***** FLASH STORAGE FAILURE *****");
    }

    // Prepare for next key
    index++;
    bzero(key, MAX_LENGTH_KV);
  }
  mbed_err = storage.registry_store.iterator_close(iterator);

  SerialUSB.println("[CFG] Registry preload complete."); 
}

// Parse an INI file that is already loaded into a buffer in memory
GigaConfig::rc GigaConfig::ini_parse() {

  // Verify an INI is loaded into memory
  SerialUSB.println("[CFG] Parsing configuration file.");

  // Declare some simpler names for use with this routine.
  using StringView = etl::string_view;
  using Token = etl::optional<StringView>;

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
// load cell calibrations and retain the existing TCP/IP settings, without having to know
// the TCP/IP addresses beforehand.
GigaConfig::rc GigaConfig::registry_load() {

  char message_text[128];
  int32_t mbed_err;

  // Check the USB mass storage subsystem for the presence of a USB flash drive.  If it is present,
  // attempt to load an INI file into the buffer in this GigaConfig instance.
  GigaStorage::rc_usb rc_usb = storage.usb_file_load(config_ini_buffer, &config_ini_buffer_length, config_ini_filename);

  switch (rc_usb) {

    case GigaStorage::rc_usb::NO_DEVICE:
      {
        SerialUSB.println("[CFG] No USB flash drive present to load an INI file from.");
        break;
      }

    case GigaStorage::rc_usb::NOT_MOUNTABLE:
      {
        SerialUSB.println("[CFG] This USB flash drive is not mountable.  It must be no larger than 32GB, and formatted for FAT32.");
        break;
      }

    case GigaStorage::rc_usb::NO_FILE:
      {
        sprintf(message_text, "[CFG] USB flash drive does not contain a file named '%s' to load settings from.", config_ini_filename);
        SerialUSB.println(message_text);
        break;
      }

    case GigaStorage::rc_usb::FILE_TOO_LARGE:
    case GigaStorage::rc_usb::FILE_NOT_READ:
      {
        sprintf(message_text, "[CFG] File '%s' on USB flash drive is too large or cannot be read into a buffer.", config_ini_filename);
        SerialUSB.println(message_text);
        break;
      }

    case GigaStorage::rc_usb::USB_NO_ERROR:
      {
        sprintf(message_text, "[CFG] Processing configuration file '%s' on USB flash drive.", config_ini_filename);
        SerialUSB.println(message_text);

        // An INI file was present and successfully loaded into memory.  Parse the file contents
        // and store the values in the registry.
        GigaConfig::rc rc_ini = ini_parse();

        if (rc_ini != GigaConfig::rc::NO_ERROR) {
          SerialUSB.println("[CFG] Failed to load INI file!");
        }
        break;
      }

    default:
      {
        sprintf(message_text, "[CFG] Unknown return code from storage.usb_file_load() = %d", rc_usb);
        SerialUSB.println(message_text);
        break;
      }
  }

  // The in-memory registry is now either empty, or loaded from values on the USB flash drive.

  // Iterate through each value in the registry and see if it's in the flash storage.  Entries
  // not in storage, or with values that differ from what is in storage, must now be written
  // to storage.

  sprintf(message_text, "[CFG] Synchronizing registry with flash, total %d keys.", registry.size());
  SerialUSB.println(message_text);

  char stored_value[MAX_LENGTH_KV];
  size_t retrieved_length;

  // Internally the registry uses an etl::pair to store elements.  
  for (const etl::pair<KVString, KVString>& element : registry) {

    // Decompose the element into the key and the value
    KVString key = element.first;
    KVString value = element.second;

    // Check the flash for this key
    bzero(stored_value, MAX_LENGTH_KV);
    mbed_err = storage.registry_store.get(key.c_str(), stored_value, sizeof(stored_value), &retrieved_length);
  
    // If the key retrieve fails, the key is not present, so write it!
    if (mbed_err == MBED_ERROR_ITEM_NOT_FOUND) {

      sprintf(message_text, "[CFG] Key %s not found in registry, creating it with '%s'.", key.c_str(), value.c_str());
      SerialUSB.println(message_text);

      mbed_err = storage.registry_store.set(key.c_str(), value.c_str(), strlen(value.c_str()), storage.registry_create_flag);

      if (mbed_err != 0) {

        sprintf(message_text, "[CFG] storage.registry_store.set failed!");
        SerialUSB.println(message_text);

        storage.print_mbed_error(mbed_err);

        SerialUSB.println("***** FLASH STORAGE FAILURE *****");
        return GigaConfig::rc::FLASH_STORAGE_FAILURE;
      }

    } else if (mbed_err == MBED_SUCCESS) {

      // Found the key, does it need updating?
      if (strcmp(value.c_str(), stored_value) > 0) {

        sprintf(message_text, "[CFG] Key %s found in registry (len %d), value update: %s -> %s", key.c_str(), retrieved_length, stored_value, value.c_str());
        SerialUSB.println(message_text);

        mbed_err = storage.registry_store.set(key.c_str(), value.c_str(), strlen(value.c_str()), storage.registry_create_flag);
        if (mbed_err != 0) {

          sprintf(message_text, "[CFG] storage.registry_store.set failed!");
          SerialUSB.println(message_text);

          storage.print_mbed_error(mbed_err);
          
          SerialUSB.println("***** FLASH STORAGE FAILURE *****");
          return GigaConfig::rc::FLASH_STORAGE_FAILURE;
        }

      } else {

        sprintf(message_text, "[CFG] Key %s found in registry (%s), no update required.", key.c_str(), value.c_str());
        SerialUSB.println(message_text);
      }

    } else {

      sprintf(message_text, "[CFG] storage.registry_store.get failed!");
      SerialUSB.println(message_text);

      storage.print_mbed_error(mbed_err);

      SerialUSB.println("***** FLASH STORAGE FAILURE *****");
      return GigaConfig::rc::FLASH_STORAGE_FAILURE;

    }
  }

  // Iterate through each value that is required by this project for use elsewhere in the code.
  // When a key is not present in the registry, attempt to load it from flash storage.  If it
  // is not present in flash storage then return with a failure such that the program will halt
  // and display a panic pattern on the LEDs.
  for (const auto& key : registry_required_keys) {

    // Is this key in the registry?
    KVStringRC s = registry_get(key);

    if (s.first != NO_ERROR) {

      


      sprintf(message_text, "[CFG] MISSING REGISTRY ENTRY: %s not present in registry!", key);
      SerialUSB.println(message_text);
      return GigaConfig::rc::REGISTRY_INCOMPLETE;
    }
  }
  




  SerialUSB.println("[CFG] Registry load complete.");

  return GigaConfig::rc::NO_ERROR;
}


// Retrieve a registry value that corresponds to a desired key (string)
KVStringRC GigaConfig::registry_get(const char key[]) {

  auto key_wrapper = KVString(key);

  if (registry.contains(key_wrapper)) {

    auto it = registry.find(key_wrapper);
    if (it != registry.end()) {
      KVStringRC get_rc(NO_ERROR, it->second);
      return (get_rc);
    }
  }

  // Key was not found in the registry
  KVStringRC get_rc(REGISTRY_KEY_NOT_FOUND, NULL);
  return (get_rc);
}
