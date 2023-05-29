#include <ServoTimer2.h>
#include <A4990MotorShield.h>
#include <Wire.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>

// ---------- PINs & Definitions ----------
// ========================================

// If true, bluetooth module won't work (uses same pins RX&TX)
#define DEBUG 1

// Distance sensor
#define TRIG 12
#define ECHO 13

// Audio
#define BUZZER 11

// Bluetooth
#define KEY 4
#define STATE 2

// Distance sensor servo (need library as A4990 uses timer1, same as Servo.h)
ServoTimer2 servo;
#define SERVO 5
#define SERVO_CENTER 1515
// Positive delta = LEFT
#define SERVO_DELTA 300

// LCD
hd44780_I2Cexp lcd;

// Bluetooth module
#define AT_DELAY 50

// Motors shield
A4990MotorShield motors;
#define MOTOR_MAX 300

// ----------- Utility functions ----------
// ========================================

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


// --------------- Main code --------------
// ========================================

void setup() {
  // Initialize pins
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
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

#if DEBUG
long lastDisplay = 0;
#endif

void loop() {
  bool isConnected = digitalRead(STATE);
  if (isConnected != displayedIsConnected) {
    displayedIsConnected = isConnected;
    if (isConnected) displayMessage("Connected!");
    else {
      // Ensure it doesn't keep going if we disconnect while moving
      motors.setSpeeds(0, 0);
      displayMessage("No BT connection");
    }
  }
  // Read incoming data
  if (isConnected && Serial.available()) {
    String s = Serial.readStringUntil('\r');
    while (Serial.read() == -1); // make sure to also read the '\n'
    // S is of format "%lf,%lf", where numbers are -1..+1 and represent the desired speed(x and y axis)
    int comma = s.indexOf(',');
    if (comma != -1) {
      double x = s.substring(0, comma).toDouble();
      double y = s.substring(comma + 1).toDouble();

      // Display incoming data
      #if DEBUG
      long now = millis();
      if (now > lastDisplay + 1000) {
        lastDisplay = millis();
        displayMessage(s);
      }
      #endif
      
      // Coerce x and y into -1..+1 interval to avoid motor overload
      if (x < -1) x = -1;
      if (x > 1) x = 1;
      if (y < -1) y = -1;
      if (y > 1) y = 1;
      // At higher speeds, allow less turning (make it more controllable)
      x *= 1 - abs(y) * 0.5;
      if (y >= 0) x = -x;
      // Calculate power for engines
      int left = y * MOTOR_MAX - x * MOTOR_MAX;
      if (left < -MOTOR_MAX) left = -MOTOR_MAX;
      int right = y * MOTOR_MAX + x * MOTOR_MAX;
      if (right > MOTOR_MAX) y = MOTOR_MAX;
      // Send speeds to controller
      motors.setSpeeds(left, right);
    }
  }
}
