/**************************************************************************/
/*!
    @file     emulatetag.cpp
    @author   Armin Wieser
    @license  BSD
*/
/**************************************************************************/

#include "emulatetag.h"
#include "PN532_debug.h"

//#include "application.h"

#define MAX_TGREAD

#ifndef bitRead
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))
#endif

// Command APDU
#define C_APDU_CLA   0
#define C_APDU_INS   1 // instruction
#define C_APDU_P1    2 // parameter 1
#define C_APDU_P2    3 // parameter 2
#define C_APDU_LC    4 // length command
#define C_APDU_DATA  5 // data

#define C_APDU_P1_SELECT_BY_ID   0x00
#define C_APDU_P1_SELECT_BY_NAME 0x04

// Response APDU
#define R_APDU_SW1_COMMAND_COMPLETE 0x90
#define R_APDU_SW2_COMMAND_COMPLETE 0x00

#define R_APDU_SW1_NDEF_TAG_NOT_FOUND 0x6a
#define R_APDU_SW2_NDEF_TAG_NOT_FOUND 0x82

#define R_APDU_SW1_FUNCTION_NOT_SUPPORTED 0x6A
#define R_APDU_SW2_FUNCTION_NOT_SUPPORTED 0x81

#define R_APDU_SW1_MEMORY_FAILURE 0x65
#define R_APDU_SW2_MEMORY_FAILURE 0x81

#define R_APDU_SW1_END_OF_FILE_BEFORE_REACHED_LE_BYTES 0x62
#define R_APDU_SW2_END_OF_FILE_BEFORE_REACHED_LE_BYTES 0x82

// ISO7816-4 commands
#define ISO7816_SELECT_FILE 0xA4
#define ISO7816_READ_BINARY 0xB0
#define ISO7816_UPDATE_BINARY 0xD6

typedef enum { NONE_Tag_File, CC_Tag_File, NDEF_Tag_File } tag_file;   // CC ... Compatibility Container

bool EmulateTag::init() {
  pn532.begin();
  uint32_t versiondata = pn532.getFirmwareVersion();

  if (!versiondata)
  {
    Serial.print(F("Didn't find PN53x board"));
    return false;
  }

  Serial.print(F("Found chip PN5")); Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print(F("Firmware ver. ")); Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.'); Serial.println((versiondata >> 8) & 0xFF, DEC);

  // configure board to read RFID tags (for some reason this returns 0 with SPI)
  pn532.SAMConfig();

  return true;
}

void EmulateTag::setNdefFile(const uint8_t* ndef, const int16_t ndefLength) {
  if (ndefLength > (NDEF_MAX_LENGTH - 2)) {
    DMSG("ndef file too large (> NDEF_MAX_LENGHT -2) - aborting");
    return;
  }

  ndef_file[0] = ndefLength >> 8;
  ndef_file[1] = ndefLength & 0xFF;
  memcpy(ndef_file + 2, ndef, ndefLength);
}

void EmulateTag::setUid(uint8_t* uid) {
  uidPtr = uid;
}

bool EmulateTag::emulate(const uint16_t tgInitAsTargetTimeout) {
  pn532.writeRegister(REG_CIU_RxMode, 0x80); // no accept invalid frame, no accept multiple frames, CRC enabled, rx speed 106 kbits
  pn532.writeRegister(REG_CIU_TxMode, 0x80); // CRC enabled, tx speed 106 kbits

  pn532.setRFField(0, 0);

  uint8_t prev = pn532.readRegister(REG_CIU_Status2);
  prev = bitClear(prev, 3); // disable CRYPTO1
  pn532.writeRegister(REG_CIU_Status2, prev);

  prev = pn532.readRegister(REG_CIU_ManualRCV);
  prev = bitClear(prev, 4); // disable ParityDisable
  pn532.writeRegister(REG_CIU_ManualRCV, prev);

  prev = pn532.readRegister(REG_CIU_TxAuto);
  prev = bitSet(prev, 2); // enable InitialRFOn
  pn532.writeRegister(REG_CIU_TxAuto, prev);

  pn532.setParameters(0x30 ); // enable fAutomaticRATS, fISO14443-4_PICC

  // http://www.nxp.com/documents/application_note/AN133910.pdf
  uint8_t command[] = {
      PN532_COMMAND_TGINITASTARGET,
      5, // MODE: PICC only, Passive only

      0x04, 0x00,       // SENS_RES
      0x00, 0x00, 0x00, // NFCID1
      0x20,             // SEL_RES

      0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, // FeliCaParams
      0, 0,

      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // NFCID3t

      0, // length of general bytes
      0  // length of historical bytes
  };

  if (uidPtr != 0) {  // if uid is set copy 3 bytes to nfcid1
    memcpy(command + 4, uidPtr, 3);
  }

  if (1 != pn532.tgInitAsTarget(command, sizeof(command), tgInitAsTargetTimeout)) {
    Serial.println("tgInitAsTarget failed or timed out!");
    DMSG("tgInitAsTarget failed or timed out!\r\n");
    return false;
  }

  uint8_t compatibility_container[] = {
    0, 0x0F,
    0x20,
    0, 0x54,
    0, 0xFF,
    0x04,       // T
    0x06,       // L
    0xE1, 0x04, // File identifier
    ((NDEF_MAX_LENGTH & 0xFF00) >> 8), (NDEF_MAX_LENGTH & 0xFF), // maximum NDEF file size
    0x00,       // read access 0x0 = granted
    0x00        // write access 0x0 = granted | 0xFF = deny
  };

  if (tagWriteable == false) {
    compatibility_container[14] = 0xFF;
  }

  tagWrittenByInitiator = false;

  uint8_t rwbuf[128];
  uint8_t sendlen;
  int16_t status;
  tag_file currentFile = NONE_Tag_File;
  uint16_t cc_size = sizeof(compatibility_container);
  bool runLoop = true;
  bool firstRead = true;
  int emptyResultCount = 0;

  while (runLoop) {
    status = pn532.tgGetData(rwbuf, sizeof(rwbuf));

    if (status < 0) {
      Serial.println("tgGetData Failed!");
      DMSG("tgGetData failed!\r\n");
      pn532.inRelease();
      return !firstRead;
    }

    firstRead = false;

    // if (rwbuf[C_APDU_CLA] != 0x00) {
    //   DMSG("C_APDU_CLA not zero");
    //   pn532.inRelease();
    //   return false;
    // }

    uint8_t p1 = rwbuf[C_APDU_P1];
    uint8_t p2 = rwbuf[C_APDU_P2];
    uint8_t lc = rwbuf[C_APDU_LC];
    uint16_t p1p2_length = ((int16_t)p1 << 8) + p2;

    // libnfc bails here but we'll look for 3 empty read binary commands since
    // we have to constantly read from buffer.
    /*if (p1p2_length < 4) {
      emptyResultCount++;
      if (emptyResultCount > 6) {
        pn532.inRelease();
        return true;
      }
    } else {
      emptyResultCount = 0;
    }*/

    switch (rwbuf[C_APDU_INS]) {
    case ISO7816_SELECT_FILE:
      switch (p1) {
      case C_APDU_P1_SELECT_BY_ID:
        if (p2 != 0x0c) {
          DMSG("C_APDU_P2 != 0x0c\r\n");
          setResponse(COMMAND_COMPLETE, rwbuf, &sendlen);
        }
        else if (lc == 2 && rwbuf[C_APDU_DATA] == 0xE1 && (rwbuf[C_APDU_DATA + 1] == 0x03 || rwbuf[C_APDU_DATA + 1] == 0x04)) {
          setResponse(COMMAND_COMPLETE, rwbuf, &sendlen);
          if (rwbuf[C_APDU_DATA + 1] == 0x03) {
            currentFile = CC_Tag_File;
          }
          else if (rwbuf[C_APDU_DATA + 1] == 0x04) {
            currentFile = NDEF_Tag_File;
          }
        }
        else {
          setResponse(TAG_NOT_FOUND, rwbuf, &sendlen);
        }
        break;
      case C_APDU_P1_SELECT_BY_NAME:
        const uint8_t ndef_tag_application_name_v2[] = { 0, 0x7, 0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01 };
        if (0 == memcmp(ndef_tag_application_name_v2, rwbuf + C_APDU_P2, sizeof(ndef_tag_application_name_v2))) {
          setResponse(COMMAND_COMPLETE, rwbuf, &sendlen);
        }
        else {
          DMSG("function not supported\r\n");
          setResponse(FUNCTION_NOT_SUPPORTED, rwbuf, &sendlen);
        }
        break;
      }
      break;
    case ISO7816_READ_BINARY:
      switch (currentFile) {
      case NONE_Tag_File:
        setResponse(TAG_NOT_FOUND, rwbuf, &sendlen);
        break;
      case CC_Tag_File:
        if (p1p2_length > NDEF_MAX_LENGTH) {
          setResponse(END_OF_FILE_BEFORE_REACHED_LE_BYTES, rwbuf, &sendlen);
        } else {
          memcpy(rwbuf, compatibility_container + p1p2_length, lc);
          setResponse(COMMAND_COMPLETE, rwbuf + lc, &sendlen, lc);
        }
        break;
      case NDEF_Tag_File:
        if (p1p2_length > NDEF_MAX_LENGTH) {
          setResponse(END_OF_FILE_BEFORE_REACHED_LE_BYTES, rwbuf, &sendlen);
        } else {
          memcpy(rwbuf, ndef_file + p1p2_length, lc);
          setResponse(COMMAND_COMPLETE, rwbuf + lc, &sendlen, lc);
        }
        break;
      }

      break;
    case ISO7816_UPDATE_BINARY:
      if (!tagWriteable) {
        setResponse(FUNCTION_NOT_SUPPORTED, rwbuf, &sendlen);
      }
      else {
        if (p1p2_length > NDEF_MAX_LENGTH) {
          setResponse(MEMORY_FAILURE, rwbuf, &sendlen);
        }
        else {
          memcpy(ndef_file + p1p2_length, rwbuf + C_APDU_DATA, lc);
          setResponse(COMMAND_COMPLETE, rwbuf, &sendlen);
          tagWrittenByInitiator = true;

          uint16_t ndef_length = (ndef_file[0] << 8) + ndef_file[1];
          if ((ndef_length > 0) && (updateNdefCallback != 0)) {
            updateNdefCallback(ndef_file + 2, ndef_length);
          }
        }
      }
      break;
    default:
      DMSG("Command not supported!");
      DMSG_HEX(rwbuf[C_APDU_INS]);
      DMSG("\r\n");
      setResponse(FUNCTION_NOT_SUPPORTED, rwbuf, &sendlen);
      break;
    }

    status = pn532.tgSetData(rwbuf, sendlen);
    if (status < 0) {
      DMSG("tgSetData failed\r\n!");
      pn532.inRelease();
      return true;
    }
  }

  DMSG("Emulation finished\r\n");
  pn532.inRelease();
  return true;
}

void EmulateTag::setResponse(responseCommand cmd, uint8_t* buf, uint8_t* sendlen, uint8_t sendlenOffset) {
  switch (cmd) {
  case COMMAND_COMPLETE:
    buf[0] = R_APDU_SW1_COMMAND_COMPLETE;
    buf[1] = R_APDU_SW2_COMMAND_COMPLETE;
    *sendlen = 2 + sendlenOffset;
    break;
  case TAG_NOT_FOUND:
    buf[0] = R_APDU_SW1_NDEF_TAG_NOT_FOUND;
    buf[1] = R_APDU_SW2_NDEF_TAG_NOT_FOUND;
    *sendlen = 2;
    break;
  case FUNCTION_NOT_SUPPORTED:
    buf[0] = R_APDU_SW1_FUNCTION_NOT_SUPPORTED;
    buf[1] = R_APDU_SW2_FUNCTION_NOT_SUPPORTED;
    *sendlen = 2;
    break;
  case MEMORY_FAILURE:
    buf[0] = R_APDU_SW1_MEMORY_FAILURE;
    buf[1] = R_APDU_SW2_MEMORY_FAILURE;
    *sendlen = 2;
    break;
  case END_OF_FILE_BEFORE_REACHED_LE_BYTES:
    buf[0] = R_APDU_SW1_END_OF_FILE_BEFORE_REACHED_LE_BYTES;
    buf[1] = R_APDU_SW2_END_OF_FILE_BEFORE_REACHED_LE_BYTES;
    *sendlen = 2;
    break;
  }
}