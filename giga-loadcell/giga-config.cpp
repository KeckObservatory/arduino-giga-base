#include "etl/string_utilities.h"
#include "etl/string.h"
#include "etl/unordered_map.h"
/*
 * giga-config.c: implementation of a configuration module for the Giga R1
 *                supports reading a settings file from a USB filesystem 
 */

#define GIGA_CONFIG_C_

#include "giga-config.h"

// Assign values to the const strings in the class
const char GigaConfig::config_ini_filename[] = CONFIG_INI_FILENAME;
const char GigaConfig::config_ini_saved_filename[] = CONFIG_INI_SAVED_FILENAME;

void GigaConfig::setup() {

  etl::string<MAX_LENGTH_KEY_VAL> s1 = registry_network_ip;
  etl::string<MAX_LENGTH_KEY_VAL> s2 = "10.77.0.210";
  auto s1s2 = etl::make_pair(s1, s2);

  etl::string<MAX_LENGTH_KEY_VAL> s3 = registry_network_netmask;
  etl::string<MAX_LENGTH_KEY_VAL> s4 = "255.255.0.0";
  auto s3s4 = etl::make_pair(s3, s4);

  registry.insert(s1s2);
  registry.insert(s3s4);

  for (const std::pair<etl::string<MAX_LENGTH_KEY_VAL>, etl::string<MAX_LENGTH_KEY_VAL>>& n : registry) {
    Serial.println(n.first.c_str());
    Serial.println(n.second.c_str());
  }



}


GigaConfig::rc GigaConfig::load_ini() {

  // Verify an INI is loaded into memory
  SerialUSB.println("[INI] Parsing configuration file.");

  using String     = etl::string<MAX_LENGTH_KEY_VAL>;
  using StringView = etl::string_view;
  using Vector     = etl::vector<String, MAX_SIZE_REGISTRY>;
  using Token      = etl::optional<StringView>;

  // Connect a string to an external buffer, using the length of the file read from disk
  etl::string_ext ini_text(config_ini_buffer, config_ini_buffer, config_ini_buffer_length);
  Vector tokens;
  Token token; 

  //token = etl::get_token(ini_text, "\n", token, true);
  //SerialUSB.print("Token: ");
  //etl::string<128> v(token.value().begin(), token.value().end()); 
  //SerialUSB.println(v.c_str());

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

      SerialUSB.print("[INI] ");
      SerialUSB.print(key.c_str());
      SerialUSB.print(" -> ");
      SerialUSB.println(val.c_str());
    } else {
      SerialUSB.print("[INI] Unparsable line: '");
      SerialUSB.print(line.c_str());
      SerialUSB.println("'");
    }

  }

  SerialUSB.println("[INI] Configuration file loaded.");
  return GigaConfig::rc::NO_ERROR;


}