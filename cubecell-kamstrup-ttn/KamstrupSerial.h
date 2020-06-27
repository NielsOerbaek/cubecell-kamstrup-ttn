#ifndef KamstrupSerial_h
#define KasmtrupSerial_h

#include "Arduino.h"
#include "Stream.h"
#include "RingBuffer.h"

#include <stdlib.h>


void kamstrup_serial_init(uint8_t pin);
void kamstrup_receiver(void);
void kamstrup_listen();
void kamstrup_stop();
bool kamstrup_isListening();
void kamstrup_receiveByte(void);
int kamstrup_available();
void kamstrup_flush();
int kamstrup_peek();
int kamstrup_read();
 #endif
