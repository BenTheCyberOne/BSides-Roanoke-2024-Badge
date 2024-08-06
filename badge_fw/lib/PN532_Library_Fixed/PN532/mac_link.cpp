
#include "mac_link.h"
#include "PN532_debug.h"

void MACLink::printHex(const uint8_t *data, const uint32_t numBytes)
{
    pn532.PrintHex(data, numBytes);
}

int8_t MACLink::activateAsTarget(uint8_t *uid, uint16_t timeout)
{
	pn532.begin();
	pn532.SAMConfig();
    return pn532.tgInitAsTarget(uid, timeout);
}

bool MACLink::write(const uint8_t *header, uint8_t hlen, const uint8_t *body, uint8_t blen)
{
    return pn532.tgSetData(header, hlen, body, blen);
}

int16_t MACLink::read(uint8_t *buf, uint8_t len)
{
    return pn532.tgGetData(buf, len);
}

bool MACLink::initPassive(uint8_t cardbaudrate, uint8_t *uid, uint8_t *uidLength, uint16_t timeout, bool inlist)
{
    bool status;
    pn532.begin();
    pn532.SAMConfig();
    status = pn532.readPassiveTargetID(cardbaudrate, uid, uidLength, timeout, inlist);
    DMSG("Status in MACLINK: ");
    DMSG_STR(status);
    return status;
}

bool MACLink::activateTarget(uint16_t timeout)
{
    return pn532.inATR(timeout);
}

bool MACLink::exchange(uint8_t *send, uint8_t sendLength, uint8_t *response, uint8_t *responseLength)
{
    return pn532.inDataExchange(send, sendLength, response, responseLength);
}