#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// DynaBotix Hardware Test
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

// Normally-closed switches:
// Normal state    = LOW
// Triggered/open  = HIGH

constexpr uint8_t ESTOP_ACTIVE_LEVEL = HIGH;
constexpr uint8_t LIMIT_ACTIVE_LEVEL = HIGH;

// ---------------- LEDs ----------------

constexpr uint8_t RED_LED   = 23;
constexpr uint8_t GREEN_LED = 22;

// ---------------- Motor drivers ----------------

// Shared active-low TMC2209 enable
constexpr uint8_t MOTOR_EN = 6;

// Motor 1
constexpr uint8_t MOTOR1_DIR  = 29;
constexpr uint8_t MOTOR1_STEP = 28;

// Motor 2
constexpr uint8_t MOTOR2_DIR  = 25;
constexpr uint8_t MOTOR2_STEP = 24;

constexpr uint8_t DRIVER_ENABLED  = LOW;
constexpr uint8_t DRIVER_DISABLED = HIGH;

// Change these if the physical directions are reversed.
constexpr uint8_t MOTOR1_CW_LEVEL = HIGH;
constexpr uint8_t MOTOR2_CW_LEVEL = HIGH;

// Change this to match your TMC2209 microstepping.
constexpr uint32_t STEPS_PER_REVOLUTION = 3200;

// Approximately one revolution per second.
constexpr uint32_t STEP_INTERVAL_US =
  500000UL / STEPS_PER_REVOLUTION;

constexpr uint32_t STEP_PULSE_US = 4;

// Motor 1 reverses for 10 seconds after hitting the limit.
constexpr uint32_t LIMIT_REVERSE_TIME_MS = 10000;

// Maximum allowed time while searching for the limit switch.
constexpr uint32_t LIMIT_SEARCH_TIMEOUT_MS = 25000;

// Both-motor test duration.
constexpr uint32_t BOTH_MOTOR_RUN_TIME_MS = 10000;

constexpr uint32_t DEBOUNCE_MS = 40;

// ============================================================
// Motor definitions
// ============================================================

struct Motor {
  uint8_t dirPin;
  uint8_t stepPin;
  uint8_t clockwiseLevel;
  const char* name;
};

Motor motor1 = {
  MOTOR1_DIR,
  MOTOR1_STEP,
  MOTOR1_CW_LEVEL,
  "Motor 1"
};

Motor motor2 = {
  MOTOR2_DIR,
  MOTOR2_STEP,
  MOTOR2_CW_LEVEL,
  "Motor 2"
};

Motor* selectedMotor = &motor1;

// ============================================================
// Debounced input
// ============================================================

struct Input {
  uint8_t pin;
  uint8_t activeLevel;

  bool stableState;
  bool previousReading;
  bool activeEvent;

  uint32_t lastChangeTime;
};

Input leftButton = {
  BTN_LEFT,
  LOW,
  HIGH,
  HIGH,
  false,
  0
};

Input rightButton = {
  BTN_RIGHT,
  LOW,
  HIGH,
  HIGH,
  false,
  0
};

Input upButton = {
  BTN_UP,
  LOW,
  HIGH,
  HIGH,
  false,
  0
};

Input downButton = {
  BTN_DOWN,
  LOW,
  HIGH,
  HIGH,
  false,
  0
};

Input limitSwitch = {
  LIMIT_PIN,
  LIMIT_ACTIVE_LEVEL,
  LOW,
  LOW,
  false,
  0
};

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

  LIMIT_READY,
  LIMIT_SEARCH_CW,
  LIMIT_REVERSE_CCW,
  LIMIT_COMPLETE,
  LIMIT_TIMEOUT,

  MANUAL_MODE,
  BOTH_MOTORS_RUN,
  WAIT_REPEAT,

  ESTOP_ACTIVE
};

SystemState state = SystemState::WAIT_LEFT;
SystemState returnAfterEstop = SystemState::WAIT_LEFT;

// ============================================================
// Timing variables
// ============================================================

uint32_t motionStartTime = 0;
uint32_t lastStepTime = 0;

bool bothButtonLatch = false;

// ============================================================
// Input functions
// ============================================================

bool estopActive() {
  return digitalRead(ESTOP_PIN) == ESTOP_ACTIVE_LEVEL;
}

bool limitTriggered() {
  return limitSwitch.stableState == LIMIT_ACTIVE_LEVEL;
}

void updateInput(Input& input) {
  input.activeEvent = false;

  bool reading = digitalRead(input.pin);

  if (reading != input.previousReading) {
    input.previousReading = reading;
    input.lastChangeTime = millis();
  }

  if (millis() - input.lastChangeTime >= DEBOUNCE_MS) {
    if (reading != input.stableState) {
      input.stableState = reading;

      if (input.stableState == input.activeLevel) {
        input.activeEvent = true;
      }
    }
  }
}

void updateInputs() {
  updateInput(leftButton);
  updateInput(rightButton);
  updateInput(upButton);
  updateInput(downButton);
  updateInput(limitSwitch);
}

bool leftAndRightPressed() {
  return (
    leftButton.stableState == LOW &&
    rightButton.stableState == LOW
  );
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

void setErrorLEDs() {
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
  display.println("LEFT+RIGHT: both");

  display.display();
}

// ============================================================
// Motor functions
// ============================================================

void setMotorDirection(
  Motor& motor,
  bool clockwise
) {
  uint8_t directionLevel;

  if (clockwise) {
    directionLevel = motor.clockwiseLevel;
  } else {
    directionLevel = !motor.clockwiseLevel;
  }

  digitalWrite(
    motor.dirPin,
    directionLevel
  );

  delayMicroseconds(10);
}

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

void updateMotor1Motion() {
  uint32_t currentTime = micros();

  if (
    currentTime - lastStepTime >=
    STEP_INTERVAL_US
  ) {
    lastStepTime = currentTime;
    pulseMotor(MOTOR1_STEP);
  }
}

void updateBothMotorMotion() {
  uint32_t currentTime = micros();

  if (
    currentTime - lastStepTime >=
    STEP_INTERVAL_US
  ) {
    lastStepTime = currentTime;
    pulseBothMotors();
  }
}

bool rotateOneRevolution(
  Motor& motor,
  bool clockwise
) {
  if (estopActive()) {
    returnAfterEstop = SystemState::MANUAL_MODE;
    return false;
  }

  enableMotors();
  setMotorDirection(motor, clockwise);

  for (
    uint32_t step = 0;
    step < STEPS_PER_REVOLUTION;
    step++
  ) {
    if (estopActive()) {
      disableMotors();
      returnAfterEstop = SystemState::MANUAL_MODE;
      return false;
    }

    // Stop Motor 1 if it reaches the limit while moving CW.
    if (
      &motor == &motor1 &&
      clockwise &&
      digitalRead(LIMIT_PIN) == LIMIT_ACTIVE_LEVEL
    ) {
      disableMotors();

      showMessage(
        "LIMIT REACHED",
        "Motor 1 stopped",
        "",
        "Returning..."
      );

      delay(800);
      return true;
    }

    uint32_t stepStart = micros();

    pulseMotor(motor.stepPin);

    while (
      micros() - stepStart <
      STEP_INTERVAL_US
    ) {
      if (estopActive()) {
        disableMotors();
        returnAfterEstop = SystemState::MANUAL_MODE;
        return false;
      }
    }
  }

  disableMotors();
  return true;
}

// ============================================================
// State entry function
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
      setErrorLEDs();

      showMessage(
        "E-STOP: PASS",
        "",
        "Twist E-STOP",
        "to release"
      );
      break;

    case SystemState::LIMIT_READY:
      disableMotors();
      setIdleLEDs();

      showMessage(
        "LIMIT MOTOR TEST",
        "Release limit first",
        "Clear motor path",
        "RIGHT: Start"
      );
      break;

    case SystemState::LIMIT_SEARCH_CW:
      enableMotors();
      setPassLEDs();

      setMotorDirection(
        motor1,
        true
      );

      motionStartTime = millis();
      lastStepTime = micros();

      showMessage(
        "LIMIT TEST ACTIVE",
        "Motor 1 turning CW",
        "Searching for limit",
        "E-STOP available"
      );
      break;

    case SystemState::LIMIT_REVERSE_CCW:
      enableMotors();
      setPassLEDs();

      setMotorDirection(
        motor1,
        false
      );

      motionStartTime = millis();
      lastStepTime = micros();

      showMessage(
        "LIMIT TRIGGERED",
        "Motor 1 turning CCW",
        "Running 10 seconds",
        "E-STOP available"
      );
      break;

    case SystemState::LIMIT_COMPLETE:
      disableMotors();
      setPassLEDs();

      showMessage(
        "LIMIT TEST: PASS",
        "Motor 1 stopped",
        "",
        "RIGHT: Continue"
      );
      break;

    case SystemState::LIMIT_TIMEOUT:
      disableMotors();
      setErrorLEDs();

      showMessage(
        "LIMIT TIMEOUT",
        "Motor 1 stopped",
        "Check switch/path",
        "LEFT: Retry"
      );
      break;

    case SystemState::MANUAL_MODE:
      disableMotors();
      setPassLEDs();

      bothButtonLatch = false;

      showManualMode();
      break;

    case SystemState::BOTH_MOTORS_RUN:
      enableMotors();
      setPassLEDs();

      setMotorDirection(
        motor1,
        true
      );

      setMotorDirection(
        motor2,
        true
      );

      motionStartTime = millis();
      lastStepTime = micros();

      showMessage(
        "BOTH MOTORS RUN",
        "Both turning CW",
        "Running 10 seconds",
        "E-STOP available"
      );
      break;

    case SystemState::WAIT_REPEAT:
      disableMotors();
      setPassLEDs();

      bothButtonLatch = true;

      showMessage(
        "BOTH MOTORS DONE",
        "Both motors stopped",
        "LEFT+RIGHT: repeat",
        "LEFT/RIGHT: manual"
      );
      break;

    case SystemState::ESTOP_ACTIVE:
      disableMotors();
      setErrorLEDs();

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
// Setup
// ============================================================

void setup() {
  Serial.begin(115200);

  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);

  pinMode(ESTOP_PIN, INPUT_PULLUP);
  pinMode(LIMIT_PIN, INPUT_PULLUP);

  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);

  pinMode(MOTOR_EN, OUTPUT);

  pinMode(MOTOR1_DIR, OUTPUT);
  pinMode(MOTOR1_STEP, OUTPUT);

  pinMode(MOTOR2_DIR, OUTPUT);
  pinMode(MOTOR2_STEP, OUTPUT);

  disableMotors();
  setIdleLEDs();

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

  // ----------------------------------------------------------
  // Required E-stop hardware test
  // ----------------------------------------------------------

  if (
    state == SystemState::WAIT_ESTOP_PRESS &&
    estopActive()
  ) {
    enterState(
      SystemState::WAIT_ESTOP_RELEASE
    );

    return;
  }

  // ----------------------------------------------------------
  // Global E-stop
  // ----------------------------------------------------------

  if (
    state != SystemState::WAIT_ESTOP_RELEASE &&
    state != SystemState::ESTOP_ACTIVE &&
    estopActive()
  ) {
    if (
      state == SystemState::LIMIT_SEARCH_CW ||
      state == SystemState::LIMIT_REVERSE_CCW
    ) {
      returnAfterEstop =
        SystemState::LIMIT_READY;
    } else if (
      state == SystemState::BOTH_MOTORS_RUN
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

  // ----------------------------------------------------------
  // State machine
  // ----------------------------------------------------------

  switch (state) {
    case SystemState::WAIT_LEFT:
      if (leftButton.activeEvent) {
        enterState(
          SystemState::WAIT_RIGHT
        );
      }
      break;

    case SystemState::WAIT_RIGHT:
      if (rightButton.activeEvent) {
        enterState(
          SystemState::WAIT_UP
        );
      }
      break;

    case SystemState::WAIT_UP:
      if (upButton.activeEvent) {
        enterState(
          SystemState::WAIT_DOWN
        );
      }
      break;

    case SystemState::WAIT_DOWN:
      if (downButton.activeEvent) {
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
          SystemState::LIMIT_READY
        );
      }
      break;

    // --------------------------------------------------------
    // Motorized limit-switch test
    // --------------------------------------------------------

    case SystemState::LIMIT_READY:
      if (rightButton.activeEvent) {
        if (limitTriggered()) {
          showMessage(
            "LIMIT IS ACTIVE",
            "Release the switch",
            "",
            "RIGHT: Retry"
          );
        } else {
          enterState(
            SystemState::LIMIT_SEARCH_CW
          );
        }
      }
      break;

    case SystemState::LIMIT_SEARCH_CW:
      if (limitTriggered()) {
        enterState(
          SystemState::LIMIT_REVERSE_CCW
        );

        break;
      }

      if (
        millis() - motionStartTime >=
        LIMIT_SEARCH_TIMEOUT_MS
      ) {
        enterState(
          SystemState::LIMIT_TIMEOUT
        );

        break;
      }

      updateMotor1Motion();
      break;

    case SystemState::LIMIT_REVERSE_CCW:
      if (
        millis() - motionStartTime >=
        LIMIT_REVERSE_TIME_MS
      ) {
        enterState(
          SystemState::LIMIT_COMPLETE
        );

        break;
      }

      updateMotor1Motion();
      break;

    case SystemState::LIMIT_COMPLETE:
      if (rightButton.activeEvent) {
        selectedMotor = &motor1;

        enterState(
          SystemState::MANUAL_MODE
        );
      }
      break;

    case SystemState::LIMIT_TIMEOUT:
      if (leftButton.activeEvent) {
        enterState(
          SystemState::LIMIT_READY
        );
      }
      break;

    // --------------------------------------------------------
    // Manual motor test
    // --------------------------------------------------------

    case SystemState::MANUAL_MODE:
      if (leftAndRightPressed()) {
        if (!bothButtonLatch) {
          bothButtonLatch = true;

          enterState(
            SystemState::BOTH_MOTORS_RUN
          );

          return;
        }
      } else {
        bothButtonLatch = false;
      }

      if (
        leftButton.activeEvent &&
        !leftAndRightPressed()
      ) {
        selectedMotor = &motor1;
        showManualMode();
      }

      if (
        rightButton.activeEvent &&
        !leftAndRightPressed()
      ) {
        selectedMotor = &motor2;
        showManualMode();
      }

      if (upButton.activeEvent) {
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
        } else {
          enterState(
            SystemState::ESTOP_ACTIVE
          );
        }
      }

      if (downButton.activeEvent) {
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
        } else {
          enterState(
            SystemState::ESTOP_ACTIVE
          );
        }
      }
      break;

    // --------------------------------------------------------
    // Both-motor test
    // --------------------------------------------------------

    case SystemState::BOTH_MOTORS_RUN:
      if (
        millis() - motionStartTime >=
        BOTH_MOTOR_RUN_TIME_MS
      ) {
        enterState(
          SystemState::WAIT_REPEAT
        );

        break;
      }

      updateBothMotorMotion();
      break;

    case SystemState::WAIT_REPEAT:
      if (!leftAndRightPressed()) {
        bothButtonLatch = false;
      }

      if (
        leftAndRightPressed() &&
        !bothButtonLatch
      ) {
        bothButtonLatch = true;

        enterState(
          SystemState::BOTH_MOTORS_RUN
        );

        return;
      }

      if (
        leftButton.activeEvent &&
        !leftAndRightPressed()
      ) {
        selectedMotor = &motor1;

        enterState(
          SystemState::MANUAL_MODE
        );
      }

      if (
        rightButton.activeEvent &&
        !leftAndRightPressed()
      ) {
        selectedMotor = &motor2;

        enterState(
          SystemState::MANUAL_MODE
        );
      }
      break;

    // --------------------------------------------------------
    // E-stop recovery
    // --------------------------------------------------------

    case SystemState::ESTOP_ACTIVE:
      if (!estopActive()) {
        enterState(returnAfterEstop);
      }
      break;
  }
}