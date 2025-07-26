/*
 * giga-storage.cpp: implementation of USB and flash storage for the Giga R1 board
 *
 */

#define GIGA_STORAGE_CPP_

#include <Arduino.h>
#include "giga-storage.h"

void GigaStorage::setup() {

  
}

void GigaStorage::clear() {


}


#ifdef zero

#include <DigitalOut.h>
#include <FATFileSystem.h>
#include <Arduino_USBHostMbed5.h>

USBHostMSD msd;
mbed::FATFileSystem usb("usb");


void setup()
{
    Serial.begin(115200);
    
    pinMode(PA_15, OUTPUT); //enable the USB-A port
    digitalWrite(PA_15, HIGH);
    
    while (!Serial)
        ;

    Serial.println("Starting USB Dir List example...");

    // if you are using a Max Carrier uncomment the following line
    // start_hub();

    while (!msd.connect()) {
        //while (!port.connected()) {
        delay(1000);
    }

    Serial.print("Mounting USB device... ");
    int err = usb.mount(&msd);
    if (err) {
        Serial.print("Error mounting USB device ");
        Serial.println(err);
        while (1);
    }
    Serial.println("done.");

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