/*
BSides Roanoke 2024 Badge FW
Wrote by BenTheCyberOne with the help of doc and KoopaTroopa8

Free to edit and distribute as you please

                      Badge UIDs
      +------------+---------------+-----------+
      |  Required  |  BADGE_TYPE   |    UID    |
      +------------+---------------+-----------+
      |    0x08    | 0xA, 0xB, 0xC | 0xXX 0xYY |
      +------------+---------------+-----------+

                  Worm Record Entry
      +----------------------+------------------+
      |  Pair Table Record   |    Worm Value    |
      +----------------------+------------------+
      |      0xXX 0xYY       | 0xEA, 0xEB, 0xEC |
      +----------------------+------------------+

Badge pairing happens over the course of 8 different functions due to how NFC P2P works on the PN532
First, two badges will share each other's UIDs. This is done with one badge being the "initiator" and the other the "target".
Each badge swaps between being the target and initiator until they both receive the message of "hello from {UID}".
To prevent any possible replay attack, both badges go through this cycle once more however sending over the message in an encrypted format
(same message, just XOR'd with the key of: 0x909B9F8C). The message however also contains a "worm" value. This is what changes the badge's light colors.
If the decrypted UID does not match the one obtained in the previous P2P cycle, the pairing fails and we know we are not talking to a real badge.

*/
#include "SPI.h"
#include "PN532_SPI.h"
#include "llcp.h"
#include "snep.h"
#include <Adafruit_NeoPixel.h>
#include <NfcAdapter.h>
#include <EEPROM.h>      //simulate EEPROM by taking last sector of flash and emulating into RAM. Saves to flash on commit()
#include <MD5Builder.h>  //MD5 lib to fix the easy hack of changing your own UID to someone else's

#define PIN 21         //GPIO for NeoPixels
#define BUTTON_PIN 14  //GPIO for mode select button
#define NUMPIXELS 6   //Number of NeoPixels on the board
#define UIDLENGTH 3  // first byte is hardcoded, so 3 free bytes for UID
#define M1LENGTH 11  //"hello from " = 11
#define MODE_PAIR 1  
#define MODE_SCAN 2
#define MODE_KIOSK 3
#define MODE_PUZZLE 4
// The definitions below are no longer utilized since we are using a modified version of <EEPROM>, allowing for a full sector for each table
//#define PAIR_TABLE_OFFSET 0       //Pair table in flash starts at 0 (1502 bytes total)
//#define WORM_TABLE_OFFSET 1501    //Worm table in flash starts at 1501 (1502 bytes total)
//#define SCAN_TABLE_OFFSET 3003    //Scan table in flash starts at 3003 (1025 bytes total)
//#define CURRENT_WORM_OFFSET 4090  //Byte to store current worm value at 4090 (1 bytes total)
#define PAIR_TABLE_SECTOR 0
#define WORM_TABLE_SECTOR 1
#define SCAN_TABLE_SECTOR 2
#define PAIR_TABLE_OFFSET 0
#define WORM_TABLE_OFFSET 0
#define SCAN_TABLE_OFFSET 0
#define CURRENT_WORM_OFFSET 4095  //very last byte of the Worm Table since we get an even 1364 enteries if we save 2 bytes for the record and 1 byte for Worm (4096 - 3)/3 = 1364

#define RPS_ROCK 0xEA
#define RPS_PAPER 0xEB
#define RPS_SCISSORS 0xEC

// XOR'd UID
uint8_t uid[3] = { 0x30, 0x25, 0x70 };

// Global secret message buffer
uint8_t secret[5] = { 0x08, 0, 0, 0, 0 };

//Global XOR key buffer
uint8_t k[4];

//Function prototypes otherwise Arduino IDE yells at us
void getPairTable(uint8_t *buffer, bool copyAll);
void getWormTable(uint8_t *buffer, bool copyAll);
void getScanTable(uint8_t *buffer, bool copyAll);

//how we will limit the amount of times getCurrentWorm needs to be called:
uint8_t currentWorm = 0xFF;

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
PN532_SPI pn532spi(SPI, 17);
SNEP nfc(pn532spi);
MD5Builder md5;

//setup tableBuf1-3 in setup to ensure correct allocation during startup/runtime. Three required for Kiosk functions
uint8_t tableBuf1[4096];
uint8_t tableBuf2[4096];
uint8_t tableBuf3[4096];

//Pairing message buffers
uint8_t m1[M1LENGTH] = { 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x66, 0x72, 0x6f, 0x6d, 0x20 };  //"hello from "
uint8_t m2[UIDLENGTH + M1LENGTH];

//set bootup mode to PAIR
int currentMode = 1;

//indicate pair uid already exists in table
void neoPairExisting() {
  pixels.clear();
  pixels.setPixelColor(0, pixels.Color(0, 0, 80));
  pixels.setPixelColor(1, pixels.Color(0, 0, 80));
  pixels.setPixelColor(2, pixels.Color(0, 0, 80));
  pixels.setPixelColor(3, pixels.Color(0, 0, 80));
  pixels.setPixelColor(4, pixels.Color(0, 0, 80));
  pixels.setPixelColor(5, pixels.Color(0, 0, 80));
  pixels.show();
  //delay(1000);
  //pixels.clear();
  //pixels.show();
}
//indicate failed pair
void neoFailure() {
  pixels.clear();
  pixels.setPixelColor(0, pixels.Color(80, 0, 0));
  pixels.setPixelColor(1, pixels.Color(80, 0, 0));
  pixels.setPixelColor(2, pixels.Color(80, 0, 0));
  pixels.setPixelColor(3, pixels.Color(80, 0, 0));
  pixels.setPixelColor(4, pixels.Color(80, 0, 0));
  pixels.setPixelColor(5, pixels.Color(80, 0, 0));
  pixels.show();
  //delay(1000);
  //pixels.clear();
  //pixels.show();
}
//Flashes all lights to represent a Morse Code Dash. Does this x times
void pixelDash(int x) {
  for (int i = 0; i < x; i++) {
    for (int j = 0; j < 6; j++) {
      pixels.setPixelColor(j, pixels.Color(0, 40, 10));
      delay(40);  //DO NOT CHANGE! Without this delay, it all breaks. Suspect data transfer speed/clock issue.
    }
    pixels.show();
    delay(2000);
    pixels.clear();
    pixels.show();
    //Serial.print("DASH ");
  }
}

//Flashes all lights to represent a Morse Code Dot. Does this x times
void pixelDot(int x) {
  for (int i = 0; i < x; i++) {
    for (int j = 0; j < 6; j++) {
      pixels.setPixelColor(j, pixels.Color(0, 40, 10));
      delay(40);  //DO NOT CHANGE! Without this delay, it all breaks. Suspect data transfer speed/clock issue.
    }
    pixels.show();
    delay(1000);
    pixels.clear();
    pixels.show();
    //Serial.print("DOT ");
  }
}

//indicate successful pair
void neoSuccess() {
  pixels.clear();
  pixels.setPixelColor(0, pixels.Color(0, 80, 0));
  pixels.setPixelColor(1, pixels.Color(0, 80, 0));
  pixels.setPixelColor(2, pixels.Color(0, 80, 0));
  pixels.setPixelColor(3, pixels.Color(0, 80, 0));
  pixels.setPixelColor(4, pixels.Color(0, 80, 0));
  pixels.setPixelColor(5, pixels.Color(0, 80, 0));
  pixels.show();
  //delay(1000);
  //pixels.clear();
  //pixels.show();
}
//generate XOR key
void gk(int j, int o) {

  uint8_t b[4];
  uint8_t t1, t2, t3, t4;
  uint8_t b5 = j & 0xFF;
  uint8_t b6 = o & 0xFF;
  t1 = (4 * b5) + (2 * b6);
  b[0] = t1;  // 0x6E for 'n'

  t2 = ((3 * b6) + b5) + 1;
  b[1] = t2;  // 0x65 for 'e'

  t3 = (3 * b5) + (b6 * 2);
  b[2] = t3;  // 0x61 for 'a'

  t4 = (2 * b5) + (3 * b6) + 1;
  b[3] = t4;  // 0x72 for 'r'
  // Additional obfuscation: use bitwise operations and complex expressions
  uint8_t mask = 0xAA;
  for (int i = 0; i < 4; i++) {
    b[i] ^= mask;
    b[i] = ~b[i];
    b[i] = (b[i] << 4) | (b[i] >> 4);
    b[i] ^= mask;
    b[i] = ~b[i];
    b[i] = (b[i] >> 4) | (b[i] << 4);
  }
  for (int i = 0; i < 4; i++) {
    k[i] = b[i] ^ 0xFE;
  }
}

//Encryption Function 1
void dataEnc(uint8_t *s) {
  for (int i = 0; i < 4; i++) {
    s[i] ^= k[i];
  }
}

//Encryption Function 2
void dataEnc2(uint8_t *b){
  for (int i = 0; i < 3; i++){
    b[i] ^= k[i];
  }
}

void setup() {
  //md5 hash of UID
  uint8_t knownMD5[16] = {0xfd,0xd0,0x14,0x4f,0x31,0x13,0x83,0x64,0x51,0xe8,0x8e,0x8f,0x47,0x14,0xeb,0x88};

  //check integrity (MD5sum)
  md5.begin();
  md5.add(uid, sizeof(uid));
  md5.calculate();
  uint8_t hashBuf[16];
  md5.getBytes(hashBuf);
  for (int m = 0; m < 16; m++) {
    if (hashBuf[m] != knownMD5[m]) {
      currentMode = 0;
    }
  }
  
  //get current worm stored in flash
  currentWorm = getCurrentWorm();

  //generate the XOR key
  gk(13, 29);

  //decrypt UID in memory:
  dataEnc2(uid);

  Serial.begin(9600);
  Serial.println("######### WELCOME TO BSIDES ROANOKE 2024 #########");
  //Serial.println("-------Peer to Peer--------");
  memcpy(m2, m1, M1LENGTH);               //copy M1 into m2
  memcpy(m2 + M1LENGTH, uid, UIDLENGTH);  //copy the UID into M2, creating "hello from [UID]"
  pixels.begin();
  pixels.clear();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

/*
Scan Mode
Reads the 4 byte UID from any NFC enabled tag/device then stores it into the Scan Table
*/
void scan() {
  NfcAdapter nfc2 = NfcAdapter(pn532spi);
  pixels.clear();

  nfc2.begin(true);
  for (int i = 0; i < NUMPIXELS; i++) {

    pixels.setPixelColor(i, pixels.Color(random(0, 128), random(0, 128), random(0, 128)));
    pixels.show();
    delay(200);
  }
  // Serial.println("aaaa");
  if (nfc2.tagPresent()) {
    for (int i = 0; i < NUMPIXELS; i++) {

      pixels.setPixelColor(i, pixels.Color(50, 100, 50));
      pixels.show();
      delay(200);
    }
    NfcTag tag = nfc2.read();
    //tag.print();
    String tagId = tag.getUidString();
    //Serial.println("Tag id");
    //Serial.println(tagId);
    uint8_t scannedUID[4];
    tag.getUid(scannedUID, 4);
    updateScanTable(scannedUID);
  }
  delay(1000);
}

// Polls for the presence of the Kiosk
bool pollKiosk() {
  pixels.clear();
  for (int i = 0; i < NUMPIXELS; i++) {

    pixels.setPixelColor(i, pixels.Color(80, 80, 0));
    pixels.show();
    delay(200);
  }
  uint8_t targetUid[] = { 0, 0, 0, 0, 0, 0, 0 };  // buffer for storing the target UID
  uint8_t uidLength;                              // Length of the UID (4 or 7 bytes depending on card type)
  uint8_t response[5];
  uint8_t responseLength;
  //delay(2000);
  bool success = nfc.initPassive(0, targetUid, &uidLength, 10000);
  if (success) {
    //Serial.println("Found a target!");
    //Serial.print("  UID Length: ");
    //Serial.print(uidLength, DEC);
    //Serial.println(" bytes");
    //Serial.print("  UID Value: ");
    //nfc.printHex(targetUid, uidLength);
    //Serial.println("");
    //Serial.print(uid[1],HEX);
    if (targetUid[1] == 0xC0) {
      //Serial.println("Kiosk is still here!");
      nfc.exchange(m2, sizeof(m2), response, &responseLength);
      return true;
    }
  } else {
    //Serial.println("Kiosk NOT found!");
  }
  return false;
}

//Sends the Scan Table to the Kiosk
bool sendScanTableToKiosk(uint16_t scanRecordNumBytes) {
  //Serial.println("SEND SCAN");
  uint8_t targetUid[] = { 0, 0, 0, 0, 0, 0, 0 };  // buffer for storing the target UID
  uint8_t uidLength;
  const uint8_t chunkSize = 250;
  uint8_t resBuf[chunkSize + 1];
  uint8_t resLength;
  bool lostConn = false;
  uint8_t s = nfc.initPassive(0, targetUid, &uidLength, 5000);
  if (s) {
    uint8_t chunkMessage[2];
    chunkMessage[1] = scanRecordNumBytes & 0xFF;
    chunkMessage[0] = (scanRecordNumBytes >> 8) & 0xFF;
    uint8_t s2 = nfc.exchange(chunkMessage, 2, resBuf, &resLength);
    for (uint16_t offset = 0; offset < scanRecordNumBytes; offset += chunkSize) {
      uint8_t success = nfc.initPassive(0, targetUid, &uidLength, 5000);
      if (lostConn) {
        offset -= chunkSize;
      }
      if (success) {
        uint8_t sendBytes = min(chunkSize, scanRecordNumBytes - offset);
        uint8_t success2 = nfc.exchange(tableBuf3 + offset, sendBytes, resBuf, &resLength);
        //Serial.print("Just exchanged: ");
        //Serial.print(sendBytes, DEC);
        //Serial.println(" bytes.");
        //Serial.print("offset: ");
        //Serial.println(offset);
        lostConn = false;
      } else {
        //Serial.println("Failed to talk to kiosk. We might be skipping a chunk! Rolling back...");
        if (offset != 0) {
          offset -= chunkSize;
        } else {
          lostConn = true;
        }
      }
    }
    //Serial.println("Done exchanging all chunks!");
    return true;
  }
  return false;
}

//Sends the Worm Table to the Kiosk
bool sendWormTableToKiosk(uint16_t wormRecordNumBytes) {
  //Serial.println("SEND WORM");
  uint8_t targetUid[] = { 0, 0, 0, 0, 0, 0, 0 };  // buffer for storing the target UID
  uint8_t uidLength;
  const uint8_t chunkSize = 249;
  uint8_t resBuf[chunkSize + 1];
  uint8_t resLength;
  bool lostConn = false;
  uint8_t s = nfc.initPassive(0, targetUid, &uidLength, 5000);
  if (s) {
    uint8_t chunkMessage[2];
    chunkMessage[1] = wormRecordNumBytes & 0xFF;
    chunkMessage[0] = (wormRecordNumBytes >> 8) & 0xFF;
    uint8_t s2 = nfc.exchange(chunkMessage, 2, resBuf, &resLength);
    for (uint16_t offset = 0; offset < wormRecordNumBytes; offset += chunkSize) {
      uint8_t success = nfc.initPassive(0, targetUid, &uidLength, 5000);
      if (lostConn) {
        offset -= chunkSize;
      }
      if (success) {
        uint8_t sendBytes = min(chunkSize, wormRecordNumBytes - offset);
        uint8_t success2 = nfc.exchange(tableBuf2 + offset, sendBytes, resBuf, &resLength);
        //Serial.print("Just exchanged: ");
        //Serial.print(sendBytes, DEC);
        //Serial.println(" bytes.");
        //Serial.print("offset: ");
        //Serial.println(offset);
        lostConn = false;
      } else {
        //Serial.println("Failed to talk to kiosk. We might be skipping a chunk! Rolling back...");
        if (offset != 0) {
          offset -= chunkSize;
        } else {
          lostConn = true;
        }
      }
    }
    //Serial.println("Done exchanging all chunks!");
    return true;
  }
  return false;
}

//Sends the Pair (Handshake) Table to the Kiosk
bool sendPairTableToKiosk(uint16_t pairRecordNumBytes) {
  //Serial.println("SEND PAIR");
  uint8_t targetUid[] = { 0, 0, 0, 0, 0, 0, 0 };  // buffer for storing the target UID
  uint8_t uidLength;
  const uint8_t chunkSize = 249;
  uint8_t resBuf[chunkSize + 1];
  uint8_t resLength;
  bool lostConn = false;
  /*
  uint8_t numChunks;
  if ((pairRecordNumBytes + chunkSize) % chunkSize) {
    numChunks = ((pairRecordNumBytes + chunkSize) / chunkSize) + 1;
  } else {
    numChunks = (pairRecordNumBytes + chunkSize) / chunkSize;
  }
  */
  uint8_t s = nfc.initPassive(0, targetUid, &uidLength, 2000);
  if (s) {
    uint8_t chunkMessage[2];
    chunkMessage[1] = pairRecordNumBytes & 0xFF;
    chunkMessage[0] = (pairRecordNumBytes >> 8) & 0xFF;

    uint8_t s2 = nfc.exchange(chunkMessage, 2, resBuf, &resLength);
    //if (s2) {
    for (uint16_t offset = 0; offset < pairRecordNumBytes; offset += chunkSize) {
      uint8_t success = nfc.initPassive(0, targetUid, &uidLength, 3000);
      if (lostConn) {
        offset -= chunkSize;
      }
      //delay(500);
      if (success) {
        //delay(1000);
        uint8_t sendBytes = min(chunkSize, pairRecordNumBytes - offset);
        ///delay(900);
        uint8_t success2 = nfc.exchange(tableBuf1 + offset, sendBytes, resBuf, &resLength);
        //Serial.print("Just exchanged: ");
        //Serial.print(sendBytes, DEC);
        //Serial.println(" bytes.");
        //Serial.print("offset: ");
        //Serial.println(offset);
        lostConn = false;
      } else {
        //Serial.println("Failed to talk to kiosk. We might be skipping a chunk! Rolling back...");
        if (offset != 0) {
          offset -= chunkSize;
        } else {
          lostConn = true;
        }
        //lostConn = true;
      }
      //}
    }
    //Serial.println("Done exchanging all chunks!");
    return true;
    //}
  }
  return false;
}

//Start finding the Kiosk
bool initKiosk() {
  uint8_t initBuf[15];
  int16_t len = nfc.read(initBuf, sizeof(initBuf), uid, 2000);
  if (len > 0) {
    uint8_t kioskUID[4] = { 0x08, initBuf[11], initBuf[12], initBuf[13] };
    //Serial.println("Have UID:");
    //nfc.printHex(kioskUID, 4);
    //Serial.println();
    if (kioskUID[1] == 0xC0) {
      //Serial.println("We are talking to a kiosk!");
      return true;
    }
  }
  //Serial.println("did not find a kiosk...");
  return false;
}

/*
Kiosk Mode
Reads what the current 3 current data tables contain then attempts to find a Kiosk. If a Kiosk is found, we send the table data. We then poll of the presence of the Kiosk so that the webapp can clear the data and present the user with a form. If the polling fails, we know the user has moved the badge away from the Kiosk and we start the cycle over again
*/
void kiosk() {
  //pixels.clear();

  //pixels.show();
  bool step1, step2, step3;
  //Setup current pair table into tableBuf1
  int pairRecordNum = numRecordsPairTable();
  uint16_t pairRecordNumBytes = (pairRecordNum * 3);
  getPairTable(tableBuf1, false);
  //Setup current worm table into tableBuf2
  int wormRecordNum = numRecordsWormTable();
  uint16_t wormRecordNumBytes = (wormRecordNum * 3);
  getWormTable(tableBuf2, false);
  //Setup current scan table into tableBuf3
  int scanRecordNum = numRecordsScanTable();
  uint16_t scanRecordNumBytes = (scanRecordNum * 4);
  getScanTable(tableBuf3, false);

  while (currentMode == MODE_KIOSK) {
    if (currentWorm == 0xFF) {
      for (int i = 0; i < 6; i++) {
        if (i != 4) {
          pixels.setPixelColor(i, pixels.Color(80, 40, 0));
          pixels.show();
          delay(200);
        }
      }
    } else if (currentWorm == RPS_ROCK) {  //orange red?
      for (int i = 0; i < 6; i++) {
        if (i != 4) {
          pixels.setPixelColor(i, pixels.Color(100, 20, 0));
          pixels.show();
          delay(200);
        }
      }
    } else if (currentWorm == RPS_PAPER) {  //seagreen?
      for (int i = 0; i < 6; i++) {
        if (i != 4) {
          pixels.setPixelColor(i, pixels.Color(26, 70, 47));
          pixels.show();
          delay(200);
        }
      }
    } else if (currentWorm == RPS_SCISSORS) {  //indigo?
      for (int i = 0; i < 6; i++) {
        if (i != 4) {
          pixels.setPixelColor(i, pixels.Color(32, 0, 80));
          pixels.show();
          delay(200);
        }
      }
    } else {
      for (int i = 0; i < 6; i++) {
        if (i != 4) {
          pixels.setPixelColor(i, pixels.Color(80, 40, 0));
          pixels.show();
          delay(200);
        }
      }
    }
    step1 = initKiosk();
    pixels.clear();
    while (step1) {

      step2 = sendPairTableToKiosk(pairRecordNumBytes);
      if (!step2) {
        step1 = false;
        break;
      }

      step2 = sendWormTableToKiosk(wormRecordNumBytes);
      if (!step2) {
        step1 = false;
        break;
      }

      step2 = sendScanTableToKiosk(scanRecordNumBytes);
      if (!step2) {
        step1 = false;
        break;
      }
      while (step2) {
        //delay(1000);
        step3 = pollKiosk();
        if (!step3) {
          step1 = false;
          step2 = false;
          //Serial.println("No longer polling...");
          break;
        }
      }
    }
  }
  pixels.clear();
  pixels.show();
}

//Changes all NeoPixels on or off depending on bool value
void sixBitsRed(bool firstBit, bool secondBit, bool thirdBit, bool fourthBit, bool fifthBit, bool sixthBit) {
  if (firstBit) {
    pixels.setPixelColor(0, pixels.Color(80, 30, 30));
    delay(20);
  }
  if (secondBit) {
    pixels.setPixelColor(1, pixels.Color(80, 30, 30));
    delay(20);
  }
  if (thirdBit) {
    pixels.setPixelColor(2, pixels.Color(80, 30, 30));
    delay(20);
  }
  if (fourthBit) {
    pixels.setPixelColor(3, pixels.Color(80, 30, 30));
    delay(20);
  }
  if (fifthBit) {
    pixels.setPixelColor(4, pixels.Color(80, 30, 30));
    delay(20);
  }
  if (sixthBit) {
    pixels.setPixelColor(5, pixels.Color(80, 30, 30));
    delay(20);
  }
  pixels.show();
  delay(2500);
  pixels.clear();
  pixels.show();
}

void puzzle3() {  //010101 010010 010100 001110 001011 010010 010111 001010 011011 100010 001011 011110 011101 010001 001110 100001 001110 001101
  //010101
  sixBitsRed(0, 1, 0, 1, 0, 1);
  //010010
  sixBitsRed(0, 1, 0, 0, 1, 0);
  //010100
  sixBitsRed(0, 1, 0, 1, 0, 0);
  //001110
  sixBitsRed(0, 0, 1, 1, 1, 0);
  //001011
  sixBitsRed(0, 0, 1, 0, 1, 1);
  //010010
  sixBitsRed(0, 1, 0, 0, 1, 0);
  //010111
  sixBitsRed(0, 1, 0, 1, 1, 1);
  //001010
  sixBitsRed(0, 0, 1, 0, 1, 0);
  //011011
  sixBitsRed(0, 1, 1, 0, 1, 1);
  //100010
  sixBitsRed(1, 0, 0, 0, 1, 0);
  //001011
  sixBitsRed(0, 0, 1, 0, 1, 1);
  //011110
  sixBitsRed(0, 1, 1, 1, 1, 0);
  //011101
  sixBitsRed(0, 1, 1, 1, 0, 1);
  //010001
  sixBitsRed(0, 1, 0, 0, 0, 1);
  //001110
  sixBitsRed(0, 0, 1, 1, 1, 0);
  //100001
  sixBitsRed(1, 0, 0, 0, 0, 1);
  //001110
  sixBitsRed(0, 0, 1, 1, 1, 0);
  //001101
  sixBitsRed(0, 0, 1, 1, 0, 1);
}

//Changes all NeoPixels to different colors depending on bool value
void sixBits(bool firstBit, bool secondBit, bool thirdBit, bool fourthBit, bool fifthBit, bool sixthBit) {
  if (firstBit) {
    pixels.setPixelColor(0, pixels.Color(30, 30, 80));
    delay(20);
  } else {
    pixels.setPixelColor(0, pixels.Color(25, 25, 25));
    delay(20);
  }
  if (secondBit) {
    pixels.setPixelColor(1, pixels.Color(30, 30, 80));
    delay(20);
  } else {
    pixels.setPixelColor(1, pixels.Color(25, 25, 25));
    delay(20);
  }
  if (thirdBit) {
    pixels.setPixelColor(2, pixels.Color(30, 30, 80));
    delay(20);
  } else {
    pixels.setPixelColor(2, pixels.Color(25, 25, 25));
    delay(20);
  }
  if (fourthBit) {
    pixels.setPixelColor(3, pixels.Color(30, 30, 80));
    delay(20);
  } else {
    pixels.setPixelColor(3, pixels.Color(25, 25, 25));
    delay(20);
  }
  if (fifthBit) {
    pixels.setPixelColor(4, pixels.Color(30, 30, 80));
    delay(20);
  } else {
    pixels.setPixelColor(4, pixels.Color(25, 25, 25));
    delay(20);
  }
  if (sixthBit) {
    pixels.setPixelColor(5, pixels.Color(30, 30, 80));
    delay(20);
  } else {
    pixels.setPixelColor(5, pixels.Color(25, 25, 25));
    delay(20);
  }
  pixels.show();
  delay(3000);
  pixels.clear();
  pixels.show();
}

//Changes the first two NeoPixels to different colors depending on bool value
void twoBits(bool firstBit, bool secondBit) {
  if (firstBit) {
    pixels.setPixelColor(0, pixels.Color(30, 30, 80));
    delay(20);
  } else {
    pixels.setPixelColor(0, pixels.Color(50, 50, 50));
    delay(20);
  }
  if (secondBit) {
    pixels.setPixelColor(1, pixels.Color(30, 30, 80));
    delay(20);
  } else {
    pixels.setPixelColor(1, pixels.Color(50, 50, 50));
    delay(20);
  }
  pixels.show();
  delay(2000);
  pixels.clear();
  pixels.show();
}

void puzzle2() {  //00010110 11100110 01001110 10011110 11111010 11101110 01101110 11100010 00011010 01101010 11110110 10101110 11001100 10010010
  pixels.clear();
  pixels.show();
  //00010110
  twoBits(0, 0);
  sixBits(0, 1, 0, 1, 1, 0);
  //11100110
  twoBits(1, 1);
  sixBits(1, 0, 0, 1, 1, 0);
  //01001110
  twoBits(0, 1);
  sixBits(0, 0, 1, 1, 1, 0);
  //10011110
  twoBits(1, 0);
  sixBits(0, 1, 1, 1, 1, 0);
  //11111010
  twoBits(1, 1);
  sixBits(1, 1, 1, 0, 1, 0);
  //11101110
  twoBits(1, 1);
  sixBits(1, 0, 1, 1, 1, 0);
  //01101110
  twoBits(0, 1);
  sixBits(1, 0, 1, 1, 1, 0);
  //11100010
  twoBits(1, 1);
  sixBits(1, 0, 0, 0, 1, 0);
  //00011010
  twoBits(0, 0);
  sixBits(0, 1, 1, 0, 1, 0);
  //01101010
  twoBits(0, 1);
  sixBits(1, 0, 1, 0, 1, 0);
  //11110110
  twoBits(1, 1);
  sixBits(1, 1, 0, 1, 1, 0);
  //10101110
  twoBits(1, 0);
  sixBits(1, 0, 1, 1, 1, 0);
  //11001100
  twoBits(1, 1);
  sixBits(0, 0, 1, 1, 0, 0);
  //10010010
  twoBits(1, 0);
  sixBits(0, 1, 0, 0, 1, 0);
}

void puzzle1() {  //..-. .-.. .--.-. ... .... .---- -. --. ..--.- .-.. .. --. .... - ..... 
  pixels.clear();
  pixels.show();
  // Letter: F (..-.)
  pixelDot(2);
  pixelDash(1);
  pixelDot(1);
  delay(700);  // Half second delay between letters

  // Letter: L (.-..)
  pixelDot(1);
  pixelDash(1);
  pixelDot(2);
  delay(700);

  // Letter: @ (.--.-.)
  pixelDot(1);
  pixelDash(2);
  pixelDot(1);
  pixelDash(1);
  pixelDot(1);
  delay(700);

  // Letter: S (...)
  pixelDot(3);
  delay(700);

  // Letter: H (....)
  pixelDot(4);
  delay(700);

  // Letter: 1 (.----)
  pixelDot(1);
  pixelDash(4);
  delay(700);

  // Letter: N (-.)
  pixelDash(1);
  pixelDot(1);
  delay(700);

  // Letter: G (--.)
  pixelDash(2);
  pixelDot(1);
  delay(700);

  // Letter: _ (..--.-)
  pixelDot(2);
  pixelDash(2);
  pixelDot(1);
  pixelDash(1);
  delay(700);

  // Letter: L (.-..)
  pixelDot(1);
  pixelDash(1);
  pixelDot(2);
  delay(700);

  // Letter: I (..)
  pixelDot(2);
  delay(700);

  // Letter: G (--.)
  pixelDash(2);
  pixelDot(1);
  delay(700);

  // Letter: H (....)
  pixelDot(4);
  delay(700);

  // Letter: T (-)
  pixelDash(1);
  delay(700);

  // Letter: 5 (.....)
  pixelDot(5);
  delay(700);
  //Serial.println("done with puzzle1");
}

//Updates the Scan Table by appending the 4 byte UID after the last known record (based off of numRecords). Increments number of records
void updateScanTable(uint8_t *pairUID) {
  uint8_t recByte1;
  uint8_t recByte2;
  int16_t numRecords = numRecordsScanTable();
  int newRecordOffset = (numRecords * 4) + (SCAN_TABLE_OFFSET + 2);  //new record offset to be placed at end of previously placed record
  for (int i = 0; i < 4; i++) {
    EEPROM.write(newRecordOffset, pairUID[i]);
    newRecordOffset++;  //make sure we increment offset to place next record portion into next byte
  }
  numRecords++;  //increment number of records
  recByte2 = numRecords & 0xFF;
  recByte1 = (numRecords >> 8) & 0xFF;
  //Serial.print("byte0: ");
  //Serial.println(recByte1, HEX);
  //Serial.print("byte1: ");
  //Serial.println(recByte2, HEX);
  EEPROM.write(SCAN_TABLE_OFFSET, recByte1);
  EEPROM.write(SCAN_TABLE_OFFSET + 1, recByte2);
  //Serial.println("Updated Scan Table!");
  //Serial.print("New number of records in scan table: ");
  //Serial.println(numRecords, DEC);
  EEPROM.commit();
  EEPROM.end();
}

//fills a buffer of every scan record
void getScanTable(uint8_t *buffer, bool copyAll = false) {
  int16_t numRecords = numRecordsScanTable();  //bitwise operations to get value of both bytes
  if (copyAll) {
    for (int i = 0; i < 4096; i++) {
      buffer[i] = EEPROM.read(i);
    }

  } else {
    int currentOffset = SCAN_TABLE_OFFSET + 2;  //start offset after record counter bytes
    int TABLESIZE = (numRecords * 4);           //size of array calculated by number of records * length of scan table record (4 bytes for full UID)
    for (int i = 0; i < TABLESIZE; i++) {
      buffer[i] = EEPROM.read(currentOffset + i);
    }
  }
  EEPROM.end();
}

//return the number of records in the Scan Table
int numRecordsScanTable() {
  EEPROM.end();  //end previous sector section just in case
  EEPROM.begin(4096, SCAN_TABLE_SECTOR);
  uint8_t recByte1 = EEPROM.read(SCAN_TABLE_OFFSET);
  uint8_t recByte2 = EEPROM.read(SCAN_TABLE_OFFSET + 1);
  //Serial.print("byte0: ");
  //Serial.println(recByte1, HEX);
  //Serial.print("byte1: ");
  //Serial.println(recByte2, HEX);
  if (recByte1 >= 0x03 && recByte2 >= 0xFF) {  //total 1023
    recByte1 = 0x00;
    recByte2 = 0x00;
  } else if (recByte1 == 0xFF && recByte2 == 0xFF) {  //table has not been set/created, so set it to 0
    recByte1 = 0x00;
    recByte2 = 0x00;
  }
  int16_t numRecords = (recByte1 << 8) | recByte2;  //bitwise operations to get value of record byte
  //Serial.print("Number of records in Scan table: ");
  //Serial.println(numRecords, DEC);
  return numRecords;
}

//return the number of records in the Worm Table
int numRecordsWormTable() {
  EEPROM.end();  //end previous sector section just in case
  EEPROM.begin(4096, WORM_TABLE_SECTOR);
  uint8_t recByte1 = EEPROM.read(WORM_TABLE_OFFSET);
  uint8_t recByte2 = EEPROM.read(WORM_TABLE_OFFSET + 1);
  //Serial.print("byte0: ");
  //Serial.println(recByte1, HEX);
  //Serial.print("byte1: ");
  //Serial.println(recByte2, HEX);
  if (recByte1 >= 0x05 && recByte2 >= 0x54) {  //max amount of records for this sector is 1364 entries (0x0554) - reset to 0000 if we hit the max to avoid overwriting other data
    recByte1 = 0x00;
    recByte2 = 0x00;
  } else if (recByte1 == 0xFF && recByte2 == 0xFF) {  //table has not been set/created, so set it to 0
    recByte1 = 0x00;
    recByte2 = 0x00;
  }
  int16_t numRecords = (recByte1 << 8) | recByte2;  //bitwise operations to get value of both bytes
  //Serial.print("Number of records in Worm table: ");
  //Serial.println(numRecords, DEC);
  return numRecords;
}

//fills a buffer of every worm record
void getWormTable(uint8_t *buffer, bool copyAll = false) {
  int16_t numRecords = numRecordsWormTable();
  if (copyAll) {
    for (int i = 0; i < 4096; i++) {
      buffer[i] = EEPROM.read(i);
    }
  } else {
    int currentOffset = WORM_TABLE_OFFSET + 2;  //start offset after record counter bytes
    int TABLESIZE = (numRecords * 3);           //size of array calculated by number of records * length of scan table record (2 bytes for pair table record counter + 1 byte worm value)
    for (int i = 0; i < TABLESIZE; i++) {
      buffer[i] = EEPROM.read(currentOffset + i);
    }
  }
  EEPROM.end();
}

//Updates the Worm Table by appending the two byte record value (+1 byte worm value) after the last known record (based off of numRecords). Increments number of records, sets worm to value
void updateWormTable(uint8_t *pairIndex) {
  uint8_t recByte1;
  uint8_t recByte2;
  int16_t numRecords = numRecordsWormTable();
  int newRecordOffset = (numRecords * 3) + (WORM_TABLE_OFFSET + 2);  //new record offset to be placed at end of previously placed record (the two number of record bytes + worm value = 3)
  for (int i = 0; i < 3; i++) {
    EEPROM.write(newRecordOffset, pairIndex[i]);
    newRecordOffset++;  //make sure we increment offset to place next record portion into next byte
  }
  numRecords++;  //increment number of records
  recByte2 = numRecords & 0xFF;
  recByte1 = (numRecords >> 8) & 0xFF;
  //Serial.print("byte0: ");
  //Serial.println(recByte1, HEX);
  //Serial.print("byte1: ");
  //Serial.println(recByte2, HEX);
  EEPROM.write(WORM_TABLE_OFFSET, recByte1);
  EEPROM.write(WORM_TABLE_OFFSET + 1, recByte2);
  //Serial.println("Updated Worm Table!");
  //Serial.print("New number of records in worm table: ");
  //Serial.println(numRecords, DEC);
  //write the new worm into memory
  EEPROM.write(CURRENT_WORM_OFFSET, pairIndex[2]);

  EEPROM.commit();
  EEPROM.end();
}

//Updates the Pair Table by appending the pairUID after the last known record (based off of numRecords). Increments number of records, returns new record count
int updatePairTable(uint8_t *pairUID) {
  uint8_t recByte1;
  uint8_t recByte2;
  int16_t numRecords = numRecordsPairTable();
  dataEnc2(pairUID);
  int newRecordOffset = (numRecords * UIDLENGTH) + (PAIR_TABLE_OFFSET + 2);  //new UID offset to be placed at end of previously placed UID (+ the two number of record bytes)
  for (int i = 0; i < UIDLENGTH; i++) {
    EEPROM.write(newRecordOffset, pairUID[i]);
    newRecordOffset++;  //make sure we increment offset to place next UID portion into next byte
  }
  numRecords++;  //increment number of records
  recByte2 = numRecords & 0xFF;
  recByte1 = (numRecords >> 8) & 0xFF;
  //Serial.print("byte0: ");
  //Serial.println(recByte1, HEX);
  //Serial.print("byte1: ");
  //Serial.println(recByte2, HEX);
  EEPROM.write(PAIR_TABLE_OFFSET, recByte1);
  EEPROM.write(PAIR_TABLE_OFFSET + 1, recByte2);
  //Serial.println("Updated Pair Table!");
  //Serial.print("New number of records in pair table: ");
  //Serial.println(numRecords, DEC);
  EEPROM.commit();
  EEPROM.end();
  return numRecords;
}

//Checks if the passed UID is already in the Pair table (not unique), else return false
bool checkPairTable(uint8_t *pairUID) {
  int16_t numRecords = numRecordsPairTable();
  int currentOffset = 2;  //initial offset
  for (int i = 0; i < numRecords; i++) {
    uint8_t uidChunk[3];
    for (int j = 0; j < UIDLENGTH; j++) {
      //Store our UID chunk into a temp holder
      EEPROM.get(currentOffset++, uidChunk[j]);
    }
    //Serial.println("uidChunk:");
    for (int k = 0; k < UIDLENGTH; k++) {
      //Serial.print(uidChunk[k], HEX);
    }
    //Serial.println();
    /*
    Serial.println("pairUID:");
    for(int k = 0; k < UIDLENGTH; k++){
      Serial.print(pairUID[k], HEX);
    }
    */
    //encode the UID before checking
    dataEnc2(pairUID);
    //Compare our temp chunk with the paired UID passed as param - return true if it has been found
    if (!memcmp(uidChunk, pairUID, UIDLENGTH)) {
      //Serial.println("This badge has already been paired with.");
      //Serial.print("Record Entry #: ");
      //Serial.println(i + 1, DEC);  //+1 to avoid off-by-one err
      EEPROM.end();                //end here since we no longer need it
      return true;
    }
  }
  // do not end since we will need it again right after
  return false;  //return false since it was never found in our chunks
}

//return the number of records in the Pair Table
int numRecordsPairTable() {
  EEPROM.end();                           //end previous sector section just in case
  EEPROM.begin(4096, PAIR_TABLE_SECTOR);  //
  uint8_t recByte1 = EEPROM.read(PAIR_TABLE_OFFSET);
  uint8_t recByte2 = EEPROM.read(PAIR_TABLE_OFFSET + 1);
  //Serial.print("byte0: ");
  //Serial.println(recByte1, HEX);
  //Serial.print("byte1: ");
  //Serial.println(recByte2, HEX);
  if (recByte1 >= 0x05 && recByte2 >= 0x54) {  //max amount of records for this sector is 1364 entries (0x0554) - reset to 0000 if we hit the max to avoid overwriting other data
    recByte1 = 0x00;
    recByte2 = 0x00;
  } else if (recByte1 == 0xFF && recByte2 == 0xFF) {  //table has not been set/created, so set it to 0
    recByte1 = 0x00;
    recByte2 = 0x00;
  }
  int16_t numRecords = (recByte1 << 8) | recByte2;  //bitwise operations to get value of both bytes
  //Serial.print("Number of records in pair table: ");
  //Serial.println(numRecords, DEC);
  return numRecords;
}

//fills a buffer of every uidPair
void getPairTable(uint8_t *buffer, bool copyAll) {
  int16_t numRecords = numRecordsPairTable();  //bitwise operations to get value of both bytes
  if (copyAll) {
    for (int i = 0; i < 1364; i++) {
      buffer[i] = EEPROM.read(i);
    }
  } else {
    int currentOffset = PAIR_TABLE_OFFSET + 2;  //start offset after record counter bytes
    int TABLESIZE = (numRecords * UIDLENGTH);   //size of array calculated by number of records * length of scan table record (3 bytes for full UID)
    for (int i = 0; i < TABLESIZE; i++) {
      buffer[i] = EEPROM.read(currentOffset + i);
    }
  }
  EEPROM.end();
}

//Sets the badge's worm value in flash
void setWorm(uint8_t w, uint8_t *u) {
  int r = updatePairTable(u);  //first update the pair table since we are changing our worm value + get the latest record counter for worm table
  uint8_t rB2 = r & 0xFF;
  uint8_t rB1 = (r >> 8) & 0xFF;
  uint8_t v[] = { rB1, rB2, w };  //create array of data for updateWormTable (record byte1, record byte2, worm value)
  updateWormTable(v);
  currentWorm = w;
  //Serial.print("Updated our worm to: ");
  //Serial.println(currentWorm, HEX);
}

//Get the badge's current worm value
uint8_t getCurrentWorm() {
  EEPROM.begin(4096, WORM_TABLE_SECTOR);
  uint8_t w = EEPROM.read(CURRENT_WORM_OFFSET);
  EEPROM.end();
  return w;
}


void encTarget2(uint8_t *pairUID) {
  bool wormed = false;
  pixels.clear();
  if (currentWorm == 0xFF) {
    pixels.setPixelColor(0, pixels.Color(80, 0, 80));
    pixels.setPixelColor(3, pixels.Color(80, 0, 80));
    pixels.setPixelColor(5, pixels.Color(80, 0, 80));
  } else if (currentWorm == RPS_ROCK) {  //orange red?
    pixels.setPixelColor(0, pixels.Color(100, 20, 0));
    pixels.setPixelColor(3, pixels.Color(100, 20, 0));
    pixels.setPixelColor(5, pixels.Color(100, 20, 0));
  } else if (currentWorm == RPS_PAPER) {  //seagreen?
    pixels.setPixelColor(0, pixels.Color(26, 70, 47));
    pixels.setPixelColor(3, pixels.Color(26, 70, 47));
    pixels.setPixelColor(5, pixels.Color(26, 70, 47));
  } else if (currentWorm == RPS_SCISSORS) {  //indigo?
    pixels.setPixelColor(0, pixels.Color(32, 0, 80));
    pixels.setPixelColor(3, pixels.Color(32, 0, 80));
    pixels.setPixelColor(5, pixels.Color(32, 0, 80));
  } else {
    pixels.setPixelColor(0, pixels.Color(80, 0, 80));
    pixels.setPixelColor(3, pixels.Color(80, 0, 80));
    pixels.setPixelColor(5, pixels.Color(80, 0, 80));
  }

  pixels.show();
  //Serial.println("-------- Starting encPair4 -------- ");
  uint8_t buf[128];
  uint8_t pairSecret[5];
  int16_t len = nfc.read(buf, sizeof(buf), uid, 2000);
  //Serial.println("Just attempted a read!");
  if (len > 0) {
    //Serial.println("get a message:");
    for (uint8_t i = 0; i < len; i++) {
      pairSecret[i] = buf[i];
      //Serial.print(pairSecret[i], HEX);
      //Serial.print(' ');
    }
    //Serial.print('\n');
    uint8_t partnerWorm = pairSecret[4];
    dataEnc(pairSecret);  //decode the secret
    for (uint8_t i = 0; i < len; i++) {
      //Serial.print(pairSecret[i], HEX);
      //Serial.print(' ');
    }
    //Serial.print('\n');
    if (!memcmp(&pairSecret[1], &pairUID[1], 3)) {  //compare the UID, leave out the worm and the initial 0x08. Done by taking address of variable and selecting 2 item in array as start
      //Serial.println("Secret matches the UID. This is a badge!");
      //Serial.print("Worm value from partner: ");
      //Serial.println(partnerWorm, HEX);
      uint8_t parsedUID[] = { pairUID[1], pairUID[2], pairUID[3] };  //leave out the 0x08 since we don't care about that default value
      if (partnerWorm != 0xFF) {
        if (currentWorm != partnerWorm) {
          if (currentWorm == RPS_ROCK && partnerWorm == RPS_PAPER) {
            setWorm(partnerWorm, parsedUID);
            wormed = true;
          } else if (currentWorm == RPS_PAPER && partnerWorm == RPS_SCISSORS) {
            setWorm(partnerWorm, parsedUID);
            wormed = true;
          } else if (currentWorm == RPS_SCISSORS && partnerWorm == RPS_ROCK) {
            setWorm(partnerWorm, parsedUID);
            wormed = true;
          } else if (currentWorm == 0xFF) {
            setWorm(partnerWorm, parsedUID);
            wormed = true;
          }
        }
      }
      if (wormed) {
        //Serial.println("Finished the exchange encPair4!");
        //Serial.println("+++++++++ SUCCESS +++++++++");
        //Serial.println("+++++++++ HANDSHAKE COMPLETE +++++++++");
        neoSuccess();
        nfc.finish();
      } else {
        if (!checkPairTable(parsedUID)) {
          //Serial.println("Going to add this UID to pair table");
          updatePairTable(parsedUID);
          neoSuccess();
          nfc.finish();
        } else {
          neoPairExisting();
          nfc.finish();
        }
        //Serial.println("Finished the exchange encPair4!");
        //Serial.println("+++++++++ SUCCESS +++++++++");
        //Serial.println("+++++++++ HANDSHAKE COMPLETE +++++++++");
      }
    } else {
      Serial.println("Secret does NOT match the UID. Not a badge?");
      neoFailure();
    }
  }
}

void encTarget1(uint8_t *pairUID) {
  bool wormed = false;
  pixels.clear();
  if (currentWorm == 0xFF) {
    pixels.setPixelColor(0, pixels.Color(80, 0, 80));
    pixels.setPixelColor(3, pixels.Color(80, 0, 80));
    pixels.setPixelColor(5, pixels.Color(80, 0, 80));
  } else if (currentWorm == RPS_ROCK) {  //orange red?
    pixels.setPixelColor(0, pixels.Color(100, 20, 0));
    pixels.setPixelColor(3, pixels.Color(100, 20, 0));
    pixels.setPixelColor(5, pixels.Color(100, 20, 0));
  } else if (currentWorm == RPS_PAPER) {  //seagreen?
    pixels.setPixelColor(0, pixels.Color(26, 70, 47));
    pixels.setPixelColor(3, pixels.Color(26, 70, 47));
    pixels.setPixelColor(5, pixels.Color(26, 70, 47));
  } else if (currentWorm == RPS_SCISSORS) {  //indigo?
    pixels.setPixelColor(0, pixels.Color(32, 0, 80));
    pixels.setPixelColor(3, pixels.Color(32, 0, 80));
    pixels.setPixelColor(5, pixels.Color(32, 0, 80));
  } else {
    pixels.setPixelColor(0, pixels.Color(80, 0, 80));
    pixels.setPixelColor(3, pixels.Color(80, 0, 80));
    pixels.setPixelColor(5, pixels.Color(80, 0, 80));
  }
  pixels.show();
  //Serial.println("-------- Starting encPair2 -------- ");
  uint8_t buf[128];
  uint8_t pairSecret[5];
  int16_t len = nfc.read(buf, sizeof(buf), uid, 2000);
  //Serial.println("Just attempted a read!");
  if (len > 0) {
    //Serial.println("get a message:");
    for (uint8_t i = 0; i < len; i++) {
      pairSecret[i] = buf[i];
      //Serial.print(pairSecret[i], HEX);
      //Serial.print(' ');
    }
    //Serial.print('\n');
    uint8_t partnerWorm = pairSecret[4];
    dataEnc(pairSecret);  //decode the secret
    for (uint8_t i = 0; i < len; i++) {
      //Serial.print(pairSecret[i], HEX);
      //Serial.print(' ');
    }
    //Serial.print('\n');
    if (!memcmp(&pairSecret[1], &pairUID[1], 3)) {  //compare the UID, leave out the worm and the initial 0x08. Done by taking address of variable and selecting 2nd item in array as start
      //Serial.println("Secret matches the UID. This is a badge!");
      //Serial.print("Worm value from partner: ");
      //Serial.println(partnerWorm, HEX);
      uint8_t parsedUID[] = { pairUID[1], pairUID[2], pairUID[3] };  //leave out the 0x08 since we don't care about that default value
      if (partnerWorm != 0xFF) {
        if (currentWorm != partnerWorm) {
          if (currentWorm == RPS_ROCK && partnerWorm == RPS_PAPER) {
            setWorm(partnerWorm, parsedUID);
            wormed = true;
          } else if (currentWorm == RPS_PAPER && partnerWorm == RPS_SCISSORS) {
            setWorm(partnerWorm, parsedUID);
            wormed = true;
          } else if (currentWorm == RPS_SCISSORS && partnerWorm == RPS_ROCK) {
            setWorm(partnerWorm, parsedUID);
            wormed = true;
          } else if (currentWorm == 0xFF) {
            setWorm(partnerWorm, parsedUID);
            wormed = true;
          }
        }
      }
      if (wormed) {
        //Serial.println("Finished the exchange encPair2!");
        //Serial.println("+++++++++ SUCCESS +++++++++");
        encInitializer2();
      } else {
        if (!checkPairTable(parsedUID)) {
          //Serial.println("Going to add this UID to pair table");
          updatePairTable(parsedUID);
        } else {
          neoPairExisting();
        }
        //Serial.println("Finished the exchange encPair2!");
        //Serial.println("+++++++++ SUCCESS +++++++++");
        encInitializer2();
      }
      //nfc.finish();
    } else {
      //Serial.println("Secret does NOT match the UID. Not a badge?");
      neoFailure();
    }
  }
}

void encInitializer2() {
  pixels.clear();
  if (currentWorm == 0xFF) {
    pixels.setPixelColor(1, pixels.Color(80, 0, 80));
    pixels.setPixelColor(2, pixels.Color(80, 0, 80));
    pixels.setPixelColor(4, pixels.Color(80, 0, 80));
  } else if (currentWorm == RPS_ROCK) {  //orange red?
    pixels.setPixelColor(1, pixels.Color(100, 20, 0));
    pixels.setPixelColor(2, pixels.Color(100, 20, 0));
    pixels.setPixelColor(4, pixels.Color(100, 20, 0));
  } else if (currentWorm == RPS_PAPER) {  //seagreen?
    pixels.setPixelColor(1, pixels.Color(26, 70, 47));
    pixels.setPixelColor(2, pixels.Color(26, 70, 47));
    pixels.setPixelColor(4, pixels.Color(26, 70, 47));
  } else if (currentWorm == RPS_SCISSORS) {  //indigo?
    pixels.setPixelColor(1, pixels.Color(32, 0, 80));
    pixels.setPixelColor(2, pixels.Color(32, 0, 80));
    pixels.setPixelColor(4, pixels.Color(32, 0, 80));
  } else {
    pixels.setPixelColor(1, pixels.Color(80, 0, 80));
    pixels.setPixelColor(2, pixels.Color(80, 0, 80));
    pixels.setPixelColor(4, pixels.Color(80, 0, 80));
  }
  pixels.show();
  //Serial.println("-------- Starting encPair3 -------- ");
  uint8_t targetUid[] = { 0, 0, 0, 0, 0, 0, 0 };  // buffer for storing the target UID
  uint8_t uidLength;
  uint8_t response[5];
  uint8_t responseLength;
  uint8_t success;
  //uint8_t currentWorm = getCurrentWorm();
  // Serial.print("Current Worm Value: ");
  //Serial.println(currentWorm, HEX);
  memcpy(secret + 1, uid, UIDLENGTH);   //copy UID into Secret (after the 0x08)
  memcpy(secret + 4, &currentWorm, 1);  //copy the current worm value into secret (after the full UID)
  dataEnc(secret);                      //encode (XOR) the secret
  success = nfc.initPassive(0, targetUid, &uidLength, 2000);
  if (success) {
    //Serial.println("Successfully activated the target!");
    success = nfc.exchange(secret, sizeof(secret), response, &responseLength);
    //Serial.println("Finished the exchange encPair3!");
    //Serial.println("+++++++++ SUCCESS +++++++++");
    //Serial.println("+++++++++ HANDSHAKE COMPLETE +++++++++");
    //neoSuccess();
    //if (success) {
    //  Serial.println("Finished the exchange encPair3!");
    //  Serial.println("+++++++++ SUCCESS +++++++++");
    //  Serial.println("+++++++++ HANDSHAKE COMPLETE +++++++++");
    //}
    nfc.finish();
  }
}

void encInitializer1() {
  pixels.clear();
  if (currentWorm == 0xFF) {
    pixels.setPixelColor(1, pixels.Color(80, 0, 80));
    pixels.setPixelColor(2, pixels.Color(80, 0, 80));
    pixels.setPixelColor(4, pixels.Color(80, 0, 80));
  } else if (currentWorm == RPS_ROCK) {  //orange red?
    pixels.setPixelColor(1, pixels.Color(100, 20, 0));
    pixels.setPixelColor(2, pixels.Color(100, 20, 0));
    pixels.setPixelColor(4, pixels.Color(100, 20, 0));
  } else if (currentWorm == RPS_PAPER) {  //seagreen?
    pixels.setPixelColor(1, pixels.Color(26, 70, 47));
    pixels.setPixelColor(2, pixels.Color(26, 70, 47));
    pixels.setPixelColor(4, pixels.Color(26, 70, 47));
  } else if (currentWorm == RPS_SCISSORS) {  //indigo?
    pixels.setPixelColor(1, pixels.Color(32, 0, 80));
    pixels.setPixelColor(2, pixels.Color(32, 0, 80));
    pixels.setPixelColor(4, pixels.Color(32, 0, 80));
  } else {
    pixels.setPixelColor(1, pixels.Color(80, 0, 80));
    pixels.setPixelColor(2, pixels.Color(80, 0, 80));
    pixels.setPixelColor(4, pixels.Color(80, 0, 80));
  }
  pixels.show();
  //Serial.println("-------- Starting encPair1 -------- ");
  uint8_t targetUid[] = { 0, 0, 0, 0, 0, 0, 0 };  // buffer for storing the target UID
  uint8_t uidLength;
  uint8_t response[5];
  uint8_t responseLength;
  uint8_t success;
  //uint8_t currentWorm = getCurrentWorm();
  //Serial.print("Current Worm Value: ");
  //Serial.println(currentWorm, HEX);
  memcpy(secret + 1, uid, UIDLENGTH);   //copy UID into Secret (after the 0x08)
  memcpy(secret + 4, &currentWorm, 1);  //copy the current worm value into secret (after the full UID)
  dataEnc(secret);                      //encode (XOR) the secret
  success = nfc.initPassive(0, targetUid, &uidLength, 2000);
  if (success) {
    //Serial.println("Successfully activated the target!");
    success = nfc.exchange(secret, sizeof(secret), response, &responseLength);
    if (success) {
      //Serial.println("Finished the exchange encPair1!");
      //Serial.println("+++++++++ SUCCESS +++++++++");
    }
    encTarget2(targetUid);
    //nfc.finish();
  }
}

void target2() {
  pixels.clear();
  if (currentWorm == 0xFF) {
    pixels.setPixelColor(0, pixels.Color(80, 40, 0));
    pixels.setPixelColor(3, pixels.Color(80, 40, 0));
    pixels.setPixelColor(5, pixels.Color(80, 40, 0));
  } else if (currentWorm == RPS_ROCK) {  //orange red?
    pixels.setPixelColor(0, pixels.Color(100, 20, 0));
    pixels.setPixelColor(3, pixels.Color(100, 20, 0));
    pixels.setPixelColor(5, pixels.Color(100, 20, 0));
  } else if (currentWorm == RPS_PAPER) {  //seagreen?
    pixels.setPixelColor(0, pixels.Color(26, 70, 47));
    pixels.setPixelColor(3, pixels.Color(26, 70, 47));
    pixels.setPixelColor(5, pixels.Color(26, 70, 47));
  } else if (currentWorm == RPS_SCISSORS) {  //indigo?
    pixels.setPixelColor(0, pixels.Color(32, 0, 80));
    pixels.setPixelColor(3, pixels.Color(32, 0, 80));
    pixels.setPixelColor(5, pixels.Color(32, 0, 80));
  } else {
    pixels.setPixelColor(0, pixels.Color(80, 40, 0));
    pixels.setPixelColor(3, pixels.Color(80, 40, 0));
    pixels.setPixelColor(5, pixels.Color(80, 40, 0));
  }
  pixels.show();
  //Serial.println("-------- Starting pair4 -------- ");
  uint8_t buf[128];
  int16_t len = nfc.read(buf, sizeof(buf), uid, 2000);
  //Serial.println("Just attempted a read!");
  if (len > 0) {
    //Serial.println("get a message:");
    for (uint8_t i = 0; i < len; i++) {
      //Serial.print(buf[i], HEX);
      //Serial.print(' ');
    }
    //Serial.print('\n');
    //Serial.println("Finished the exchange pair4!");
    //Serial.println("+++++++++ SUCCESS +++++++++");
    encInitializer1();
    //nfc.finish();
  }
}

void initializer2() {
  pixels.clear();
  if (currentWorm == 0xFF) {
    pixels.setPixelColor(1, pixels.Color(80, 40, 0));
    pixels.setPixelColor(2, pixels.Color(80, 40, 0));
    pixels.setPixelColor(4, pixels.Color(80, 40, 0));
  } else if (currentWorm == RPS_ROCK) {  //orange red?
    pixels.setPixelColor(1, pixels.Color(100, 20, 0));
    pixels.setPixelColor(2, pixels.Color(100, 20, 0));
    pixels.setPixelColor(4, pixels.Color(100, 20, 0));
  } else if (currentWorm == RPS_PAPER) {  //seagreen?
    pixels.setPixelColor(1, pixels.Color(26, 70, 47));
    pixels.setPixelColor(2, pixels.Color(26, 70, 47));
    pixels.setPixelColor(4, pixels.Color(26, 70, 47));
  } else if (currentWorm == RPS_SCISSORS) {  //indigo?
    pixels.setPixelColor(1, pixels.Color(32, 0, 80));
    pixels.setPixelColor(2, pixels.Color(32, 0, 80));
    pixels.setPixelColor(4, pixels.Color(32, 0, 80));
  } else {
    pixels.setPixelColor(1, pixels.Color(80, 40, 0));
    pixels.setPixelColor(2, pixels.Color(80, 40, 0));
    pixels.setPixelColor(4, pixels.Color(80, 40, 0));
  }
  pixels.show();
  //Serial.println("-------- Starting pair3 -------- ");
  uint8_t targetUid[] = { 0, 0, 0, 0 };  // buffer for storing the target UID
  uint8_t uidLength;
  uint8_t response[5];
  uint8_t responseLength;
  uint8_t success;
  success = nfc.initPassive(0, targetUid, &uidLength, 2000);
  if (success) {
    //Serial.println("Successfully activated the target!");
    success = nfc.exchange(m2, sizeof(m2), response, &responseLength);
    //Serial.println("Finished the exchange pair3!");
    encTarget1(targetUid);
    /*
    if (success) {
      Serial.println("Finished the exchange pair3!");
      //Serial.println("+++++++++ SUCCESS +++++++++");
      encTarget1(targetUid);
    }
    nfc.finish();
    */
  }
}

void target1() {
  uint8_t buf[128];
  pixels.clear();
  if (currentWorm == 0xFF) {
    pixels.setPixelColor(0, pixels.Color(80, 40, 0));
    pixels.setPixelColor(3, pixels.Color(80, 40, 0));
    pixels.setPixelColor(5, pixels.Color(80, 40, 0));
  } else if (currentWorm == RPS_ROCK) {  //orange red?
    pixels.setPixelColor(0, pixels.Color(100, 20, 0));
    pixels.setPixelColor(3, pixels.Color(100, 20, 0));
    pixels.setPixelColor(5, pixels.Color(100, 20, 0));
  } else if (currentWorm == RPS_PAPER) {  //seagreen?
    pixels.setPixelColor(0, pixels.Color(26, 70, 47));
    pixels.setPixelColor(3, pixels.Color(26, 70, 47));
    pixels.setPixelColor(5, pixels.Color(26, 70, 47));
  } else if (currentWorm == RPS_SCISSORS) {  //indigo?
    pixels.setPixelColor(0, pixels.Color(32, 0, 80));
    pixels.setPixelColor(3, pixels.Color(32, 0, 80));
    pixels.setPixelColor(5, pixels.Color(32, 0, 80));
  } else {
    pixels.setPixelColor(0, pixels.Color(80, 40, 0));
    pixels.setPixelColor(3, pixels.Color(80, 40, 0));
    pixels.setPixelColor(5, pixels.Color(80, 40, 0));
  }
  pixels.show();
  //Serial.println("-------- Starting pair2 -------- ");
  //Serial.println("MY UID Pair2:");
  /*
  for (uint8_t i = 0; i < 3; i++) {
    char c = uid[i];
    if (c <= 0x1f || c > 0x7f) {
      Serial.print(c, HEX);
    } else {
      Serial.print(c);
    }
  }
  Serial.print('\n');
  */
  // uint8_t rbuf[4];
  //uint8_t success;
  //uint8_t success2;
  //uint8_t response[5];
  // uint8_t responseLength;
  //uint8_t targetUid[] = { 0, 0, 0, 0, 0, 0, 0 };  // buffer for storing the target UID
  //uint8_t uidLength;
  //pixels.clear();
  /*
  for (int i = 0; i < NUMPIXELS; i++) {

    pixels.setPixelColor(i, pixels.Color(15, 75, 0));
    pixels.show();
    delay(30);
  }
  */
  int16_t len = nfc.read(buf, sizeof(buf), uid, 2000);
  //Serial.println("Just attempted a read!");

  if (len > 0) {
    /*
    Serial.println("get a message:");
    for (uint8_t i = 0; i < len; i++) {
      Serial.print(buf[i], HEX);
      Serial.print(' ');
    }
    Serial.print('\n');
    for (uint8_t i = 0; i < len; i++) {
      char c = buf[i];
      if (c <= 0x1f || c > 0x7f) {
        Serial.print(c, HEX);
      } else {
        Serial.print(c);
      }
    }
    Serial.print('\n');
    */
    //grab UID from "hello from UID"
    uint8_t partnerUID[4] = { 0x08, buf[11], buf[12], buf[13] };
    //Serial.println("Have partner UID");
    //nfc.printHex(partnerUID, 4);
    //Serial.println();
    if (partnerUID[1] == 0xA0 || partnerUID[1] == 0xB0) {
      initializer2();
    } else {
      //Serial.println("Paired, but not a badge...");
    }


    //delay(3000);
  }
}

void initializer1() {
  pixels.clear();
  if (currentWorm == 0xFF) {
    pixels.setPixelColor(1, pixels.Color(80, 40, 0));
    pixels.setPixelColor(2, pixels.Color(80, 40, 0));
    pixels.setPixelColor(4, pixels.Color(80, 40, 0));
  } else if (currentWorm == RPS_ROCK) {  //orange red?
    pixels.setPixelColor(1, pixels.Color(100, 20, 0));
    pixels.setPixelColor(2, pixels.Color(100, 20, 0));
    pixels.setPixelColor(4, pixels.Color(100, 20, 0));
  } else if (currentWorm == RPS_PAPER) {  //seagreen?
    pixels.setPixelColor(1, pixels.Color(26, 70, 47));
    pixels.setPixelColor(2, pixels.Color(26, 70, 47));
    pixels.setPixelColor(4, pixels.Color(26, 70, 47));
  } else if (currentWorm == RPS_SCISSORS) {  //indigo?
    pixels.setPixelColor(1, pixels.Color(32, 0, 80));
    pixels.setPixelColor(2, pixels.Color(32, 0, 80));
    pixels.setPixelColor(4, pixels.Color(32, 0, 80));
  } else {
    pixels.setPixelColor(1, pixels.Color(80, 40, 0));
    pixels.setPixelColor(2, pixels.Color(80, 40, 0));
    pixels.setPixelColor(4, pixels.Color(80, 40, 0));
  }
  pixels.show();
  //Serial.println("-------- Starting pair1 -------- ");
  //Serial.println("MY UID Pair1:");
  /*
  for (uint8_t i = 0; i < 3; i++) {
    char c = uid[i];
    if (c <= 0x1f || c > 0x7f) {
      Serial.print(c, HEX);
    } else {
      Serial.print(c);
    }
  }
  Serial.print('\n');
  */
  pixels.clear();
  uint8_t success;
  uint8_t success2;
  uint8_t success3;
  uint8_t targetUid[] = { 0, 0, 0, 0, 0, 0, 0 };  // buffer for storing the target UID
  uint8_t uidLength;                              // Length of the UID (4 or 7 bytes depending on card type)
  uint8_t response[5];
  uint8_t responseLength;
  // uint8_t buf[128];
  uint8_t rbuf[4];
  success = nfc.initPassive(0, targetUid, &uidLength, 2000);
  /*
  for (int i = 0; i < NUMPIXELS; i++) {

    pixels.setPixelColor(i, pixels.Color(0, 75, 0));
    pixels.show();
    delay(30);
  }
  */
  if (success) {
    //Serial.println("Found a target!");
    //Serial.print("  UID Length: ");
    //Serial.print(uidLength, DEC);
    //Serial.println(" bytes");
    //Serial.print("  UID Value: ");
    //nfc.printHex(targetUid, uidLength);
    //Serial.println("");
    //Serial.print(uid[1],HEX);
    if (targetUid[1] == 0xA0 || targetUid[1] == 0xB0) {
      success3 = nfc.exchange(m2, sizeof(m2), response, &responseLength);
      //Serial.println("Finished first exchange pair1!");
      target2();
      /*
      if (success3) {
        Serial.println("Finished first exchange pair1!");
        pair4();
        
        }
        */
    }
  }
}

//Locks the badge if it detects that the UID does not match the given MD5 hash
void badgeLock() {
  pixels.clear();
  while (1) {
    pixels.setPixelColor(0, pixels.Color(40, 0, 0));
    pixels.setPixelColor(1, pixels.Color(40, 0, 0));
    pixels.setPixelColor(2, pixels.Color(40, 0, 0));
    pixels.setPixelColor(3, pixels.Color(40, 0, 0));
    pixels.setPixelColor(4, pixels.Color(40, 0, 0));
    pixels.setPixelColor(5, pixels.Color(40, 0, 0));
    pixels.show();
    Serial.println("BADGE INTEGRITY CHECK FAILED");
    delay(1000);
    pixels.clear();
    pixels.show();
    delay(1000);
  }
}

//Serial puzzle interview based off of BladeRunner 2049
bool interview(char *question, char *answer) {
  const int serialCommandLength = 80;
  char receivedChars[serialCommandLength];
  char commandChar;
  static int sCount = 0;
  char endMarker = '\n';
  bool ans = false;
  Serial.println(question);
  unsigned long startTime = millis();
  while (millis() - startTime < 500 && ans == false) {
    while (Serial.available() > 0) {
      //Serial.println("inside interview");
      commandChar = Serial.read();
      if (commandChar != endMarker) {
        receivedChars[sCount] = commandChar;
        sCount++;
        if (sCount >= serialCommandLength) {
          sCount = serialCommandLength - 1;
        }
      } else if (commandChar == endMarker) {
        receivedChars[sCount] = '\0';  //string terminated
        sCount = 0;
        ans = true;
      }
    }
  }
  // Serial.println("outside interview");
  //Serial.println(receivedChars);
  if (ans) {
    //Serial.println("inside ans");
    if (strcmp(answer, receivedChars) == 0) {
      return true;
    } else {
      return false;
    }
  } else {
    return false;
  }
}

void printFlag3(uint8_t j) {
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DashDotDotDotDotDash");
  Serial.print("DotDashDotDashDotDash");
  Serial.println("DotDashDotDashDotDash");
}

void printFlag2(uint8_t k) {
  uint8_t buf[] = {
    0x3F, 0x54, 0x1F, 0x58, 0x58, 0x5F, 0x4B, 0x63, 0x55, 0x40,
    0x34, 0x1D, 0x5A, 0x4B, 0x3F, 0x34, 0x31, 0x1D, 0x1D, 0x46, 0x00
  };
  for (int i = 0; i < 20; i++) {
    buf[i] = buf[i] + 0x14;
  }
  Serial.println((char *)buf);  //Sh3lls_wiTH1n_SHE11Z
}

void startInterview() {
  if (interview("Cells.", "Cells.")) {
    if (interview("Have you ever been in an institution? Cells.", "Cells.")) {
      if (interview("Do they keep you in a cell? Cells.", "Cells.")) {
        if (interview("When you're not performing your duties do they keep you in a little box? Cells.", "Cells.")) {
          if (interview("Interlinked.", "Interlinked.")) {
            if (interview("What's it like to hold the hand of someone you love? Interlinked.", "Interlinked.")) {
              if (interview("Did they teach you how to feel finger to finger? Interlinked.", "Interlinked.")) {
                if (interview("Do you long for having your heart interlinked? Interlinked.", "Interlinked.")) {
                  if (interview("Do you dream about being interlinked?", "Interlinked.")) {
                    if (interview("What's it like to hold your child in your arms? Interlinked.", "Interlinked.")) {
                      if (interview("Do you feel that there's a part of you that's missing? Interlinked.", "Interlinked.")) {
                        if (interview("Within cells interlinked.", "Within cells interlinked.")) {
                          if (interview("Why don't you say that three times: Within cells interlinked.", "Within cells interlinked. Within cells interlinked. Within cells interlinked.")) {
                            Serial.println("Great job, we're done. You can have the flag:");
                            printFlag2(0x14);
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  Serial.println("You're not even close to baseline...");
}

void printFlag1(uint8_t i) {
  uint8_t buf[18];
  buf[0] = 0x32 + i;
  buf[1] = 0x1F + i;
  buf[2] = 0x5C + i;
  buf[3] = 0x5C + i;
  buf[4] = 0x2F + i;
  buf[5] = 0x5D + i;
  buf[6] = 0x53 + i;
  buf[7] = 0x4E + i;
  buf[8] = 0x15 + i;
  buf[9] = 0x4E + i;
  buf[10] = 0x32 + i;
  buf[11] = 0x3E + i;
  buf[12] = 0x3D + i;
  buf[13] = 0x40 + i;
  buf[14] = 0x44 + i;
  buf[15] = 0x22 + i;
  buf[16] = 0x41 + i;
  buf[17] = 0x00;               //terminate the flag!
  Serial.println((char *)buf);  //C0mm@nd_&_CONQU3R
}

bool newCommand = false;
void loop() {
  const int serialCommandLength = 32;
  char receivedChars[serialCommandLength];

  char commandChar;
  static int sCount = 0;
  char endMarker = '\n';
  pixels.clear();
  static int interval = random(250, 350);
  if (currentMode == 0) {
    badgeLock();
  }
  while (Serial.available() > 0 && newCommand == false) {
    commandChar = Serial.read();
    if (commandChar != endMarker) {
      receivedChars[sCount] = commandChar;
      sCount++;
      if (sCount >= serialCommandLength) {
        sCount = serialCommandLength - 1;
      }
    } else if (commandChar == endMarker) {
      receivedChars[sCount] = '\0';  //string terminated
      sCount = 0;
      newCommand = true;
    }
    //newCommand = false;
  }
  if (newCommand) {
    //Serial.println(receivedChars);
    if (strcmp("help", receivedChars) == 0 || strcmp("?", receivedChars) == 0) {
      //Serial.println("HELP_HERE");
      Serial.println("<HELP MENU>");
      Serial.println("---------- Command : Info ----------");
      Serial.println("help : Prints this table");
      //Serial.println("get_uid : Get your badge's unique ID");
      Serial.println("set_mode MODE : Sets the running mode on the badge to MODE");
      Serial.println("get_table TABLE : Outputs the number of records + each record in table TABLE");
      Serial.println("light NUM : Runs the light puzzle NUM on demand");
      Serial.println("flag : Gives a flag");
    } else if (strcmp("flag", receivedChars) == 0) {
      Serial.println("Which one?");
    } else if (strncmp("light", receivedChars, 5) == 0 && receivedChars[5] == ' ') {
      int lightType = (int)(receivedChars[6]) - '0';
      if (lightType == 1) {
        Serial.println("Got it. Going to run light 1.");
        puzzle1();
      } else if (lightType == 2) {
        Serial.println("Got it. Going to run light 2.");
        puzzle2();
      } else if (lightType == 3) {
        Serial.println("Got it. Going to run light 3.");
        puzzle3();
      } else {
        Serial.println("Which puzzle did you want to run???");
      }
    } else if (strncmp("flag", receivedChars, 4) == 0 && receivedChars[4] == ' ') {
      int flagType = (int)(receivedChars[5]) - '0';
      if (flagType == 1) {
        char flagChar;
        static int sFlagCount = 0;
        char endFlagMarker = '\n';
        bool flagCommand = false;
        Serial.println("We are an army of undead, though we were never alive. We quench the mightiest of walls and eat all your resources. What are we?");
        //Here we have to start a timer using millis() (10 seconds) to wait for the answer input. Since We are in a nested reading loop already, this is the best way to read from Serial
        unsigned long startTime = millis();
        while (millis() - startTime < 10000 && !flagCommand) {
          while (Serial.available() > 0) {
            //Serial.println("inside of flag1 answer read");
            flagChar = Serial.read();
            if (flagChar != endFlagMarker) {
              receivedChars[sFlagCount] = flagChar;
              sFlagCount++;
              if (sFlagCount >= serialCommandLength) {
                sFlagCount = serialCommandLength - 1;
              }
            } else if (flagChar == endFlagMarker) {
              receivedChars[sFlagCount] = '\0';  //string terminated
              sFlagCount = 0;
              flagCommand = true;
              //Serial.println(receivedChars);
            }
          }
        }
        //Serial.println("outside of flag1 answer read");
        if (flagCommand) {

          if (strcmp("botnet", receivedChars) == 0) {
            Serial.println("Yes! Flag:");
            printFlag1(0x11);
          } else {
            Serial.println("no....");
          }
        } else {
          Serial.println("No response received in time (TIMEOUT)");
        }
      } else if (flagType == 2) {
        startInterview();
      } else {
        Serial.println("I don't know which flag that is...");
      }
    } else if (strncmp("set_mode", receivedChars, 8) == 0 && receivedChars[8] == ' ') {
      int mode = (int)(receivedChars[9]) - '0';
      if (mode >= 1 && mode <= 4) {
        Serial.print("SET_MODE with value: ");
        Serial.println(mode);
        currentMode = mode;
      } else {
        Serial.println("MODE must be between 1 and 4.");
      }
    //} else if (strcmp("get_uid", receivedChars) == 0) {
    //  Serial.print("Your UID is: ");
    //  for (uint8_t i = 0; i < UIDLENGTH; i++) {
    //    Serial.print(uid[i], HEX);
    //  }
    //  Serial.println();
    } else if (strcmp("get_worm", receivedChars) == 0) {
      Serial.print("Your WORM is: ");
      Serial.println(currentWorm, HEX);
    } else if (strncmp("get_table", receivedChars, 9) == 0 && receivedChars[9] == ' ') {
      char *tableType = &receivedChars[10];
      if (strcmp(tableType, "scan") == 0) {
        Serial.println("GET_TABLE: SCAN");
        int tableSize = numRecordsScanTable();
        Serial.print("Number of entries in scan table: ");
        Serial.println(tableSize);
        //uint8_t tableBuf[tableSize * 4];
        getScanTable(tableBuf1);
        for (uint16_t i = 0; i < (tableSize * 4); i++) {
          Serial.print(tableBuf1[i], HEX);
          Serial.print(' ');
        }
        Serial.print('\n');
      } else if (strcmp(tableType, "worm") == 0) {
        Serial.println("GET_TABLE: WORM");
        int tableSize = numRecordsWormTable();
        Serial.print("Number of entries in worm table: ");
        Serial.println(tableSize);
        //uint8_t tableBuf[tableSize * 3];
        getWormTable(tableBuf1);
        for (uint16_t i = 0; i < (tableSize * 3); i++) {
          Serial.print(tableBuf1[i], HEX);
          Serial.print(' ');
        }
        Serial.print('\n');
      } else if (strcmp(tableType, "pair") == 0) {
        Serial.println("GET_TABLE: PAIR");
        int tableSize = numRecordsPairTable();
        Serial.print("Number of entries in pair table: ");
        Serial.println(tableSize);
        //uint8_t tableBuf[tableSize * 3];
        getPairTable(tableBuf1, false);
        for (uint16_t i = 0; i < (tableSize * 3); i++) {
          Serial.print(tableBuf1[i], HEX);
          Serial.print(' ');
        }
        Serial.print('\n');

      } else {
        Serial.println("Unknown table type");
      }
    } else {
      Serial.print("Unknown command: ");
      Serial.print(receivedChars);
      Serial.println(". Type help for commands.");
    }
    newCommand = false;
  }
  //Serial.println(receivedChars);
  if (currentMode == MODE_PAIR) {
    Serial.println("HANDSHAKE MODE");
    //Serial.print("WORM VAL:");
    //Serial.println(currentWorm, HEX);
    initializer1();
    delay(interval);
    target1();
    delay(interval);
  } else if (currentMode == MODE_SCAN) {
    Serial.println("SCAN MODE");
    scan();
  } else if (currentMode == MODE_KIOSK) {
    Serial.println("KIOSK MODE");
    kiosk();
    //sendPairTableToKiosk();
    delay(5000);
  } else if (currentMode == MODE_PUZZLE) {
    int puzzle_num = 1;
    while(currentMode == MODE_PUZZLE) {
        delay(200);
        if(puzzle_num == 1) {
          Serial.println("PUZZLE 1");
          puzzle1();
          puzzle_num = 2;
        }
        else if (puzzle_num == 2) {
          Serial.println("PUZZLE 2");
          puzzle2();
          puzzle_num = 3;
        }
        else if (puzzle_num == 2) {
          Serial.println("PUZZLE 3");
          puzzle3();
          puzzle_num = 1;
        }
    }
  }
}


//This next section contains code to translate button presses from SW3 to morse code. Taken from https://www.hackster.io/vladakrsmanovic/arduino-morse-code-machine-709cc5.
//Big thanks to Vladimir Krsmanovic
//Note: Conditions have been reversed since we start our button logic on HIGH (1) NOT LOW (0)

//Morse Code setup
int buttonState = LOW;      //0
int lastButtonState = LOW;  //0
const int buttonPin = BUTTON_PIN;
int pause_value = 250;  // depending on your skill and how fast your fingers are you can change this value to make typing a message faster or slower
long signal_length = 0;
long pause = 0;
String morse = "";
String dash = "-";
String dot = "*";
boolean cheker = false;
boolean linecheker = false;
//setup core1
void setup1() {
}
//loop our morse code function in core1. This will allow core0 to continue while letting us change modes
void loop1() {
  //uint32_t ints = save_and_disable_interrupts();
  //detachInterrupt(0);
  //int TIMER = 1200;
  //int c = 0;
  //detachInterrupt(digitalPinToInterrupt(BUTTON_PIN));
  if (currentMode != 0) {
    buttonState = digitalRead(buttonPin);
    if (!buttonState && !lastButtonState) {  // basic state machine depending on the state of the signal from the button
      ++signal_length;
      //Serial.println(signal_length, DEC);
      if (signal_length < 2 * pause_value) {  //this help to notice that there is a change in the signal length aka that its not a dot anymore but a dash
                                              // best use for the measuring of signal_length would be use of the millis() but this was used  for simplicity
        //tone(buzzer, 1500) ;
        //Serial.println("dash");
      } else {
        // tone(buzzer, 1000) ;
        //Serial.println("dot");
      }
    } else if (buttonState && !lastButtonState) {  //this part of the code happens when the button is released and it send either * or - into the buffer
      if (signal_length > 50 && signal_length < 2 * pause_value) {
        morse = morse + dot;
        Serial.print(".");
      } else if (signal_length > 2 * pause_value) {
        morse = morse + dash;
        Serial.print("-");
      }
      signal_length = 0;
    } else if (!buttonState && lastButtonState) {  // this part happens when the button is pressed and its use to reset several values
      pause = 0;
      //digitalWrite(13, HIGH);
      cheker = true;
      linecheker = true;
    } else if (buttonState && lastButtonState) {
      ++pause;
      if ((pause > 3 * pause_value) && (cheker)) {
        if (morse == "****") {  // H
          morse = "";
          currentMode = MODE_PAIR;
        } else if (morse == "***") {  // S
          morse = "";
          currentMode = MODE_SCAN;
        } else if (morse == "*-**") {  // K
          morse = "";
          currentMode = MODE_KIOSK;
        } else if (morse == "*--*") {  // P
          morse = "";
          currentMode = MODE_PUZZLE;
        } else if (morse == "*-*---*--*----*-*") {  //ROANOKE
          morse = "";
          printFlag3(0x24);
        } else {
          printaj(morse);
        }
        cheker = false;
        morse = "";
      }
      if ((pause > 15 * pause_value) && (linecheker)) {
        //Serial.println();
        linecheker = false;
      }
    }
    lastButtonState = buttonState;
    //c++;
    delay(1);
  }
}
void printaj(String morseChar)  //ugly part of the code but it works fine
{                               //compare morse string to known morse values and print out the letter or a number
                                //the code is written based on the international morse code, one thing i changed is that insted of typing a special string to end the line it happens with enough delay
  if (morseChar == "*-")
    Serial.print("A");
  else if (morseChar == "-***")
    Serial.print("B");
  else if (morseChar == "-*-*")
    Serial.print("C");
  else if (morseChar == "-**")
    Serial.print("D");
  else if (morseChar == "*")
    Serial.print("E");
  else if (morseChar == "**-*")
    Serial.print("F");
  else if (morseChar == "--*")
    Serial.print("G");
  else if (morseChar == "****")
    Serial.print("H");
  else if (morseChar == "**")
    Serial.print("I");
  else if (morseChar == "*---")
    Serial.print("J");
  else if (morseChar == "-*-")
    Serial.print("K");
  else if (morseChar == "*-**")
    Serial.print("L");
  else if (morseChar == "--")
    Serial.print("M");
  else if (morseChar == "-*")
    Serial.print("N");
  else if (morseChar == "---")
    Serial.print("O");
  else if (morseChar == "*--*")
    Serial.print("P");
  else if (morseChar == "--*-")
    Serial.print("Q");
  else if (morseChar == "*-*")
    Serial.print("R");
  else if (morseChar == "***")
    Serial.print("S");
  else if (morseChar == "-")
    Serial.print("T");
  else if (morseChar == "**-")
    Serial.print("U");
  else if (morseChar == "***-")
    Serial.print("V");
  else if (morseChar == "*--")
    Serial.print("W");
  else if (morseChar == "-**-")
    Serial.print("X");
  else if (morseChar == "-*--")
    Serial.print("Y");
  else if (morseChar == "--**")
    Serial.print("Z");

  else if (morseChar == "*----")
    Serial.print("1");
  else if (morseChar == "**---")
    Serial.print("2");
  else if (morseChar == "***--")
    Serial.print("3");
  else if (morseChar == "****-")
    Serial.print("4");
  else if (morseChar == "*****")
    Serial.print("5");
  else if (morseChar == "-****")
    Serial.print("6");
  else if (morseChar == "--***")
    Serial.print("7");
  else if (morseChar == "---**")
    Serial.print("8");
  else if (morseChar == "----*")
    Serial.print("9");
  else if (morseChar == "-----")
    Serial.print("0");

  Serial.print(" ");

  morseChar = "";
}