"""
cal_fit.py - calculate the slope and constant for a given set of wire rope load cell calibration
data provided by the vendor

QUICKSTART: Either type or OCR the calibration values provided. Put these values into a text
file where this Python code runs.  See example file "2668.txt".  Run "python3 cal_fit.py 2668.txt"
to generate the cal constants and put that into the config.ini file of the USB disk associated
with that load cell.


Per the initial data set provided in October 2025, the measured values for load cell serial
numbered 2668 are as follows:

Kg	    ADC reading
0	    19600
825	    165700
1960	410600
2990	601700
6250	1185900
10280	1877500

In this code we perform a Numpy polyfit, to generate two constants that are used in an
equation to convert the 12 bit ADC value into kilograms.
"""

import os
import sys

import numpy as np
import re

serial_number = 0
x = list()
y = list()

try:
    filename = sys.argv[1]

    if not os.path.isfile(filename):
        print('File not found: ' + filename)
        sys.exit(1)

except IndexError:
    print('No filename provided')
    sys.exit(1)

with open(filename, "r") as file:
    lines = file.readlines()
    for line in lines:
        line = line.strip()

        # Skip blank lines
        if line in ['']:
            continue

        # Skip comments
        if line.startswith('#'):
            continue

        if line.startswith('SN'):
            match = re.match(r'SN\s+(\d+)', line)
            if match:
                serial_number = match.group(1)
                continue
            else:
                print(f'Serial number line malformed ("{line}").  Example: SN 1234')
                sys.exit(1)

        # Process lines that are simply two numbers
        match = re.match(r'(\d+)\s+(\d+)', line)
        if match:
            x.append(float(match.group(1)))
            y.append(float(match.group(2)))

        else:
            print('Ignoring spurious line: ' + line)

# Fit to a 1st order polynomial
z = np.polyfit(y, x, 1)

mx = z[0]
b = z[1]

if serial_number == 0:
    print(f'No serial number detected in file, example:')
    print(f'SN 1234')
else:
    print('Paste the following lines into the config.ini file for the load cell:')
    print('sensor.sn = ' + f'{serial_number}')
    print('cal.slope = ' + f'{mx:0.4g}')
    print('cal.const = ' + f'{b:0.4g}')
