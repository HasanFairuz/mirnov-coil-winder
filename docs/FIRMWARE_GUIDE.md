# Firmware Guide

[← README](../README.md)

## Main sketch

`firmware/WorkingDemo/WorkingDemo.ino` is the archived final-team demonstration firmware. It programs Teensy 4.1 and the SSD1306 OLED and implements limit switch homing, motor configuration, winding layers, and completion displays. Build with Teensy-compatible Arduino tooling and install **Adafruit GFX Library**, **Adafruit SSD1306**, and its dependencies including **Adafruit BusIO** as required. The public layout has not been independently compiled or physically retested.

## Development tests

Selected archived test programs may be stored below `firmware/tests/`, separately from the main build. Their exact file names and behavior should be verified against the local test archive before publication.

**Caution:** The earlier `Test_PushButtons2.ino` uses a historical motor pin map (2–5), not the final sketch's 24/25 and 28/29 mapping. Never run it as a drop-in final-wiring diagnostic. Do not merge prototype code into `WorkingDemo.ino` without revalidation.

## Current pin map

See [Electrical and Control](ELECTRICAL_AND_CONTROL.md) for the final-sketch GPIO map. `BTN_RIGHT` is configured but non-functional in the final sketch. The step accumulators report commanded motion, not physical encoder-measured motion.
