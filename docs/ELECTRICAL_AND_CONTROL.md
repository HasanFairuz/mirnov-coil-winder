# Electrical Architecture and Control

[← README](../README.md)

## Installed architecture

The supplied final sketch configures a Teensy 4.1 with separate STEP/DIR outputs for spindle and traverse motors, a shared driver enable, a 128×64 I²C SSD1306 OLED, four buttons, a homing limit switch, an emergency-stop input, and two status LEDs.

| Signal | Teensy GPIO |
|:--|--:|
| Traverse STEP / DIR | 24 / 25 |
| Spindle STEP / DIR | 28 / 29 |
| Driver enable | 6 |
| Buttons LEFT / RIGHT / UP / DOWN | 36 / 37 / 15 / 14 |
| Limit switch / E-stop | 7 / 8 |
| Green / Red LED | 22 / 23 |
| I²C SDA / SCL | 18 / 19 |

**Archived source code is the authority for the current programmed mapping.** Earlier test sketches and drawings may use different pins. Do not assume that a historical schematic exactly reproduces the final wiring.

![Wiring diagram](../media/Wiring%20Diagram.jpg)

![Interface assembly](../media/Assembled%20Interface.png)

## Motion and safety

The program uses open-loop STEP/DIR pulses, software-managed motor enable, limit-switch homing, and emergency-stop monitoring. Firmware stop handling is not a substitute for a professionally validated hardwired safety function. Always verify driver logic voltage, supply rails, motor wiring, electrical isolation, and emergency-stop circuit behavior before any physical reuse.

## Libraries

The firmware includes `Adafruit_GFX.h` and `Adafruit_SSD1306.h`. Obtain Adafruit GFX Library, Adafruit SSD1306, and appropriate Adafruit BusIO versions from official library sources. Third-party library copies are not bundled.
