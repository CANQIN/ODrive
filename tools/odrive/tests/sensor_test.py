
import test_runner

import time
import math
import os
import numpy as np

from odrive.enums import *
from test_runner import *

def test_assert_not_stale(data):
    if len(set(data)) <= 2:
        raise TestFailed(f"data seems stale: {set(data)}")

class TestVbusVoltage():
    """
    Checks if the onboard DC voltage sensor measures the expected test rig
    voltage without too much noise.
    """

    def get_test_cases(self, testrig: TestRig):
        for odrive in testrig.get_components(ODriveComponent):
            yield odrive, None

    def run_test(self, odrive: ODriveComponent, logger: Logger):
        # TODO: vary voltage with programmable power supply
        expected_voltage = float(odrive.yaml['vbus-voltage'])

        data = record_log(lambda: [odrive.handle.vbus_voltage], duration=5.0)
        slope, offset, fitted_curve = fit_line(data[:,(0,1)])
        test_assert_not_stale(data[:,1])
        test_assert_eq(offset, expected_voltage, accuracy=0.02)
        test_assert_eq(slope, 0, range=0.01)
        test_curve_fit(data[:,(0,1)], fitted_curve, max_mean_err = expected_voltage * 0.01, inlier_range = expected_voltage * 0.02, max_outliers = len(data[:,0]) * 0.01)

class TestOnboardTempSensors():
    """
    Checks if the onboard FET thermistor measures a plausible non-stale
    temperature without too much noise.
    """

    def get_test_cases(self, testrig: TestRig):
        for axis in testrig.get_components(ODriveAxisComponent):
            yield axis, None

    def run_test(self, axis: ODriveAxisComponent, logger: Logger):
        data = record_log(lambda: [axis.handle.motor.fet_thermistor.temperature], duration=5.0)
        slope, offset, fitted_curve = fit_line(data[:,(0,1)])
        test_assert_not_stale(data[:,1])
        test_assert_within(offset, 10, 80) # fet temp expected to be between 10°C and 80°C
        test_assert_eq(slope, 0, range=0.01)
        test_curve_fit(data[:,(0,1)], fitted_curve, max_mean_err = 0.2, inlier_range = 0.5, max_outliers = len(data[:,0]) * 0.01)


tests = [
  TestVbusVoltage(),
  TestOnboardTempSensors()
]

if __name__ == '__main__':
    test_runner.run(tests)
