# ESPBleControler
This controler is a remote controler for my sdrberry project.
Based on ESP32 and ILI9341 display (optical) encoders.
The ESP32 acts as a BLE server, where the radioberry sdrberry is a BLE client.
The protocol is still very basic/

This source code is still in development.  
Goal is to support Adalm pluto SDR and Radioberry SDR but it should also support other SDR receivers based on SoapySDR.
Also the goal is to support optical encoder and support I2C / serial interface for bandpass filtering
Currently it also supports basic BLE as controler input. I use a ESP32 as BLE server with a service for an optical encoder.

Code can be compiled using VisualStudio 2019 with arduino support or Arduino IDE

ToDo:
- Test touch interface
- complete GUI design

Done:
- Radioberry support
- BLE Server

Installation of libraries is necessary:
- lvgl v8.11
- AceButton
- ESP32Encoder
- TFT_eSPI

https://www.youtube.com/watch?v=BMJiv3YGv-k
