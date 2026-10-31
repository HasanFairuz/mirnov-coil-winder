# Firmware inventory and archival notes

The main demo firmware is located in [`final/WorkingDemo/WorkingDemo.ino`](final/WorkingDemo/WorkingDemo.ino). This is the latest supplied program and the reference for final pin assignments.

Each Arduino/Teensy sketch is placed in its own same-named folder for straightforward opening in Arduino IDE.

| Sketch | Purpose / status |
|---|---|
| `final/WorkingDemo/WorkingDemo.ino` | Final supplied demonstration code; reference implementation |
| `tests/Test_LimitSwitch/Test_LimitSwitch.ino` | Historical limit-switch / hardware check; motor pins agree with final source |
| `tests/Test_WorkingTwoMotors/Test_WorkingTwoMotors.ino` | Historical two-motor check; motor pins agree with final source |
| `tests/WorkingDryRun/WorkingDryRun.ino` | Historical traverse dry-run; traverse pins agree with final source |
| `tests/Test_PushButtons2/Test_PushButtons2.ino` | **Earlier hardware configuration**: spindle STEP/DIR pins 2/3 and traverse pins 4/5, not the final 28/29 and 24/25; do **not** upload to final wiring without reviewing and adapting it |

## Dependencies

Install Adafruit GFX Library and Adafruit SSD1306 through Arduino Library Manager. Adafruit BusIO may be installed as a dependency. Vendor source libraries are not redistributed here. The supplied sketches have not been independently compiled or retested during this repository preparation.

## Final pin-reference cross-check

The final `WorkingDemo.ino` declares spindle STEP 28 / DIR 29, traverse STEP 24 / DIR 25, driver enable 6, emergency stop 8, and limit switch 7. Verify the physical hardware before powering motors.

## Publication and rights

The project was developed by a team with an industry sponsor. Ensure that the team and applicable rights holders authorize public source-code distribution before pushing the firmware. Do not infer that publication of the repository grants third-party reuse rights.
