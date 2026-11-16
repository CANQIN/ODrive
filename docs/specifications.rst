================================================================================
Specifications
================================================================================

.. contents::
   :depth: 1
   :local:
   
Electrical Specifications
--------------------------------------------------------------------------------

This project supports ODrive v3.6-56V, with an input voltage range of 12V to 56V.

.. note:: ODrive v3.6 board files and schematics are not publicly available.

ODrive v3.6-56V
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Peak current per motor: 120 Amps
* Max continuous current depends on cooling. See `this <https://discourse.odriverobotics.com/t/odrive-mosfet-temperature-rise-measurements-using-the-onboard-thermistor/972>`__ for more details.
    * Heatsink in still air: 40A per channel
    * Heatsink with basic fan cooling: 75A per channel
    * Heatsink with overkill fan cooling: 90A per channel
* Max motor RPM: This depends on your power supply voltage, motor, and encoder. It is the lesser of: 
    * motor RPM limit
    * encoder RPM limit
    * motor KV * 0.7 * Supply voltage
    * 35000 eRPM / # of motor pole pairs
    * (840M counts/minute) / encoder counts per revolution (for incremental encoders - 4 x pulses per revolution).

Mechanical Specifications
--------------------------------------------------------------------------------

Board Outline and Mounting Hole Dimensions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. image:: figures/mech_dimensions.png
    :scale: 30 %
    :align: center
    :alt: board dimensions
