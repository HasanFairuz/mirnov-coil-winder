# System Architecture

[← README](../README.md)

The DynaBotix winder combines a bobbin spindle and a lead-screw-based wire-guide traverse, each driven by a NEMA 17 stepper motor and TMC2209 driver under Teensy 4.1 control. The supply spool, passive drag, ceramic eyelets, and ruby-tipped nozzle form the fine-wire path. A local OLED, buttons, LEDs, limit switch, and emergency-stop input provide operator interaction and supervisory functions.

## Operating sequence

1. Run startup input checks and establish allowed system state.
2. Home the traverse against the reference limit switch, subject to a maximum search distance.
3. Move to the calibrated winding origin.
4. Select layers on the OLED interface.
5. Command synchronized spindle and traverse motion and reverse the traverse direction on alternate layers.
6. Stop motor drive and display the commanded winding outcome.

Both axes are open-loop. Mechanical slip, missed motor steps and actual wire placement are not detected automatically. The winding design is a laboratory prototype, not an industrially certified winding system.

See also [Mechanical Design](MECHANICAL_DESIGN.md), [Electrical and Control](ELECTRICAL_AND_CONTROL.md) and [Safety and Limitations](SAFETY_AND_LIMITATIONS.md).
