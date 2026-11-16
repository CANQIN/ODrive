
import test_runner

import struct
import time
import os
import io
import functools
import operator

from fibre.utils import Logger
from odrive.enums import *
from test_runner import *


def append_checksum(command):
    return command + b'*' + str(functools.reduce(operator.xor, command)).encode('ascii')

def strip_checksum(command):
    command, _, checksum = command.partition(b'*')
    test_assert_eq(int(checksum.strip()), functools.reduce(operator.xor, command))
    return command

def reset_state(ser):
    """Resets the state of the ASCII protocol by flushing all buffers"""
    ser.flushOutput() # ensure that all previous bytes are sent
    time.sleep(0.1) # wait for ODrive to handle last input (buffer might be full)
    ser.write(b'\n') # terminate line
    ser.flushOutput() # ensure that end-of-line is sent
    time.sleep(0.1) # wait for any response that this may generate
    ser.flushInput() # discard response

class UartTest():
    """
    Base class for UART tests
    """

    def get_test_cases(self, testrig: TestRig):
        for odrive in testrig.get_components(ODriveComponent):
            ports = testrig.get_connected_components({
                'rx': (odrive.gpio1, True),
                'tx': (odrive.gpio2, False)
            }, SerialPortComponent)
            yield AnyTestCase(*[(odrive, 0, 1, 2, port, tf) for port, tf in ports])

            # Enable the line below to manually test UART_B. For this you need
            # to move the UART wires from GPIO1/2 to GPIO3/4.
            #yield (odrive, 1, 3, 4, ports)

    def prepare(self, odrive: ODriveComponent, uart_num: int, tx_gpio: list, rx_gpio: list, logger: Logger):
        logger.debug('Enabling UART {}...'.format(chr(ord('A') + uart_num)))
        
        # GPIOs might be in use by something other than UART and some components
        # might be configured so that they would fail in the later test.
        odrive.disable_mappings()
        odrive.handle.config.enable_uart_a = False
        odrive.handle.config.uart_a_baudrate = 115200
        odrive.handle.config.enable_uart_b = False
        odrive.handle.config.uart_b_baudrate = 115200
        odrive.handle.config.enable_uart_c = False
        odrive.handle.config.uart_c_baudrate = 115200

        if uart_num == 0:
            odrive.handle.config.enable_uart_a = True
            mode = GPIO_MODE_UART_A
        elif uart_num == 1:
            odrive.handle.config.enable_uart_b = True
            mode = GPIO_MODE_UART_B
        elif uart_num == 2:
            odrive.handle.config.enable_uart_c = True
            mode = GPIO_MODE_UART_C
        else:
            raise TestFailed(f"unknown UART: {uart_num}")
        setattr(odrive.handle.config, f'gpio{tx_gpio}_mode', mode)
        setattr(odrive.handle.config, f'gpio{rx_gpio}_mode', mode)

        odrive.save_config_and_reboot()

class TestUartAscii(UartTest):
    """
    Tests the most important functions of the ASCII protocol.
    """

    def run_test(self, odrive: ODriveComponent, uart_num: int, tx_gpio: list, rx_gpio: list, port: SerialPortComponent, logger: Logger):
        self.prepare(odrive, uart_num, tx_gpio, rx_gpio, logger)

        with port.open(115200) as ser:
            # reset port to known state
            reset_state(ser)

            # Read a top-level attribute
            ser.write(b'r vbus_voltage\n')
            response = float(ser.readline().strip())
            test_assert_eq(response, odrive.handle.vbus_voltage, accuracy=0.1)

            # Read an unknown attribute
            ser.write(b'r blahblah\n')
            response = ser.readline().strip()
            test_assert_eq(response, b'invalid property')

            # Send command with delays in between
            for byte in b'r vbus_voltage\n':
                ser.write([byte])
                time.sleep(0.1)
            response = float(ser.readline().strip())
            test_assert_eq(response, odrive.handle.vbus_voltage, accuracy=0.1)

            # Test GCode checksum and comments
            ser.write(b'r vbus_voltage *12\n') # invalid checksum
            test_assert_eq(ser.readline(), b'')
            cmd = append_checksum(b'r vbus_voltage ') + b' ; this is a comment\n'
            # cmd evalutates to "r vbus_voltage *93 ; this is a comment"
            logger.debug(f'sending command with checksum: "{cmd}"')
            ser.write(cmd) # valid checksum
            response = float(strip_checksum(ser.readline()).strip())
            test_assert_eq(response, odrive.handle.vbus_voltage, accuracy=0.1)

            # Read an attribute with a long name
            ser.write(b'r axis0.motor.current_control.v_current_control_integral_d\n')
            response = float(ser.readline().strip())
            test_assert_eq(response, odrive.handle.axis0.motor.current_control.v_current_control_integral_d, accuracy=0.1)

            # Write an attribute
            ser.write(b'w test_property 12345\n')
            ser.write(b'r test_property\n')
            response = int(ser.readline().strip())
            test_assert_eq(response, 12345)

            # Test custom setter (aka property write hook)
            odrive.handle.axis0.motor.config.phase_resistance = 1
            odrive.handle.axis0.motor.config.phase_inductance = 1
            odrive.handle.axis0.motor.config.current_control_bandwidth = 1000
            old_gain = odrive.handle.axis0.motor.current_control.p_gain
            test_assert_eq(old_gain, 1000, accuracy=0.0001) # must be non-zero for subsequent check to work
            ser.write('w axis0.motor.config.current_control_bandwidth {}\n'.format(odrive.handle.axis0.motor.config.current_control_bandwidth / 2).encode('ascii'))
            test_assert_eq(ser.readline(), b'')
            test_assert_eq(odrive.handle.axis0.motor.current_control.p_gain, old_gain / 2, accuracy=0.0001)

            # Test 'c', 'v', 'p', 'q' and 'f' commands

            odrive.handle.axis0.controller.input_torque = 0
            ser.write(b'c 0 12.5\n')
            test_assert_eq(ser.readline(), b'')
            test_assert_eq(odrive.handle.axis0.controller.input_torque, 12.5, accuracy=0.001)
            test_assert_eq(odrive.handle.axis0.controller.config.control_mode, CONTROL_MODE_TORQUE_CONTROL)

            odrive.handle.axis0.controller.input_vel = 0
            odrive.handle.axis0.controller.input_torque = 0
            ser.write(b'v 0 567.8 12.5\n')
            test_assert_eq(ser.readline(), b'')
            test_assert_eq(odrive.handle.axis0.controller.input_vel, 567.8, accuracy=0.001)
            test_assert_eq(odrive.handle.axis0.controller.input_torque, 12.5, accuracy=0.001)
            test_assert_eq(odrive.handle.axis0.controller.config.control_mode, CONTROL_MODE_VELOCITY_CONTROL)

            odrive.handle.axis0.controller.input_pos = 0
            odrive.handle.axis0.controller.input_vel = 0
            odrive.handle.axis0.controller.input_torque = 0
            ser.write(b'p 0 123.4 567.8 12.5\n')
            test_assert_eq(ser.readline(), b'')
            test_assert_eq(odrive.handle.axis0.controller.input_pos, 123.4, accuracy=0.001)
            test_assert_eq(odrive.handle.axis0.controller.input_vel, 567.8, accuracy=0.001)
            test_assert_eq(odrive.handle.axis0.controller.input_torque, 12.5, accuracy=0.001)
            test_assert_eq(odrive.handle.axis0.controller.config.control_mode, CONTROL_MODE_POSITION_CONTROL)

            odrive.handle.axis0.controller.input_pos = 0
            odrive.handle.axis0.controller.config.vel_limit = 0
            odrive.handle.axis0.motor.config.current_lim = 0
            ser.write(b'q 0 123.4 567.8 12.5\n')
            test_assert_eq(ser.readline(), b'')
            test_assert_eq(odrive.handle.axis0.controller.input_pos, 123.4, accuracy=0.001)
            test_assert_eq(odrive.handle.axis0.controller.config.vel_limit, 567.8, accuracy=0.001)
            test_assert_eq(odrive.handle.axis0.motor.config.torque_lim, 12.5, accuracy=0.001)
            test_assert_eq(odrive.handle.axis0.controller.config.control_mode, CONTROL_MODE_POSITION_CONTROL)

            ser.write(b'f 0\n')
            response = ser.readline().strip()
            test_assert_eq(float(response.split()[0]), odrive.handle.axis0.encoder.pos_estimate, accuracy=0.001)
            test_assert_eq(float(response.split()[1]), odrive.handle.axis0.encoder.vel_estimate, accuracy=0.001)

            test_watchdog(odrive.handle.axis0, lambda: ser.write(b'u 0\n'), logger)
            test_assert_eq(ser.readline(), b'') # check if the device remained silent during the test


            # TODO: test cases for 't', 'ss', 'se', 'sr' commands


class TestUartBaudrate(UartTest):
    """
    Tests if the UART baudrate setting works as intended.
    """

    def run_test(self, odrive: ODriveComponent, uart_num: int, tx_gpio: list, rx_gpio: list, port: SerialPortComponent, logger: Logger):
        self.prepare(odrive, uart_num, tx_gpio, rx_gpio, logger)

        odrive.handle.config.uart_a_baudrate = 9600
        odrive.save_config_and_reboot()

        # Control test: talk to the ODrive with the wrong baudrate
        with port.open(115200) as ser:
            # reset port to known state
            reset_state(ser)

            ser.write(b'r vbus_voltage\n')
            test_assert_eq(ser.readline().strip(), b'')

        with port.open(9600) as ser:
            # reset port to known state
            reset_state(ser)

            # Check if protocol works
            ser.write(b'r vbus_voltage\n')
            response = float(ser.readline().strip())
            test_assert_eq(response, odrive.handle.vbus_voltage, accuracy=0.1)

        odrive.handle.config.uart_a_baudrate = 115200
        odrive.save_config_and_reboot()


class TestUartBurnIn(UartTest):
    """
    Tests if the ASCII protocol can handle 64kB of random data being thrown at it.
    """

    def run_test(self, odrive: ODriveComponent, uart_num: int, tx_gpio: list, rx_gpio: list, port: SerialPortComponent, logger: Logger):
        self.prepare(odrive, uart_num, tx_gpio, rx_gpio, logger)

        with port.open(115200) as ser:
            with open('/dev/random', 'rb') as rand:
                buf = rand.read(65536)
            ser.write(buf)

            # reset port to known state
            reset_state(ser)

            # Check if protocol still works
            ser.write(b'r vbus_voltage\n')
            response = float(ser.readline().strip())
            test_assert_eq(response, odrive.handle.vbus_voltage, accuracy=0.1)


tests = [
    TestUartAscii(),
    TestUartBaudrate(),
    TestUartBurnIn()
]

if __name__ == '__main__':
    test_runner.run(tests)
