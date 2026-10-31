#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// DynaBotix Traverse Dry Run
//
// Sequence:
// 1. Home traverse toward limit switch
// 2. Slow red LED blink during homing
// 3. When limit is pressed, fast double red blink
// 4. Reverse and move to 4.0 mm from limit
// 5. Wait for LEFT button
// 6. Oscillate continuously between 4.0 mm and 6.0 mm
// 7. E-stop can stop motion at any time
//
// Confirmed hardware:
// Traverse STEP = 24
// Traverse DIR  = 25
// HIGH = toward limit switch
// LOW  = away from limit switch
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

const int TRAVERSE_STEP = 24;
const int TRAVERSE_DIR  = 25;

const int LIMIT_PIN = 7;
const int ESTOP_PIN = 8;

const int BTN_LEFT = 36;

const int RED_LED   = 23;
const int GREEN_LED = 22;

// ============================================================
// CONFIRMED DIRECTION LOGIC
// ============================================================

const int TOWARD_LIMIT = HIGH;
const int AWAY_FROM_LIMIT = LOW;

// ============================================================
// MOTION CALIBRATION
// ============================================================

const long FULL_STEPS_PER_REV = 200;

// Use the same 16-microstep setting as the previous working code
const long MICROSTEPS = 16;

// Traverse lead screw pitch
const float LEAD_SCREW_PITCH_MM = 1.0;

// 200 × 16 ÷ 1 mm = 3200 steps/mm
const float STEPS_PER_MM =
  (FULL_STEPS_PER_REV * MICROSTEPS) /
  LEAD_SCREW_PITCH_MM;

// Absolute positions from the limit switch
const float START_POSITION_MM = 4.0;
const float END_POSITION_MM   = 6.0;

// 4.0 mm from limit
const long HOME_TO_START_STEPS =
  (long)(
    START_POSITION_MM *
    STEPS_PER_MM +
    0.5
  );

// 6.0 mm - 4.0 mm = 2.0 mm travel
const long START_TO_END_STEPS =
  (long)(
    (END_POSITION_MM - START_POSITION_MM) *
    STEPS_PER_MM +
    0.5
  );

// Maximum homing search distance
const float MAX_HOME_DISTANCE_MM = 50.0;

const long MAX_HOME_STEPS =
  (long)(
    MAX_HOME_DISTANCE_MM *
    STEPS_PER_MM
  );

// ============================================================
// SPEED SETTINGS
// ============================================================

// 500 us HIGH + 500 us LOW
const unsigned int STEP_DELAY_US = 250;

// Delay after changing direction
const unsigned int DIR_SETTLE_MS = 100;

// Pause at each reversal point
const unsigned int REVERSAL_PAUSE_MS = 300;

// ============================================================
// INPUT FUNCTIONS
// ============================================================

bool limitPressed() {
  // Normally closed switch:
  // Released = LOW
  // Pressed  = HIGH
  return digitalRead(LIMIT_PIN) == HIGH;
}

bool estopPressed() {
  // Normally closed E-stop:
  // Released = LOW
  // Pressed  = HIGH
  return digitalRead(ESTOP_PIN) == HIGH;
}

bool leftPressed() {
  // INPUT_PULLUP button:
  // Released = HIGH
  // Pressed  = LOW
  return digitalRead(BTN_LEFT) == LOW;
}

// ============================================================
// OLED HELPER
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

// ============================================================
// MOTOR ENABLE
// ============================================================

void enableMotors() {
  // TMC2209 EN is active LOW
  digitalWrite(EN_PIN, LOW);
}

void disableMotors() {
  digitalWrite(EN_PIN, HIGH);
}

// ============================================================
// BUTTON HELPER
// ============================================================

void waitForLeftRelease() {
  while (leftPressed()) {
    delay(10);
  }

  delay(100);
}

// ============================================================
// E-STOP HANDLER
// ============================================================

void handleEstop() {
  disableMotors();

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);

  showMessage(
    "EMERGENCY STOP",
    "Motion disabled",
    "Release E-Stop",
    ""
  );

  Serial.println("E-STOP PRESSED");

  while (estopPressed()) {
    delay(10);
  }

  showMessage(
    "E-Stop released",
    "Press LEFT",
    "to resume",
    ""
  );

  Serial.println("E-STOP RELEASED");
  Serial.println("PRESS LEFT TO RESUME");

  while (!leftPressed()) {
    delay(10);
  }

  waitForLeftRelease();

  enableMotors();

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);

  showMessage(
    "Motion resumed",
    "",
    "",
    ""
  );

  delay(500);
}

void checkEstop() {
  if (estopPressed()) {
    handleEstop();
  }
}

// ============================================================
// STEP PULSE
// ============================================================

void pulseTraverse() {
  digitalWrite(TRAVERSE_STEP, HIGH);
  delayMicroseconds(STEP_DELAY_US);

  digitalWrite(TRAVERSE_STEP, LOW);
  delayMicroseconds(STEP_DELAY_US);
}

// ============================================================
// FIXED-DISTANCE MOTION
// ============================================================

void moveTraverse(
  long numberOfSteps,
  int direction
) {
  digitalWrite(TRAVERSE_DIR, direction);
  delay(DIR_SETTLE_MS);

  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);

  for (long i = 0; i < numberOfSteps; i++) {
    checkEstop();
    pulseTraverse();
  }

  digitalWrite(RED_LED, LOW);
}

// ============================================================
// FAST DOUBLE BLINK FOR REVERSAL
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
    ""
  );

  Serial.println("MOVING TOWARD LIMIT");

  digitalWrite(TRAVERSE_DIR, TOWARD_LIMIT);
  delay(DIR_SETTLE_MS);

  unsigned long previousBlinkTime = 0;
  bool ledState = false;

  long homingSteps = 0;

  while (!limitPressed()) {
    checkEstop();

    // Slow red blink during homing
    if (millis() - previousBlinkTime >= 600) {
      previousBlinkTime = millis();
      ledState = !ledState;
      digitalWrite(RED_LED, ledState);
    }

    pulseTraverse();

    homingSteps++;

    if (homingSteps >= MAX_HOME_STEPS) {
      disableMotors();

      digitalWrite(RED_LED, HIGH);
      digitalWrite(GREEN_LED, LOW);

      showMessage(
        "HOMING ERROR",
        "Limit switch",
        "not detected",
        "Restart system"
      );

      Serial.println("HOMING TIMEOUT");

      return false;
    }
  }

  digitalWrite(RED_LED, LOW);

  Serial.println("LIMIT SWITCH DETECTED");

  showMessage(
    "LIMIT DETECTED",
    "Reversing",
    "direction",
    ""
  );

  // Fast double blink before moving away
  blinkReverseSignal();

  return true;
}

// ============================================================
// MOVE TO 4.0 MM START POSITION
// ============================================================

void moveToStartPosition() {
  showMessage(
    "POSITIONING",
    "Moving to",
    "4.0 mm",
    ""
  );

  Serial.println("MOVING TO 4.0 MM POSITION");

  moveTraverse(
    HOME_TO_START_STEPS,
    AWAY_FROM_LIMIT
  );

  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);

  showMessage(
    "START POSITION",
    "Position: 4.0 mm",
    "Press LEFT",
    "to begin demo"
  );

  Serial.println("AT 4.0 MM START POSITION");
}

// ============================================================
// WAIT FOR USER
// ============================================================

void waitForDemoStart() {
  while (!leftPressed()) {
    checkEstop();
    delay(10);
  }

  waitForLeftRelease();

  digitalWrite(GREEN_LED, LOW);
}

// ============================================================
// LOOP BETWEEN 4.0 MM AND 6.0 MM
// ============================================================

void runTraverseDemo() {
  while (true) {
    // --------------------------------------------------------
    // Move from 4.0 mm to 6.0 mm
    // --------------------------------------------------------

    showMessage(
      "DRY RUN",
      "4.0 -> 6.0 mm",
      "Moving away",
      ""
    );

    Serial.println("MOVING 4.0 MM TO 6.0 MM");

    moveTraverse(
      START_TO_END_STEPS,
      AWAY_FROM_LIMIT
    );

    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);

    showMessage(
      "END POSITION",
      "Position: 6.0 mm",
      "Reversing",
      ""
    );

    Serial.println("AT 6.0 MM");

    delay(REVERSAL_PAUSE_MS);

    // --------------------------------------------------------
    // Move from 6.0 mm to 4.0 mm
    // --------------------------------------------------------

    digitalWrite(GREEN_LED, LOW);

    showMessage(
      "DRY RUN",
      "6.0 -> 4.0 mm",
      "Moving toward",
      ""
    );

    Serial.println("MOVING 6.0 MM TO 4.0 MM");

    moveTraverse(
      START_TO_END_STEPS,
      TOWARD_LIMIT
    );

    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);

    showMessage(
      "START POSITION",
      "Position: 4.0 mm",
      "Reversing",
      ""
    );

    Serial.println("AT 4.0 MM");

    delay(REVERSAL_PAUSE_MS);
  }
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);

  pinMode(EN_PIN, OUTPUT);

  pinMode(TRAVERSE_STEP, OUTPUT);
  pinMode(TRAVERSE_DIR, OUTPUT);

  pinMode(LIMIT_PIN, INPUT_PULLUP);
  pinMode(ESTOP_PIN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  digitalWrite(TRAVERSE_STEP, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);

  disableMotors();

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

  showMessage(
    "DynaBotix",
    "Traverse",
    "Dry Run",
    "Starting..."
  );

  delay(1500);

  enableMotors();

  if (!homeTraverse()) {
    return;
  }

  moveToStartPosition();

  waitForDemoStart();

  runTraverseDemo();
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  // Program remains inside runTraverseDemo().
}