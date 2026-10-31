# Winding Process and Synchronization

[← README](../README.md)

## Sequence

The delivered firmware initializes and checks operator inputs, homes the traverse, moves to a calibrated start position, allows selection of 1–20 layers, and winds alternating traverse passes with automatic direction changes. The completion display reports commanded spindle motion and completed layers.

## Nominal software motion

- Spindle: 200 full steps/revolution × 16 microsteps = **3,200 commands/revolution**.
- Traverse: same microstep scale and a nominal **1 mm/revolution lead screw**, yielding 3,200 commands/mm.
- Wire pitch: **0.127 mm per spindle revolution** requested by the command scheduler.
- Accumulator: for each spindle microstep, add 127; issue a traverse microstep each time the accumulator reaches 1,000, then subtract 1,000.
- Winding endpoints in final sketch: **3.8 mm** and **5.5 mm** (nominal **1.7 mm span**).
- Approximate commanded turns: 1.7 / 0.127 ≈ **13.4 per layer**.

These values reflect configured geometry and software counters, not calibrated independent feedback of shaft rotation or physical wire spacing. The original performance targets must not be mistaken for verified tolerances. Winding preparation, spool loading and wire attachment remain manual.
