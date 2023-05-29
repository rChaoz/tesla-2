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
#define DEBUG 2

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
#define BUZZER 11

// Bluetooth
#define KEY 4
#define STATE 2

// Distance sensor servo (need library as A4990 uses timer1, same as Servo.h)
ServoTimer2 servo;
#define SERVO 5
#define SERVO_CENTER 1520
// Positive delta = LEFT
#define SERVO_MAX_DELTA 300
#define SERVO_STEP 50
// Scanning info
#define SCAN_INTERVAL 40
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
#define MOTOR_MAX 350

// ----------- Utility functions ----------
// ========================================

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

/**
 * Reads forward distance, updates distances vector and moves servo to next step
 */
void scan() {
  int distance = sonar.ping_cm();

  // Display on LCD for debug
  #if DEBUG == DEBUG_DISTANCE
  debugMessage(String(distance) + " cm");
  #endif

  // Update distances vector
  distances[servoPos] = distance;

  if (!sweeping) return;
  // Move to next position
  if (servoPos == MAX_STEPS) servoDir = LEFT;
  else if (servoPos == -MAX_STEPS) servoDir = RIGHT;
  servoPos += servoDir;
  servo.write(SERVO_CENTER + SERVO_STEP * servoPos);
}

float speedLimit = 1.0f;

void dodgeObstacles() {
  
  // TODO
}

/**
 * Limit speed based on distance to forward obstacle
 */
void limitSpeed() {
  int distance = distances[0];
  if (sweeping) {
    // Use max of frontal 5 distances
    if (distances[1] > distance) distance = distances[1];
    if (distances[2] > distance) distance = distances[2];
    if (distances[-1] > distance) distance = distances[-1];
    if (distances[-2] > distance) distance = distances[-2];
  }
  // Don't limit speed for distances above 50cm
  if (distance > 50) {
    speedLimit = 1.0f;
    return;
  }
  
  // Limit linearly to a minimum speed of 20%
  speedLimit = max(distance / 50.0f, 0.20f);
}

void setSpeeds(int s1, int s2) {
  // Limit applies less to backwards movement
  float limit = (s1 < 0 && s2 < 0) ? sqrt(speedLimit) : speedLimit;
  motors.setSpeeds(s1 * limit, s2 * limit);
}

// -------- Communication functions -------
// ========================================

void readBluetooth() {
  String s = Serial.readStringUntil('\r');
  while (Serial.read() == -1); // make sure to also read the '\n'

  // Parse incoming data
  // S is of format "$x,$y,$sweeping", where x, y are double -1..+1 and represent the desired speed (x and y axis)
  int numValues = 4;
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
  double x = values[0].toDouble();
  double y = values[1].toDouble();
  int sweeping = values[2].toInt();
  /*global*/ dodging = values[3].toInt();

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
  if (y >= 0) x = -x;
  // Calculate power for engines
  int left = y * MOTOR_MAX - x * MOTOR_MAX;
  if (left < -MOTOR_MAX) left = -MOTOR_MAX;
  int right = y * MOTOR_MAX + x * MOTOR_MAX;
  if (right > MOTOR_MAX) y = MOTOR_MAX;
  // Send speeds to controller
  setSpeeds(left, right);

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
      motors.setSpeeds(0, 0);
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
    if (dodging) dodgeObstacles();
    else limitSpeed();
  }
}
