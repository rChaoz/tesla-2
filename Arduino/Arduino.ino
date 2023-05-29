#include <ServoTimer2.h>
#include <A4990MotorShield.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <NewPing.h>

// ----------- Debugging Helper- ----------
// ========================================

// Displays stuff instead of connection status on LCD
#define DEBUG_DATA_IN 1
#define DEBUG_DISTANCE 2
// Comment this line to disable debug information
//#define DEBUG 2

// Used to not spam LCD with updates
#ifdef DEBUG

long lastDisplay = 0;
#define DEBUG_INTERVAL 200

void displayMessage(String message);
void debugMessage(String message) {
  long now = millis();
  if (now < lastDisplay + DEBUG_INTERVAL) return;
  lastDisplay = now;
  displayMessage(message);
}

#endif

// ------------ PINs & Devices-- ----------
// ========================================

// Distance sensor
NewPing sonar(13, 12, 300);

// Audio
#define BUZZER 5

// Bluetooth
#define KEY 4
#define STATE 2

// Distance sensor servo (need library as A4990 uses timer1, same as Servo.h)
ServoTimer2 servo;
#define SERVO 11
#define SERVO_CENTER 1507
// Positive delta = LEFT
#define SERVO_MAX_DELTA 300
#define SERVO_STEP 60
// Scanning info
#define SCAN_INTERVAL 75
long lastScan = 0;
#define LEFT -1
#define RIGHT 1
int servoDir = RIGHT;
int servoPos = 0;
#define MAX_STEPS (SERVO_MAX_DELTA / SERVO_STEP)

// LCD
hd44780_I2Cexp lcd;

// Bluetooth module
#define AT_DELAY 50

// Motors shield
A4990MotorShield motors;
#define MOTOR_MAX_SWEEP 350
#define MOTOR_MAX 250
#define MIN_SPEED 0.2f

// ----------- Utility functions ----------
// ========================================

// Current x and y directional inputs
double x, y, finalX;

/**
 * Displays a message centered on the LCD second row. If too long, message will be trimmed.
 */
bool displayedIsConnected = 1;
void displayMessage(String message) {
  int len = message.length();
  lcd.setCursor(0, 1);
  if (len >= 16) {
   lcd.print(message.substring(0, 16));
  } else if (len < 16) {
    int spaceCount = (16 - len) / 2;
    for (int i = 0; i < spaceCount; ++i) lcd.write(' ');
    lcd.print(message);
    for (int i = 0; i < 16 - len - spaceCount; ++i) lcd.write(' ');
  }
}

/**
 * Sets sweeping enabled. If false, distance sensor will only look straight forward, instead of constantly turning left-right
 */
bool sweeping = false;
void setSweeping(bool sweep) {
  sweeping = sweep;
  if (!sweep) {
    servo.write(SERVO_CENTER);
    servoPos = 0;
  }
}

/**
 * Whether to attempt to dodge (swirve around) obstacles rather than slowing down
 */
bool dodging = false;

// Allow negative-index aceess of distances vector
const int NUM_DISTANCES = MAX_STEPS * 2 + 1;
int _distances[NUM_DISTANCES], *distances = _distances + MAX_STEPS;
int frontalDistance = 0;

/**
 * Reads distance, updates distances vector, calculates frontal distance and moves servo to next step
 */
void scan() {
  int distance = sonar.ping_cm();

  // Display on LCD for debug
  #if DEBUG == DEBUG_DISTANCE
  debugMessage(String(distance) + " cm");
  #endif

  // Use 1000 instead of 0 for infinity / "very far"
  if (distance == 0) distance = 1000;
  // Update distances vector
  distances[servoPos] = distance;
  if (!sweeping) {
    frontalDistance = distance;
    return;
  }
  
  // Calculate forward distance (frontal 5 distances)
  if (abs(servoPos) < 3) {
    frontalDistance = distances[0];
    if (distances[ 1] < frontalDistance) frontalDistance = distances[ 1];
    if (distances[ 2] < frontalDistance) frontalDistance = distances[ 2];
    if (distances[-1] < frontalDistance) frontalDistance = distances[-1];
    if (distances[-2] < frontalDistance) frontalDistance = distances[-2];
  }

  // Move to next position
  if (servoPos == MAX_STEPS) servoDir = LEFT;
  else if (servoPos == -MAX_STEPS) servoDir = RIGHT;
  servoPos += servoDir;
  servo.write(SERVO_CENTER - SERVO_STEP * servoPos);
}

float speedLimit = 1.0f;

void dodgeObstacles() {
  speedLimit = 1.0f;
  
  // Test if we should dodge in the first place
  if (frontalDistance > 60) return;

  // If we are too close, also limit speed
  if (frontalDistance < 20) speedLimit = max(frontalDistance / 25.0f, MIN_SPEED);

  // Dont turn if we are not moving forward
  if (y <= 0) return;

  // Calculate in which direction to turn
  int leftAverage = 0, rightAverage = 0;
  for (int i = MAX_STEPS; i > 0; --i) {
    leftAverage += distances[-i];
    rightAverage += distances[i];
  }
  leftAverage /= MAX_STEPS;
  rightAverage /= MAX_STEPS;

  int dir = leftAverage > rightAverage ? -1 : 1;
  
  // If we are already turning, prefer turning in the same direction
  if (abs(x) > .2 && abs(leftAverage - rightAverage) < 20) dir = x > 0 ? 1 : -1;
  
  // Turn harder the closer we are to the obstacle
  // Also, scale turn speed with car speed
  double xRem; // remaining x in chosen direction
  if (dir == 1) xRem = (1.0 - x) * abs(y) * 0.8;
  else xRem = (-1.0 - x) * abs(y) * 0.8;
  finalX = x + xRem * (60.0 - frontalDistance) / 60.0;
}

/**
 * Limit speed based on distance to forward obstacle
 */
void limitSpeed() {
  int distance = distances[0];
  if (sweeping) {
    // Use max of frontal 5 distances
    if (distances[ 1] < distance) distance = distances[ 1];
    if (distances[ 2] < distance) distance = distances[ 2];
    if (distances[-1] < distance) distance = distances[-1];
    if (distances[-2] < distance) distance = distances[-2];
  }
  // Don't limit speed for distances above 50cm
  if (distance > 50) {
    speedLimit = 1.0f;
    return;
  }
  
  // Limit linearly to a minimum speed
  speedLimit = max(distance / 50.0f, MIN_SPEED);
}

/**
 * Update motor's actual speeds to match x and y directional input values
 */
void setSpeeds() {
  // Calculate power for engines based on directional inputs
  const int M = sweeping ? MOTOR_MAX_SWEEP : MOTOR_MAX;
  int left = y * M + finalX * M;
  if (left < -M) left = -M;
  int right = y * M - finalX * M;
  if (right > M) right = M;
  
  // Limit applies less to backwards movement
  float limit = (left < 0 && right < 0) ? sqrt(speedLimit) : speedLimit;

  // Adjust for right motor being slightly slower (for whatever reason)
  motors.setSpeeds(left * limit, right * limit * 1.05f);
}

/*
 * Beep when we are close to an obstacle
 */

#define BEEP_DURATION 80

long lastBeep = 0;
bool beeping = false;
bool soundEnabled = false;

void beep() {
  if (frontalDistance < 8) {
    beeping = true;
    lastBeep = 0;
    analogWrite(BUZZER, 127);
    return;
  }

  long now = millis();
  
  // If beeping, keep going until timer runs out
  if (beeping && now < lastBeep + BEEP_DURATION) return;
  else if (beeping) {
    lastBeep = now;
    beeping = false;
    analogWrite(BUZZER, 0);
  }

  // Don't beep if no obstacles are close
  if (frontalDistance > 60) return;
  
  // Calculate beep pause duration
  // Aprox. 600ms at 60cm, 60ms at 10cm
  long pause = (frontalDistance - 5) * 10;

  // Start beeping if we waited long enough
  if (now > lastBeep + pause) {
    lastBeep = now;
    beeping = true;
    analogWrite(BUZZER, 127);
  }
}

void noBeep() {
  if (!beeping) return;
  beeping = false;
  analogWrite(BUZZER, 0);
}

// -------- Communication functions -------
// ========================================

void readBluetooth() {
  String s = Serial.readStringUntil('\r');
  while (Serial.read() == -1); // make sure to also read the '\n'

  // Parse incoming data
  // S is of format "$x,$y,$sweeping", where x, y are double -1..+1 and represent the desired speed (x and y axis)
  int numValues = 5;
  String values[numValues];
  int comma = s.indexOf(','), prev = 0;
  for (int i = 0; i < numValues - 1; ++i) {
    if (comma == -1) {
      displayMessage("INPUT ERROR");
      return;
    }
    values[i] = s.substring(prev, comma);
    prev = comma + 1;
    comma = s.indexOf(',', prev);
  }
  if (comma != -1) {
    displayMessage("INPUT ERROR");
    return;
  }
  values[numValues - 1] = s.substring(prev);

  // Convert data to correct types
  x = values[0].toDouble();
  y = values[1].toDouble();
  int sweeping = values[2].toInt();
  /*global*/ dodging = values[3].toInt();
  /*global*/ soundEnabled = values[4].toInt();

  // Display incoming data
  #if DEBUG == DEBUG_DATA_IN
  debugMessage(s);
  #endif
  
  // Coerce x and y into -1..+1 interval to avoid motor overload
  if (x < -1) x = -1;
  if (x > 1) x = 1;
  if (y < -1) y = -1;
  if (y > 1) y = 1;
  // Reduce rotation speed to make it more controllable
  x /= 2;
  if (y < 0) x = -x;

  // Set sweeping
  setSweeping(sweeping);
}

void sendBluetooth() {
  // First, send distances vector
  Serial.print(_distances[0]);
  for (int i = 1; i < NUM_DISTANCES; ++i) {
    Serial.print(',');
    Serial.print(_distances[i]);
  }

  // TODO Sent other information

  // Finish message
  Serial.println();
}

// --------------- Main code --------------
// ========================================

void setup() {
  // Initialize pins
  pinMode(BUZZER, OUTPUT);
  pinMode(KEY, OUTPUT);
  pinMode(STATE, INPUT);
  // Initialize servo
  servo.attach(SERVO);
  // Initialize USART for bluetooth/debug
  Serial.begin(9600);
  // Initialize motors
  motors.flipM2(true);
  // Initialize lcd
  lcd.begin(16, 2);
  lcd.setCursor(4, 0);
  lcd.print("Tesla II");
}

// If should send data on next loop
bool shouldSend = false;

void loop() {
  long now = millis();
  
  // Check connection
  bool isConnected = digitalRead(STATE);
  // Only display this if debug is disabled, to not interfere
  #ifndef DEBUG
  if (isConnected != displayedIsConnected) {
    displayedIsConnected = isConnected;
    if (isConnected) displayMessage("Connected!");
    else {
      // Ensure it doesn't keep going if we disconnect while moving
      x = y = 0;
      setSweeping(false);
      displayMessage("No BT connection");
    }
  }
  #endif

  // Send data to host
  if (isConnected && shouldSend) {
    sendBluetooth();
    shouldSend = false;
  }
  // Read incoming data
  if (isConnected && Serial.available()) {
    readBluetooth();
    // Send data on next loop
    shouldSend = true;
  }

  // Scan distances routine
  if (now > lastScan + SCAN_INTERVAL) {
    lastScan = now;
    scan();
    // Perform actions based on scan results
    finalX = x;
    if (dodging) dodgeObstacles();
    else limitSpeed();
    setSpeeds();
  }

  // Don't forget to be noisy
  if (soundEnabled) beep();
  else noBeep();
}
