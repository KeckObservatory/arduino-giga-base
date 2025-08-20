#!/kroot/rel/default/bin/kpython3
#
# kpython safely sets RELDIR, KROOT, LROOT, and PYTHONPATH before invoking
# the actual Python interpreter.

# Sole Digital DRC-xT rope clamp load cell IOC

import os
import coloredlogs, logging
import argparse
import collections
import traceback
from concurrent import futures
from threading import Lock, Event
import datetime
import SerialStream

from softioc import softioc, builder

try:
    import epicscorelibs.path.pyepics
except ImportError:
    pass
import epics


# Default port for devices
LOADCELL_DEFAULT_PORT = 23

# How long between reconnection messages, in minutes?
RECONNECT_LOG_INTERVAL = 10

debug = False

class ChannelCreateException(BaseException):
    """Exception to indicate a channel could not be created"""
    pass


class CountdownTimer:
    """
    Define a timer that counts down.  Resolution in seconds.
    """
    def start(self, seconds=0, minutes=0, hours=0):
        """
        Start the timer for the provided duration.

        :param duration: Length of timer, in seconds.
        :return: Nothing.
        """
        self.timedelta = datetime.timedelta(seconds=seconds, minutes=minutes, hours=hours)
        self.tstart = self.now
        self.tend = self.tstart + self.timedelta

    def restart(self):
        """
        Restart the timer with the interval used last time.

        :return: Nothing.
        """
        self.tstart = self.now
        self.tend = self.tstart + self.timedelta

    @property
    def now(self): return datetime.datetime.today()

    @property
    def expired(self): return self.now >= self.tend

    def expire(self):
        """
        Expire the timer now.

        :return: Nothing.
        """
        self.tend = self.now

    @property
    def elapsed(self):
        """Determine how much time has elapsed since it started (in seconds)"""
        return int((self.now - self.tstart).total_seconds())

    @property
    def remaining(self):
        """Return how much time is remaining on the timer"""
        return (self.timedelta.total_seconds() - self.elapsed)


class DRCxT:
    """
    Main class for DRC-xT load cell
    """
    ioc_channels = [ ('connected', bool),
                     ('uptime', int),
                     ('load', int),
                     ]

    def __init__(self, prefix, address, log):

        # Task properties
        self.tickrate = 0.1 # Run the loop at 2 Hz, as the load cell is reporting state at 1Hz
        self.ready = Event()
        self.ready.clear()
        self._stop = Event()
        self._stop.clear()
        self.prefix = prefix
        self.log = log

        # Device communicaton properties
        self.address = address
        self.host = None
        self.port = None
        self.stream = None
        self.timeout = 2000  # ms

        # EPICS interface properties
        self.channels = dict()

        # Log timing
        self.reconnectLogTimer = CountdownTimer()
        self.reconnectLogTimer.expire()

    def startIOC(self):
        """Create the channels with the supplied prefix"""

        # Make sure the IOC prefix ends with a colon
        if not self.prefix.endswith(':'):
            self.prefix = prefix + ':'

        try:
            # Build channels about the IOC itself
            for channel, type in self.ioc_channels:
                name = self.prefix + channel
                self.log.info(f'Creating EPICS channel {name}')

                chan = self.createChannel(name, type)

                # Build the dictionary of channels
                self.channels[channel] = chan

            builder.LoadDatabase()

            # Load the IOC stats module
            softioc.devIocStats(ioc_name=f'{self.prefix}:stats')

            # Start the IOC running
            softioc.iocInit()

            # IOC is running, let the main thread run now
            self.log.debug('IOC thread started.')
            self.ready.set()

        except Exception as e:
            self.log.critical(f'Exception in IOC thread: {e}')
            self.log.critical(traceback.print_tb(e.__traceback__))
            return False

        # Finished
        return True

    def createChannel(self, name, type):
        """Create an EPICS channel database entry"""

        if type is float:
            result = builder.aOut(name, initial_value=0.0)
        elif type == int:
            result = builder.longOut(name, initial_value=0)
        elif type == str:
            result = builder.stringOut(name, initial_value='')
        elif type == bool:
            result = builder.boolOut(name, False, True, initial_value=False)
        else:
            raise ChannelCreateException(f'Unknown channel type {type} for {name}')

        return result

    def reconnectLog(self, message, show=False):
        """Throttle the reconnection logs"""

        # If the device was responding before this log was emitted, then it's a "fresh"
        # reconnect attempt, so show it.
        if self.client is not None:
            show = True

        # Log the reconnection if the timer has expired, which demonstrates that the process
        # is still alive
        if self.reconnectLogTimer.expired:
            show = True

        if show:
            self.log.warning(message)
            self.reconnectLogTimer.start(minutes=RECONNECT_LOG_INTERVAL)

    def ioDriver(self):
        """Run the IO for the device."""

        # ------------------------------------------------------------
        # Connect to and read from the PLC
        if self.stream is None:
            try:
                self.host, self.port = self.address.split(':')
            except ValueError:
                raise Exception('Invalid address specified, use "host:port" format')

            """Connect to the device via SerialStream (similar to telnetlib) TCP"""
            try:
                self.stream = SerialStream.factory(location=self.host, port=self.port, delimiter=None, bytes=True)
                self.stream.connect()
                self.stream.clear()
                self.log.info(f'Connected to device at {self.host}:{self.port}')
                self.channels['connected'].set(True)
            except BaseException as e:
                self.stream = None
                raise Exception(f'Cannot connect to device at {self.host}:{self.port}, {e}')

        # ------------------------------------------------------------
        # Process the incoming data
        try:
            # Look for the data stream
            message = self.stream.receive(delimiter=b'\n', strip=True, timeout=self.timeout)
            uptime, connected, _, load = message.decode('UTF-8').split(';')

            self.channels[f'uptime'].set(int(uptime, 16))
            self.channels[f'load'].set(int(load))

        except Exception as e:
            log.critical(f'Load cell messaging failed: {e}')

            # Close the connection
            try:
                self.stream.disconnect()
            except Exception as e:
                log.critical(f'SerialStream disconnection failure (ignored): {e}')
            finally:
                self.channels['connected'].set(False)
                self.stream = None

    @property
    def stopping(self): return self._stop.isSet()

    def stop(self):
        self._stop.set()

    def startDevice(self):
        """
        Main thread loop.
        """
        log.debug('Device thread waiting on IOC to start...')
        while not self.ready.wait(self.tickrate):
            pass

        log.debug('Device thread running.')
        while not self._stop.wait(self.tickrate):
            try:
                # Run the modbus IO once
                self.ioDriver()

            except Exception as e:
                log.critical(f'Exception during processing: {e}')
                log.critical(traceback.print_tb(e.__traceback__))

        self.channels['shconnected'].set(False)
        log.debug('Device thread stopped.')

if __name__ == "__main__":

    # -------------------------------------------------------------------------
    # Commandline arguments
    parser = argparse.ArgumentParser(description='Sole Digital DRC-xT rope clamp load cell IOC')
    parser.add_argument('-d', '--debug', help='Enable debugging output', action='store_true')
    parser.add_argument('-ioc', '--ioc', help='IOC prefix (e.g. k1:tcs:dom)', required=True, type=str, default='k1:tcs:dom')
    parser.add_argument('-addr', '--addr', help='Load cell device IP address (e.g. 10.96.15.32:502)', required=True, type=str)
    parser.add_argument('-iocport', '--iocport', help='IOC port', required=True, type=str)
    args = parser.parse_args()

    # Get the debug argument first, as it drives our logging choices
    if args.debug:
        debug = True

    # -------------------------------------------------------------------------
    # Set up the base logger all threads will use, once we know the debug flag
    coloredlogs.DEFAULT_LOG_FORMAT = '%(asctime)s [%(levelname)s] %(message)s'
    coloredlogs.DEFAULT_DATE_FORMAT = '%Y-%m-%d %H:%M:%S.%f'
    if debug:
        coloredlogs.install(level='DEBUG')
    else:
        coloredlogs.install(level='INFO')
    log = logging.getLogger('')

    if args.iocport:
        log.info(f'Setting server port to {args.iocport}')
        os.environ['EPICS_CA_SERVER_PORT'] = args.iocport

    prefix = 'k0:tcs:dom'
    if args.ioc:
        prefix = args.ioc
    log.info(f'Setting IOC channel name prefix to "{prefix}"')

    # -------------------------------------------------------------------------
    # Instantiate the device, which will contain two threads: the main thread
    # for telnet traffic, and the IOC thread
    device = DRCxT(prefix=prefix, address=args.addr, log=log)

    log.info('Starting load cell IOC.')

    # Create a task pool to track our thread, set workers to match the number of threads
    pool = futures.ThreadPoolExecutor(thread_name_prefix=prefix, max_workers=2)
    try:
        # Start the thread
        iocthread = pool.submit(device.startIOC)
        devicethread = pool.submit(device.startDevice)

        # Wait for the thread to complete
        for r in futures.as_completed([iocthread, devicethread]):
            pool.shutdown(True)
            log.info('Shutdown: threads stopped normally.')

    except (SystemExit, KeyboardInterrupt):
        log.info('Exit/Interrupt: shutting down.')
        device.stop()
        pool.shutdown(False)

    log.info('Done.')
