// This is an ultra simple software servo driver. For best
// results, use with a timer0 interrupt to refresh() all
// your servos once every 20 milliseconds!
// Written by Limor Fried for Adafruit Industries, BSD license

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "SoftServo.h"
/**
 * @brief Construct a new Adafruit_SoftServo::Adafruit_SoftServo object
 *
 */
SoftServo::SoftServo(void) {
  isAttached = false;
  servoPin = 255;
  //pos = 90;
}
/**
 * @brief Attacht to a supplied pin
 *
 * @param pin The pin to attach to for controlling the servo
 */
void SoftServo::attach(uint8_t _pin, uint16_t _min, uint16_t _max, uint16_t _pos) {
  servoPin = _pin;
  //pos = _pos;
  isAttached = true;
  min = _min;
  max = _max;
  pinMode(servoPin, OUTPUT);
}
/**
 * @brief Detach from the supplied pin
 *
 */
void SoftServo::detach(void) {
  isAttached = false;
  pinMode(servoPin, INPUT);
}
/**
 * @brief Get the attachment status of the pin
 *
 * @return boolean true: a pin is attached false: no pin is attached
 */
boolean SoftServo::attached(void) { return isAttached; }
/**
 * @brief Update the servo's angle setting and the corresponding pulse width
 *
 * @param a The target servo angle
 */
void SoftServo::write(uint8_t a) {
  

  if (!isAttached)
  {  
    return;
  }
  
  if(a < 200) // consider as angle
  {
    //pos = a;
    micros = map(a, 0, 180, min, max);
  }
  else // consider as pulseWidth
  {
    micros = a;
    if (a < min)
      micros = min;
    else if (a > max)
      micros = max;

    //pos = map(micros, min, max, 0, 180,);
  }
}
/**
 * @brief Pulse the control pin for the amount of time determined when the angle
 * was set with `write`
 *
 */
void SoftServo::refresh(void) {
  digitalWrite(servoPin, HIGH);
  delayMicroseconds(micros);
  digitalWrite(servoPin, LOW);
}
