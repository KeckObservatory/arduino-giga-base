# Sole Digital Rope Clamp Load Cell sensor 
### Installation Procedure 
###### August 2025, Paul Richards, W. M. Keck Observatory

* * *

## Abstract

This document contains instructions on how to perform the various installation stages of a load cell device.  The steps are divided into procedures that cover each task needed to deploy a load sensor.

Packaging of the Arduino stack and its enclosure is beyond the scope of this documentation.

## Installation Phases

The installation of a single load cell has four distinct procedures that must be followed before data from a load cell will be visible in the EPICS archiver and the Grafana dashboards.

## Procedure 1 - Deployment plan and config file creation

In this procedure, the deployed configuration is represented in an INI file for use with the Arduino stack.

Required components:
* One or more USB flash drives formatted for MS-DOS (FAT32), no larger than 32GB.
* [TBD] One or more sets of load cell calibrations intended for use with 

1) Identify the port(s) on a PoE network switch where the load cells will be installed.
1) Obtain one or more IP addresses from Keck computer support to be assigned to this and other load cell devices.
1) Note the subnet mask (usually either 255.255.255.0 or 255.255.0.0), the gateway address (normally x.y.z.1 or x.y.0.1), and DNS settings (can default to 128.171.1.1 within the Keck ops network).
1) [TBD] Obtain the calibration coefficients associated with each load cell that is to be installed.
1) For the first load cell to be deployed, edit the `config.ini` file located in the root of this GitHub repository (https://github.com/KeckObservatory/arduino-giga-base) and set the lines as follows:

    ```
    net.ip = IP_address_of_device
    net.netmask = netmask_value
    net.gateway = gateway_value
    net.dns = 128.171.1.1
    ```
1) Repeat the previous step for each device to be installed.  Essentially, as each one is prepared (using the next procedure), change the IP address in the file for use with the next one.

## Procedure 2 - Arduino GIGA stack preparation

In this procedure, an Arduino GIGA is assembled with two shields and programmed with released firmware.

Required components:
* Arduino GIGA device
* USB-C to USB-A cable (provided with Arduino GIGA)
* Ethernet shield
* RS-485 shield (modified to provide clearance of the RJ-45 port on the Ethernet shield)
* Label maker

1) Prepare the Arduino development environment using the procedure detailed in the [Sole Digital Rope Load Cell Sensor](Sole_Digital_Rope_Load_Cell_Sensor.md) document.
1) Assemble one or more Arduino stacks.
1) Insert the USB flash drive created in Procedure 1 into the Arduino's port.
1) Connect an assembled stack to the PC via the USB cable.
1) Use the Tools > Port menu to select the Arduino, if it is not automatically detected.
1) Upload the compiled firmware to the Arduino.
1) Monitor the serial console in the Arduino IDE to ensure the device accepted the `config.ini` file contents.  The final lines of output will look like this:

    ```
    13:46:13.200 -> [ETH] Configuring IP address 10.77.0.211 (netmask 255.255.0.0, gateway 10.77.0.1, dns 128.171.1.1)
    13:46:13.763 -> [ETH] Starting TCP/IP server.
    13:46:13.763 -> >>> Init: load cell.
    13:46:13.763 -> >>> Initialization complete.
    ```
1) Remove the USB flash drive.
1) Press any of the 3 RESET buttons on the Arduino.
1) Verify again on the serial console that the settings were read from internal flash and that the green LED is blinking at 1Hz.
1) Print a label with the IP address 


## Procedure 3 - Installation in the field

In this procedure, the Arduino stack is installed for operational usage.

## Procedure 4 - Configuration of the EPICS IOC

In this procedure, an EPICS IOC is configured to recieve the telemetry from the Arduino stack.




## Procedure - Load cell repair/replacement

## Procedure - Arduino GIGA stack repair/replacement




