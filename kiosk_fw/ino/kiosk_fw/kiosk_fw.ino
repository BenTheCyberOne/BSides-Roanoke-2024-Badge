/*
BSides Roanoke 2024 Kiosk FW
Wrote by BenTheCyberOne with the help of doc and KoopaTroopa8

Free to edit and distribute as you please

*/

#include "SPI.h"
#include "PN532_SPI.h"
#include "llcp.h"
#include "snep.h"
#include <NfcAdapter.h>

PN532_SPI pn532spi(SPI, 17);
SNEP nfc(pn532spi);
//Static Kiosk UID
uint8_t uid[3] = { 0xC0, 0xFE, 0xED };

//Buffer for reading tables from badge (max size is 4096)
uint8_t badgeBuf[4096];

//Buffer for reading misc messages from Badge, table int sizes
uint8_t readBuf[252];

//Message buffer for polling, init kiosk <-> badge
uint8_t m1[11] = { 0x68, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x66, 0x72, 0x6f, 0x6d, 0x20 };  //"hello from "
uint8_t m2[14];

void setup() {
  Serial.begin(9600);
  //Serial.println("Connected");

  memcpy(m2, m1, 11);       //copy M1 into m2
  memcpy(m2 + 11, uid, 3);  //copy the UID into M2, creating "hello from [UID]"
}
int count = 0;
bool step1, step2, step3;

void loop() {
  step1 = initKiosk();
  while (step1) {
    step2 = getBadgePairData();
    if (!step2) {
      step1 = false;
      break;
    }
    step2 = getBadgeWormData();
    if (!step2) {
      step1 = false;
      break;
    }
    step2 = getBadgeScanData();
    if (!step2) {
      step1 = false;
      break;
    }
    while (step2) {
      //delay(3000);
      step3 = pollBadge();
      if (!step3) {
        step1 = false;
        step2 = false;
        step3 = false;
        //Serial.println("No longer polling...");
        break;
      }
    }
  }
  //pollBadge();
  delay(2000);
}

//Reads the Scan Table data from the badge and outputs to Serial (webapp)
bool getBadgeScanData() {
  uint8_t resLength;
  const uint8_t chunkSize = 250;
  uint8_t resBuf[chunkSize + 1];
  //bool lostConn = false;
  uint8_t s = chunkSize;
  delay(2500);  // required for badge to finish setting up table buffer
  int16_t r = nfc.read(readBuf, sizeof(readBuf), uid, 4000);
  if (r > 0) {
    //Serial.println("SCAN TRANSFER");
    uint16_t numBytesExpected = readBuf[1] | (readBuf[0] << 8);
    //Serial.print("Number of bytes expected: ");
    //Serial.println(numBytesExpected);
    //Serial.print("Chunks=");
    //Serial.println(((numBytesExpected/chunkSize)+1),DEC);
    //Serial.println("EOT");
    delay(2000);
    for (uint16_t offset = 0; offset < numBytesExpected; offset += s) {
      //delay(1500); This delay is no longer needed - we can loop and reset the offset if we fail a cycle!
      //if (lostConn) {
      //  offset -= chunkSize;
      //}
      uint8_t w = nfc.read(readBuf, sizeof(readBuf), uid, 2000);
      if (w > 0 && w < 254) {
        //Serial.println("Successfully got chunk!");
        //Serial.println(w, DEC);
        memcpy(badgeBuf + offset, readBuf, w);
        //lostConn = false;
        //Serial.print("PART_");
        //Serial.println((offset/chunkSize),DEC);
        //for(int i = 0; i < w; i++){
        //  Serial.print(readBuf[i], HEX);
        //}
        //Serial.println();
        //Serial.println("EOT");
        s = w;
      } else {
        //Serial.println("Failed to read from badge. We might be skipping a chunk! Rolling back...");
        //lostConn = true;
        if (offset != 0) {
          offset -= s;
        } else {
          //lostConn = true;
          s = 0;
        }
      }
    }
    Serial.println("PART_2");
    //Serial.println("current badgeBuf:");
    if (numBytesExpected > 0) {
      for (uint16_t i = 0; i < numBytesExpected; i++) {
        Serial.print(badgeBuf[i], HEX);
      }
    }
    Serial.println();
    Serial.println("EOT");

    //Serial.println();
    return true;
  }
  return false;
}

//Reads the Worm Table data from the badge and outputs to Serial (webapp)
bool getBadgeWormData() {
  uint8_t resLength;
  const uint8_t chunkSize = 249;
  uint8_t resBuf[chunkSize + 1];
  bool lostConn = false;
  uint8_t s = chunkSize;
  //Serial.println("WORM TRANSFER 1");
  delay(2500);  // required for badge to finish setting up table buffer
  int16_t r = nfc.read(readBuf, sizeof(readBuf), uid, 4000);
  if (r > 0) {
    //Serial.println("WORM TRANSFER 2");
    uint16_t numBytesExpected = readBuf[1] | (readBuf[0] << 8);
    //Serial.print("Number of bytes expected: ");
    //Serial.println(numBytesExpected);
    //Serial.print("Chunks=");
    //Serial.println(((numBytesExpected/chunkSize)+1),DEC);
    //Serial.println("EOT");
    delay(2000);
    for (uint16_t offset = 0; offset < numBytesExpected; offset += s) {
      //delay(1500); This delay is no longer needed - we can loop and reset the offset if we fail a cycle!
      //if (lostConn) {
      //  offset -= s;
      //}
      uint8_t w = nfc.read(readBuf, sizeof(readBuf), uid, 2000);
      if (w > 0 && w < 252) {
        //Serial.println("Successfully got chunk!");
        //Serial.println(w, DEC);
        memcpy(badgeBuf + offset, readBuf, w);
        //Serial.print("PART_");
        //Serial.println((offset/chunkSize),DEC);
        //for(int i = 0; i < w; i++){
        //  Serial.print(readBuf[i], HEX);
        // }
        //Serial.println();
        //Serial.println("EOT");
        //lostConn = false;
        s = w;
      } else {
        //Serial.println("Failed to read from badge. We might be skipping a chunk! Rolling back...");
        //lostConn = true;
        if (offset != 0) {
          offset -= s;
        } else {
          s = 0;
          //lostConn = true;
        }
      }
    }
    //Serial.print("Offset:");
    //Serial.println(offset, DEC);
    //Serial.println("current badgeBuf:");
    Serial.println("PART_1");
    if (numBytesExpected > 0) {
      for (uint16_t i = 0; i < numBytesExpected; i++) {
        Serial.print(badgeBuf[i], HEX);
      }
    }
    Serial.println();
    Serial.println("EOT");
    //Serial.println();
    return true;
  }
  return false;
}

//Reads the Pair (Handshake) Table data from the badge and outputs to Serial (webapp)
bool getBadgePairData() {
  uint8_t resLength;
  const uint8_t chunkSize = 249;
  uint8_t resBuf[chunkSize + 1];
  bool lostConn = false;
  uint8_t s = chunkSize;
  int16_t r = nfc.read(readBuf, sizeof(readBuf), uid, 4000);
  if (r > 0) {

    uint16_t numBytesExpected = readBuf[1] | (readBuf[0] << 8);
    //Serial.print("Number of bytes expected: ");
    //Serial.println(numBytesExpected);
    Serial.println("Chunks=3");
    //Serial.println(((numBytesExpected/chunkSize)+1),DEC);

    Serial.println("EOT");
    delay(2000);
    for (uint16_t offset = 0; offset < numBytesExpected; offset += s) {

      uint8_t w = nfc.read(readBuf, sizeof(readBuf), uid, 5000);
      if (w > 0 && w < 252) {
        //Serial.println("Successfully got chunk!");
        //Serial.println(w, DEC);
        memcpy(badgeBuf + offset, readBuf, w);
        //Serial.print("t1_PART_");
        //Serial.println((offset/chunkSize),DEC);
        //for(int i = 0; i < w; i++){
        //  Serial.print(readBuf[i], HEX);
        //}
        // Serial.println();
        //Serial.println("EOT");
        s = w;
        //lostConn = false;
        //for (uint8_t i = 0; i < w; i++) {
        //  badgeBuf[offset + i] = readBuf[i];
        //}
      } else {
        // Serial.println("Failed to read from badge. We might be skipping a chunk! Rolling back...");
        //lostConn = true;
        if (offset != 0) {
          offset -= s;
        } else {
          s = 0;
          //lostConn = true;
        }
      }
    }
    //Serial.println("current badgeBuf:");
    Serial.println("PART_0");
    //Serial.println((offset/chunkSize),DEC);
    //for(int i = 0; i < w; i++){
    //  Serial.print(readBuf[i], HEX);
    //}
    // Serial.println();
    //Serial.println("EOT");
    if (numBytesExpected > 0) {
      for (uint16_t i = 0; i < numBytesExpected; i++) {
        Serial.print(badgeBuf[i], HEX);
      }
    }

    Serial.println();
    Serial.println("EOT");
    //Serial.println();
    return true;
  }
  return false;
}

// Attempts to pair with a badge to start the transfer process
bool initKiosk() {
  uint8_t response[5];
  uint8_t responseLength;
  uint8_t targetUid[] = { 0, 0, 0, 0, 0, 0, 0 };  // buffer for storing the target UID
  uint8_t uidLength;                              // Length of the UID (4 or 7 bytes depending on card type)
  bool success = nfc.initPassive(0, targetUid, &uidLength, 3000);
  if (success) {
    //Serial.println("Found a target!");
    //Serial.print("  UID Length: ");
    //Serial.print(uidLength, DEC);
    // Serial.println(" bytes");
    //Serial.print("  UID Value: ");
    //nfc.printHex(targetUid, uidLength);
    //Serial.println("");
    //Serial.print(uid[1],HEX);
    if (targetUid[1] == 0xA0 || targetUid[1] == 0xB0) {
      //Serial.println("Found a badge!");
      Serial.print("Connected ");
      nfc.printHex(targetUid, uidLength);
      //Serial.println();
      Serial.println("EOT");
      nfc.exchange(m2, sizeof(m2), response, &responseLength);
      //TODO: send UID to webapp

      return true;
    }
  }
  Serial.println("No Badge");
  return false;
}

//Polls for the badge to ensure it is still on the Kiosk
bool pollBadge() {
  uint8_t initBuf[15];
  delay(6000);
  int16_t len = nfc.read(initBuf, sizeof(initBuf), uid, 10000);

  if (len > 0) {
    uint8_t kioskUID[4] = { 0x08, initBuf[11], initBuf[12], initBuf[13] };
    //Serial.println("Have UID:");
    nfc.printHex(kioskUID, 4);
    //Serial.println();
    if (kioskUID[1] == 0xA0) {
      //Serial.println("Badge is still here!");
      return true;
    }
  }
  //Serial.println("did not find the badge...");
  Serial.println("No Badge");
  return false;
}