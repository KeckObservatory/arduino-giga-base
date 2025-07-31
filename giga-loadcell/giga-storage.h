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

// Define the configuration filenames
const char config_ini_filename[] = "config.ini";
const char config_ini_saved_filename[] = "config.ini.S";


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


    GigaStorage() : usb("usb") {}


    void setup();
    GigaStorage::rc load();

    void clear();

};

#endif