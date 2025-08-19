/*
 * giga-storage.cpp: implementation of USB and flash storage for the Giga R1 board
 *
 */

#define GIGA_STORAGE_CPP_

#include "giga-storage.h"
using namespace mbed;

/******************************************************************************************************************************
 * @brief Setup the storage subsystem.
 ******************************************************************************************************************************/
void GigaStorage::setup() {

  // Enable the USB-A port
  pinMode(PA_15, OUTPUT); 
  digitalWrite(PA_15, HIGH); 

  // Enable the registry flash storage
  registry_store.init();

  // Check that the flash storage is formatted for use.  If not, format it now.
  rc_flash err = flash_init();
  if ((err == rc_flash::FLASH_UNFORMATTED) || KVSTORE_FORCE_REFORMAT) {
    SerialUSB.println("[STO] Flash is unreadable or unformatted! Auto initializing...");
    flash_format();
  }

}

/******************************************************************************************************************************
 * @brief Determine if the flash has been formatted by testing the 1st partition for presence of the "tdb.initialized" key in 
 *        the registry; if the key is not accessible, attempt to format the flash partition for usage.
 *
 * @returns FLASH_NO_ERROR for success accessing the registry
 *          FLASH_UNFORMATTED for when flash is not yet formatted
 ******************************************************************************************************************************/
GigaStorage::rc_flash GigaStorage::flash_init() {

  char initialized_text_dest[MAX_LENGTH_KV];
  size_t retrieved_length;

  int32_t mbed_err;
  char mbed_err_hex_text[32];

  // Retrieve the registry key
  mbed_err = registry_store.get(registry_tdb_initialized, &initialized_text_dest, sizeof(initialized_text_dest), &retrieved_length);
  
  // If the key retrieve fails, we have to assume this device is not formatted properly for this firmware
  if (mbed_err != 0) {
    sprintf(mbed_err_hex_text, "%lX", mbed_err);
    SerialUSB.print("[STO] registry_store.get error = ");
    SerialUSB.println(mbed_err_hex_text);

    return GigaStorage::rc_flash::FLASH_UNFORMATTED;
  }

  SerialUSB.print("[STO] Flash registry is valid, test key ");
  SerialUSB.print(registry_tdb_initialized);
  SerialUSB.print(" = '");
  SerialUSB.print(initialized_text_dest);
  SerialUSB.println("'");
  
  return GigaStorage::rc_flash::FLASH_NO_ERROR;
}

/******************************************************************************************************************************
 * @brief Reformat the on-board 16MB flash for use with the KVStore/TDBStore library.
 *
 * @returns FLASH_NO_ERROR for success
 *          FLASH_FORMAT_FAILURE for failure to format the flash partition
 ******************************************************************************************************************************/
GigaStorage::rc_flash GigaStorage::flash_format() {

  char initialized_text_dest[64];
  int32_t mbed_err;
  char mbed_err_hex_text[64];

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
  mbed_err = registry_store.set(registry_tdb_initialized, registry_tdb_initialized_text, sizeof(registry_tdb_initialized_text), registry_create_flag);
  
  // If the key set fails, assume flash formatting did not work and bail out now
  if (mbed_err != 0) {
    sprintf(mbed_err_hex_text, "%lX", mbed_err);
    SerialUSB.print("[STO] registry_store.set error = ");
    SerialUSB.println(mbed_err_hex_text);

    return GigaStorage::rc_flash::FLASH_FORMAT_FAILURE;
  }

  SerialUSB.println("[STO] Testing registry key retrieval.");

  // Retrieve the key that we just stored (as a test)
  mbed_err = registry_store.get(registry_tdb_initialized, &initialized_text_dest, sizeof(initialized_text_dest));
  
  if (mbed_err != 0) {
    sprintf(mbed_err_hex_text, "%lX", mbed_err);
    SerialUSB.print("[STO] registry_store.get error = ");
    SerialUSB.println(mbed_err_hex_text);

    return GigaStorage::rc_flash::FLASH_FORMAT_FAILURE;
  }

  // Print the contents of the key
  SerialUSB.print("[STO] Registry retrieval test success, test key ");
  SerialUSB.print(registry_tdb_initialized);
  SerialUSB.print(" = '");
  SerialUSB.print(initialized_text_dest);
  SerialUSB.println("'");

  return GigaStorage::rc_flash::FLASH_NO_ERROR;
}

/******************************************************************************************************************************
 * @brief Load a file from the USB flash drive into memory.  The destination buffer is provided, as is the length, and a 
 *        filename to load from.  The mbed library mounts the flash drive under the path "/usb" which is defined by the 
 *        initializer in the header file.  All filenames on the drive are therefore prefixed first by "/usb/".  This routine 
 *        is typically used for reading the configuration INI file off disk.
 *
 * @param[out] buffer              Pointer to a buffer to copy the file into.
 * @param[in]  buffer_length       Max size of the buffer.
 * @param[in]  filename            Filename to load.
 *
 * @returns USB_NO_ERROR           Success: file loaded into the buffer.
 *          NO_DEVICE              Failure: no USB flash drive is inserted.
 *          NOT_MOUNTABLE          Failure: could not mount the flash drive (filesystem type probably wrong)
 *          NO_FILE                Failure: no config.ini file on the drive.
 *          FILE_TOO_LARGE         Failure: file larget that 16KBytes, won't fit.
 *          FILE_NOT_READ          Failure: unable to read the file for some reason.
 ******************************************************************************************************************************/
GigaStorage::rc_usb GigaStorage::usb_file_load(char* buffer, uint32_t* buffer_length, const char* filename) {

  char error_text[128];

  // There is a race condition for connecting to the USB subsystem.  Give it 10 tries with 100ms
  // delay between them, and msd.connect() will eventually succeed.
  int8_t retries = 10;

  while (retries > 0) {
    if (!msd.connect()) {    
      SerialUSB.println("[STO] Trying to connect to USB...");
      delay(100);
      retries--;
    } else {      
      break;
    }
  }

  // If no USB flash drive is present then bail out now.
  if (retries) {
    SerialUSB.println("[STO] USB mass storage device is present.");
  } else {
    SerialUSB.println("[STO] No USB mass storage device detected.");
    return GigaStorage::rc_usb::NO_DEVICE;
  }

  // A device is present.  Determine if it can be mounted.
  int err = usb.mount(&msd);
  if (err) {
    SerialUSB.print("[STO] Error mounting USB mass storage device: ");
    SerialUSB.println(err);
    return GigaStorage::rc_usb::NOT_MOUNTABLE;
  } else {
    SerialUSB.println("[STO] USB mass storage device mounted.");
  }

  // The device is mounted.  Does it contain the specified file?
  FILE* file = fopen(filename, "r");

  if (!file) {
    sprintf(error_text, "[STO] File %s cannot be opened on USB mass storage device, errno = %d", filename, errno);
    SerialUSB.println(error_text);
    return GigaStorage::rc_usb::NO_FILE;
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
    SerialUSB.println(error_text);
    fclose(file);
    return GigaStorage::rc_usb::FILE_TOO_LARGE;
  } else {
    sprintf(error_text, "[STO] File %s found, %ld bytes.", filename, file_size);
    SerialUSB.println(error_text);
  }

  // Load the file into the buffer.
  rewind(file);
  uint32_t bytes_read = fread(buffer, sizeof(char), file_size, file);

  // Make sure we got what we asked for.
  if (bytes_read != file_size) {
    sprintf(error_text, "[STO] File %s not fully read: %ld bytes loaded of %ld bytes total.", filename, bytes_read, file_size);
    SerialUSB.println(error_text);
    return GigaStorage::rc_usb::FILE_NOT_READ;
  } 

  // Done with file handle.
  fclose(file);

  // Update the pointer to the buffer length with the actual size of the file inside it.
  *buffer_length = bytes_read;

  // Return the buffer to the user.
  sprintf(error_text, "[STO] File %s loaded.", filename);
  SerialUSB.println(error_text);
  return GigaStorage::rc_usb::USB_NO_ERROR;
}

/******************************************************************************************************************************
 * @brief Translate the error codes from the MBED library into human readable text, and print it.
 *
 * @param[in]  mbed_err               The error code returned from an MBED call.
 ******************************************************************************************************************************/
void GigaStorage::print_mbed_error(int32_t mbed_err) {

  char message_text[64];

  sprintf(message_text, "MBED err %lX: ", mbed_err);
  SerialUSB.print(message_text);

  switch (mbed_err) {
    case MBED_SUCCESS: 
      SerialUSB.println("success.");
      break;

    case MBED_ERROR_NOT_READY:
      SerialUSB.println("not initialized.");
      break;

    case MBED_ERROR_READ_FAILED:
      SerialUSB.println("unable to read from media.");
      break;

    case MBED_ERROR_WRITE_FAILED:
      SerialUSB.println("unable to write to media.");
      break;

    case MBED_ERROR_INVALID_ARGUMENT:
      SerialUSB.println("invalid argument given in function arguments.");
      break;

    case MBED_ERROR_INVALID_SIZE:
      SerialUSB.println("invalid size given in function arguments.");
      break;

    case MBED_ERROR_INVALID_DATA_DETECTED:
      SerialUSB.println("data is corrupt.");
      break;

    case MBED_ERROR_ITEM_NOT_FOUND:
      SerialUSB.println("no such key.");
      break;

    case MBED_ERROR_MEDIA_FULL:
      SerialUSB.println("not enough room on media.");
      break;

    case MBED_ERROR_WRITE_PROTECTED:
      SerialUSB.println("already stored with 'write once' flag.");
      break;

    default:
      SerialUSB.println("error code unknown!");
      break;
  }

}


