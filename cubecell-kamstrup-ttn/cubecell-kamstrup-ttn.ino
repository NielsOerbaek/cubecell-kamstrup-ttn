#include "LoRaWan_APP.h"
#include "Arduino.h"

#include "gcm.h"
#include "mbusparser.h"
#include "secrets.h" // <-- create this file using "secrets.h.TEMPLATE"
#include "KamstrupSerial.h"

#define DEBUG_BEGIN Serial.begin(115200);
#define DEBUG_PRINT(x) Serial.print(x);
#define DEBUG_PRINTLN(x) Serial.println(x);

/* Second serial port for meter reading 
 * SoftwareSerial isnt really working, so we've made a small custom library that does the trick. 
*/
#define KAMSTRUP_DATA_PIN GPIO0
#define BAUD_RATE 2400

const size_t headersize = 11;
const size_t footersize = 3;
uint8_t encryption_key[16];
uint8_t authentication_key[16];
uint8_t receiveBuffer[500];
uint8_t decryptedFrameBuffer[500];
VectorView decryptedFrame(decryptedFrameBuffer, 0);
MbusStreamParser streamParser(receiveBuffer, sizeof(receiveBuffer));
mbedtls_gcm_context m_ctx;

int8_t TX_INTERVAL = 110; // Time to wait between transmissions, not including TX windows
//int8_t TX_INTERVAL = 10; // Time to wait between transmissions, not including TX windows

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
uint8_t appPort = 2;

/*the application data transmission duty cycle.  value in [ms].*/
uint32_t appTxDutyCycle = 0;

uint8_t confirmedNbTrials = 4;

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
    int bat = getBatteryVoltage();
    DEBUG_PRINT(F("Sending Batt: "));
    DEBUG_PRINTLN(F(bat));
    appDataSize = 2;
    appData[0] = bat & 0x000000ff;
    appData[1] = (bat & 0x0000ff00) >> 8;
}


/* Payload Format: 7 2-byte integers: 14 bytes:
 * 0: Avg import in W
 * 1: Min import in W
 * 2: Max import in W
 * 3: Avg export in W
 * 4: Min export in W
 * 5: Max export in W
 * 6: Number of valid frames this is based on
 */

int16_t payload[7];

int32_t read_cnt;
int break_time;
int time_left;

uint8_t system_title[8];
uint8_t initialization_vector[12];
uint8_t additional_authenticated_data[17];
uint8_t authentication_tag[12];
uint8_t cipher_text[10];
uint8_t plaintext[10];


void setup() {
  DEBUG_BEGIN

  DEBUG_PRINTLN("DEVICE_STATE_INIT");
  deviceState = DEVICE_STATE_INIT;
  LoRaWAN.ifskipjoin();
  
  DEBUG_PRINTLN("Initiating KamstrupSerial on RX=GPIO0");
  kamstrup_serial_init(KAMSTRUP_DATA_PIN);
  hexStr2bArr(encryption_key, conf_key, sizeof(encryption_key));
  hexStr2bArr(authentication_key, conf_authkey, sizeof(authentication_key));
  DEBUG_PRINTLN("Setup completed");
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
      read_frame();
      prepareTxFrame( appPort );
      LoRaWAN.send();
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


void read_frame() {
  DEBUG_PRINTLN("read_frame");
  reset_payload();
  read_cnt = 0;
  break_time = millis() + TX_INTERVAL*1000;
  time_left;
  int width = 0;
  while(true) {
    if(millis() > break_time) {
        DEBUG_PRINTLN("Timeout! Got "+String(read_cnt)+" frames.");
        return;            
      }
      if((millis()-break_time)%3000 == 0) {
        DEBUG_PRINTLN("");
        width = 0;
        delay(1);
        
      }
    while (kamstrup_available() > 0) {
      uint8_t val = kamstrup_read();
      if(width%20 == 0) {
        DEBUG_PRINTLN("");
      } 
      width++;
      printHex2(val);
      if (streamParser.pushData(val)) {
        DEBUG_PRINTLN("\nPushdata returned True");
        VectorView frame = streamParser.getFrame();
        if (streamParser.getContentType() == MbusStreamParser::COMPLETE_FRAME) {
          DEBUG_PRINTLN(F("Frame complete. Decrypting..."));
          if (!decrypt(frame)){
            DEBUG_PRINTLN(F("Decrypt failed"));
          } else {
            MeterData md = parseMbusFrame(decryptedFrame);
            if(md.activePowerPlusValid && md.activePowerMinusValid) {
              read_cnt++;
              payload[6] = read_cnt;
              DEBUG_PRINTLN("Decrypt ok, cnt:"+String(read_cnt));
              payload[0] = do_average(payload[0], md.activePowerPlus, read_cnt);
              payload[3] = do_average(payload[3], md.activePowerMinus, read_cnt);
              
              if(payload[1] == -1) { payload[1] = md.activePowerPlus; }
              else { payload[1] = std::min((int) payload[1], (int) md.activePowerPlus); }
              
              if(payload[2] == -1) { payload[2] = md.activePowerPlus; }
              else { payload[2] = std::min((int) payload[2], (int) md.activePowerPlus); }
              
              if(payload[4] == -1) { payload[4] = md.activePowerMinus; }
              else { payload[4] = std::min((int) payload[4], (int) md.activePowerMinus); }
              
              if(payload[5] == -1) { payload[5] = md.activePowerMinus; }
              else { payload[5] = std::min((int) payload[5], (int) md.activePowerMinus); }
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
  DEBUG_PRINTLN("Hello from decrypt");
  if (frame.size() < headersize + footersize + 12 + 18) {
    DEBUG_PRINTLN("Invalid frame size.");
  }
  memcpy(decryptedFrameBuffer, &frame.front(), frame.size());

  memcpy(system_title, decryptedFrameBuffer + headersize + 2, 8);

  memcpy(initialization_vector, system_title, 8);
  memcpy(initialization_vector + 8, decryptedFrameBuffer + headersize + 14, 4);

  memcpy(additional_authenticated_data, decryptedFrameBuffer + headersize + 13, 1);
  memcpy(additional_authenticated_data + 1, authentication_key, 16);

  memcpy(authentication_tag, decryptedFrameBuffer + headersize + frame.size() - headersize - footersize - 12, 12);

  memcpy(cipher_text, decryptedFrameBuffer + headersize + 18, frame.size() - headersize - footersize - 12 - 18);

  mbedtls_gcm_init(&m_ctx);
  int success = mbedtls_gcm_setkey(&m_ctx, MBEDTLS_CIPHER_ID_AES, encryption_key, sizeof(encryption_key) * 8);
  if (0 != success) {
    Serial.println("Setkey failed: " + String(success));
    return false;
  }
  success = mbedtls_gcm_auth_decrypt(&m_ctx, sizeof(cipher_text), initialization_vector, sizeof(initialization_vector),
                                     additional_authenticated_data, sizeof(additional_authenticated_data), authentication_tag, sizeof(authentication_tag),
                                     cipher_text, plaintext);
  if (0 != success) {
    Serial.println("authdecrypt failed: " + String(success));
    return false;
  }
  mbedtls_gcm_free(&m_ctx);

  //copy replace encrypted data with decrypted for mbusparser library. Checksum not updated. Hopefully not needed
  memcpy(decryptedFrameBuffer + headersize + 18, plaintext, sizeof(plaintext));
  decryptedFrame = VectorView(decryptedFrameBuffer, frame.size());

  return true;
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
