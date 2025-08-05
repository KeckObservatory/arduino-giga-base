/*
 * giga-storage.h: implementation of USB and flash storage for the Giga R1 board
 *
 */

#ifndef GIGA_STORAGE_H_
#define GIGA_STORAGE_H_

#include <stdint.h>
#include <stdbool.h>

#include <Arduino.h>
#include <DigitalOut.h>
#include <FATFileSystem.h>
#include <Arduino_USBHostMbed5.h>

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

  public:
    enum rc {
        NO_ERROR = 0,
        NO_DEVICE,
        NOT_MOUNTABLE,
        NO_FILE,
        FILE_TOO_LARGE,
        FILE_NOT_READ,
    };

    GigaStorage() : msd(), usb("usb") {}

    void setup();
    const char * get_error_text(GigaStorage::rc error);
    GigaStorage::rc load_file(char* buffer, uint32_t buffer_length, const char* filename);

    void clear();

};

#endif