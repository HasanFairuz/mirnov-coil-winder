# Testing and Validation

[← README](../README.md)

The **August 2026 team final report** records multiple completed fine-wire bobbins and functional testing of startup verification, limit-switch homing, selectable layer operation, automatic traverse reversal, end-of-cycle stop, and emergency-stop recovery. Images of completed coils and winding setup support documentation of a built prototype.

![Finished coil](../media/Finished%20Coil.png)

## Evidence boundaries

| Claim | Documentation status |
|:--|:--|
| Working laboratory prototype and completed winding samples | Reported by project team |
| Software spindle/traverse synchronization | Implemented in archived final sketch |
| Commanded 0.127 mm traverse pitch | Calculated from firmware parameters |
| Physical ±1-turn accuracy | Not independently confirmed by encoder data |
| Sub-15-minute cycle | Design/presentation target; no independent timing distribution reproduced |
| Process repeatability / edge quality | Qualitatively described in team's final report |
| Industrial safety/quality certification | Not established |

The repository should not promise a particular number of reliable cycles, measured positioning accuracy, payload or production qualification. Tests were conducted during the team's development, and the public repository package has not been independently built and re-tested.
