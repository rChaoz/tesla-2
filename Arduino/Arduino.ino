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

// ---------- PINs & Definitions ----------
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

void loop() {
  
}
