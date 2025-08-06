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


GigaStorage::rc GigaStorage::load_file(char* buffer, uint32_t* buffer_length, const char* filename) {

  char error_text[128];

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

  // The device is mounted.  Does it contain a config.ini file?
  FILE* file = fopen(filename, "r");

  if (!file) {
    sprintf(error_text, "File %s cannot be opened on USB mass storage device, errno = %d", filename, errno);
    Serial.println(error_text);
    return GigaStorage::rc::NO_FILE;
  }

  // Determine the size of the file
  fseek(file, 0L, SEEK_END);
  uint32_t file_size = ftell(file);
  if (file_size > *buffer_length) {
    sprintf(error_text, "File %s too large (%d bytes) for internal buffer (%d bytes).", filename, file_size, *buffer_length);
    Serial.println(error_text);
    fclose(file);
    return GigaStorage::rc::FILE_TOO_LARGE;
  } else {
    sprintf(error_text, "File %s found, %d bytes.", filename, file_size);
    Serial.println(error_text);
  }

  // Load the file into the buffer
  rewind(file);
  uint32_t bytes_read = fread(buffer, sizeof(char), file_size, file);

  // Make sure we got what we asked for
  if (bytes_read != file_size) {
    sprintf(error_text, "File %s not fully read: %d bytes loaded of %d bytes total.", filename, bytes_read, file_size);
    Serial.println(error_text);
    return GigaStorage::rc::FILE_NOT_READ;
  } 

  // Done with file handle
  fclose(file);

  // Update the pointer to the buffer length with the actual size of the file inside it
  *buffer_length = bytes_read;

  // Return the buffer to the user
  sprintf(error_text, "File %s loaded.", filename, bytes_read);
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