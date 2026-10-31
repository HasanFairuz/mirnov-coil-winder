#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// DynaBotix Two-Motor Hardware Test
// Teensy 4.1 + OLED + 2x TMC2209
// ============================================================

// ---------------- OLED ----------------

constexpr uint8_t OLED_SDA  = 18;
constexpr uint8_t OLED_SCL  = 19;
constexpr uint8_t OLED_ADDR = 0x3C;

constexpr int SCREEN_WIDTH  = 128;
constexpr int SCREEN_HEIGHT = 64;
constexpr int OLED_RESET    = -1;

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

// ---------------- Buttons ----------------

constexpr uint8_t BTN_RIGHT = 37;
constexpr uint8_t BTN_LEFT  = 36;
constexpr uint8_t BTN_UP    = 15;
constexpr uint8_t BTN_DOWN  = 14;

// ---------------- Safety inputs ----------------

constexpr uint8_t ESTOP_PIN = 8;
constexpr uint8_t LIMIT_PIN = 7;

// ---------------- LEDs ----------------

constexpr uint8_t RED_LED   = 23;
constexpr uint8_t GREEN_LED = 22;

// ---------------- Motor drivers ----------------

// Shared TMC2209 enable pin
constexpr uint8_t MOTOR_EN = 6;

// Motor 1
constexpr uint8_t MOTOR1_DIR  = 29;
constexpr uint8_t MOTOR1_STEP = 28;

// Motor 2
constexpr uint8_t MOTOR2_DIR  = 25;
constexpr uint8_t MOTOR2_STEP = 24;

// TMC2209 enable is active LOW
constexpr uint8_t DRIVER_ENABLED  = LOW;
constexpr uint8_t DRIVER_DISABLED = HIGH;

// Change this if your microstep configuration is different
constexpr uint32_t STEPS_PER_REVOLUTION = 3200;

// One revolution per second
constexpr uint32_t STEP_INTERVAL_US =
  500000UL / STEPS_PER_REVOLUTION;

constexpr uint32_t STEP_PULSE_US = 4;

// Both motors run for 10 seconds
constexpr uint32_t RUN_DURATION_MS = 10000;

// Button debounce
constexpr uint32_t DEBOUNCE_MS = 40;

// ============================================================
// Motor structure
// ============================================================

struct Motor {
  uint8_t dirPin;
  uint8_t stepPin;
  const char* name;
};

Motor motor1 = {
  MOTOR1_DIR,
  MOTOR1_STEP,
  "Motor 1"
};

Motor motor2 = {
  MOTOR2_DIR,
  MOTOR2_STEP,
  "Motor 2"
};

Motor* selectedMotor = &motor1;

// ============================================================
// State machine
// ============================================================

enum class SystemState {
  WAIT_LEFT,
  WAIT_RIGHT,
  WAIT_UP,
  WAIT_DOWN,
  WAIT_ESTOP_PRESS,
  WAIT_ESTOP_RELEASE,
  WAIT_LIMIT_TEST,
  MANUAL_MODE,
  BOTH_MOTORS_RUN,
  WAIT_REPEAT,
  ESTOP_ACTIVE
};

SystemState state = SystemState::WAIT_LEFT;
SystemState returnAfterEstop = SystemState::WAIT_LEFT;

// ============================================================
// Debounced inputs
// ============================================================

struct Button {
  uint8_t pin;
  bool stableState;
  bool previousReading;
  bool pressEvent;
  uint32_t lastChangeTime;
};

Button leftButton  = {BTN_LEFT,  HIGH, HIGH, false, 0};
Button rightButton = {BTN_RIGHT, HIGH, HIGH, false, 0};
Button upButton    = {BTN_UP,    HIGH, HIGH, false, 0};
Button downButton  = {BTN_DOWN,  HIGH, HIGH, false, 0};

// Limit switch is normally closed:
// normal = LOW
// pressed/open = HIGH
Button limitSwitch = {LIMIT_PIN, LOW, LOW, false, 0};

// ============================================================
// Automatic-run variables
// ============================================================

uint32_t runStartTime = 0;
uint32_t lastStepTime = 0;
uint32_t lastDisplayUpdate = 0;

// ============================================================
// Input functions
// ============================================================

bool estopActive() {
  // Normally closed:
  // normal = LOW
  // pressed/open = HIGH
  return digitalRead(ESTOP_PIN) == HIGH;
}

void clearButtonEvents() {
  leftButton.pressEvent = false;
  rightButton.pressEvent = false;
  upButton.pressEvent = false;
  downButton.pressEvent = false;
  limitSwitch.pressEvent = false;
}

void updateButton(Button& button, bool activeHigh = false) {
  bool reading = digitalRead(button.pin);

  if (reading != button.previousReading) {
    button.previousReading = reading;
    button.lastChangeTime = millis();
  }

  if (millis() - button.lastChangeTime >= DEBOUNCE_MS) {
    if (reading != button.stableState) {
      button.stableState = reading;

      if (activeHigh) {
        if (button.stableState == HIGH) {
          button.pressEvent = true;
        }
      } else {
        if (button.stableState == LOW) {
          button.pressEvent = true;
        }
      }
    }
  }
}

void updateInputs() {
  clearButtonEvents();

  updateButton(leftButton);
  updateButton(rightButton);
  updateButton(upButton);
  updateButton(downButton);

  // Limit switch becomes active HIGH
  updateButton(limitSwitch, true);
}

// ============================================================
// Driver and LED functions
// ============================================================

void enableMotors() {
  digitalWrite(MOTOR_EN, DRIVER_ENABLED);
}

void disableMotors() {
  digitalWrite(MOTOR_EN, DRIVER_DISABLED);

  digitalWrite(MOTOR1_STEP, LOW);
  digitalWrite(MOTOR2_STEP, LOW);
}

void setIdleLEDs() {
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
}

void setPassLEDs() {
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, HIGH);
}

void setEstopLEDs() {
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);
}

// ============================================================
// OLED functions
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
  display.println("DynaBotix Test");

  display.drawLine(
    0,
    10,
    127,
    10,
    SSD1306_WHITE
  );

  display.setCursor(0, 17);
  display.println(line1);

  display.setCursor(0, 29);
  display.println(line2);

  display.setCursor(0, 41);
  display.println(line3);

  display.setCursor(0, 53);
  display.println(line4);

  display.display();
}

void showManualMode() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("MANUAL MOTOR TEST");

  display.drawLine(
    0,
    10,
    127,
    10,
    SSD1306_WHITE
  );

  display.setCursor(0, 16);
  display.print("Selected: ");
  display.println(selectedMotor->name);

  display.setCursor(0, 29);
  display.println("LEFT M1 | RIGHT M2");

  display.setCursor(0, 41);
  display.println("UP CW | DOWN CCW");

  display.setCursor(0, 53);
  display.println("LIMIT: Run both");

  display.display();
}

void showRunCountdown() {
  uint32_t elapsed = millis() - runStartTime;

  uint32_t remainingSeconds =
    (RUN_DURATION_MS - elapsed + 999) / 1000;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("BOTH MOTORS RUN");

  display.drawLine(
    0,
    10,
    127,
    10,
    SSD1306_WHITE
  );

  display.setCursor(0, 18);
  display.println("Speed: 1 rev/sec");

  display.setCursor(0, 33);
  display.print("Remaining: ");
  display.print(remainingSeconds);
  display.println(" sec");

  display.setCursor(0, 50);
  display.println("E-STOP available");

  display.display();
}

// ============================================================
// State-entry function
// ============================================================

void enterState(SystemState newState) {
  state = newState;

  switch (state) {
    case SystemState::WAIT_LEFT:
      disableMotors();
      setIdleLEDs();

      showMessage(
        "Hardware Test",
        "",
        "Press LEFT",
        "to begin"
      );
      break;

    case SystemState::WAIT_RIGHT:
      showMessage(
        "LEFT: PASS",
        "",
        "Press RIGHT",
        "to proceed"
      );
      break;

    case SystemState::WAIT_UP:
      showMessage(
        "RIGHT: PASS",
        "",
        "Press UP",
        "to proceed"
      );
      break;

    case SystemState::WAIT_DOWN:
      showMessage(
        "UP: PASS",
        "",
        "Press DOWN",
        "to proceed"
      );
      break;

    case SystemState::WAIT_ESTOP_PRESS:
      showMessage(
        "DOWN: PASS",
        "",
        "Press E-STOP",
        "to proceed"
      );
      break;

    case SystemState::WAIT_ESTOP_RELEASE:
      disableMotors();
      setEstopLEDs();

      showMessage(
        "E-STOP: PASS",
        "",
        "Twist E-STOP",
        "to release"
      );
      break;

    case SystemState::WAIT_LIMIT_TEST:
      setIdleLEDs();

      showMessage(
        "E-STOP RELEASED",
        "",
        "Press LIMIT",
        "switch"
      );
      break;

    case SystemState::MANUAL_MODE:
      disableMotors();
      setPassLEDs();
      showManualMode();
      break;

    case SystemState::BOTH_MOTORS_RUN:
      enableMotors();
      setPassLEDs();

      // Both motors clockwise
      digitalWrite(MOTOR1_DIR, HIGH);
      digitalWrite(MOTOR2_DIR, HIGH);

      delayMicroseconds(10);

      runStartTime = millis();
      lastStepTime = micros();
      lastDisplayUpdate = 0;

      showRunCountdown();
      break;

    case SystemState::WAIT_REPEAT:
      disableMotors();
      setPassLEDs();

      showMessage(
        "10 SECOND RUN DONE",
        "Both motors stopped",
        "",
        "LIMIT: Run again"
      );
      break;

    case SystemState::ESTOP_ACTIVE:
      disableMotors();
      setEstopLEDs();

      showMessage(
        "!!! E-STOP !!!",
        "Motors disabled",
        "",
        "Twist to release"
      );
      break;
  }
}

// ============================================================
// Motor functions
// ============================================================

void pulseMotor(uint8_t stepPin) {
  digitalWrite(stepPin, HIGH);
  delayMicroseconds(STEP_PULSE_US);
  digitalWrite(stepPin, LOW);
}

void pulseBothMotors() {
  digitalWrite(MOTOR1_STEP, HIGH);
  digitalWrite(MOTOR2_STEP, HIGH);

  delayMicroseconds(STEP_PULSE_US);

  digitalWrite(MOTOR1_STEP, LOW);
  digitalWrite(MOTOR2_STEP, LOW);
}

bool rotateOneRevolution(
  Motor& motor,
  bool clockwise
) {
  if (estopActive()) {
    returnAfterEstop = SystemState::MANUAL_MODE;
    enterState(SystemState::ESTOP_ACTIVE);
    return false;
  }

  enableMotors();

  digitalWrite(
    motor.dirPin,
    clockwise ? HIGH : LOW
  );

  delayMicroseconds(10);

  for (
    uint32_t step = 0;
    step < STEPS_PER_REVOLUTION;
    step++
  ) {
    if (estopActive()) {
      returnAfterEstop = SystemState::MANUAL_MODE;
      enterState(SystemState::ESTOP_ACTIVE);
      return false;
    }

    uint32_t stepStart = micros();

    pulseMotor(motor.stepPin);

    while (
      micros() - stepStart <
      STEP_INTERVAL_US
    ) {
      if (estopActive()) {
        returnAfterEstop =
          SystemState::MANUAL_MODE;

        enterState(
          SystemState::ESTOP_ACTIVE
        );

        return false;
      }
    }
  }

  disableMotors();
  return true;
}

void updateBothMotorsRun() {
  if (estopActive()) {
    returnAfterEstop =
      SystemState::WAIT_REPEAT;

    enterState(
      SystemState::ESTOP_ACTIVE
    );

    return;
  }

  uint32_t elapsed =
    millis() - runStartTime;

  if (elapsed >= RUN_DURATION_MS) {
    enterState(SystemState::WAIT_REPEAT);
    return;
  }

  uint32_t currentTime = micros();

  if (
    currentTime - lastStepTime >=
    STEP_INTERVAL_US
  ) {
    lastStepTime = currentTime;
    pulseBothMotors();
  }
}

// ============================================================
// Setup
// ============================================================

void setup() {
  Serial.begin(115200);

  // Buttons
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);

  // Safety switches
  pinMode(ESTOP_PIN, INPUT_PULLUP);
  pinMode(LIMIT_PIN, INPUT_PULLUP);

  // LEDs
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  // Driver enable
  pinMode(MOTOR_EN, OUTPUT);

  // Motor 1
  pinMode(MOTOR1_DIR, OUTPUT);
  pinMode(MOTOR1_STEP, OUTPUT);

  // Motor 2
  pinMode(MOTOR2_DIR, OUTPUT);
  pinMode(MOTOR2_STEP, OUTPUT);

  disableMotors();
  setIdleLEDs();

  // OLED
  Wire.setSDA(OLED_SDA);
  Wire.setSCL(OLED_SCL);
  Wire.begin();

  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      OLED_ADDR
    )
  ) {
    while (true) {
      disableMotors();

      digitalWrite(RED_LED, HIGH);
      delay(250);

      digitalWrite(RED_LED, LOW);
      delay(250);
    }
  }

  showMessage(
    "Starting...",
    "",
    "Hardware test",
    "initializing"
  );

  delay(1000);

  // Initialize button states
  leftButton.stableState =
    digitalRead(BTN_LEFT);

  leftButton.previousReading =
    leftButton.stableState;

  rightButton.stableState =
    digitalRead(BTN_RIGHT);

  rightButton.previousReading =
    rightButton.stableState;

  upButton.stableState =
    digitalRead(BTN_UP);

  upButton.previousReading =
    upButton.stableState;

  downButton.stableState =
    digitalRead(BTN_DOWN);

  downButton.previousReading =
    downButton.stableState;

  limitSwitch.stableState =
    digitalRead(LIMIT_PIN);

  limitSwitch.previousReading =
    limitSwitch.stableState;

  enterState(SystemState::WAIT_LEFT);
}

// ============================================================
// Main loop
// ============================================================

void loop() {
  updateInputs();

  // Hardware-test E-stop step
  if (
    state == SystemState::WAIT_ESTOP_PRESS &&
    estopActive()
  ) {
    enterState(
      SystemState::WAIT_ESTOP_RELEASE
    );

    return;
  }

  // Global E-stop
  if (
    state !=
      SystemState::WAIT_ESTOP_RELEASE &&
    state !=
      SystemState::ESTOP_ACTIVE &&
    estopActive()
  ) {
    if (
      state ==
      SystemState::BOTH_MOTORS_RUN
    ) {
      returnAfterEstop =
        SystemState::WAIT_REPEAT;
    } else {
      returnAfterEstop = state;
    }

    enterState(
      SystemState::ESTOP_ACTIVE
    );

    return;
  }

  switch (state) {
    case SystemState::WAIT_LEFT:
      if (leftButton.pressEvent) {
        enterState(
          SystemState::WAIT_RIGHT
        );
      }
      break;

    case SystemState::WAIT_RIGHT:
      if (rightButton.pressEvent) {
        enterState(
          SystemState::WAIT_UP
        );
      }
      break;

    case SystemState::WAIT_UP:
      if (upButton.pressEvent) {
        enterState(
          SystemState::WAIT_DOWN
        );
      }
      break;

    case SystemState::WAIT_DOWN:
      if (downButton.pressEvent) {
        enterState(
          SystemState::WAIT_ESTOP_PRESS
        );
      }
      break;

    case SystemState::WAIT_ESTOP_PRESS:
      break;

    case SystemState::WAIT_ESTOP_RELEASE:
      if (!estopActive()) {
        enterState(
          SystemState::WAIT_LIMIT_TEST
        );
      }
      break;

    case SystemState::WAIT_LIMIT_TEST:
      if (limitSwitch.pressEvent) {
        enterState(
          SystemState::MANUAL_MODE
        );
      }
      break;

    case SystemState::MANUAL_MODE:
      if (leftButton.pressEvent) {
        selectedMotor = &motor1;
        showManualMode();
      }

      if (rightButton.pressEvent) {
        selectedMotor = &motor2;
        showManualMode();
      }

      if (upButton.pressEvent) {
        showMessage(
          selectedMotor->name,
          "",
          "Turning CW",
          "one revolution"
        );

        if (
          rotateOneRevolution(
            *selectedMotor,
            true
          )
        ) {
          enterState(
            SystemState::MANUAL_MODE
          );
        }
      }

      if (downButton.pressEvent) {
        showMessage(
          selectedMotor->name,
          "",
          "Turning CCW",
          "one revolution"
        );

        if (
          rotateOneRevolution(
            *selectedMotor,
            false
          )
        ) {
          enterState(
            SystemState::MANUAL_MODE
          );
        }
      }

      if (limitSwitch.pressEvent) {
        enterState(
          SystemState::BOTH_MOTORS_RUN
        );
      }
      break;

    case SystemState::BOTH_MOTORS_RUN:
      updateBothMotorsRun();
      break;

    case SystemState::WAIT_REPEAT:
      if (limitSwitch.pressEvent) {
        enterState(
          SystemState::BOTH_MOTORS_RUN
        );
      }

      // LEFT or RIGHT returns to manual mode
      if (leftButton.pressEvent) {
        selectedMotor = &motor1;

        enterState(
          SystemState::MANUAL_MODE
        );
      }

      if (rightButton.pressEvent) {
        selectedMotor = &motor2;

        enterState(
          SystemState::MANUAL_MODE
        );
      }
      break;

    case SystemState::ESTOP_ACTIVE:
      if (!estopActive()) {
        enterState(returnAfterEstop);
      }
      break;
  }
}