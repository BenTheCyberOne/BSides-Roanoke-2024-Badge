# Badge Firmware
This directory holds the firmware and required libraries for the BSides Roanoke 2024 Badge

## ino
Because this firmware was built using Arduino IDE, the firmware is contained in one large .ino file. Arduino IDE made this firmware easy to develop, especially since I have never done something like this before. While it turned out to be quite a double-edged sword, Arduino IDE makes development of firmware for these small controllers quite an easy process. This folder also contains the compiled binary of the firmware, should you not want to build it yourself.

## lib
These are the external libraries used for the badge. 
- The PN532 library originally failed to function with our hardware, so alterations were made to ensure successful communication could happen between two badges.
- The NeoPixel library is unchanged and allows the NeoPixels on the board to function
- The EEPROM2 library is actually a modified version of the built-in "EEPROM" library of the Pico SDK. For some strange reason, there does not appear to be any libraries that allow easy control over sectors of flash via the rp2040. The original EEPROM library works by using up to the last 4KB of flash and storing it into RAM, allowing for quick read/write before commiting and saving to the real flash. EEPROM2 allows for sector selection (starting from the last 4KB sector). While not very memory-safe, it works perfectly for firmware that does not take a lot of space or code that does not utilize much of the RAM in the rp2040. Using this library (and a little computer science logic) we were able to quickly store and retrieve 3 different 4KB data structures stored in flash.

## How to build
1. Download Arduino IDE here: https://www.arduino.cc/en/software
2. Install the RP2040 device/board files: https://learn.adafruit.com/rp2040-arduino-with-the-earlephilhower-core/installing-the-earlephilhower-core
3. Ensure the libraries included in 'lib' are placed in your local Arduino IDE library file location
 	- "EEPROM2" should replace your builtin "EERPOM" library. This one is a bit more tricky to find, but on Windows should be somewhere like: C:\Users\User\AppData\Local\Arduino15\packages\rp2040\hardware\rp2040\3.9.2\libraries\EEPROM
4. Connect your Pico to your device and be sure to select the board type of "Raspberry Pi Pico" inside of Arduino IDE