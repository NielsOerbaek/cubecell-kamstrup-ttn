#include "KamstrupSerial.h"

uint8_t _rxpin = GPIO0;
uint8_t s_currentRx = ~0;
uint16_t s_timeDelay = 417;

RingBuffer<uint8_t, 128> s_rxDataBuffer = RingBuffer<uint8_t, 128>();
bool s_overflow = false;

void kamstrup_serial_init(uint8_t pin) {
  _rxpin = pin;
  pinMode(_rxpin,INPUT);
  digitalWrite(_rxpin,HIGH);
  kamstrup_listen();
}

void kamstrup_receiver(void)
{
  ClearPinInterrupt(s_currentRx);
  kamstrup_receiveByte();
  digitalWrite(s_currentRx, HIGH);
  attachInterrupt(s_currentRx, kamstrup_receiver, FALLING);
}

void kamstrup_listen() {
  kamstrup_stop();
  s_currentRx = _rxpin;
  digitalWrite(s_currentRx, HIGH);
  attachInterrupt(s_currentRx, kamstrup_receiver, FALLING);
}

void kamstrup_stop() {
  if (s_currentRx != uint8_t(~0)) {
    detachInterrupt(s_currentRx);
    s_currentRx = ~0;
    kamstrup_flush();
  }
}

bool kamstrup_isListening() {
  return (_rxpin == s_currentRx);
}

void kamstrup_receiveByte(void)
{
  uint8_t bitCount(0);
  uint8_t tempByte(0);

  delayMicroseconds(s_timeDelay);

  while (bitCount < 8) {
    if (!digitalRead(s_currentRx)) { 
        tempByte |= (1 << bitCount);
    }
    bitCount++;
    delayMicroseconds(s_timeDelay);
  }
  if (s_rxDataBuffer.full() == false) {
    s_rxDataBuffer.write(~tempByte);
  } else {
    s_overflow = true;
  }
}

int kamstrup_available()
{
  return s_rxDataBuffer.size();
}

void kamstrup_flush() {
  s_rxDataBuffer.reset();
  s_overflow = false;
}

int kamstrup_peek() {
  if (s_rxDataBuffer.empty() == false)
  {
    return s_rxDataBuffer.peek();
  }
  return (-1);
}


int kamstrup_read() {
  if (s_rxDataBuffer.empty() == false)
  {
    s_overflow = false;
    return s_rxDataBuffer.read();
  }
  return (-1);
}
