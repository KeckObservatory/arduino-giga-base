/*
 * giga-storage.h: implementation of USB and flash storage for the Giga R1 board
 *
 * This file contains two subsystems.  First, it provides a mechanism to read files
 * from a USB flash drive into a memory buffer.  Second, it provides an interface
 * to the key/value store (TDBStore) in the mbed library, which will be abstracted
 * into a registry that is fronted by GigaConfig.
 *
 */

#ifndef GIGA_STORAGE_H_
#define GIGA_STORAGE_H_

#include <stdint.h>
#include <stdbool.h>

#include <Arduino.h>
#include <DigitalOut.h>
#include <Arduino_USBHostMbed5.h>
#include <QSPIFBlockDevice.h>
#include <MBRBlockDevice.h>
#include <TDBStore.h>
#include <FATFileSystem.h>

#include "timing.h"

/* ************************************************************************** */
/* CONFIGURATION / SETTINGS                                                   */
/* ************************************************************************** */

/*
  To configure this Giga device, the user is required to populate a USB thumb
  drive with a file that contains the settings

*/


class GigaStorage {

  private:
    USBHostMSD msd;
    mbed::FATFileSystem usb;

    QSPIFBlockDevice block_device;
    mbed::MBRBlockDevice tdb_data;
    mbed::TDBStore registry_store;

    uint32_t registry_create_flags = mbed::KVStore::WRITE_ONCE_FLAG;

  public:
    enum rc {
        NO_ERROR = 0,
        NO_DEVICE,
        NOT_MOUNTABLE,
        NO_FILE,
        FILE_TOO_LARGE,
        FILE_NOT_READ,
    };

    GigaStorage() : msd(), 
                    usb("usb"),  
                    block_device(QSPI_SO0, QSPI_SO1, QSPI_SO2, QSPI_SO3, QSPI_SCK, QSPI_CS, QSPIF_POLARITY_MODE_1, 40000000),
                    tdb_data(&block_device, 4),
                    registry_store(&tdb_data)
                    {}

    void setup();
    const char * get_error_text(GigaStorage::rc error);
    GigaStorage::rc load_file(char* buffer, uint32_t* buffer_length, const char* filename);

};

#endif