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


#ifdef zero
  char buf[256];



  // Display the root directory
  Serial.print("Opening the root directory... ");
  DIR* d = opendir("/usb/");
  Serial.println(!d ? "Fail :(" : "Done");
  if (!d) {
      snprintf(buf, sizeof(buf), "error: %s (%d)\r\n", strerror(errno), -errno);
      Serial.print(buf);
  }
  Serial.println("done.");

  Serial.println("Root directory:");
  unsigned int count { 0 };
  while (true) {
      struct dirent* e = readdir(d);
      if (!e) {
          break;
      }
      count++;
      snprintf(buf, sizeof(buf), "    %s\r\n", e->d_name);
      Serial.print(buf);
  }
  Serial.print(count);
  Serial.println(" files found!");

  snprintf(buf, sizeof(buf), "Closing the root directory... ");
  Serial.print(buf);
  fflush(stdout);
  err = closedir(d);
  snprintf(buf, sizeof(buf), "%s\r\n", (err < 0 ? "Fail :(" : "OK"));
  Serial.print(buf);
  if (err < 0) {
      snprintf(buf, sizeof(buf), "error: %s (%d)\r\n", strerror(errno), -errno);
      Serial.print(buf);
  }
#endif
  
}


const char * GigaStorage::get_error_text(uint8_t error) {

  switch (error) {
    case GigaUsbIniFile::errorNoError:        return("no error");
    case GigaUsbIniFile::errorFileNotFound:   return("file not found");
    case GigaUsbIniFile::errorFileNotOpen:    return("file not open");      
    case GigaUsbIniFile::errorBufferTooSmall: return("buffer too small");
    case GigaUsbIniFile::errorSeekError:      return("seek error");
    case GigaUsbIniFile::errorSectionNotFound:return("section not found");
    case GigaUsbIniFile::errorKeyNotFound:    return("key not found");
    case GigaUsbIniFile::errorEndOfFile:      return("end of file");
    case GigaUsbIniFile::errorUnknownError:   return("unknown error");
    default:                           return("unknown error value");
  }
}




GigaStorage::rc GigaStorage::load() {

  // Read the INI into a 2k buffer, which is rougly double the (current) size of config.ini
  const size_t buffer_len = 2048;
  char buffer[buffer_len];
  char error_text[80];

  int8_t retries = 10;

  while (retries) {

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

/*
  // Attempt to connect to a mass storage device
  if (!msd.connect()) {    
    Serial.println("No USB mass storage device detected.");
    return GigaStorage::rc::NO_DEVICE;
  } else {
    Serial.println("USB mass storage device is present.");
  }
*/

  // A device is present.  Determine if it can be mounted.
  int err = usb.mount(&msd);
  if (err) {
    Serial.print("Error mounting USB mass storage device: ");
    Serial.println(err);
    return GigaStorage::rc::NOT_MOUNTABLE;
  } else {
    Serial.println("USB mass storage device mounted.");
  }

/*
  // The device is mounted.  Does it contain a config.ini file?
  GigaUsbIniFile ini(config_ini_filename);
  if (ini.open() == GigaUsbIniFile::error_t::errorFileNotFound) {
    Serial.print("INI file ");
    Serial.print(config_ini_filename);
    Serial.println(" does not exist on USB device.");
    return GigaStorage::rc::NO_CONFIG_INI;
  } else {
    Serial.print("INI file ");
    Serial.print(config_ini_filename);
    Serial.println(" detected on USB device.");
  }

  // A config file is present.  Does it parse?  Are any lines longer than the buffer?
  if (!ini.validate(buffer, buffer_len)) {
    Serial.print("Error parsing INI file: ");
    sprintf(error_text, "%s", get_error_text(ini.getError()));
    Serial.println(error_text);
  } else {
    Serial.println("INI file loaded.");
  }
*/

  // The config file is a valid INI file.  Retrieve all the fields for each section and 
  // store them in the registry.




  return GigaStorage::rc::CONFIG_INI_LOAD_SUCCESS;

}

void GigaStorage::clear() {


}







#ifdef zero



void setup()
{
    Serial.begin(115200);
    while (!Serial)
        ;

    Serial.println("Starting USB Dir List example...");

    // if you are using a Max Carrier uncomment the following line
    // start_hub();


}

void loop()
{
}

#endif





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