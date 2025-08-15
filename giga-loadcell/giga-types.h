/*
 * giga-types.h: defines (typedefs) for types shared between giga modules
 *
 */

#ifndef GIGA_TYPES_H_
#define GIGA_TYPES_H_

#include <stdint.h>
#include <string.h>
#include <Embedded_Template_Library.h>
#include <etl/unordered_map.h>
#include <etl/string.h>
#include <etl/string_utilities.h>
#include <etl/optional.h>

// High level defines used to bound the memory usage of the complex types below
#define MAX_LENGTH_KV 64

// Define complex types derived from STL/ETL for use with the registry

// A string for use as either a key or value, that is no longer than 64 chars
using KVString = etl::string<MAX_LENGTH_KV>;

// A key-value pair that also has a return code with it, for use when returning from
// functions that might fail to locate/create the desired string.  If the enumerations
// for the return code are declared as uint8_t, this will allow returning a KVStringRC
// with enum values!
using KVStringRC = std::pair<uint8_t, KVString>;

// A collection of KVString objects
template<uint16_t T>
using KVVector = etl::vector<KVString, T>;

// A registry is an unordered collection of key/value string pairs that can be retrieved
// using the key string.
template<uint16_t T>
using GigaRegistry = etl::unordered_map<KVString, KVString, T>;

// An unordered collection of keys that represent what should be in the registry, 
// as well as in the flash storage.
template<uint16_t T>
using GigaKeys = etl::vector<etl::string<MAX_LENGTH_KV>, T>;

// A fixed collection of keys that represent the minimum number of values that
// _must_ be in the registry for the system to function.
using GigaRegistryRequiredKeys = const std::initializer_list<const char *>;


#endif