// This is an ultra simple software servo driver. For best
// results, use with a timer0 interrupt to refresh() all
// your servos once every 20 milliseconds!
// Written by Limor Fried for Adafruit Industries, BSD license

#if ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
/**
 * @brief Class for basic software servo control
 *
 */
class SoftServo {
public:
  SoftServo(void);
  void attach(uint8_t _pin, uint16_t _min = 1000, uint16_t _max = 2000, uint16_t _pos = 0);
  void detach();
  boolean attached();
  void write(uint8_t a);
  void refresh(void);

private:
  boolean isAttached;
  uint8_t servoPin; //, pos;
  uint16_t min, max, micros;
};
