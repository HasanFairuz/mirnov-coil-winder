#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// DynaBotix Automated Bobbin Winder
//
// STARTUP:
// 1. Hardware input test
// 2. Home traverse to limit switch
// 3. Move traverse to start position
// 4. Select number of layers
// 5. Press LEFT to begin
//
// LAYER MENU:
// UP   = increase layers
// DOWN = decrease layers
// LEFT = confirm and begin winding
//
// WINDING:
// One traverse pass equals one layer.
// Spindle advances one revolution per 0.127 mm traverse.
// OLED updates only after each complete spindle revolution.
//
// E-STOP:
// Motors disable immediately.
// After E-stop release, the Teensy resets and returns to startup.
//
// RIGHT button is non-functional and intentionally unused.
// ============================================================

// ============================================================
// OLED
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// ============================================================
// PIN DEFINITIONS
// ============================================================

const int EN_PIN = 6;

// Traverse motor
const int TRAVERSE_STEP = 24;
const int TRAVERSE_DIR  = 25;

// Spindle motor
const int SPINDLE_STEP = 28;
const int SPINDLE_DIR  = 29;

// Buttons
const int BTN_LEFT  = 36;
const int BTN_RIGHT = 37;  // Non-functional
const int BTN_UP    = 15;
const int BTN_DOWN  = 14;

// Safety inputs
const int LIMIT_PIN = 7;
const int ESTOP_PIN = 8;

// LEDs
const int RED_LED   = 23;
const int GREEN_LED = 22;

// ============================================================
// DIRECTION LOGIC
// ============================================================

const int TOWARD_LIMIT = HIGH;
const int AWAY_FROM_LIMIT = LOW;

// Change HIGH to LOW only if spindle direction is incorrect.
const int SPINDLE_WIND_DIRECTION = HIGH;

// ============================================================
// MOTION CALIBRATION
// ============================================================

const long FULL_STEPS_PER_REV = 200;
const long MICROSTEPS = 16;

// 200 × 16 = 3200 microsteps per spindle revolution
const long SPINDLE_STEPS_PER_REV =
  FULL_STEPS_PER_REV * MICROSTEPS;

// Traverse lead-screw pitch
const float LEAD_SCREW_PITCH_MM = 1.0;

// 200 × 16 ÷ 1.0 mm = 3200 traverse steps/mm
const float TRAVERSE_STEPS_PER_MM =
  (FULL_STEPS_PER_REV * MICROSTEPS) /
  LEAD_SCREW_PITCH_MM;

// Calibrated positions
const float START_POSITION_MM = 3.8;
const float END_POSITION_MM   = 5.5;

const float LAYER_WIDTH_MM =
  END_POSITION_MM - START_POSITION_MM;

// Home switch to start position
const long HOME_TO_START_STEPS =
  (long)(
    START_POSITION_MM *
    TRAVERSE_STEPS_PER_MM +
    0.5
  );

// Traverse steps per winding layer
const long LAYER_TRAVERSE_STEPS =
  (long)(
    LAYER_WIDTH_MM *
    TRAVERSE_STEPS_PER_MM +
    0.5
  );

// Maximum homing search distance
const float MAX_HOME_DISTANCE_MM = 50.0;

const long MAX_HOME_STEPS =
  (long)(
    MAX_HOME_DISTANCE_MM *
    TRAVERSE_STEPS_PER_MM
  );

// ============================================================
// WINDING PITCH
// ============================================================

const float WIRE_PITCH_MM = 0.127;

/*
  One spindle revolution advances the traverse by 0.127 mm.

  Since both axes use a 3200-step scale:

  For every spindle microstep:
      accumulator += 127

  When accumulator reaches 1000:
      one traverse microstep is generated
      accumulator -= 1000
*/

const long SYNC_NUMERATOR = 127;
const long SYNC_DENOMINATOR = 1000;

// ============================================================
// LAYER MENU
// ============================================================

int selectedLayers = 4;

const int MIN_LAYERS = 1;
const int MAX_LAYERS = 20;

// ============================================================
// SPEED SETTINGS
// ============================================================

// Homing and initial positioning:
// 250 us HIGH + 250 us LOW
const unsigned int TRAVERSE_STEP_DELAY_US = 250;

// Synchronized winding speed
const unsigned int WINDING_STEP_DELAY_US = 500;

// STEP pulse width
const unsigned int STEP_PULSE_US = 5;

// Direction settling time
const unsigned int DIR_SETTLE_MS = 100;

// Pause after each completed layer
const unsigned int REVERSAL_PAUSE_MS = 300;

// ============================================================
// INPUT FUNCTIONS
// ============================================================

bool leftPressed() {
  return digitalRead(BTN_LEFT) == LOW;
}

bool upPressed() {
  return digitalRead(BTN_UP) == LOW;
}

bool downPressed() {
  return digitalRead(BTN_DOWN) == LOW;
}

bool limitPressed() {
  // Normally closed:
  // released = LOW
  // pressed  = HIGH
  return digitalRead(LIMIT_PIN) == HIGH;
}

bool estopPressed() {
  // Normally closed:
  // released = LOW
  // pressed  = HIGH
  return digitalRead(ESTOP_PIN) == HIGH;
}

// ============================================================
// OLED HELPERS
// ============================================================

void showMessage(
  const char* line1,
  const char* line2 = "",
  const char* line3 = "",
  const char* line4 = ""
) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println(line1);

  display.setCursor(0, 16);
  display.println(line2);

  display.setCursor(0, 32);
  display.println(line3);

  display.setCursor(0, 48);
  display.println(line4);

  display.display();
}

void showLayerMenu() {
  float estimatedTurns =
    selectedLayers * LAYER_WIDTH_MM / WIRE_PITCH_MM;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("WINDING SETUP");

  display.setCursor(0, 16);
  display.print("Layers: ");
  display.println(selectedLayers);

  display.setCursor(0, 32);
  display.print("Est. turns: ");
  display.println(estimatedTurns, 1);

  display.setCursor(0, 48);
  display.println("UP/DOWN LEFT start");

  display.display();
}

void showWindingProgress(
  int currentLayer,
  int totalLayers,
  float totalTurns,
  float layerTurns
) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("WINDING");

  display.setCursor(0, 16);
  display.print("Layer: ");
  display.print(currentLayer);
  display.print("/");
  display.println(totalLayers);

  display.setCursor(0, 32);
  display.print("Turns: ");
  display.println(totalTurns, 2);

  display.setCursor(0, 48);
  display.print("Layer turns: ");
  display.println(layerTurns, 2);

  display.display();
}

void showCompletionScreen(
  int completedLayers,
  float completedTurns
) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("WINDING COMPLETE");

  display.setCursor(0, 16);
  display.print("Layers: ");
  display.println(completedLayers);

  display.setCursor(0, 32);
  display.print("Turns: ");
  display.println(completedTurns, 2);

  display.setCursor(0, 48);
  display.println("Press LEFT restart");

  display.display();
}

// ============================================================
// MOTOR ENABLE
// ============================================================

void enableMotors() {
  // TMC2209 EN is active LOW.
  digitalWrite(EN_PIN, LOW);
}

void disableMotors() {
  digitalWrite(EN_PIN, HIGH);
}

// ============================================================
// BUTTON HELPERS
// ============================================================

void waitForButtonRelease(int pin) {
  while (digitalRead(pin) == LOW) {
    delay(10);
  }

  delay(100);
}

void waitForLeftRelease() {
  waitForButtonRelease(BTN_LEFT);
}

// ============================================================
// HARDWARE TEST
// ============================================================

void testLeftButton() {
  showMessage(
    "STARTUP NOTICE",
    "RIGHT button is",
    "non-functional",
    "Press LEFT"
  );

  while (!leftPressed()) {
    delay(10);
  }

  waitForButtonRelease(BTN_LEFT);

  showMessage(
    "LEFT BUTTON",
    "PASS"
  );

  delay(700);
}

void testUpButton() {
  showMessage(
    "BUTTON TEST",
    "Press UP"
  );

  while (!upPressed()) {
    delay(10);
  }

  waitForButtonRelease(BTN_UP);

  showMessage(
    "UP BUTTON",
    "PASS"
  );

  delay(700);
}

void testDownButton() {
  showMessage(
    "BUTTON TEST",
    "Press DOWN"
  );

  while (!downPressed()) {
    delay(10);
  }

  waitForButtonRelease(BTN_DOWN);

  showMessage(
    "DOWN BUTTON",
    "PASS"
  );

  delay(700);
}

void testEstop() {
  disableMotors();

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);

  showMessage(
    "E-STOP TEST",
    "Press E-Stop"
  );

  while (!estopPressed()) {
    delay(10);
  }

  digitalWrite(RED_LED, HIGH);

  showMessage(
    "E-STOP PRESSED",
    "Red LED ON",
    "Twist E-Stop",
    "to release"
  );

  while (estopPressed()) {
    delay(10);
  }

  digitalWrite(RED_LED, LOW);

  showMessage(
    "E-STOP RELEASED",
    "Red LED OFF",
    "Proceeding to",
    "limit test"
  );

  delay(1200);
}

void testLimitSwitch() {
  showMessage(
    "LIMIT TEST",
    "Press limit",
    "switch"
  );

  while (!limitPressed()) {
    delay(10);
  }

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);

  showMessage(
    "LIMIT PRESSED",
    "Red LED ON",
    "Release switch"
  );

  while (limitPressed()) {
    delay(10);
  }

  digitalWrite(RED_LED, LOW);

  showMessage(
    "LIMIT RELEASED",
    "Red LED OFF"
  );

  delay(700);
}

void runHardwareTest() {
  disableMotors();

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);

  testLeftButton();
  testUpButton();
  testDownButton();
  testEstop();
  testLimitSwitch();

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);

  showMessage(
    "SYSTEM TEST PASS",
    "Green LED ON",
    "Press LEFT",
    "to continue"
  );

  while (!leftPressed()) {
    delay(10);
  }

  waitForLeftRelease();

  digitalWrite(GREEN_LED, LOW);

  enableMotors();

  showMessage(
    "SYSTEM STARTING",
    "Motion enabled",
    "E-Stop active"
  );

  delay(1000);
}

// ============================================================
// SOFTWARE RESET
// ============================================================

void restartTeensy() {
  disableMotors();

  SCB_AIRCR = 0x05FA0004;

  while (true) {
    disableMotors();
  }
}

// ============================================================
// E-STOP DURING MOTION
// ============================================================

void handleMotionEstop() {
  disableMotors();

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);

  showMessage(
    "EMERGENCY STOP",
    "Motion disabled",
    "Twist E-Stop",
    "to release"
  );

  while (estopPressed()) {
    delay(10);
  }

  digitalWrite(RED_LED, LOW);

  showMessage(
    "E-STOP RELEASED",
    "Returning to",
    "start menu"
  );

  delay(1200);

  restartTeensy();
}

void checkMotionEstop() {
  if (estopPressed()) {
    handleMotionEstop();
  }
}

// ============================================================
// MOTOR STEP PULSES
// ============================================================

void pulseTraverseOnly() {
  digitalWrite(TRAVERSE_STEP, HIGH);
  delayMicroseconds(TRAVERSE_STEP_DELAY_US);

  digitalWrite(TRAVERSE_STEP, LOW);
  delayMicroseconds(TRAVERSE_STEP_DELAY_US);
}

void pulseSpindle() {
  digitalWrite(SPINDLE_STEP, HIGH);
  delayMicroseconds(STEP_PULSE_US);

  digitalWrite(SPINDLE_STEP, LOW);
}

void pulseTraverseSynchronized() {
  digitalWrite(TRAVERSE_STEP, HIGH);
  delayMicroseconds(STEP_PULSE_US);

  digitalWrite(TRAVERSE_STEP, LOW);
}

// ============================================================
// TRAVERSE-ONLY MOVEMENT
// ============================================================

void moveTraverseOnly(
  long numberOfSteps,
  int direction
) {
  digitalWrite(TRAVERSE_DIR, direction);
  delay(DIR_SETTLE_MS);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);

  for (long i = 0; i < numberOfSteps; i++) {
    checkMotionEstop();
    pulseTraverseOnly();
  }

  digitalWrite(RED_LED, LOW);
}

// ============================================================
// REVERSAL LED SIGNAL
// ============================================================

void blinkReverseSignal() {
  digitalWrite(GREEN_LED, LOW);

  for (int i = 0; i < 2; i++) {
    digitalWrite(RED_LED, HIGH);
    delay(120);

    digitalWrite(RED_LED, LOW);
    delay(120);
  }
}

// ============================================================
// HOMING
// ============================================================

bool homeTraverse() {
  showMessage(
    "HOMING",
    "Moving toward",
    "limit switch",
    "Fast mode"
  );

  digitalWrite(TRAVERSE_DIR, TOWARD_LIMIT);
  delay(DIR_SETTLE_MS);

  unsigned long previousBlinkTime = 0;
  bool ledState = false;

  long homingSteps = 0;

  while (!limitPressed()) {
    checkMotionEstop();

    if (millis() - previousBlinkTime >= 600) {
      previousBlinkTime = millis();
      ledState = !ledState;
      digitalWrite(RED_LED, ledState);
    }

    pulseTraverseOnly();
    homingSteps++;

    if (homingSteps >= MAX_HOME_STEPS) {
      disableMotors();

      digitalWrite(GREEN_LED, LOW);
      digitalWrite(RED_LED, HIGH);

      showMessage(
        "HOMING ERROR",
        "Limit switch",
        "not detected",
        "Restart system"
      );

      return false;
    }
  }

  digitalWrite(RED_LED, LOW);

  showMessage(
    "LIMIT DETECTED",
    "Reversing",
    "direction"
  );

  blinkReverseSignal();

  return true;
}

// ============================================================
// MOVE TO START POSITION
// ============================================================

void moveToStartPosition() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("POSITIONING");

  display.setCursor(0, 16);
  display.print("Moving to ");
  display.print(START_POSITION_MM, 1);
  display.println(" mm");

  display.setCursor(0, 32);
  display.println("Fast mode");

  display.display();

  moveTraverseOnly(
    HOME_TO_START_STEPS,
    AWAY_FROM_LIMIT
  );

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);

  showMessage(
    "START POSITION",
    "Position reached",
    "Select layers"
  );

  delay(1000);
}

// ============================================================
// LAYER SELECTION MENU
// ============================================================

void selectLayers() {
  showLayerMenu();

  while (true) {
    checkMotionEstop();

    if (upPressed()) {
      if (selectedLayers < MAX_LAYERS) {
        selectedLayers++;
      }

      waitForButtonRelease(BTN_UP);
      showLayerMenu();
    }

    if (downPressed()) {
      if (selectedLayers > MIN_LAYERS) {
        selectedLayers--;
      }

      waitForButtonRelease(BTN_DOWN);
      showLayerMenu();
    }

    if (leftPressed()) {
      waitForLeftRelease();
      break;
    }

    delay(10);
  }

  digitalWrite(GREEN_LED, LOW);
}

// ============================================================
// SYNCHRONIZED LAYER MOVEMENT
// ============================================================

long moveSynchronizedLayer(
  long requiredTraverseSteps,
  int traverseDirection,
  int currentLayer,
  int totalLayers,
  long previousSpindleSteps
) {
  // Only the traverse motor changes direction.
  digitalWrite(TRAVERSE_DIR, traverseDirection);
  delay(DIR_SETTLE_MS);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);

  long completedTraverseSteps = 0;
  long traverseAccumulator = 0;
  long layerSpindleSteps = 0;

  /*
    This stores the next total spindle-step count at which
    the OLED should be refreshed.

    Example:
    3200 steps  = 1 turn
    6400 steps  = 2 turns
    9600 steps  = 3 turns
  */
  long nextOLEDUpdateStep =
    (
      previousSpindleSteps /
      SPINDLE_STEPS_PER_REV +
      1
    ) *
    SPINDLE_STEPS_PER_REV;

  while (completedTraverseSteps < requiredTraverseSteps) {
    checkMotionEstop();

    // Advance spindle by one microstep.
    pulseSpindle();
    layerSpindleSteps++;

    // Generate synchronized traverse movement.
    traverseAccumulator += SYNC_NUMERATOR;

    if (traverseAccumulator >= SYNC_DENOMINATOR) {
      pulseTraverseSynchronized();

      traverseAccumulator -= SYNC_DENOMINATOR;
      completedTraverseSteps++;
    }

    long currentTotalSpindleSteps =
      previousSpindleSteps + layerSpindleSteps;

    /*
      Update the OLED only when the total commanded spindle
      movement crosses another complete revolution.
    */
    if (currentTotalSpindleSteps >= nextOLEDUpdateStep) {
      float totalTurns =
        (float)currentTotalSpindleSteps /
        (float)SPINDLE_STEPS_PER_REV;

      float layerTurns =
        (float)layerSpindleSteps /
        (float)SPINDLE_STEPS_PER_REV;

      showWindingProgress(
        currentLayer,
        totalLayers,
        totalTurns,
        layerTurns
      );

      nextOLEDUpdateStep += SPINDLE_STEPS_PER_REV;
    }

    delayMicroseconds(WINDING_STEP_DELAY_US);
  }

  digitalWrite(RED_LED, LOW);

  // Show the exact fractional result at the layer endpoint.
  long finalTotalSpindleSteps =
    previousSpindleSteps + layerSpindleSteps;

  float finalTotalTurns =
    (float)finalTotalSpindleSteps /
    (float)SPINDLE_STEPS_PER_REV;

  float finalLayerTurns =
    (float)layerSpindleSteps /
    (float)SPINDLE_STEPS_PER_REV;

  showWindingProgress(
    currentLayer,
    totalLayers,
    finalTotalTurns,
    finalLayerTurns
  );

  return layerSpindleSteps;
}

// ============================================================
// FINITE WINDING CYCLE
// ============================================================

void runWindingCycle() {
  long totalSpindleSteps = 0;
  int completedLayers = 0;

  for (int layer = 1; layer <= selectedLayers; layer++) {
    int traverseDirection;

    if (layer % 2 == 1) {
      // Odd layer: start position toward end position.
      traverseDirection = AWAY_FROM_LIMIT;
    } else {
      // Even layer: end position toward start position.
      traverseDirection = TOWARD_LIMIT;
    }

    float turnsBeforeLayer =
      (float)totalSpindleSteps /
      (float)SPINDLE_STEPS_PER_REV;

    showWindingProgress(
      layer,
      selectedLayers,
      turnsBeforeLayer,
      0.0
    );

    long layerSpindleSteps =
      moveSynchronizedLayer(
        LAYER_TRAVERSE_STEPS,
        traverseDirection,
        layer,
        selectedLayers,
        totalSpindleSteps
      );

    totalSpindleSteps += layerSpindleSteps;
    completedLayers++;

    digitalWrite(GREEN_LED, HIGH);

    delay(REVERSAL_PAUSE_MS);

    digitalWrite(GREEN_LED, LOW);
  }

  disableMotors();

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);

  float finalTurns =
    (float)totalSpindleSteps /
    (float)SPINDLE_STEPS_PER_REV;

  showCompletionScreen(
    completedLayers,
    finalTurns
  );

  while (!leftPressed()) {
    delay(10);
  }

  waitForLeftRelease();

  restartTeensy();
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);

  // Buttons
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);

  // Safety inputs
  pinMode(LIMIT_PIN, INPUT_PULLUP);
  pinMode(ESTOP_PIN, INPUT_PULLUP);

  // Motor driver
  pinMode(EN_PIN, OUTPUT);

  // Traverse motor
  pinMode(TRAVERSE_STEP, OUTPUT);
  pinMode(TRAVERSE_DIR, OUTPUT);

  // Spindle motor
  pinMode(SPINDLE_STEP, OUTPUT);
  pinMode(SPINDLE_DIR, OUTPUT);

  // LEDs
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  disableMotors();

  digitalWrite(TRAVERSE_STEP, LOW);
  digitalWrite(SPINDLE_STEP, LOW);

  digitalWrite(TRAVERSE_DIR, TOWARD_LIMIT);

  /*
    Set the spindle direction once.

    The program does not change this output during winding.
  */
  digitalWrite(
    SPINDLE_DIR,
    SPINDLE_WIND_DIRECTION
  );

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);

  // OLED
  Wire.setSDA(18);
  Wire.setSCL(19);
  Wire.begin();

  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS
      )) {

    while (true) {
      digitalWrite(RED_LED, HIGH);
      delay(250);

      digitalWrite(RED_LED, LOW);
      delay(250);
    }
  }

  display.clearDisplay();
  display.display();

  showMessage(
    "DynaBotix",
    "Automated",
    "Bobbin Winder",
    "Starting..."
  );

  delay(1500);

  runHardwareTest();

  if (!homeTraverse()) {
    return;
  }

  moveToStartPosition();

  selectLayers();

  runWindingCycle();
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  // Program remains inside runWindingCycle().
}