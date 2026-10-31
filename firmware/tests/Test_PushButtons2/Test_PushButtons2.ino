// Dynabotix Mirnov Coil Winder
// Teensy 4.1 + Dual TMC2209 + SSD1306 OLED + Buttons + LEDs
// Pseudo Coil Winding Functional Test
//
// Behavior:
// - Green LED ON while idle/ready
// - Red LED ON while winding
// - OLED prompts user through button checks
// - User confirms: Right/OK, Up, Down, Left
// - Motors perform one pseudo winding cycle
// - After completion, asks user to press OK/RIGHT to run another test

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------- OLED SETTINGS ----------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledReady = false;

// ---------------- MOTOR PIN DEFINITIONS ----------------
// Motor 1: Spindle
const int SPN_STEP_PIN = 2;
const int SPN_DIR_PIN  = 3;

// Motor 2: Traverse
const int TRV_STEP_PIN = 4;
const int TRV_DIR_PIN  = 5;

// Shared enable for both TMC2209 drivers
const int EN_PIN = 6;

// ---------------- BUTTON PIN DEFINITIONS ----------------
const int BTN_RIGHT_PIN = 9;   // OK / Right
const int BTN_LEFT_PIN  = 10;  // Back / Left
const int BTN_UP_PIN    = 11;  // Up / +
const int BTN_DOWN_PIN  = 12;  // Down / -

// ---------------- LED PIN DEFINITIONS ----------------
// Recommended: avoid pin 13 if external LED behavior is confusing.
// Pin 13 also has the Teensy onboard LED.
const int GREEN_LED_PIN = 13;  // Ready / idle
const int RED_LED_PIN   = 14;  // Winding / active

// ---------------- MOTOR SETTINGS ----------------
const int FULL_STEPS_PER_REV = 200;
const int MICROSTEPS = 16;
const int STEPS_PER_REV = FULL_STEPS_PER_REV * MICROSTEPS;

// ---------------- SPEED SETTINGS ----------------
const int STEP_PULSE_US = 5;
const int STEP_DELAY_US = 800;

// ---------------- PSEUDO WINDING SETTINGS ----------------
// For pseudo demo only.
// This does not yet implement true 0.127 mm/rev traverse pitch.
const int WINDING_REVOLUTIONS = 3;
const int TRAVERSE_STEPS_PER_SEGMENT = 800;

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("Dynabotix Pseudo Coil Winding Test Starting...");

  // OLED initialization
  Wire.begin();

  if (display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    oledReady = true;
    displayStatus("Dynabotix", "Pseudo Winding", "Initializing", "Please wait");
    Serial.println("OLED initialized.");
  } else {
    oledReady = false;
    Serial.println("OLED initialization failed. Continuing without OLED.");
  }

  // Motor pins
  pinMode(SPN_STEP_PIN, OUTPUT);
  pinMode(SPN_DIR_PIN, OUTPUT);
  pinMode(TRV_STEP_PIN, OUTPUT);
  pinMode(TRV_DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);

  // Button pins
  pinMode(BTN_RIGHT_PIN, INPUT_PULLUP);
  pinMode(BTN_LEFT_PIN, INPUT_PULLUP);
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);

  // LED pins
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  // Default motor states
  digitalWrite(SPN_STEP_PIN, LOW);
  digitalWrite(SPN_DIR_PIN, LOW);
  digitalWrite(TRV_STEP_PIN, LOW);
  digitalWrite(TRV_DIR_PIN, LOW);

  // Disable drivers during setup
  // TMC2209 EN is active LOW:
  // LOW = enabled
  // HIGH = disabled
  digitalWrite(EN_PIN, HIGH);

  // Idle state: green ON, red OFF
  setIdleLEDs();

  displayStatus("Dynabotix", "System Ready", "Green = Ready", "Red = Winding");
  delay(1500);
}

// ---------------- MAIN LOOP ----------------
void loop() {
  runUserPromptSequence();
  runPseudoWindingCycle();
  askToRunAgain();
}

// ---------------- USER PROMPT SEQUENCE ----------------
void runUserPromptSequence() {
  setIdleLEDs();
  digitalWrite(EN_PIN, HIGH);  // keep drivers disabled while waiting

  // Step 1: Confirm winding
  displayStatus("Confirm Winding", "Press RIGHT / OK", "to continue", "");
  Serial.println("Waiting for RIGHT / OK button...");
  waitForButtonPress(BTN_RIGHT_PIN);

  displayStatus("Input Check", "RIGHT confirmed", "Next: press UP", "");
  delay(500);

  // Step 2: Press UP
  displayStatus("Input Check", "Press UP / +", "to proceed", "");
  Serial.println("Waiting for UP button...");
  waitForButtonPress(BTN_UP_PIN);

  displayStatus("Input Check", "UP confirmed", "Next: press DOWN", "");
  delay(500);

  // Step 3: Press DOWN
  displayStatus("Input Check", "Press DOWN / -", "to proceed", "");
  Serial.println("Waiting for DOWN button...");
  waitForButtonPress(BTN_DOWN_PIN);

  displayStatus("Input Check", "DOWN confirmed", "Next: press LEFT", "");
  delay(500);

  // Step 4: Press LEFT
  displayStatus("Input Check", "Press LEFT / BACK", "to proceed", "");
  Serial.println("Waiting for LEFT button...");
  waitForButtonPress(BTN_LEFT_PIN);

  displayStatus("Input Check", "LEFT confirmed", "Starting winding", "");
  delay(1000);
}

// ---------------- ASK TO RUN AGAIN ----------------
void askToRunAgain() {
  setIdleLEDs();
  digitalWrite(EN_PIN, HIGH);  // disable drivers while idle

  displayStatus("Test Complete", "Run another test?", "Press RIGHT / OK", "to repeat");

  Serial.println("Test complete.");
  Serial.println("Waiting for RIGHT / OK to run another test...");

  waitForButtonPress(BTN_RIGHT_PIN);

  displayStatus("Restarting", "New test cycle", "Please wait", "");
  Serial.println("Restarting test cycle...");
  delay(1000);
}

// ---------------- PSEUDO WINDING CYCLE ----------------
void runPseudoWindingCycle() {
  Serial.println("Starting pseudo winding cycle...");

  // Enable drivers
  digitalWrite(EN_PIN, LOW);

  // Active state: green OFF, red ON
  setWindingLEDs();

  displayStatus("WINDING", "Motors running", "Pseudo cycle", "Do not touch");

  // Set directions
  digitalWrite(SPN_DIR_PIN, HIGH);
  digitalWrite(TRV_DIR_PIN, HIGH);

  // Segment 1: Spindle and traverse together
  Serial.println("Segment 1: forward traverse");
  displayStatus("WINDING", "Segment 1", "Traverse forward", "Running");

  for (int rev = 0; rev < WINDING_REVOLUTIONS; rev++) {
    moveBothMotorsDifferentSteps(STEPS_PER_REV, TRAVERSE_STEPS_PER_SEGMENT);
  }

  delay(500);

  // Reverse traverse direction
  digitalWrite(TRV_DIR_PIN, LOW);

  // Segment 2: Spindle continues, traverse reverses
  Serial.println("Segment 2: reverse traverse");
  displayStatus("WINDING", "Segment 2", "Traverse reverse", "Running");

  for (int rev = 0; rev < WINDING_REVOLUTIONS; rev++) {
    moveBothMotorsDifferentSteps(STEPS_PER_REV, TRAVERSE_STEPS_PER_SEGMENT);
  }

  delay(500);

  // Disable drivers after one cycle
  digitalWrite(EN_PIN, HIGH);

  // Back to idle LEDs
  setIdleLEDs();

  Serial.println("Pseudo winding cycle complete.");
  displayStatus("Cycle Complete", "Motors stopped", "Drivers disabled", "Done");
  delay(1500);
}

// ---------------- MOTOR MOVE FUNCTIONS ----------------

// Move spindle and traverse together using different step counts.
// This is a simple proportional stepping routine for demo purposes.
void moveBothMotorsDifferentSteps(int spindleSteps, int traverseSteps) {
  int maxSteps = max(spindleSteps, traverseSteps);

  for (int i = 0; i < maxSteps; i++) {
    bool stepSpindle = (i < spindleSteps);
    bool stepTraverse = false;

    if (traverseSteps > 0) {
      stepTraverse = ((long)i * traverseSteps / maxSteps) != ((long)(i - 1) * traverseSteps / maxSteps);
    }

    if (stepSpindle) {
      digitalWrite(SPN_STEP_PIN, HIGH);
    }

    if (stepTraverse) {
      digitalWrite(TRV_STEP_PIN, HIGH);
    }

    delayMicroseconds(STEP_PULSE_US);

    if (stepSpindle) {
      digitalWrite(SPN_STEP_PIN, LOW);
    }

    if (stepTraverse) {
      digitalWrite(TRV_STEP_PIN, LOW);
    }

    delayMicroseconds(STEP_DELAY_US);
  }
}

// ---------------- BUTTON HANDLING ----------------

void waitForButtonPress(int buttonPin) {
  // Wait until button is released first
  while (digitalRead(buttonPin) == LOW) {
    delay(10);
  }

  // Wait for press
  while (digitalRead(buttonPin) == HIGH) {
    delay(10);
  }

  // Debounce delay
  delay(50);

  // Wait for release
  while (digitalRead(buttonPin) == LOW) {
    delay(10);
  }

  // Debounce release
  delay(50);
}

// ---------------- LED STATES ----------------
// Use this version for active-high LED wiring:
// Teensy pin -> 1k resistor -> LED anode -> LED cathode -> GND

void setIdleLEDs() {
  digitalWrite(GREEN_LED_PIN, HIGH);  // Green ON
  digitalWrite(RED_LED_PIN, LOW);     // Red OFF
}

void setWindingLEDs() {
  digitalWrite(GREEN_LED_PIN, LOW);   // Green OFF
  digitalWrite(RED_LED_PIN, HIGH);    // Red ON
}

// ---------------- OLED DISPLAY ----------------

void displayStatus(String line1, String line2, String line3, String line4) {
  if (!oledReady) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("Dynabotix Winder");

  display.setCursor(0, 16);
  display.println(line1);

  display.setCursor(0, 28);
  display.println(line2);

  display.setCursor(0, 40);
  display.println(line3);

  display.setCursor(0, 52);
  display.println(line4);

  display.display();
}