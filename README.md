<div align="center">

# 🧲 Automated Mirnov Coil Winder

**General Fusion–sponsored SFU capstone · Teensy 4.1 · STEP/DIR motion control · 36 AWG magnet wire · SolidWorks · 3D printing**

An automated tabletop bobbin-winding prototype developed by **DynaBotix (Group 6)** for the **Simon Fraser University MSE 410/411 capstone** in collaboration with **General Fusion**. The prototype coordinates spindle rotation and linear traverse to wind fine insulated wire onto miniature bobbins for magnetic-field measurement coils used in plasma diagnostics.

[![Project](https://img.shields.io/badge/Project-SFU%20Capstone-2563EB?style=for-the-badge)](#-project-overview)
[![Controller](https://img.shields.io/badge/Controller-Teensy%204.1-0F766E?style=for-the-badge)](#-electronics-and-control)
[![Status](https://img.shields.io/badge/Status-Lab%20Prototype-CA8A04?style=for-the-badge)](#-testing-and-results)
[![Portfolio](https://img.shields.io/badge/Portfolio-Visit-7C3AED?style=for-the-badge)](https://hasanfairuz.com/)

</div>

---

## 🔎 Project Overview

Hand-winding a Mirnov coil requires the operator to regulate bobbin rotation, lateral placement of each turn, wire tension, counting, and layer transitions simultaneously. Our team designed and assembled a lab-scale system that automates the main winding motion while preserving hands-on bobbin preparation and wire threading.

<p align="center"><img src="media/Labelled%20Exploded%20CAD.png" alt="Annotated design overview of the automated bobbin winder" width="850" /></p>

### ✨ Key Features

- **Synchronized winding:** Dual NEMA 17 stepper axes coordinate spindle rotation with lead-screw traverse.
- **Nominal 0.127 mm pitch:** Software-controlled travel per spindle revolution, selected for the team's 36 AWG wire winding process.
- **Layer transitions:** Automatic traverse reversal at calibrated endpoints; operator selects 1–20 layers in the archived firmware.
- **Homing:** Limit-switch-based traverse referencing before winding.
- **Operator interface:** 128 × 64 OLED, four physical buttons (three active in the final sketch), and two status LEDs.
- **Safety controls:** Emergency-stop input, stepper driver enable handling, and a hardware startup verification sequence.
- **Modular construction:** Aluminum extrusion frame, linear guide, lead screw, printed brackets, and configurable wire guides.

## 📷 Prototype and Winding Results

<p align="center"><img src="media/Coil%20Winding.gif" alt="Coil winding media supplied by the project team" width="850" /></p>
<p align="center"><img src="media/Finished%20Coil.png" alt="Completed fine-wire coil on a bobbin" width="580" /></p>

### Prototype and Electronics

| Operator interface | Controller and motor drivers |
|:--:|:--:|
| <img src="media/User%20Interface.jpg" alt="Assembled OLED and pushbutton operator panel" width="380" /> | <img src="media/MCU%20and%20Motor%20Drivers.jpg" alt="Teensy controller and motor drivers on protoboard" width="380" /> |

Additional photographs: [assembled interface](media/Assembled%20Interface.png) · [soldered connections](media/Soldered%20Connections.jpg) · [winding close-up](media/Coil%20Winding.png) · [demo-day team photograph](media/GF%20Demo.jpg).

## 🧩 System Architecture

```text
36 AWG supply spool → passive drag / ceramic guides → ruby-tipped nozzle → bobbin
                                ↑                                ↑
                         wire-path alignment                spindle rotation
                                ↑                                ↑
                        lead-screw carriage           NEMA 17 spindle motor
                                ↑                                ↑
                        NEMA 17 traverse motor ──┐ ┌───────────┘
                                                 │ │
                                          TMC2209 drivers
                                                 ↑
                                          Teensy 4.1
                            ┌────────────────────┼────────────────────┐
                       OLED + buttons       limit + E-stop       status LEDs
```

The final control software uses **open-loop microstep commands**; no independent shaft encoder feedback is used to prove actual rotations or identify skipped steps. See [System Architecture](docs/SYSTEM_ARCHITECTURE.md).

## ⚙️ Mechanical Design

The assembly integrates a modular frame, spindle and bobbin holders, an MGN9 guide, lead-screw carriage with anti-backlash nut, a ruby-tipped nozzle, and ceramic wire guides. SolidWorks supported subsystem design, fit assessment, and assembly visualization. The team's **native SolidWorks CAD files are intentionally not published**.

<p align="center"><img src="media/Exploded%20CAD%20View.png" alt="Exploded SolidWorks view of the bobbin winder" width="850" /></p>

See [Mechanical Design](docs/MECHANICAL_DESIGN.md).

## 🔌 Electronics and Control

| Component | Role |
|:--|:--|
| Teensy 4.1 | Main controller, operator logic, coordinated step commands |
| Two TMC2209 drivers | STEP/DIR drive interfaces for traverse and spindle motors |
| Two NEMA 17 motors | Lead-screw traverse and bobbin spindle |
| SSD1306 128 × 64 OLED | State, layer progress, commanded turns, prompts |
| Limit switch | Traverse referencing |
| Emergency-stop input | Stop request and motor enable handling |
| Buttons and LEDs | Local configuration, status and operator feedback |

### Final firmware pin reference

| Function | Teensy pins |
|:--|:--|
| Traverse STEP / DIR | 24 / 25 |
| Spindle STEP / DIR | 28 / 29 |
| Shared enable | 6 |
| Left / Right / Up / Down | 36 / 37 / 15 / 14 |
| Limit / E-stop | 7 / 8 |
| Green / Red LED | 22 / 23 |
| OLED SDA / SCL | 18 / 19 |

The final sketch reserves the **Right** button but does not assign it an operational function. Check the final firmware before using historical wiring drawings or test programs: some earlier sketches used different motor pins.

<p align="center"><img src="media/Wiring%20Diagram.jpg" alt="Project electrical wiring illustration; verify against final sketch before use" width="850" /></p>

See [Electrical and Control](docs/ELECTRICAL_AND_CONTROL.md). This illustration is an archival design reference, **not a certified electrical construction diagram**.

## 🔄 Winding Process

The final sketch performs startup checks, limit-switch homing, offset travel to the calibrated start, selection of the number of layers, synchronized motion across the working span, reversal between layers, and completion reporting. In the archived code, spindle and traverse both use a nominal **3,200 microsteps/revolution**, with a 1 mm lead-screw pitch and a **127:1000 accumulator ratio** to achieve nominal 0.127 mm traverse per spindle revolution.

The calibrated travel window in the supplied final firmware is **3.8–5.5 mm** (1.7 mm across the bobbin). At this nominal pitch, the command model corresponds to about **13.4 turns per layer**, or **53.5 turns for four layers**. These figures describe commanded motion, not independently measured winding turn counts.

See [Winding Process](docs/WINDING_PROCESS.md) for the sequence and control assumptions.

## ✅ Testing and Results

The team's **August 2026 final report** records successful winding of multiple bobbins, improved side-by-side placement after traverse calibration, repeatable homing, operator-selectable layers, automatic completion, and emergency-stop recovery. This repository does not independently reproduce the hardware tests or establish measured turn-count error, cycle-time distributions, repeatability statistics, or quality-assurance certification.

| Outcome | Evidence level |
|:--|:--|
| Completed bobbins and close-up winding | Documented in final team report and project media |
| Synchronization, layer changes and homing | Implemented in final sketch; reported functional in team testing |
| Nominal 0.127 mm pitch | Software command ratio, not externally measured wire spacing |
| ±1-turn accuracy / sub-15-minute targets | Original project targets; not independently verified here |
| Industrial deployment readiness | Not claimed; lab prototype only |

See [Testing and Validation](docs/TESTING_AND_VALIDATION.md) and [Safety and Limitations](docs/SAFETY_AND_LIMITATIONS.md).

## 🧰 Firmware and Dependencies

The archived final program is [`firmware/final/WorkingDemo/WorkingDemo.ino`](firmware/final/WorkingDemo/WorkingDemo.ino). It targets Teensy 4.1 in the Arduino/Teensy toolchain and uses **Adafruit GFX**, **Adafruit SSD1306**, and their **Adafruit BusIO** dependency (as applicable to installed library versions). Install those libraries through their official distributions; vendor libraries are not duplicated in this repository.

Historical firmware tests are retained separately under `firmware/tests/`; they are **not all drop-in compatible with the final hardware pinout**. Refer to [Firmware Guide](docs/FIRMWARE_GUIDE.md). No public build or fresh hardware verification is claimed for this curated repository.

## 📁 Repository Guide

```text
mirnov-coil-winder/
├── README.md
├── .gitignore
├── docs/
│   ├── SYSTEM_ARCHITECTURE.md
│   ├── MECHANICAL_DESIGN.md
│   ├── ELECTRICAL_AND_CONTROL.md
│   ├── WINDING_PROCESS.md
│   ├── TESTING_AND_VALIDATION.md
│   ├── BOM_SUMMARY.md
│   ├── SAFETY_AND_LIMITATIONS.md
│   └── FIRMWARE_GUIDE.md
├── firmware/
│   ├── final/WorkingDemo/WorkingDemo.ino
│   └── tests/                     # selected archived test programs
└── media/                         # selected photographs, renders, GIF
```

## 🤝 Collaboration, Attribution and Rights

This is a **team-developed capstone**, not an independently developed personal project. The DynaBotix Group 6 team included **Mark Villanueva, Hasan Fairuz, Muhammad Sarim Obaid, and Sham Singh Namdhari**; academic supervision was provided at SFU, and **General Fusion** was the industry sponsor. The engineering outcomes described here are attributed to the team unless a specific individual contribution is documented separately.

This repository is a curated technical portfolio record, not an official General Fusion release. The original final report, user manual, sponsor-sensitive BOM, and editable native CAD models are **not included**. Permission to publish the firmware, wiring, images, and project-specific technical details should be established with applicable rights holders **before making the repository public**. No general license is asserted for third-party materials or project-owned IP.

## Portfolio

**Hasan Fairuz** — Mechatronic Systems Engineering

[Engineering Portfolio](https://hasanfairuz.com/) · [GitHub Profile](https://github.com/HasanFairuz)
