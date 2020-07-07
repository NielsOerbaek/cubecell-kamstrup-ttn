#include <Wire.h>
#include "dasya_logo.h"
#include <ACROBOTIC_SSD1306.h>

static const uint8_t PROGMEM PAYLOAD[] = {
    0x7E, 0xA1, 0x3B, 0x41, 0x03, 0x13, 0xC8, 0xF6, 
    0xE6, 0xE7, 0x00, 0xDB, 0x08, 0x4B, 0x41, 0x4D, 
    0x45, 0x01, 0xC7, 0x72, 0x3A, 0x82, 0x01, 0x22, 
    0x30, 0x00, 0x04, 0x56, 0xF9, 0x4E, 0xFF, 0x9E, 
    0x97, 0xAF, 0x17, 0xB2, 0xFC, 0xAF, 0xD6, 0x2F, 
    0x9B, 0x10, 0xDA, 0x2A, 0x87, 0xAA, 0xB0, 0xC1, 
    0x21, 0xE0, 0x5B, 0xBB, 0x61, 0xFA, 0xAB, 0x10, 
    0x24, 0x98, 0xAE, 0x47, 0x92, 0x33, 0xAE, 0x65, 
    0xA6, 0xB3, 0xCF, 0x9B, 0xE3, 0x15, 0x33, 0x69, 
    0x48, 0xF2, 0x60, 0x61, 0xBA, 0xC4, 0x65, 0x1B, 
    0x42, 0x87, 0xC1, 0xE5, 0xF2, 0x3C, 0x26, 0x18, 
    0x39, 0x67, 0xCA, 0x24, 0x55, 0x3C, 0x7A, 0x38, 
    0xA6, 0x59, 0xE1, 0x0E, 0x3F, 0x3F, 0x71, 0x74, 
    0x34, 0x48, 0x25, 0x33, 0xCE, 0x1A, 0x01, 0xC8, 
    0x7A, 0xDD, 0xC5, 0x5A, 0xBE, 0xF7, 0x45, 0x91, 
    0x85, 0x0E, 0xA6, 0xEB, 0x34, 0x12, 0xE0, 0x22, 
    0xF5, 0x8A, 0x5E, 0x52, 0x28, 0x17, 0x2B, 0x16, 
    0x75, 0x11, 0x87, 0xC1, 0x50, 0xCA, 0x5F, 0x7F, 
    0x44, 0x70, 0x66, 0xDB, 0x7F, 0x22, 0xBD, 0xC3, 
    0x47, 0xA3, 0xA8, 0xD7, 0x2E, 0x2C, 0x76, 0x5E, 
    0xD8, 0xD6, 0x2D, 0x53, 0xFC, 0x26, 0x59, 0x30, 
    0xEE, 0x75, 0x50, 0x04, 0x42, 0xB9, 0x2B, 0x05, 
    0xB5, 0x4A, 0x10, 0x77, 0x4D, 0x24, 0x6E, 0x98, 
    0x2A, 0x55, 0xF4, 0xD1, 0xFD, 0x5A, 0xCB, 0xD7, 
    0x41, 0x86, 0x19, 0xF0, 0x14, 0x73, 0x76, 0x96, 
    0x41, 0x3B, 0x21, 0xE3, 0xF4, 0x79, 0x69, 0x78, 
    0xE1, 0xA0, 0x49, 0x3F, 0x42, 0x0E, 0xC3, 0x9D, 
    0x65, 0xF5, 0xDF, 0x74, 0x44, 0x2E, 0xA9, 0x0B, 
    0x7D, 0xA4, 0x7D, 0x4D, 0x66, 0x7B, 0x1B, 0x59, 
    0xF2, 0x6E, 0x85, 0x19, 0x5D, 0x42, 0x52, 0x5D, 
    0xF6, 0x7C, 0x70, 0xC3, 0x8C, 0xB7, 0x4B, 0x93, 
    0xE3, 0x72, 0x9E, 0xB1, 0xC8, 0x6B, 0xC7, 0x16, 
    0x75, 0x97, 0x82, 0x30, 0xF9, 0xBC, 0x63, 0x31, 
    0x92, 0x60, 0xD1, 0x56, 0xC0, 0xD7, 0xBE, 0x21, 
    0x05, 0x8D, 0x15, 0xF9, 0x53, 0x9C, 0xBD, 0xB1, 
    0xCD, 0xF3, 0xA5, 0xB8, 0x11, 0x28, 0x08, 0x00, 
    0xAF, 0xC1, 0x45, 0x3C, 0x66, 0x85, 0xC8, 0x54, 
    0x78, 0xEA, 0xAE, 0xF8, 0x72, 0x52, 0xF9, 0x47, 
    0x68, 0x34, 0x5B, 0x64, 0xC2, 0x52, 0x1A, 0x58, 
    0x67, 0xCE, 0x85, 0xB9, 0x7E };

/* Second serial port for meter reading */
#define RXD2 23 //16 is used for OLED_RST !
#define TXD2 17
#define BAUD_RATE 2400

int send_interval = 4 * 1000; // Send every ten seconds
int last_send;

void setup() {
  pinMode(16,OUTPUT);
  digitalWrite(16, LOW);    // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(16, HIGH); // while OLED is running, must set GPIO16 to high  
  Wire.begin(4,15);  
  oled.init();                      // Initialze SSD1306 OLED display
  oled.clearDisplay();

  // Show DASYA Logo. Not really needed for anything, but looks nice.
  draw_logo("Omnipower Sim");
  delay(3000);
  oled.clearDisplay();
  update_display(F("Setup Serial"));
  Serial2.begin(BAUD_RATE, SERIAL_8N1, RXD2, TXD2);
  last_send = millis()-send_interval;
}

void loop() {
  // put your main code here, to run repeatedly:
  if(millis() > last_send+send_interval) {
    update_display(F("Sending payload"));
    last_send = millis();
    for(int i=0; i<sizeof(PAYLOAD); i++) {
      Serial2.write(PAYLOAD[i]);
    }
  } else {
    if(millis()%1000 == 0) {
      update_display(String((last_send+send_interval-millis())/1000) + "s left");
    }
  }
}

void update_display(String msg) {
  oled.setTextXY(0,0);       
  oled.putString("Msg:");
  int line = 1;
  int curs = 0;
  while(line < 8) {
    oled.setTextXY(line,0);
    oled.putString("                "); // First clear the line
    oled.setTextXY(line,0); 
    oled.putString(msg.substring(curs,curs+16));
    msg = msg.substring(curs);
    curs += 16;
    line++;
  }
}

/*

7E A1 3B 41 03 13 C8 F6 E6 E7 00 DB 08 4B 41 4D 45 01 C7 72 3A 82 01 22 30 00 04 56 F9 4E FF 9E 97 AF 17 B2 FC AF D6 2F 9B 10 DA 2A 87 AA B0 C1 21 E0 5B BB 61 FA AB 10 24 98 AE 47 92 33 AE 65 A6 B3 CF 9B E3 15 33 69 48 F2 60 61 BA C4 65 1B 42 87 C1 E5 F2 3C 26 18 39 67 CA 24 55 3C 7A 38 A6 59 E1 0E 3F 3F 71 74 34 48 25 33 CE 1A 01 C8 7A DD C5 5A BE F7 45 91 85 0E A6 EB 34 12 E0 22 F5 8A 5E 52 28 17 2B 16 75 11 87 C1 50 CA 5F 7F 44 70 66 DB 7F 22 BD C3 47 A3 A8 D7 2E 2C 76 5E D8 D6 2D 53 FC 26 59 30 EE 75 50 04 42 B9 2B 05 B5 4A 10 77 4D 24 6E 98 2A 55 F4 D1 FD 5A CB D7 41 86 19 F0 14 73 76 96 41 3B 21 E3 F4 79 69 78 E1 A0 49 3F 42 0E C3 9D 65 F5 DF 74 44 2E A9 0B 7D A4 7D 4D 66 7B 1B 59 F2 6E 85 19 5D 42 52 5D F6 7C 70 C3 8C B7 4B 93 E3 72 9E B1 C8 6B C7 16 75 97 82 30 F9 BC 63 31 92 60 D1 56 C0 D7 BE 21 05 8D 15 F9 53 9C BD B1 CD F3 A5 B8 11 28 08 00 AF C1 45 3C 66 85 C8 54 78 EA AE F8 72 52 F9 47 68 34 5B 64 C2 52 1A 58 67 CE  85 B9 7E

 */