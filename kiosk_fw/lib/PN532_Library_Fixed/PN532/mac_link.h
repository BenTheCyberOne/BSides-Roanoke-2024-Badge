

#ifndef __MAC_LINK_H__
#define __MAC_LINK_H__

#include "PN532.h"

class MACLink {
public:
    MACLink(PN532Interface &interface) : pn532(interface) {

    };
    
    /**
    * @brief    Activate PN532 as a target
    * @param    timeout max time to wait, 0 means no timeout
    * @return   > 0     success
    *           = 0     timeout
    *           < 0     failed
    */
    int8_t activateAsTarget(uint8_t *uid, uint16_t timeout = 1);

    /**
    * @brief    write a PDU packet, the packet should be less than (255 - 2) bytes
    * @param    header  packet header
    * @param    hlen    length of header
    * @param 	body	packet body
    * @param 	blen	length of body
    * @return   true    success
    *           false   failed
    */
    bool write(const uint8_t *header, uint8_t hlen, const uint8_t *body = 0, uint8_t blen = 0);

    /**
    * @brief    read a PDU packet, the packet will be less than (255 - 2) bytes
    * @param    buf     the buffer to contain the PDU packet
    * @param    len     lenght of the buffer
    * @return   >=0     length of the PDU packet 
    *           <0      failed
    */
    int16_t read(uint8_t *buf, uint8_t len);

    uint8_t *getHeaderBuffer(uint8_t *len) {
        return pn532.getBuffer(len);
    };
/**************************************************************************/
/*!
    Waits for an ISO14443A target to enter the field

    @param  cardBaudRate  Baud rate of the card
    @param  uid           Pointer to the array that will be populated
                          with the card's UID (up to 7 bytes)
    @param  uidLength     Pointer to the variable that will hold the
                          length of the card's UID.
    @param  timeout       The number of tries before timing out
    @param  inlist        If set to true, the card will be inlisted

    @returns 1 if everything executed properly, 0 for an error
*/
/**************************************************************************/
    bool initPassive(uint8_t cardbaudrate, uint8_t *uid, uint8_t *uidLength, uint16_t timeout = 1000, bool inlist = true);

    bool activateTarget(uint16_t timeout = 1);

    bool exchange(uint8_t *send, uint8_t sendLength, uint8_t *response, uint8_t *responseLength);

    void printHex(const uint8_t *data, const uint32_t numBytes);
    
private:
    PN532 pn532;
};

#endif // __MAC_LINK_H__
