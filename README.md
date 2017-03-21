# android-nfc-http-tunnel

Sample code for blog post. Contains the following:

- Android app that uses Host Card Emulation (NFC) to communicate with an NFC terminal
- libnfc-based terminal interfacing with a PN532 reader connected to a RaspberryPi via SPI

The app can provide a URL to be downloaded by the terminal. The terminal will download the URL and send its contents as a series of packets back to the app.
