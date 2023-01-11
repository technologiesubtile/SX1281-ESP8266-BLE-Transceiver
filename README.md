# SX1281-ESP8266-BLE-Transceiver

Development of a BLE 4.2 transceiver and a partial bluetooth stack based on the Semtech SX1281 transceiver module

Heinrich Diesinger, 
Univ Lille, Univ Polytech Hauts France, CNRS, Centrale Lille, Junia, Inst Elect Microelect & Nanotchnol (IEMN) UMR 8520, F-59652 Villeneuve d'Ascq, France

Abstract

Bluetooth low energy (BLE) is a communication protocol for low power applications that require longlasting operation in battery powered applications at low weight and size (typical: smartwatches, sensors, drones, cargo tracking). With baud rates betweeen 125 kbps and 2 Mbps it is located between large bandwidth (WiFi) short range, and low bandwidth long range (LoRa, GFSK), FLRC being in the middle. In the sensitivity performance, FLRC and LoRa are closer to the Shannon bound than the 3 others due to the use of spread spectrum techniques.
Another interesting aspect of GFSK based BLE protocol is the number of device classes or profiles and the option by definition of the standard to display data beyond the device name or SSID already in the advertisement mode (previous BT versions: discovery) in the preview, prior to the handshake and pairing. Therefore, BLE devices are able to display for example a reduced set of sensor data to the public in the preview, or transmit public messages prior to the pairing process involving cryptography.

Requirements

For research purposes (sensitivity performance, protocol immunity, Doppler or carrier frequency offset stability), we require a BLE transceiver that can communicate at the standard 1 Mbps and reduced baud rate of 125 kbps. BLE 4.2 radios are now widely available but often lacking the 125 kbps rate. The Semtech SX128x chip seems to offer it at low cost and ease of use e.g. compared to the Nordic Semiconductor modules that come with their own IDE or an Arduino core that lacks advanced functionality. Moreover, the SX128x module is a most versatile 2.4 GHZ transceiver  that features, in addition to basic GFSK and GFSK based BLE, also the modes FLRC, 2.4 GHz LoRa and in the 1280 version also a ranging engine. While GFSK, FLRC and LoRa stacks are implemented in hardware, BLE 4.2 compatibility is implemented only up to the communication layer.
We preferred this module despite the absence of the stack since for the intended characterization, a partial stack implementation below handshake would readily enable the advertisement mode. A first approach of testing it with available libraries (Stuart Robinson, Sandeep Mistry, Radiohead McCaulny, RadioLib) was disappointing because either the communication could not be established at all in BLE (authors focusing on LoRa ?) and/or some parameters (sync word aka access address) were not accessible through the library.
In the end, the decision was to abandon available libraries and to write our own library based on the commands given by the manufacturer datasheet [https://semtech.my.salesforce.com/sfc/p/#E0000000JelG/a/2R000000HoCb/X1yTNr5aeVlmviwNhvHyX9a2wSTSla.JWmnEtvAAlRk], using only the standard SPI library to transfer machine code and register values over the port.

Hardware

The aim is a transceiver that can be controlled over serial USB from a computer, tablet or smartphone and combines a radio module and a microcontroller, also enabling to run autonomously and store own code and some data. Whereas a choice of boards (Lillygo, TTgo etc.) are available that combine an ESP32 with a SX1278 LoRa radio, the SX128x can up to now (mai 2022) not be purchased on a similar board (susceptible of changing rapidly...). In principle it would also be possible to control a SX128x over serial port solely by a usb to serial bridge since its novelty is the ability to pass commands either via SPI or RS232, but this would require software on the host computer to be used ergonomously and preclude running code on the board. Our choice is to build our own experimental platform while a commercially available solution is still unavailable. Since the ESP32 is oversized for basic control of the SX128x, we opt for a ESP8266 D1 mini module. A requirement for us is a 50 Ohm output for connecting external antennas and measurment gear, rather than a PCB inverted F antenna often seen on bluetooth and WiFi modules. A SX1281 module in a can package by niceRF has a solder pad antenna output (neither IFA nor IPEX connectors).


The board front and back

![image](https://user-images.githubusercontent.com/96028811/211768814-5b886371-02ec-4cfe-bb24-30fe8d40f451.png)   ![image](https://user-images.githubusercontent.com/96028811/211768846-309d72ba-1946-471b-86a9-14424868efc6.png)

PCBoard design

A pcboard is made based on a previous [https://github.com/technologiesubtile/LoRa-MQTT-Telnet-Terminal-Gateway] LoRa board with minor adaptations to account for different pinout on the same footprint as the Ra-02 case. It is a single layer double sided component design that holds the D1 mini and SX128x on opposite sides. There are two more connections, DIO1 shall be connected in case interrupts are mapped to an output and the Busy connector is additional compared to the LoRa board. These two pins are connected to the nearby SDA and SCL pins of the Wemos respectively, meaning the I2C port becomes unusable and we sacrify the option of connecting sensors to it. The antenna is a normal solder pad and we route it with a cm of (approximative...) coplanar waveguide towards an external sma edge connector. 
This setup was obtained with minor changes from a previous LoRa board. The SX128x module is located between the solder pad rows of the D1 mini DIL package. PCBoard design has the minor drawback that lanes pass underneath some unused solder pads of the SX128x module that must be isolated, e.g. by putting some mica or insulating tape underneath the module. At large scale production, solder stop mask could do the job. Compared to the previous board, an external voltage regulator revealed to be unnecessary because the radio draws less power and can be hooked to the LDO regulator even of the budget clones of the D1 mini.


Circuit Diagram

![image](https://user-images.githubusercontent.com/96028811/211768973-d0315c1c-28d2-4c60-9df6-42ef8adbf63f.png)

The code

is programmed in Arduino IDE with the ESP8266 core. The code is commented and self explaining. The name of the functions are close to the ones used in the datasheet [https://semtech.my.salesforce.com/sfc/p/#E0000000JelG/a/2R000000HoCb/X1yTNr5aeVlmviwNhvHyX9a2wSTSla.JWmnEtvAAlRk]. Features are the ability to enter PDUs (protocol data unit) by entering a space separated list of hex values or by ascii, and to form packets by calculating a header which the hardware does not perform for BLE since it is only hardware-compatible. The same way, byte arrays for configuration registers can be entered. Configuration data and the output packet can be stored on EEPROM to be loaded at the next power up. If EEPROM is absent, the hardcoded defaults will be used instead. The serial interface uses the settings 9k6 8N1. At startup a help screen is displayed. It can be re-displayed by typing "help".


The help screen

![blehelpscreen](https://user-images.githubusercontent.com/96028811/211929275-8f953076-6a45-44f4-82b1-80d7f84f1971.jpg)


A bit of Syntax for the basic usage

Let us assume that we want to generate the BLE advertisement package as described by the sample 2 of the document
https://docs.silabs.com/bluetooth/latest/general/adv-and-scanning/bluetooth-adv-data-basics#example
It means we have a list of hex values from which we want to generate an advertisement packet. We first define the payload by typing

blepayload 0x02 0x01 0x06 0x11 0x07 0x07 0xb9 0xf9 0xd7 0x50 0xa4 0x20 0x89 0x77 0x40 0xcb 0xfd 0x2c 0xc1 0x80 0x48 0x09 0x08 0x42 0x47 0x4d 0x31 0x31 0x31 0x20 0x53

Next, to generate an advertisement package from that payload, we type

makebleadv

this adds a header depending on the different settings (outpdutype....outmd) but let's leave the defaults for now.

to transmit it over the air, type

sendoutpdutotal

Later on, if we want to transmit the packet periodically, we define an interval <> 0 by setting it e.g. to 5 seconds:

intvl 5000

and telling our board to do a transmission every time the interval is due, otherwise it would only receive and display the result every interval.

txflag 1

the loop does reception by default and interrogates the receive buffer also every interval.

Packet received:

Upon packet reception, it is displayed along with the length, PDUtype from the header, the present frequency, RSSI, and the checksum :


![blepacketreceived](https://user-images.githubusercontent.com/96028811/211929617-428b98b5-8512-4edb-8f15-b8b4a3e3fb11.jpg)
