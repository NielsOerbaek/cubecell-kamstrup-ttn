#include "LoRaWan_APP.h"
#include "Arduino.h"

/* The Heltec-provided "Arduino.h" defines min and max as macros that can only take two arguments
 * But some of the libraries we need use other variations, so we undef them
 */
#undef max
#undef min

#include "gcm.h"
#include "mbusparser.h"
#include "secrets.h" // <-- create this file using "secrets.h.TEMPLATE"

#define DEBUG_BEGIN Serial.begin(115200);
#define DEBUG_PRINT(x) Serial.print(x);
#define DEBUG_PRINTLN(x) Serial.println(x);

#include "CubeCell_NeoPixel.h"
CubeCell_NeoPixel pixels(1, RGB, NEO_GRB + NEO_KHZ800);

/* Second serial port for meter reading 
 * SoftwareSerial isnt really working, so we've made a small custom library that does the trick. 
*/
#include "KamstrupSerial.h"
#define KAMSTRUP_DATA_PIN GPIO0
#define BAUD_RATE 2400 // Not actually used right now.

const size_t headersize = 11;
const size_t footersize = 3;
uint8_t encryption_key[16];
uint8_t authentication_key[16];
uint8_t receiveBuffer[400];
uint8_t decryptedFrameBuffer[400];
VectorView decryptedFrame(decryptedFrameBuffer, 0);
MbusStreamParser streamParser(receiveBuffer, sizeof(receiveBuffer));
mbedtls_gcm_context m_ctx;

/* These were moved from dynamically allocated from within the decrypt() */
uint8_t system_title[8];
uint8_t initialization_vector[12];
uint8_t additional_authenticated_data[17];
uint8_t authentication_tag[12];
uint8_t cipher_text[400];
uint8_t plaintext[400];
uint16_t cipher_text_size;

int8_t TX_INTERVAL = 110; // Time to wait between transmissions, not including TX windows
//int8_t TX_INTERVAL = 30; // Time to wait between transmissions, not including TX windows

/* LoraWan Config
/*LoraWan channelsmask, default channels 0-7*/ 
PROGMEM uint16_t userChannelsMask[6]={ 0x00FF,0x0000,0x0000,0x0000,0x0000,0x0000 };
/*LoraWan region, select in arduino IDE tools*/
LoRaMacRegion_t loraWanRegion = ACTIVE_REGION;
/*LoraWan Class, Class A and Class C are supported*/
DeviceClass_t  loraWanClass = LORAWAN_CLASS;
/*OTAA or ABP*/
bool overTheAirActivation = LORAWAN_NETMODE;
/*ADR enable*/
bool loraWanAdr = LORAWAN_ADR;
/* set LORAWAN_Net_Reserve ON, the node could save the network info to flash, when node reset not need to join again */
bool keepNet = LORAWAN_NET_RESERVE;
/* Indicates if the node is sending confirmed or unconfirmed messages */
bool isTxConfirmed = LORAWAN_UPLINKMODE;
/* Application port */
uint8_t appPort = 1; //NOTE: Back to the proper appPort

/*the application data transmission duty cycle.  value in [ms].*/
uint32_t appTxDutyCycle = 0; // We don't use this. We handle the timeouts in read_frame() instead.

uint8_t confirmedNbTrials = 4;

/* Payload Format: 7 2-byte integers: 14 bytes:
 * 0: Avg import in W
 * 1: Min import in W
 * 2: Max import in W
 * 3: Avg export in W
 * 4: Min export in W
 * 5: Max export in W
 * 6: Number of valid frames this is based on
 * 7: Battery voltage
 */
int16_t payload[8];

int32_t read_cnt;
int break_time;
int time_left;
int no_valid_timeout = 2; // After two times two minutes with no valid reads, we think something is probably wrong. 
int no_valid_counter = 0;

/* Prepares the payload of the frame */
static void prepareTxFrame( uint8_t port )
{
  /*appData size is LORAWAN_APP_DATA_MAX_SIZE which is defined in "commissioning.h".
  *appDataSize max value is LORAWAN_APP_DATA_MAX_SIZE.
  *if enabled AT, don't modify LORAWAN_APP_DATA_MAX_SIZE, it may cause system hanging or failure.
  *if disabled AT, LORAWAN_APP_DATA_MAX_SIZE can be modified, the max value is reference to lorawan region and SF.
  *for example, if use REGION_CN470, 
  *the max value for different DR can be found in MaxPayloadOfDatarateCN470 refer to DataratesCN470 and BandwidthsCN470 in "RegionCN470.h".
  */
    appPort = port;
    if(port == 1) { // If data is good
      payload[7] = getBatteryVoltage();
      appDataSize = sizeof(payload);
      memcpy(appData, payload, appDataSize);
      DEBUG_PRINT(F("Sending Payload: "));
      DEBUG_PRINTLN(appDataSize);
      return;
    }
    if(port == 4) { // Sending reset msg
      appDataSize = 1;
      appData[0] = 1;
    }
}

void setup() {
  //boardInitMcu();
  DEBUG_BEGIN
  blink(10,10,10,1);
  DEBUG_PRINTLN("DEVICE_STATE_INIT");
  deviceState = DEVICE_STATE_INIT;
  LoRaWAN.ifskipjoin();
  blink(10,10,10,2);
  DEBUG_PRINTLN("INIT LED");
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext,LOW); //SET POWER
  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.clear(); // Set all pixel colors to 'off'
  
  DEBUG_PRINTLN("Initiating KamstrupSerial on RX=GPIO0");
  kamstrup_serial_init(KAMSTRUP_DATA_PIN);
  hexStr2bArr(encryption_key, conf_key, sizeof(encryption_key));
  hexStr2bArr(authentication_key, conf_authkey, sizeof(authentication_key));
  DEBUG_PRINTLN("Setup completed");
  blink(10,10,10,3);
}

void loop()
{
  switch( deviceState )
  {
    case DEVICE_STATE_INIT:
    {
#if(AT_SUPPORT)
      getDevParam();
#endif
      printDevParam();
      LoRaWAN.init(loraWanClass,loraWanRegion);
      deviceState = DEVICE_STATE_JOIN;
      break;
    }
    case DEVICE_STATE_JOIN:
    {
      LoRaWAN.join();
      break;
    }
    case DEVICE_STATE_SEND:
    {
      if(read_frame()) {
        no_valid_counter = 0;
        prepareTxFrame( appPort );
        LoRaWAN.send();
      } else {
        no_valid_counter++;
        if(no_valid_counter >= no_valid_timeout) {
          prepareTxFrame(4);
          LoRaWAN.send();
          delay(5000); // Give a little time to send
          reset_board();
        }
      }
      deviceState = DEVICE_STATE_CYCLE;
      break;
    }
    case DEVICE_STATE_CYCLE:
    {
      // Schedule next packet transmission
      txDutyCycleTime = appTxDutyCycle + randr( 0, APP_TX_DUTYCYCLE_RND );
      LoRaWAN.cycle(txDutyCycleTime);
      deviceState = DEVICE_STATE_SLEEP;
      break;
    }
    case DEVICE_STATE_SLEEP:
    {
      LoRaWAN.sleep();
      break;
    }
    default:
    {
      deviceState = DEVICE_STATE_INIT;
      break;
    }
  }
}

void blink(int r, int g, int b, int times) {
  for(int i = 0; i < times; i++) {
    pixels.setPixelColor(0, pixels.Color(r, g, b));
    pixels.show();
    delay(100); // Pause before next pass through loop
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();
    delay(100);
  }
}

bool read_frame() {
  DEBUG_PRINTLN("read_frame");
  reset_payload();
  read_cnt = 0;
  break_time = millis() + TX_INTERVAL*1000;
  time_left;
  while(true) {
    if(millis() > break_time) {
        DEBUG_PRINTLN("Timeout! Got "+String(read_cnt)+" frames.");
        if(read_cnt > 0) {  // Got valid values
          blink(0,0,20,3);
          return true;     
        } else {            // No valid values
          blink(20,0,0,3);
          return false;    
        }
      }
    while (kamstrup_available() > 0) {
      if (streamParser.pushData(kamstrup_read())) {
        VectorView frame = streamParser.getFrame();
        if (streamParser.getContentType() == 0) {
          if (!decrypt(frame)){
            blink(20,0,0,1);
          } else {
            MeterData md = parseMbusFrame(decryptedFrame);
            if(md.activePowerPlusValid && md.activePowerMinusValid) {
              read_cnt++;
              payload[6] = read_cnt;
              DEBUG_PRINTLN("Decrypt ok, cnt:"+String(read_cnt));
              blink(0,20,0,read_cnt);
              payload[0] = do_average(payload[0], md.activePowerPlus, read_cnt);
              payload[3] = do_average(payload[3], md.activePowerMinus, read_cnt);
              
              if(payload[1] == -1) { payload[1] = md.activePowerPlus; }
              else { payload[1] = std::min((int) payload[1], (int) md.activePowerPlus); }
              
              if(payload[2] == -1) { payload[2] = md.activePowerPlus; }
              else { payload[2] = std::max((int) payload[2], (int) md.activePowerPlus); }
              
              if(payload[4] == -1) { payload[4] = md.activePowerMinus; }
              else { payload[4] = std::min((int) payload[4], (int) md.activePowerMinus); }
              
              if(payload[5] == -1) { payload[5] = md.activePowerMinus; }
              else { payload[5] = std::max((int) payload[5], (int) md.activePowerMinus); }
            }
          }
        }
      }
    }
  }
}


int32_t do_average(int32_t avg, int32_t val, int32_t cnt) {
  return (avg*(cnt-1)+val)/cnt;
}

void printHex(const unsigned char* data, const size_t length) {
  for (int i = 0; i < length; i++) {
    Serial.printf("%02X", data[i]);
  }
}

void printHex(const VectorView& frame) {
  for (int i = 0; i < frame.size(); i++) {
    Serial.printf("%02X", frame[i]);
  }
}

bool decrypt(const VectorView& frame) {

  if (frame.size() < headersize + footersize + 12 + 18) {
    Serial.println("Invalid frame size.");
    return false;
  }

  cipher_text_size = frame.size() - headersize - footersize - 18 - 12;
  if(cipher_text_size > 400) {
    Serial.println("Decrypt failed: Cipher-text too big for memory");
    return false;
  }

  memcpy(decryptedFrameBuffer, &frame.front(), frame.size());
  
  memcpy(system_title, decryptedFrameBuffer + headersize + 2, 8);
  
  memcpy(initialization_vector, system_title, 8);
  memcpy(initialization_vector + 8, decryptedFrameBuffer + headersize + 14, 4);

  memcpy(additional_authenticated_data, decryptedFrameBuffer + headersize + 13, 1);
  memcpy(additional_authenticated_data + 1, authentication_key, 16);
  
  memcpy(authentication_tag, decryptedFrameBuffer + headersize + frame.size() - headersize - footersize - 12, 12);

  memcpy(cipher_text, decryptedFrameBuffer + headersize + 18, cipher_text_size);

  mbedtls_gcm_init(&m_ctx);
  int success = mbedtls_gcm_setkey(&m_ctx, MBEDTLS_CIPHER_ID_AES, encryption_key, sizeof(encryption_key) * 8);
  if (0 != success) {
    Serial.println("Setkey failed: " + String(success));
    return false;
  }
  success = mbedtls_gcm_auth_decrypt(&m_ctx, cipher_text_size, initialization_vector, sizeof(initialization_vector),
                                     additional_authenticated_data, sizeof(additional_authenticated_data), authentication_tag, sizeof(authentication_tag),
                                     cipher_text, plaintext);
  if (0 != success) {
    Serial.println("authdecrypt failed: " + String(success));
    return false;
  }
  mbedtls_gcm_free(&m_ctx);

  //copy replace encrypted data with decrypted for mbusparser library. Checksum not updated. Hopefully not needed
  memcpy(decryptedFrameBuffer + headersize + 18, plaintext, cipher_text_size);
  decryptedFrame = VectorView(decryptedFrameBuffer, frame.size());

  return true;
}

void reset_board() {
  HW_Reset(0);
}

void hexStr2bArr(uint8_t* dest, const char* source, int bytes_n)
{
  uint8_t* dst = dest;
  uint8_t* end = dest + sizeof(bytes_n);
  unsigned int u;

  while (dest < end && sscanf(source, "%2x", &u) == 1)
  {
    *dst++ = u;
    source += 2;
  }
}

/** METHODS FROM HELTEC OTAA SKETCH **/
void printHex2(unsigned v) {
  v &= 0xff;
  if (v < 16)
    Serial.print('0');
  Serial.print(v, HEX);
  Serial.print(" ");
}

void reset_payload() {
  // Reset the payload values
  payload[6] = 0;
  payload[0] = 0;
  payload[1] = -1;
  payload[2] = -1;
  payload[3] = 0;
  payload[4] = -1;
  payload[5] = -1;
}
