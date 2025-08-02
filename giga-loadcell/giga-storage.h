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

#include "giga-usb-ini.h"
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
        CONFIG_INI_LOAD_SUCCESS,
        NO_DEVICE,
        NOT_MOUNTABLE,
        NO_CONFIG_INI,
        CONFIG_INI_PARSE_FAILED,
        CONFIG_INI_MISSING_KEY,
    };


    GigaStorage() : msd(), usb("usb") {}


    void setup();
    const char * get_error_text(uint8_t error);
    GigaStorage::rc load();

    void clear();

};

#endif