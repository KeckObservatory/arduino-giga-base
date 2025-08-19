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
#include <BlockDevice.h>
#include <MBRBlockDevice.h>
#include <TDBStore.h>
#include <FATFileSystem.h>
#include <mbed_error.h>

#include "giga-types.h"
#include "timing.h"

// The key/value storage partition was created when formatting the device previously with this code.
// It lives on partition 1 which is currently the only one we need.  Store 1MB of keys here with 15MB
// remaining for later expansion.
#define KVSTORE_PARTITION_NUMBER      1
#define KVSTORE_PARTITION_SIZE_MB     1
#define KVSTORE_PARTITION_SIZE       (KVSTORE_PARTITION_SIZE_MB * 1024 * 1024)

// The partition type is FAT32 with CHS addressing, see https://en.wikipedia.org/wiki/Partition_type#List_of_partition_IDs
#define KVSTORE_PARTITION_TYPE_FAT32  0x0B

// Define this to 1 during development/testing to force a reformat of the flash
#define KVSTORE_FORCE_REFORMAT        0

// Define registry keys for use with flash init
const char registry_tdb_initialized[] = "tdb.initialized";
const char registry_tdb_initialized_text[] = "format 1";  // Indicates the registry format version, for future use

class GigaStorage {

  private:
    USBHostMSD msd;
    mbed::FATFileSystem usb;

    QSPIFBlockDevice block_device;
    mbed::MBRBlockDevice tdb_data;

  public:
    //uint32_t registry_create_flags = mbed::KVStore::WRITE_ONCE_FLAG;
    uint32_t registry_create_flags = 0;

    mbed::TDBStore registry_store;

    enum rc_usb : uint8_t {
        USB_NO_ERROR = 0,
        NO_DEVICE,
        NOT_MOUNTABLE,
        NO_FILE,
        FILE_TOO_LARGE,
        FILE_NOT_READ
    };

    enum rc_flash : uint8_t {
        FLASH_NO_ERROR = 0,
        FLASH_UNFORMATTED,
        FLASH_FORMAT_FAILURE
    };

    GigaStorage() : msd(), 
                    usb("usb"),  
                    block_device(QSPI_SO0, QSPI_SO1, QSPI_SO2, QSPI_SO3, QSPI_SCK, QSPI_CS, QSPIF_POLARITY_MODE_1, 40000000),
                    tdb_data(&block_device, KVSTORE_PARTITION_NUMBER),
                    registry_store(&tdb_data)
                    {}

    void setup();
    GigaStorage::rc_flash flash_test();
    GigaStorage::rc_flash flash_format();
    GigaStorage::rc_usb usb_file_load(char* buffer, uint32_t* buffer_length, const char* filename);
    void print_mbed_error(int32_t mbed_err);

};

#endif