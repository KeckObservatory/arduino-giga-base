/*
 * giga-storage.cpp: implementation of USB and flash storage for the Giga R1 board
 *
 */

#define GIGA_STORAGE_CPP_

#include "giga-storage.h"
using namespace mbed;

void GigaStorage::setup() {

  // Enable the USB-A port
  pinMode(PA_15, OUTPUT); 
  digitalWrite(PA_15, HIGH); 

  // Enable the registry
  registry_store.init();
}

// Determine if the flash has been formatted by testing the 1st partition for presence
// of the "tdb.initialized" key in the registry.
GigaStorage::rc GigaStorage::test_flash() {

  char initialized_text_dest[64];

  int32_t mbed_err;
  char mbed_err_hex_text[32];

  // Retrieve the registry key
  mbed_err = registry_store.get(registry_tdb_initialized, &initialized_text_dest, sizeof(initialized_text_dest));
  
  // If the key retrieve fails, we have to assume this device is not formatted properly for this firmware
  if (mbed_err != 0) {
    sprintf(mbed_err_hex_text, "%lX", mbed_err);
    Serial.print("[STO] registry_store.get error = ");
    Serial.println(mbed_err_hex_text);

    return GigaStorage::rc::FLASH_UNFORMATTED;
  }

  Serial.print("[STO] Flash registry is valid, test key ");
  Serial.print(registry_tdb_initialized);
  Serial.print(" = '");
  Serial.print(initialized_text_dest);
  Serial.println("'");
  
  return GigaStorage::rc::NO_ERROR;
}

// Reformat the on-board 16MB flash for use with the KVStore/TDBStore library
GigaStorage::rc GigaStorage::format_flash() {

  char initialized_text_dest[64];
  int32_t mbed_err;
  char mbed_err_hex_text[32];

  // Disable the registry interface, as we are about to reformat flash out from underneath it
  registry_store.deinit();

  // Erase takes about 3 seconds
  SerialUSB.println("[STO] Erasing flash.");
  block_device.erase(0x0, block_device.size());
  
  // Create one partition on the flash in the first megabyte.  The other 15MB remain for future use
  SerialUSB.print("[STO] Formatting flash: 1 partition, size ");
  SerialUSB.print(KVSTORE_PARTITION_SIZE_MB);
  SerialUSB.println("MB.");
  MBRBlockDevice::partition(block_device.get_default_instance(), 1, KVSTORE_PARTITION_TYPE_FAT32, 0, KVSTORE_PARTITION_SIZE);

  // Now restart the registry interface on top of the newly formatted FAT32 partition
  SerialUSB.println("[STO] Initializing registry.");
  registry_store.init();

  // Set the key that indicates flash is setup properly
  mbed_err = registry_store.set(registry_tdb_initialized, registry_tdb_initialized_text, sizeof(registry_tdb_initialized_text), registry_create_flags);
  
  // If the key set fails, assume flash formatting did not work and bail out now
  if (mbed_err != 0) {
    sprintf(mbed_err_hex_text, "%lX", mbed_err);
    Serial.print("[STO] registry_store.set error = ");
    Serial.println(mbed_err_hex_text);

    return GigaStorage::rc::FLASH_FORMAT_FAILURE;
  }

  SerialUSB.println("[STO] Testing registry key retrieval.");

  // Retrieve the key that we just stored (as a test)
  mbed_err = registry_store.get(registry_tdb_initialized, &initialized_text_dest, sizeof(initialized_text_dest));
  
  if (mbed_err != 0) {
    sprintf(mbed_err_hex_text, "%lX", mbed_err);
    Serial.print("[STO] registry_store.get error = ");
    Serial.println(mbed_err_hex_text);

    return GigaStorage::rc::FLASH_FORMAT_FAILURE;
  }

  // Print the contents of the key
  Serial.print("[STO] Registry retrieval test success, test key ");
  Serial.print(registry_tdb_initialized);
  Serial.print(" = '");
  Serial.print(initialized_text_dest);
  Serial.println("'");

  return GigaStorage::rc::NO_ERROR;
}

/* Load a file from the USB flash drive into memory.  The destination buffer is provided, as is the length, 
 * and a filename to load from.  The mbed library mounts the flash drive under the path "/usb" which is 
 * defined by the initializer in the header file.  All filenames on the drive are therefore prefixed first 
 * by "/usb/".  This routine is typically used for reading the configuration INI file off disk.
 */
GigaStorage::rc GigaStorage::load_file(char* buffer, uint32_t* buffer_length, const char* filename) {

  char error_text[128];

  // There is a race condition for connecting to the USB subsystem.  Give it 10 tries with 100ms
  // delay between them, and msd.connect() will eventually succeed.
  int8_t retries = 10;

  while (retries > 0) {
    if (!msd.connect()) {    
      Serial.println("[STO] Trying to connect to USB...");
      delay(100);
      retries--;
    } else {      
      break;
    }
  }

  // If no USB flash drive is present then bail out now.
  if (retries) {
    Serial.println("[STO] USB mass storage device is present.");
  } else {
    Serial.println("[STO] No USB mass storage device detected.");
    return GigaStorage::rc::NO_DEVICE;
  }

  // A device is present.  Determine if it can be mounted.
  int err = usb.mount(&msd);
  if (err) {
    Serial.print("[STO] Error mounting USB mass storage device: ");
    Serial.println(err);
    return GigaStorage::rc::NOT_MOUNTABLE;
  } else {
    Serial.println("[STO] USB mass storage device mounted.");
  }

  // The device is mounted.  Does it contain the specified file?
  FILE* file = fopen(filename, "r");

  if (!file) {
    sprintf(error_text, "[STO] File %s cannot be opened on USB mass storage device, errno = %d", filename, errno);
    Serial.println(error_text);
    return GigaStorage::rc::NO_FILE;
  }

  // Determine the size of the file, such that we can determine if it will fit in the buffer.  
  // As of the time of development this routine is only used for reading INI files from flash drives.  The 
  // maximum size of the file is dictated by the buffer it will be read into, which for the moment is
  // set to 16Kbytes.  Increase this as needed for your project but be aware the Giga R1 only has
  // 512Kbytes of RAM.
  fseek(file, 0L, SEEK_END);
  uint32_t file_size = ftell(file);
  if (file_size > *buffer_length) {
    sprintf(error_text, "[STO] File %s too large (%ld bytes) for internal buffer (%ld bytes).", filename, file_size, *buffer_length);
    Serial.println(error_text);
    fclose(file);
    return GigaStorage::rc::FILE_TOO_LARGE;
  } else {
    sprintf(error_text, "[STO] File %s found, %ld bytes.", filename, file_size);
    Serial.println(error_text);
  }

  // Load the file into the buffer.
  rewind(file);
  uint32_t bytes_read = fread(buffer, sizeof(char), file_size, file);

  // Make sure we got what we asked for.
  if (bytes_read != file_size) {
    sprintf(error_text, "[STO] File %s not fully read: %ld bytes loaded of %ld bytes total.", filename, bytes_read, file_size);
    Serial.println(error_text);
    return GigaStorage::rc::FILE_NOT_READ;
  } 

  // Done with file handle.
  fclose(file);

  // Update the pointer to the buffer length with the actual size of the file inside it.
  *buffer_length = bytes_read;

  // Return the buffer to the user.
  sprintf(error_text, "[STO] File %s loaded.", filename);
  Serial.println(error_text);
  return GigaStorage::rc::NO_ERROR;
}






#ifdef zero

#include "QSPIFBlockDevice.h"
#include "MBRBlockDevice.h"
//#include "FATFileSystem.h"
#include <TDBStore.h>

QSPIFBlockDevice root(QSPI_SO0, QSPI_SO1, QSPI_SO2, QSPI_SO3,  QSPI_SCK, QSPI_CS, QSPIF_POLARITY_MODE_1, 40000000);
//mbed::MBRBlockDevice ota_data(&root, 2);
//mbed::MBRBlockDevice user_data(&root, 3);
//mbed::MBRBlockDevice tdb_data(&root, 1);
mbed::MBRBlockDevice tdb_data(&root, 4);
//mbed::FATFileSystem ota_data_fs("fs");
//mbed::FATFileSystem user_data_fs("user");

//QSPIFBlockDevice root;
mbed::TDBStore config(&tdb_data);



const char tdb_EthernetMAC[] = "EthernetMAC";
const char newmac[] = "0xDEAFBEEF";
char ethernetMAC[32];
char mbederr[32];
uint32_t create_flags = mbed::KVStore::WRITE_ONCE_FLAG;

void setup() {
  int err;
  Serial.begin(115200);
  while (!Serial);

  //mbed::MBRBlockDevice::partition(&root, 2, 0x0B,  1 * 1024 * 1024,  6 * 1024 * 1024);
  //mbed::MBRBlockDevice::partition(&root, 3, 0x0B,  6 * 1024 * 1024, 10 * 1024 * 1024);
  //err = mbed::MBRBlockDevice::partition(&root, 1, 0x0B, 0, 14 * 1024 * 1024);

/*
  if (err != 0) {
    Serial.print("partition error = ");
    Serial.println(err);
    while(1);
  }
*/
  config.init();

/*
  Serial.print("Setting MAC to ");
  Serial.println(newmac);

  err = config.set(tdb_EthernetMAC, newmac, sizeof(newmac), create_flags);
  Serial.print("config.set error = ");
  Serial.print(err);
*/

  err = config.get(tdb_EthernetMAC, &ethernetMAC, sizeof(ethernetMAC));
  sprintf(mbederr, "%X", err);
  Serial.print("config.get error = ");
  Serial.println(mbederr);

  Serial.print("MAC = ");
  Serial.println(ethernetMAC);

  Serial.println();
  Serial.println("done");
}



void loop() {

}

#endif