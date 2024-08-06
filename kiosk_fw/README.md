# Kiosk Firmware
This directory holds the firmware and required libraries for the BSides Roanoke 2024 Kiosk

## ino
Because this firmware was built using Arduino IDE, the firmware is contained in one large .ino file. Arduino IDE made this firmware easy to develop, especially since I have never done something like this before. While it turned out to be quite a double-edged sword, Arduino IDE makes development of firmware for these small controllers quite an easy process. 

## lib
These are the external libraries used for the kiosk. 
- The PN532 library originally failed to function with our hardware, so alterations were made to ensure successful communication could happen between the badges and kiosk.


## How to build
1. Download Arduino IDE here: https://www.arduino.cc/en/software
2. Install the RP2040 device/board files: https://learn.adafruit.com/rp2040-arduino-with-the-earlephilhower-core/installing-the-earlephilhower-core
3. Ensure the libraries included in 'lib' are placed in your local Arduino IDE library file location
4. Connect your Pico to your device and be sure to select the board type of "Raspberry Pi Pico" inside of Arduino IDE