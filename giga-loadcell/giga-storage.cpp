/*
 * giga-storage.cpp: implementation of USB and flash storage for the Giga R1 board
 *
 */

#define GIGA_STORAGE_CPP_

#include "giga-storage.h"

void GigaStorage::setup() {

  // Enable the USB-A port
  pinMode(PA_15, OUTPUT); 
  digitalWrite(PA_15, HIGH); 

  // Enable the registry
  registry_store.init();
}


const char * GigaStorage::get_error_text(GigaStorage::rc error) {

  switch (error) {
    
    case GigaStorage::rc::NO_ERROR:           return("no error");
    case GigaStorage::rc::NO_DEVICE:          return("no device detected in USB port");
    case GigaStorage::rc::NOT_MOUNTABLE:      return("device not mountable");
    case GigaStorage::rc::NO_FILE:            return("file not present");
    case GigaStorage::rc::FILE_TOO_LARGE:     return("file too large for provided buffer");
    default:                                  return("unknown error value");
  }
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
      Serial.println("Trying to connect to USB...");
      delay(100);
      retries--;
    } else {      
      break;
    }
  }

  // If no USB flash drive is present then bail out now.
  if (retries) {
    Serial.println("USB mass storage device is present.");
  } else {
    Serial.println("No USB mass storage device detected.");
    return GigaStorage::rc::NO_DEVICE;
  }

  // A device is present.  Determine if it can be mounted.
  int err = usb.mount(&msd);
  if (err) {
    Serial.print("Error mounting USB mass storage device: ");
    Serial.println(err);
    return GigaStorage::rc::NOT_MOUNTABLE;
  } else {
    Serial.println("USB mass storage device mounted.");
  }

  // The device is mounted.  Does it contain the specified file?
  FILE* file = fopen(filename, "r");

  if (!file) {
    sprintf(error_text, "File %s cannot be opened on USB mass storage device, errno = %d", filename, errno);
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
    sprintf(error_text, "File %s too large (%ld bytes) for internal buffer (%ld bytes).", filename, file_size, *buffer_length);
    Serial.println(error_text);
    fclose(file);
    return GigaStorage::rc::FILE_TOO_LARGE;
  } else {
    sprintf(error_text, "File %s found, %ld bytes.", filename, file_size);
    Serial.println(error_text);
  }

  // Load the file into the buffer.
  rewind(file);
  uint32_t bytes_read = fread(buffer, sizeof(char), file_size, file);

  // Make sure we got what we asked for.
  if (bytes_read != file_size) {
    sprintf(error_text, "File %s not fully read: %ld bytes loaded of %ld bytes total.", filename, bytes_read, file_size);
    Serial.println(error_text);
    return GigaStorage::rc::FILE_NOT_READ;
  } 

  // Done with file handle.
  fclose(file);

  // Update the pointer to the buffer length with the actual size of the file inside it.
  *buffer_length = bytes_read;

  // Return the buffer to the user.
  sprintf(error_text, "File %s loaded.", filename);
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