/*
   Semtech SX1281 own library hdi
   This sketch uses the library SPI.h instead of a specific radio library and allows arbitrary access to
   registers and buffers of SPI peripherals. Its main functions are to create a byte array by the command
   spiupload [0x12 0x34 0x56 ..] that writes arbitrary bytes, entered as 2-digit hex numbers,  into the 
   array uploadbytearray; in a next step, the uploadbytearray is transfered to the peripheral using the 
   function spitransfer, which simultaneously fills the array downloadbytearray during the SPI transaction.
   The downloadbytearray is then displayed in 3 ways, 1) as 2-digit hex, 2) as ascii, and 3), as status byte
   doublets since the status of the transceiver is given as 2 3-bit values, 7:5 and 4:2 ranging from 0 to 6
   according to page 73 of the datasheet of the SX128x.
   Own functions have been implemented according to the SX1281 datasheet, but the functions are void and have 
   no arguments; they use the global variables instead.
*/

// variable declarations
// serial input handler
const byte numchars = 80;
const byte numascihex = 255;
char message[numascihex] = "";
boolean newdata = false;  // whether the reception is complete
int ndx =0; // this is used both in LoRa and serial reception, seems to work
unsigned long lastchar;
char rc;

// the payload as char array and its length
char blemessage[38];
char spimessage[numchars];
byte paylen = 31; //payload length for either type, must be initialized to number of elements in outppdutotal -2

// buffer for received msgs and command acknowledgments
char outmessage[numascihex];
char* outmessptr = outmessage;
char *messptr = outmessage;
int cur_len; // current length of charray
char interimcharray[20]; // used for temporary stuff with dtostrf()

// variables for outgoing packages, advertisement and data:
// int16_t SX128x::setAccessAddress  ( uint32_t  addr  ) 
byte outpdutotal[numchars] = {0x70, 0x88, 0x02, 0x01, 0x06, 0x11, 0x07, 0x07, 0xb9, 0xf9, 0xd7, 0x50, 0xa4, 0x20,
 0x89, 0x77, 0x40, 0xcb, 0xfd, 0x2c, 0xc1, 0x80, 0x48, 0x09, 0x08, 0x42, 0x47, 0x4d, 0x31, 0x31, 0x31, 0x20, 0x53}; //init paylen !!
byte outpduhdr[2] ;
byte outpayload[numchars];
// advertisement specific:
char charoutpdutype = '7';
byte outpdutype = 7;
byte outrfua =0; // rfu "reserved for future use"
char charouttx = '0';
char charoutrx = '0';
bool outtx = false;
bool outrx = false;
byte outrfub = 0;
// and data specific:
char charoutllid = '0'; // 0 to 3, 2 bits
byte outllid;
char charoutnesn = '0'; // 0 or 1
char charoutsn = '0';
char charoutmd = '0';
bool outnesn, outsn, outmd = false;
byte outrfuc = 0;
byte outrfud = 0;

// variables for register up and download by spitransfer()
byte uploadbytearray[numchars];
byte downloadbytearray[numchars];
byte xfersiz = 0;
byte state;

// conversion twodigithextobyte
byte entirebyte;
byte upperhalfbyte;
byte lowerhalfbyte;
char charhex[3];

// conversion twodigithextobytearray()
byte arraysiz = 0;
byte resultbytearray[numchars];
char resultchararray[numascihex];

// variables for inbound messages // for the moment the pdu will be displayed as a whole w/o interpreting its header
byte inpdutotal[numchars];

// misc
char charintvl[8] = "5000";
long intvl = 5000; // repeat something every n second
long lastSendTime = 0;        // last send time for recurrent sending
int i = 0;
long pllsteps = 12098953;
char charscanflag = '0';
char chartxflag = '1';
bool scanflag = 0;
bool txflag = 1;
float rssi = 0;
byte rpaylen = 0; // length of received payload
byte rpdutype = 0; // PDUtype of received payload

// radio settings:
char packetchartype[3]="04";
byte packetbytetype = 0x04;
char charfreq[7] = "2402.0"; //freq in MHz
//char charbitrate[5] = "125"; //bitrate in kBps
//char chardeviation[7] = "400.0"; // displacement in kHz -> float
char charpower[4] = "13"; // power in dBm ranging from -18 to 13
//char charshape = '2'; // 1 to 5 integer
float freq = 2402.0;
//uint16_t bitrate = 125;
//float deviation = 400.0;
//int8_t power =13;
//uint8_t shape = 2; // allowed 0_5, 1_0, or disabled, aka 2, 4, 0
char txparchararray[6] = "1f:80"; // power 0x1f = 31, -18 -> 13 dBm , ramptime list p.88 10 us
char syncchararray[45] = "00:8e:89:be:d6:00:00:00:00:00:00:00:00:00:00"; // 3 syncwords for gfsk a 5 bytes; 1 syncword a 4 bytes BLE
// 0x8E89BED6 for advertisement packets thesis gonce p. 13
char modparchararray[9] = "45:01:20"; // 3 mod params bitrate/BW, mod_indx, gauss_shaping
 // 1 Mbps, 1.2 MHz BW, p.113 BLE-specific
 // Mod Ind 0.5 [0.35 .. 4] p. 105, p. 114 BLE-specific BT 0.5
 // filtering BT_0_5 p. 106, p. 114 BLE-specific
 // for 125k, change the first one to 0xef, p. 105
char packparchararray[21] = "80:10:18:00:00:00:00"; // 7 packet params, 4 for BLE: conn-state, crc-length, testpayload, whitening
// paylaod length max 80 bytes, p.114 BLE-specific
// CRC 3 bytes p. 114 BLE-specific
// 00001111 test payload, p. 114 BLE-specific
// whitening enable, p. 114 BLE-specific
// remaining packet params must be set to zero and sent to radio
char irqmaskchararray[12] = "00:00:00:00"; // irqmask, dio1mask, dio2mask, dio3mask
char buffbasechararray[6] = "80:20"; // tx/rx base addresses
char autotxchararray[6]= "00:5c"; // auto tx after delay p.84, 120
// same in byte array version:
byte txparbytearray[2]={0x17, 0x80};
byte syncbytearray[15]={0x00, 0x8e, 0x89, 0xbe, 0xd6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
byte modparbytearray[3]={0x45, 0x01, 0x20};
byte packparbytearray[7]={0x80, 0x10, 0x18, 0x00, 0x00, 0x00, 0x00};
byte irqmaskbytearray[4]={0x00, 0x00, 0x00, 0x00};
byte buffbasebytearray[2]={0x80, 0x20};
byte autotxbytearray[2]={0x00, 0x5c};
// for legacy purpose leave the function and variables for the moment:
// char charoutaccadr[11] = "305419896";
// uint32_t outaccadr = 0x12345678;


// battery measurement:
int battlevel;
float voltf;
float warnthreshold = 3.3;
float protectthreshold = 2.9;
 

// eeprom stuff
const long nvlength = 256;
char shadoweeprom[nvlength];
// char shadowshadow[nvlength]; // security copy unaltered by strtoktoken
char checker[5] = "";
char *strtokIndx;
char charpaylen[5];


// include WiFi only to turn off the transceiver for saving current and avoiding interference to 2.4 GHz BLE
#include <ESP8266WiFi.h>      // libraries for wifi
#include <EEPROM.h>           // eeprom
#include <SPI.h>         // generic SPI library instead of RadioLib

// SX1280 has the following connections: NSS DIO1 NRST BUSY = 15 4 16 5
// SX1280 radio = new Module(10, 2, 3, 9);
// SX1280 radio = new Module(15, 4, 16, 5); // modif hdi for the Wemos D1 mini board

// pin definitions SX... GPIO
#define NSS 15
#define BUSY 5
#define DIO1 4
#define NRESET 16




void publish() {
  Serial.println(outmessage); // for the moment we do not plan running telnet or MQTT over WiFi to prevent
  // spectrum congestion at 2.4 HGz
}



void helpscreen() {
  strcpy(outmessage, ""); publish();
  strcpy(outmessage, "SX128x utilities with own functions as suggested by datasheet"); publish();
  strcpy(outmessage, "and direct SPI access to Opcodes/registers/buffers"); publish();
  strcpy(outmessage, "By heinrich.diesinger@cnrs.fr, F4HYZ"); publish();
  strcpy(outmessage, "Usage: type commands without prefix"); publish();
  //strcpy(outmessage, "to send a message, first set it, e.g. blemessage hello world !"); publish();
  //strcpy(outmessage, "then sendbleadv to send as advertisement, or sendbledat as data"); publish();
   
  /*
   * strcpy(outmessage, "to enter a command, it must be preceded by AT+, e.g. AT+targetip 192.168.1.101"); publish();
  strcpy(outmessage, "to send a command as msg w/o executing, use verbatim prefix: \\verb AT+batlevel"); publish();
  strcpy(outmessage, "to relay msg or cmd the same side of gateway, use relay prefix: \\relay text"); publish();
  strcpy(outmessage, "anything else is considered a message and cross transferred local/LoRa"); publish();
  strcpy(outmessage, "command acknowledgements bounce to the side (local/LoRa) from where cmd issued"); publish();
  strcpy(outmessage, "system msgs, warnings go to the side (local/LoRa) from where last cmd issued"); publish();
  strcpy(outmessage, "WiFi related settings: "); publish();
  strcpy(outmessage, "setupwifi reinitializes wifi to apply changed parameters; serial ack only"); publish();
  strcpy(outmessage, "wifimode ap/sta : access point or wifi client <sta>"); publish();
  strcpy(outmessage, "ssid: for either wifi mode"); publish();
   delay(500);  // not saturate the serial buffer
  strcpy(outmessage, "password: for either wifi mode"); publish();
  strcpy(outmessage, "localmode tns/tnc/mqtt: telnet server/client, mqtt client <mqtt>"); publish();
  strcpy(outmessage, "Telnet related settings: "); publish();
  strcpy(outmessage, "setuptelnet reinitializes telnet to apply changed parameters"); publish();
  strcpy(outmessage, "targetip: ip for either telnet or mqtt <192.168.4.1>"); publish();
  strcpy(outmessage, "targetport: port for either telnet or mqtt <1883>, must be 23 for mqtt"); publish();
  strcpy(outmessage, "MQTT related settings: "); publish();
  strcpy(outmessage, "intopic: topic to subscribe to <misc>"); publish();
  strcpy(outmessage, "outtopic: topic to publish to <misc>"); publish();
  strcpy(outmessage, "mqttconnect: initialize MQTT connection"); publish();
 */
  strcpy(outmessage, "System commands: "); publish();
  strcpy(outmessage, "batlevel: shows the supply voltage"); publish();
  strcpy(outmessage, "help: displays this info again"); publish();
  // strcpy(outmessage, "payload 1/0: switches the open drain power output"); publish();
  strcpy(outmessage, "eepromstore: stores the settings to EEPROM (must be called manually)"); publish();  // eeprom
  strcpy(outmessage, "eepromdelete: clears EEPROM"); publish(); // eeprom
  strcpy(outmessage, "eepromretrieve: reads settings from EEPROM"); publish(); // eeprom
  strcpy(outmessage, "reboot: restarts the firmware"); publish();
  // strcpy(outmessage, "shutdown: suspends processor and LoRa power"); publish();
 /* strcpy(outmessage, "System messages: startup msgs, battery warning, message received "); publish();
  strcpy(outmessage, "tncontext 1/0: enables system messages on telnet terminal <1>"); publish();
  strcpy(outmessage, "sercontext 1/0: enables system messages on serial terminal <1>"); publish();
  delay(1000); // not saturate the serial buffer
  strcpy(outmessage, "lorcontext 1/0: enables system messages on LoRa terminal <1>"); publish(); */
  delay(1000); // not saturate the serial buffer
  
  strcpy(outmessage, "Radio related settings: "); publish();
  //strcpy(outmessage, "linkstrength: returns RSSI and SNR"); publish();
  //strcpy(outmessage, "loclora sets the local device address byte (hex) <0xbd>"); publish();
  //strcpy(outmessage, "destlora sets the destination device address byte (hex) <0xbe>"); publish();
  //strcpy(outmessage, "promiscuous 1/0 sets LoRa promiscuous mode <1> "); publish();
  strcpy(outmessage, "packettype <0x04>: sets mode (GFSK, LoRa, range, FLRC, BLE)"); publish();
  strcpy(outmessage, "freq <2402.0>: sets the LoRa frequency in MHz [2400..2500]"); publish();
  strcpy(outmessage, "txparams <0x1f 0x80>: sets LoRa power in dBm + 18 [0..31] and ramptime (manual)"); publish();
  strcpy(outmessage, "power <13>: sets RF power in dBm "); publish();
  strcpy(outmessage, "syncarray <0x00 0x8e 0x89 0xbe 0xd6 0x00 0x00 0x00 ...>: acc addr or sync words"); publish();
  strcpy(outmessage, "modparams <0x45 0x01 0x20>: baud/BW, mod_indx, gauss_fit"); publish();
  strcpy(outmessage, "packparams <0x80 0x10 0x18 0x00 0x00 0x00 0x00>: con_st, crc, test, whiten"); publish();
  strcpy(outmessage, "irqmask <0x00 0x00 0x00 0x00>: irq masks interrupt, dio1, dio2, dio3"); publish();
  strcpy(outmessage, "buffbase <0x20 0x80>: tx/rx buffer base addresses"); publish();
  strcpy(outmessage, "autotx <0x00 0x5c>: auto tx delay required by BLE"); publish();
  
  strcpy(outmessage, "setupble: manually applies the BLE radio settings "); publish();
  strcpy(outmessage, "required at startup and to apply changed settings !!!"); publish();
  strcpy(outmessage, "setrffreq: manually applies the freq only "); publish();
  strcpy(outmessage, "setrftxparams: manually applies power, ramptime only "); publish();
  
  // strcpy(outmessage, "bitrate <125>: sets bitrate in kBps [125, 250, 400, 500, 800, 1000, 1600 and 2000]"); publish();
  // strcpy(outmessage, "shape <2>: sets the shaping factor [0, 2, 4 = disable, 0.5, 1.0]"); publish();
  // strcpy(outmessage, "deviation <400.0>: sets the frequency displacement in kHz [0..3200]"); publish();
  
  delay(1000); // not saturate the serial buffer
  
  strcpy(outmessage, "Header (and pre-header) settings for computing outgoing packets: "); publish();
  // strcpy(outmessage, "outaccadr <305419896>: sets the access adress [0 to ff:ff:ff:ff] enter decimal"); publish();
  strcpy(outmessage, "outpdutype <7>: [0..8]"); publish();
  strcpy(outmessage, "outtx <0>: [0, 1]"); publish();
  strcpy(outmessage, "outrx <0>: [0, 1]"); publish();
  strcpy(outmessage, "outllid <0>: [0..3]"); publish();
  strcpy(outmessage, "outnesn <0>: [0, 1]"); publish();
  strcpy(outmessage, "outsn <0>: [0, 1]"); publish();
  strcpy(outmessage, "outmd <0>: [0, 1]"); publish();
  
  delay(1000); // not saturate the serial buffer
  
  strcpy(outmessage, "Payload, PDU and SPI raw data input and transmitting: "); publish();
  strcpy(outmessage, "blemessage  < >: [any text] sets blemessage char array and "); publish();
   strcpy(outmessage, "      converts it to outpayload byte array (6..37 adv, 0..31 dat)"); publish();
  strcpy(outmessage, "blepayload: [0x12 0x34 0x56 ..] creates outpayload byte array from user entered 2 digit hex list"); publish();
  strcpy(outmessage, "makebleadv: from outpayload, calculates header for advertising packet and composes outpdutotal"); publish();
  strcpy(outmessage, "makebledat: from outpayload, calculates header for data packet and composes outpdutotal"); publish();
  strcpy(outmessage, "sendoutpdutotal: transmits outpdutotal over the air"); publish();
  // strcpy(outmessage, "spimessage: [any text] sets spimessage char array and "); publish();
  // strcpy(outmessage, "      converted outpayload byte array (0..80)"); publish();
  strcpy(outmessage, "spiupload:  [0x12 0x34 0x56 ..] creates uploadbytearray from user entered 2 digit hex list"); publish();
  strcpy(outmessage, "ATTENTION this gets overwritten by loop activity if intvl is set"); publish();
  strcpy(outmessage, "spitransfer: transfers uploadbytearray to radio transceiver, retrieves downloadbytearray "); publish();
  strcpy(outmessage, "        and displays it as hex, ascii and decomposed (upper/lower 3 bit) status bytes"); publish();
  delay(1000); // not saturate the serial buffer
  
  strcpy(outmessage, "Radio control commands: "); publish();
  strcpy(outmessage, "clearirqstatus: clears IRQ, needed before settx etc."); publish();
  strcpy(outmessage, "setstandby: sets the radio to standby RC 13 MHz"); publish();
  strcpy(outmessage, "setrx: sets the radio to single receive mode"); publish();
  strcpy(outmessage, "settx: sets the radio to single transmit mode"); publish();
  strcpy(outmessage, "setcw: sets radio to transmit a carrier"); publish();
  strcpy(outmessage, "getstatus: gets radio status"); publish();

  strcpy(outmessage, "Loop related: "); publish();
  strcpy(outmessage, "intvl <5000>: repeat intvl for rx tx in ms; inactive if 0"); publish();
  strcpy(outmessage, "scanflag <0>: if set, freq steps through advertisement channels"); publish();
  strcpy(outmessage, "txflag <1>: if set, settx() is issued each intvl"); publish();
  
  strcpy(outmessage, ""); publish();
}



byte parse_char(char c) //needed by twodigithextobyte() functiom
// convert a hex symbol to byte
{ if ( c >= '0' && c <= '9' ) return ( c - '0' );
  if ( c >= 'a' && c <= 'f' ) return ( c - 'a' + 10 );
  if ( c >= 'A' && c <= 'F' ) return ( c - 'A' + 10 );
  // if nothing,
  return 16;
  // or alternatively
  //  abort()
}


bool twodigithextobyte() {
  // convert the adress char array into a byte and compare with the local device adreess
  if (strlen(charhex) == 2)
  { // convert the two hex characters into integers from 0 to 15, 16 if not a hex symbol
    upperhalfbyte = parse_char(charhex[0]);
    lowerhalfbyte = parse_char(charhex[1]);
    if ((upperhalfbyte == 16) || (lowerhalfbyte == 16))
    {
      strcpy(outmessage, "malformatted local address - either char is not a hex symbol");
      return false;
    }
    else
    { entirebyte = upperhalfbyte * 0x10 + lowerhalfbyte;
    return true;
    }
  }
  else
  {
    strcpy(outmessage, "malformed local address - length != 2");
    return false;
   }
}



void twodigithextobytearray() {
// used by setblepayload() and setspiupload()
// requires strtokIndx to be set to begin of the 2 digit hex chars and next space replaced by \0
arraysiz=0;
strcpy(resultchararray, "");
  while (strtokIndx != NULL)
  {
    // debug
    //Serial.println("in while strotokIndx != NULL loop");
    //Serial.print("arraysiz= ");  Serial.println(arraysiz);
     
  messptr = strtokIndx + 2; // omit "0x"
  strncpy(charhex, messptr, 2);
  //strcat(charhex, "\0"); //terminate
  
  // debug
  // Serial.print("charhex= ");  Serial.println(charhex);
  
  if (!twodigithextobyte()) {
  publish(); //ouput error message from twodigithextobyte() and return
  return;
  }
  resultbytearray[arraysiz]=entirebyte;
  strcat(resultchararray, charhex);
  strcat(resultchararray, ":");

  // debug
  // Serial.print("result [arraysiz]= ");  Serial.println(entirebyte);
  
  arraysiz++ ;
  strtokIndx = strtok(NULL, " ");
  }
  // after the last, remove ":"
  resultchararray[strlen(resultchararray)]='\0';
} // end  twodigithextobytearray()




void twodigcolhextobytearray() { // put before eepromretrieve; similar to twodigithextobytearray() but expecting : separator no 0x
  // used by eepromretrieve to set syncbytearray, modparbytearray, packparbytearray from their respective char array versions
  arraysiz=0;
strcpy(resultchararray, "");
  while (strtokIndx != NULL)
  {
    // debug
    //Serial.println("in while strotokIndx != NULL loop");
    //Serial.print("arraysiz= ");  Serial.println(arraysiz);
     
  // messptr = strtokIndx + 2; // omit "0x"; in the : separated verison there is no 0x
  strncpy(charhex, strtokIndx, 2);
  //strcat(charhex, "\0"); //terminate
  
  // debug
  //Serial.print("charhex= ");  Serial.println(charhex);
  
  if (!twodigithextobyte()) {
  publish(); //ouput error message from twodigithextobyte() and return
  return;
  }
  resultbytearray[arraysiz]=entirebyte;
  strcat(resultchararray, charhex); // make a col version from the col version, isnt't it ??
  strcat(resultchararray, ":"); // but keep it this way for max similarity with twodigithextobytearray

  // debug
  // Serial.print("result [arraysiz]= ");  Serial.println(entirebyte);
  
  arraysiz++ ;
  strtokIndx = strtok(NULL, ":");
  }
  // after the last, remove ":"
  resultchararray[strlen(resultchararray)]='\0';
} // end  twodigcolhextobytearray()




void eepromstore() //concatenates all setup data in a csv to the shadoweeprom, writes 1 by 1 into eeprom, and commit()
{
  strcpy(shadoweeprom, "99,");
  strcat(shadoweeprom, packetchartype); // it is a char array because we prepare for it to be byte range > 0..9
  // although it is 0 to 4, >=5 RFU
  strcat(shadoweeprom, ",");
  strcat(shadoweeprom, charfreq);
  strcat(shadoweeprom, ",");
  strcat(shadoweeprom, txparchararray); //power, ramp
  strcat(shadoweeprom, ",");
  strcat(shadoweeprom, syncchararray); // sync word in gfsk, LoRa, = access adress in BLE
  strcat(shadoweeprom, ",");
  strcat(shadoweeprom, modparchararray); // modulation parameters
  strcat(shadoweeprom, ",");
  strcat(shadoweeprom, packparchararray); // packet parameters
  strcat(shadoweeprom, ",");
  strcat(shadoweeprom, irqmaskchararray); // irq mask parameters
  strcat(shadoweeprom, ",");
  strcat(shadoweeprom, buffbasechararray); // irq mask parameters
  strcat(shadoweeprom, ",");
  strcat(shadoweeprom, charintvl);
  strcat(shadoweeprom, ",");
  sprintf(charpaylen, "%u", paylen);
  strcat(shadoweeprom, charpaylen);
  strcat(shadoweeprom, ",");
  // now the single char byte-booleans:
  cur_len = strlen(shadoweeprom);
  shadoweeprom[cur_len] = charoutpdutype;
  shadoweeprom[cur_len + 1] = ',';
  shadoweeprom[cur_len + 2] = '\0';
  cur_len += 2;
  cur_len = strlen(shadoweeprom);
  shadoweeprom[cur_len] = charouttx;
  shadoweeprom[cur_len + 1] = ',';
  shadoweeprom[cur_len + 2] = '\0';
  cur_len += 2;
  shadoweeprom[cur_len] = charoutrx;
  shadoweeprom[cur_len + 1] = ',';
  shadoweeprom[cur_len + 2] = '\0';
  cur_len += 2;
  cur_len = strlen(shadoweeprom);
  shadoweeprom[cur_len] = charoutllid;
  shadoweeprom[cur_len + 1] = ',';
  shadoweeprom[cur_len + 2] = '\0';
  cur_len += 2;
  cur_len = strlen(shadoweeprom);
  shadoweeprom[cur_len] = charoutnesn;
  shadoweeprom[cur_len + 1] = ',';
  shadoweeprom[cur_len + 2] = '\0';
  cur_len += 2;
  shadoweeprom[cur_len] = charoutsn;
  shadoweeprom[cur_len + 1] = ',';
  shadoweeprom[cur_len + 2] = '\0';
  cur_len += 2;
  shadoweeprom[cur_len] = charoutmd;
  shadoweeprom[cur_len + 1] = ',';
  shadoweeprom[cur_len + 2] = '\0';
  cur_len += 2;
  shadoweeprom[cur_len] = chartxflag;
  shadoweeprom[cur_len + 1] = ',';
  shadoweeprom[cur_len + 2] = '\0';
  cur_len += 2;
  shadoweeprom[cur_len] = charscanflag;
  shadoweeprom[cur_len + 1] = ',';
  shadoweeprom[cur_len + 2] = '\0';
  cur_len += 2;
  for (int i = 0; i < paylen+2; ++i) {
  shadoweeprom[cur_len + i] = (char)outpdutotal[i];
  }
  
  strcpy(outmessage, "shadoweeprom is: "); strcat(outmessage, shadoweeprom);
  // Serial.println(outmessage);  
  publish();

  for (int i = 0; i < nvlength; ++i)
  {
    EEPROM.write(i, shadoweeprom[i]);
    if (shadoweeprom[i] = '\0') break; // stop at the termination
  }
  
  if (EEPROM.commit()) strcpy(outmessage, "EEPROM successfully committed");
    
  else strcpy(outmessage, "writing to EEPROM failed");

}




void eepromretrieve() { // reads all eeprom into shadoweeprom 1 by 1, split into token by strtok to charfreq etc, converts by atoi
  // write eeprom into shadow eeprom buffer
  char eeprchar;
  for (int i = 0; i < nvlength; i++) {
  eeprchar = char(EEPROM.read(i));
  shadoweeprom[i] = eeprchar;
  if (eeprchar = '\0') break; }
  // since strtok destroys the charray, make a copy of it
  //strcpy(shadowshadow, shadoweeprom);
  
  strcpy(outmessage, "EEPROM content: \r\n");
  strcat(outmessage, shadoweeprom);
  strcat(outmessage, "\r\n");
  
 
  // decompose it into the char versions of the parameters
  strtokIndx = strtok(shadoweeprom, ",");
  if (strtokIndx != NULL) strncpy(checker, strtokIndx, 2); // add checking mechanism if eeprom has ever been written,
  // and if not, return from the fct to use hardcoded defaults
  else strcpy(checker, "");
  if (strstr(checker, "99")) {  // only if "checksum" is ok, continue decomposing
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL) strcpy(packetchartype, strtokIndx); // it is a 1 emenent char array that contains a charhex
    // it can theoretically go from 0..255 because 0..4 is not enough; we could have done a single char as charpdutype
    else strcpy(packetchartype, "");
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL) strcpy(charfreq, strtokIndx);
    else strcpy(charfreq, "");
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL) strcpy(txparchararray, strtokIndx);
    else strcpy(txparchararray, "");
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL) strcpy(syncchararray, strtokIndx);
    else strcpy(syncchararray, "");
    strtokIndx = strtok(NULL, ",");
     if (strtokIndx != NULL) strcpy(modparchararray, strtokIndx);
    else strcpy(modparchararray, "");
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL) strcpy(packparchararray, strtokIndx);
    else strcpy(packparchararray, "");
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL) strcpy(irqmaskchararray, strtokIndx);
    else strcpy(irqmaskchararray, "");
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL) strcpy(buffbasechararray, strtokIndx);
    else strcpy(buffbasechararray, "");
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL) strcpy(charintvl, strtokIndx);
    else strcpy(charintvl, "");
     strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL) strcpy(charpaylen, strtokIndx);
    else strcpy(charintvl, "");
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL)
    charoutpdutype = strtokIndx[0]; // ! single charcter
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL)
    charouttx = strtokIndx[0]; // ! single charcter
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL)
    charoutrx = strtokIndx[0]; // ! single charcter
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL)
    charoutllid = strtokIndx[0]; // ! single charcter
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL)
    charoutnesn = strtokIndx[0]; // ! single charcter
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL)
    charoutsn = strtokIndx[0]; // ! single charcter
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL)
    charoutmd = strtokIndx[0]; // ! single charcter
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL)
    chartxflag = strtokIndx[0]; // ! single charcter
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL)
    charscanflag = strtokIndx[0]; // ! single charcter
    // convert this first
    paylen = atoi(charpaylen);
    // cause it is required for determining how many chars to read next:
    strtokIndx = strtok(NULL, ",");
    if (strtokIndx != NULL) 
    for (int i = 0; i < paylen + 2; ++i) {
    outpdutotal[i]=(byte)strtokIndx[i];
  }
   

    //strtokIndx = strtok(NULL, "\0");
    // strtokIndx += 2; // advances srtokIndx to the last inserted '\0' 
    // without inserting another one if blemessage contains comma
    // if (strtokIndx != NULL) strcpy(blemessage, strtokIndx); // and copies to the end of the charray into blemessage
    //else strcpy(blemessage, "");  
     
    // and further convert it into non-char format with atoi(), atof(), twodigcolhextobytearray functions and the like:

   // procedure for column separated charhex lists kinda MAC address:  1a:2b:3c:4d
   // parameterchararray
   // strtokIndx = parameterchararray // if it has no : separators
   // strtokIndx = strtok(parameterchararray, ":"); // put it at the beginning and replace first : separator by \0
   // twodigcolhextobytearray();
   //for (int i = 0; i < arraylength; ++i) {
   //parameterbytearray[i] = resultbytearray[i];
   // }

   publish();
   strtokIndx = strtok(packetchartype, "\0");
   twodigcolhextobytearray();
   packetbytetype = resultbytearray[0];
   //Serial.println("packetchartype decoded");
   
 
   intvl = atoi(charintvl);
   freq = atof(charfreq);
  
   
   strtokIndx = strtok(txparchararray, ":");
   twodigcolhextobytearray();
   for (int i = 0; i < 2; ++i) {
   txparbytearray[i] = resultbytearray[i];
   }
   //Serial.println("txparams decoded");
   //publish();

   strtokIndx = strtok(syncchararray, ":");
   twodigcolhextobytearray();
   for (int i = 0; i < 15; ++i) {
   syncbytearray[i] = resultbytearray[i];
   }
   //Serial.println("syncarray decoded");
   //publish();

   strtokIndx = strtok(modparchararray, ":");
   twodigcolhextobytearray();
   for (int i = 0; i < 3; ++i) {
   modparbytearray[i] = resultbytearray[i];
   }
   //Serial.println("modparams decoded");
   //publish();
   
   strtokIndx = strtok(packparchararray, ":");
   twodigcolhextobytearray();
   for (int i = 0; i < 7; ++i) {
   packparbytearray[i] = resultbytearray[i];
   }
   //Serial.println("packparams decoded");
   //publish();

   strtokIndx = strtok(irqmaskchararray, ":");
   twodigcolhextobytearray();
   for (int i = 0; i < 4; ++i) {
   irqmaskbytearray[i] = resultbytearray[i];
   }
   //Serial.println("irqmask decoded");
   //publish();

   strtokIndx = strtok(buffbasechararray, ":");
   twodigcolhextobytearray();
   for (int i = 0; i < 2; ++i) {
   buffbasebytearray[i] = resultbytearray[i];
   }
   //Serial.println("buffbase decoded");
   //publish();

   outpdutype = charoutpdutype - '0'; // single char number, atoi() doesnt work
   // byte-booleans
   outtx = charouttx - '0'; // boolean; single characters cannot be converted by atoi() ! also works for single digit integers
   outrx = charoutrx - '0'; // bool
   outllid = charoutllid - '0'; // 0..3
   outnesn = charoutnesn - '0'; // bool
   outsn = charoutsn - '0'; // bool
   outmd = charoutmd - '0'; // bool
   scanflag = charscanflag - '0'; // bool
   txflag = chartxflag - '0'; // bool

   // texttopayload(); // convert blemessage char array into byte array

   // reset the "checksum"
   strcpy(checker, ""); //resetting the checking mechanism

   strcpy(outmessage, "hardcoded initial values successfully overwritten with EEPROM settings \r\n");
  
  }
  else // if checker not == 99
    strcpy(outmessage, "EEPROM empty, using factory defaults");

    // newdata = false;
    
}  // end eepromretrieve





void eepromdelete()
{
  for (int i = 0; i < nvlength; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  strcpy(outmessage, "EEPROM successfully deleted");
}




//outtx
void setouttx() {
  if (strstr(outmessage, "outtx ")) {
    strtokIndx = outmessptr + 6;
    if (strlen(strtokIndx) == 1) {
      //strcpy(chardestlorad, strtokIndx);
      charouttx = strtokIndx[0];
      strcpy(outmessage, "outtx bit set to : ");
      // cur_len = strlen(outmessage);
      // outmessage[cur_len] = charouttx;
      // outmessage[cur_len + 1] = '\0';
      outtx = charouttx - '0';
      // new: append the numerical value to output text to see if converted correctly.
      outmessptr += strlen(outmessage); // use pointer as cursor
      outmessptr += sprintf(outmessptr, "%u", outtx);
      outmessptr = outmessage; // reset to begin of char array
    } else {
      strcpy(outmessage, "error: argument too short or long");
    }
  } else {
    strcpy(outmessage, "outtx bit is: ");
    // cur_len = strlen(outmessage);
    // outmessage[cur_len] = charouttx;
    // outmessage[cur_len + 1] = '\0';
    // new: append the numerical value to output text to see if converted correctly.
    outmessptr += strlen(outmessage); // use pointer as cursor
    outmessptr += sprintf(outmessptr, "%u", outtx);
    outmessptr = outmessage; // reset to begin of char array
  }
}



//outrx
void setoutrx() {
  if (strstr(outmessage, "outrx ")) {
    strtokIndx = outmessptr + 6;
    if (strlen(strtokIndx) == 1) {
      //strcpy(chardestlorad, strtokIndx);
      charoutrx = strtokIndx[0];
      strcpy(outmessage, "outrx bit set to : ");
      // cur_len = strlen(outmessage);
      // outmessage[cur_len] = charoutrx;
      // outmessage[cur_len + 1] = '\0';
      outrx = charoutrx - '0';
      // new: append the numerical value to output text to see if converted correctly.
      outmessptr += strlen(outmessage); // use pointer as cursor
      outmessptr += sprintf(outmessptr, "%u", outrx);
      outmessptr = outmessage; // reset to begin of char array
    } else {
      strcpy(outmessage, "error: argument too short or long");
    }
  } else {
    strcpy(outmessage, "outrx bit is: ");
    // cur_len = strlen(outmessage);
    // outmessage[cur_len] = charoutrx;
    // outmessage[cur_len + 1] = '\0';
    // new: append the numerical value to output text to see if converted correctly.
    outmessptr += strlen(outmessage); // use pointer as cursor
    outmessptr += sprintf(outmessptr, "%u", outrx);
    outmessptr = outmessage; // reset to begin of char array
  }
}



//outnesn
void setoutnesn() {
  if (strstr(outmessage, "outnesn ")) {
    strtokIndx = outmessptr + 8;
    if (strlen(strtokIndx) == 1) {
      //strcpy(chardestlorad, strtokIndx);
      charoutnesn = strtokIndx[0];
      strcpy(outmessage, "outnesn bit set to : ");
      // cur_len = strlen(outmessage);
      // outmessage[cur_len] = charoutnesn;
      // outmessage[cur_len + 1] = '\0';
      outnesn = charoutnesn - '0';
      // new: append the numerical value to output text to see if converted correctly.
      outmessptr += strlen(outmessage); // use pointer as cursor
      outmessptr += sprintf(outmessptr, "%u", outnesn);
      outmessptr = outmessage; // reset to begin of char array
    } else {
      strcpy(outmessage, "error: argument too short or long");
    }
  } else {
    strcpy(outmessage, "outnesn bit is: ");
    // cur_len = strlen(outmessage);
    // outmessage[cur_len] = charoutnesn;
    // outmessage[cur_len + 1] = '\0';
    // new: append the numerical value to output text to see if converted correctly.
    outmessptr += strlen(outmessage); // use pointer as cursor
    outmessptr += sprintf(outmessptr, "%u", outnesn);
    outmessptr = outmessage; // reset to begin of char array
  }
}




//outsn
void setoutsn() {
  if (strstr(outmessage, "outsn ")) {
    strtokIndx = outmessptr + 6;
    if (strlen(strtokIndx) == 1) {
      //strcpy(chardestlorad, strtokIndx);
      charoutsn = strtokIndx[0];
      strcpy(outmessage, "outsn bit set to : ");
      // cur_len = strlen(outmessage);
      // outmessage[cur_len] = charoutsn;
      // outmessage[cur_len + 1] = '\0';
      outsn = charoutsn - '0';
      // new: append the numerical value to output text to see if converted correctly.
      outmessptr += strlen(outmessage); // use pointer as cursor
      outmessptr += sprintf(outmessptr, "%u", outsn);
      outmessptr = outmessage; // reset to begin of char array
    } else {
      strcpy(outmessage, "error: argument too short or long");
    }
  } else {
    strcpy(outmessage, "outsn bit is: ");
    // cur_len = strlen(outmessage);
    // outmessage[cur_len] = charoutsn;
    // outmessage[cur_len + 1] = '\0';
    // new: append the numerical value to output text to see if converted correctly.
    outmessptr += strlen(outmessage); // use pointer as cursor
    outmessptr += sprintf(outmessptr, "%u", outsn);
    outmessptr = outmessage; // reset to begin of char array
  }
}




//outmd
void setoutmd() {
  if (strstr(outmessage, "outmd ")) {
    strtokIndx = outmessptr + 6;
    if (strlen(strtokIndx) == 1) {
      //strcpy(chardestlorad, strtokIndx);
      charoutmd = strtokIndx[0];
      strcpy(outmessage, "outmd bit set to : ");
      //cur_len = strlen(outmessage);
      //outmessage[cur_len] = charoutmd;
      //outmessage[cur_len + 1] = '\0';
      outmd = charoutmd - '0';
    // new: append the numerical value to output text to see if converted correctly.
    outmessptr += strlen(outmessage); // use pointer as cursor
    outmessptr += sprintf(outmessptr, "%u", outmd);
    outmessptr = outmessage; // reset to begin of char array
    } else {
      strcpy(outmessage, "error: argument too short or long");
    }
  } else {
    strcpy(outmessage, "outmd bit is: ");
    //cur_len = strlen(outmessage);
    //outmessage[cur_len] = charoutmd;
    //outmessage[cur_len + 1] = '\0';
    // new: append the numerical value to output text to see if converted correctly.
    outmessptr += strlen(outmessage); // use pointer as cursor
    outmessptr += sprintf(outmessptr, "%u", outmd);
    outmessptr = outmessage; // reset to begin of char array
  }
}



//outllid
void setoutllid()
{
  if (strstr(outmessage, "outllid ")) {
    strtokIndx = outmessptr + 8;
    if (strlen(strtokIndx) == 1) {
      //strcpy(chardestlorad, strtokIndx);
      charoutllid = strtokIndx[0];
      strcpy(outmessage, "outllid (2bit) set to : ");
      //cur_len = strlen(outmessage);
      //outmessage[cur_len] = charoutllid;
      //outmessage[cur_len + 1] = '\0';
      outllid = charoutllid - '0';
      // new: append the numerical value to output text to see if converted correctly.
      outmessptr += strlen(outmessage); // use pointer as cursor
      outmessptr += sprintf(outmessptr, "%u", outllid);
      outmessptr = outmessage; // reset to begin of char array
    } else {
      strcpy(outmessage, "error: argument too short or long");
    }
  } else {
    strcpy(outmessage, "outllid (2bit) is: ");
    //cur_len = strlen(outmessage);
    //outmessage[cur_len] = charoutllid;
    //outmessage[cur_len + 1] = '\0';
    // new: append the numerical value to output text to see if converted correctly.
    outmessptr += strlen(outmessage); // use pointer as cursor
    outmessptr += sprintf(outmessptr, "%u", outllid);
    outmessptr = outmessage; // reset to begin of char array
  }
}


/*
//outaccadr
void setoutaccadr()
{
  if (strstr(outmessage, "outaccadr ")) {
    strtokIndx = outmessptr + 10;
    if (strlen(strtokIndx) <= 11) {
      strcpy(charoutaccadr, strtokIndx);
      outaccadr = atoi(charoutaccadr);
      strcpy(outmessage, "outaccadr set to: ");
      strcat(outmessage, charoutaccadr);
      strcat(outmessage, " (ascii entered) and numerical value in hex: 0x");
      
      // strcat(outmessage, "in ascii and converted to: ");
      // dtostrf(outaccadr, 0, 1, interimcharray);
      // strcat(outmessage, interimcharray);
      
      // appending also the numerical value, and converted to hex format
      outmessptr += strlen(outmessage); // use pointer as cursor
      outmessptr += sprintf(outmessptr, "%08X", outaccadr);
      outmessptr = outmessage; // reset to begin of char array
      
      //Serial.print("outaccadr in hex is: 0x");
      //Serial.println(outaccadr, HEX);
      
      //setupble();
    } else {
      strcpy(outmessage, "error: argument too long");
    }
  } else {
    strcpy(outmessage, "outaccadr is: ");
    strcat(outmessage, charoutaccadr);
    strcat(outmessage, " (ascii entered) and numerical value in hex: 0x");

    // appending also the numerical value, and converted to hex format
    outmessptr += strlen(outmessage); // use pointer as cursor
    outmessptr += sprintf(outmessptr, "%08X", outaccadr);
    outmessptr = outmessage; // reset to begin of char array
    
    //Serial.print("outaccadr in hex is: 0x");
    //Serial.println(outaccadr, HEX);
  }
}
*/


//outpdutype
void setoutpdutype()
{
  if (strstr(outmessage, "outpdutype ")) {
    strtokIndx = outmessptr + 11;
    if (strlen(strtokIndx) == 1) {
      //strcpy(chardestlorad, strtokIndx);
      charoutpdutype = strtokIndx[0];
      strcpy(outmessage, "outpdutype (4bits) set to : ");
      //cur_len = strlen(outmessage);
      //outmessage[cur_len] = charoutpdutype;
      //outmessage[cur_len + 1] = '\0';
      outpdutype = charoutpdutype - '0';
      // new: append the numerical pdutype to output text to see if converted correctly.
      outmessptr += strlen(outmessage); // use pointer as cursor
      outmessptr += sprintf(outmessptr, "%u", outpdutype);
      outmessptr = outmessage; // reset to begin of char array
    } else {
      strcpy(outmessage, "error: argument too short or long");
    }
  } else {
    strcpy(outmessage, "outpdutype (4bit) is: ");
    //cur_len = strlen(outmessage);
    //outmessage[cur_len] = charoutpdutype;
    //outmessage[cur_len + 1] = '\0';
    // new: append the numerical pdutype to output text to see if converted correctly.
    outmessptr += strlen(outmessage); // use pointer as cursor
    outmessptr += sprintf(outmessptr, "%u", outpdutype);
    outmessptr = outmessage; // reset to begin of char array
  }
}



void reboot()
{
  ESP.restart();
  // ESP.reset(); // less than restart ! can leave some of the registers in the old state
}



void batmeasure()
{
  battlevel = analogRead(A0);
  voltf = battlevel * (5.4 / 1024.0); //220k + 220k + 100k
}



void batlevel()
{
  batmeasure();
  dtostrf(voltf, 0, 2, interimcharray);
  strcpy(outmessage, "The voltage is: ");
  strcat(outmessage, interimcharray);
  strcat(outmessage, " V");
}



void setintvl()
{
  if (strstr(outmessage, "intvl ")) {
    strtokIndx = outmessptr + 6;
    if (strlen(strtokIndx) <= 6) {
      strcpy(charintvl, strtokIndx);
      intvl = atoi(charintvl);
      strcpy(outmessage, "Interval set to: ");
      // strcat(outmessage, charintvl);
      // new: append the numerical value to output text to see if converted correctly.
      outmessptr += strlen(outmessage); // use pointer as cursor
      outmessptr += sprintf(outmessptr, "%u", intvl);
      outmessptr = outmessage; // reset to begin of char array
    } else {
      strcpy(outmessage, "error: argument too long");
    }
  } else {
     strcpy(outmessage, "intvl is: ");
     // strcat(outmessage, charintvl);
     // new: append the numerical value to output text to see if converted correctly.
     outmessptr += strlen(outmessage); // use pointer as cursor
     outmessptr += sprintf(outmessptr, "%u", intvl);
     outmessptr = outmessage; // reset to begin of char array
  }
}


// txflag
void settxflag() {
  if (strstr(outmessage, "txflag ")) {
    strtokIndx = outmessptr + 7;
    if (strlen(strtokIndx) == 1) {
      //strcpy(chardestlorad, strtokIndx);
      chartxflag = strtokIndx[0];
      strcpy(outmessage, "txflag bit set to : ");
      //cur_len = strlen(outmessage);
      //outmessage[cur_len] = charoutmd;
      //outmessage[cur_len + 1] = '\0';
      txflag = chartxflag - '0';
    // new: append the numerical value to output text to see if converted correctly.
    outmessptr += strlen(outmessage); // use pointer as cursor
    outmessptr += sprintf(outmessptr, "%u", txflag);
    outmessptr = outmessage; // reset to begin of char array
    } else {
      strcpy(outmessage, "error: argument too short or long");
    }
  } else {
    strcpy(outmessage, "txflag bit is: ");
    //cur_len = strlen(outmessage);
    //outmessage[cur_len] = charoutmd;
    //outmessage[cur_len + 1] = '\0';
    // new: append the numerical value to output text to see if converted correctly.
    outmessptr += strlen(outmessage); // use pointer as cursor
    outmessptr += sprintf(outmessptr, "%u", txflag);
    outmessptr = outmessage; // reset to begin of char array
  }
}


// scanflag
void setscanflag() {
  if (strstr(outmessage, "scanflag ")) {
    strtokIndx = outmessptr + 9;
    if (strlen(strtokIndx) == 1) {
      //strcpy(chardestlorad, strtokIndx);
      charscanflag = strtokIndx[0];
      strcpy(outmessage, "scanflag bit set to : ");
      //cur_len = strlen(outmessage);
      //outmessage[cur_len] = charoutmd;
      //outmessage[cur_len + 1] = '\0';
      scanflag = charscanflag - '0';
    // new: append the numerical value to output text to see if converted correctly.
    outmessptr += strlen(outmessage); // use pointer as cursor
    outmessptr += sprintf(outmessptr, "%u", scanflag);
    outmessptr = outmessage; // reset to begin of char array
    } else {
      strcpy(outmessage, "error: argument too short or long");
    }
  } else {
    strcpy(outmessage, "scanflag bit is: ");
    //cur_len = strlen(outmessage);
    //outmessage[cur_len] = charoutmd;
    //outmessage[cur_len + 1] = '\0';
    // new: append the numerical value to output text to see if converted correctly.
    outmessptr += strlen(outmessage); // use pointer as cursor
    outmessptr += sprintf(outmessptr, "%u", scanflag);
    outmessptr = outmessage; // reset to begin of char array
  }
}




void texttopayload() { // writes blemessage char array into byte array and sets the length paylen
  paylen = strlen(blemessage);
  for (int i = 0; i < paylen; ++i) {
    outpayload[i] = (byte) blemessage[i];
   }
}




void setblemessage() 
{
 if (strstr(outmessage, "blemessage ")) {
    strtokIndx = outmessptr + 11;
    if (strlen(strtokIndx) <= 37) {
      strcpy(blemessage, strtokIndx);
      texttopayload();
      // strcpy(outmessage, "blemessage set to: ");
      // strcat(outmessage, blemessage);
      Serial.print("blemessage set to:");
      Serial.println(blemessage);
      Serial.print("outpayload is: ");
      for (int i = 0; i < paylen; ++i) {
        Serial.print(" 0x");
        Serial.print(outpayload[i], HEX);
      }
      strcpy(outmessage, " \r\n");
    } else {
      strcpy(outmessage, "error: argument too long");
    }
  } else {
    // strcpy(outmessage, "blemessage is: ");
    // strcat(outmessage, blemessage);
     Serial.print("blemessage is: ");
     Serial.println(blemessage);
     Serial.print("outpayload is: ");
      for (int i = 0; i < paylen; ++i) {
        Serial.print(" 0x");
        Serial.print(outpayload[i], HEX);
      }
    strcpy(outmessage, " \r\n");
  }
}



void texttoupload() { // writes spimessage char array into byte array and sets the length paylen
  xfersiz = strlen(spimessage);
  for (int i = 0; i < xfersiz; ++i) {
    uploadbytearray[i] = (byte) spimessage[i];
   }
}



void setspimessage() 
{
 if (strstr(outmessage, "spimessage ")) {
    strtokIndx = outmessptr + 11;
    if (strlen(strtokIndx) <= numchars) {
      strcpy(spimessage, strtokIndx);
      texttoupload();
      strcpy(outmessage, "spimessage set to: ");
      strcat(outmessage, spimessage); 
      } else {
      strcpy(outmessage, "error: argument too long");
    }
  } else {
     strcpy(outmessage, "spimessage is: ");
    strcat(outmessage, spimessage);
  }
}



void setblepayload() {
// uses twodigithextobytearray(); creates outpayload from a sequence of 2dig hex number kindof "payload 0xbd 0x1e 0xea ..."
if (strstr(outmessage, "blepayload ")) {
      strtokIndx = outmessptr + 11;
      if ((strlen(strtokIndx) <= numascihex) && (strlen(strtokIndx) >= 4 )) {
      // set strtokIndx to begin of "0xab 0xbc ...
      strtokIndx = strtok(outmessage, " "); // put it at the beginning and replace first space by \0
      strtokIndx = strtok(NULL, " "); // put it on the \0 = previous first space
      twodigithextobytearray();
      paylen= arraysiz;
      for (i = 0; i < paylen; i++) {
        outpayload[i] = resultbytearray[i];
      }
      Serial.print("Payload set to:");  
     for (i = 0; i < paylen; i++) {
       // appending also the numerical value, and converted to hex format
       Serial.print(" 0x");
       Serial.print(outpayload[i], HEX);
      }
     Serial.println();
     //strcpy(outmessage, "(data was displayed by Serial.print instead of sprintf() to outmessage)");
     strcpy(outmessage, " \r\n");
     } else {
      strcpy(outmessage, "error: argument too short or too long");
    }
  } else {
     Serial.print("Payload:");  
     for (i = 0; i < paylen; i++) {
       // appending also the numerical value, and converted to hex format
       Serial.print(" 0x");
       Serial.print(outpayload[i], HEX);
      }
     Serial.println();
     //strcpy(outmessage, "(data was displayed by Serial.print instead of sprintf() to outmessage)");
     strcpy(outmessage, " \r\n");
    }
} // end setblepayload();



void makebleadv()  { // takes paylen, outpayload and other variables, and generates and sends a BLE advertisement packet
// calculating the pdu header:
outpdutotal[0] = (outpdutype << 4) | ((outrfua <<2) & 0b00001100);
if (outtx) outpdutotal[0] += 0x02;
if (outrx) outpdutotal[0] += 0x01;
if ((paylen <= 37) && (paylen >= 6)) {
// outpdutotal[1] = paylen << 2; 
outpdutotal[1] = (paylen + 2) << 2; // add the length of MIC or CRC ???
}
else {
   strcpy(outmessage, "Incorrect payload length, must be within 6..37 for adv packets");
  return; }
outpdutotal[1] += (outrfub & 0b00000011);
// appending the payload to the pdu:
for (int i = 0; i < paylen; ++i) {
outpdutotal[i+2] = outpayload[i]; }
// transmitting BLE packet
/*  int state = radio.transmit(outpdutotal, paylen+2);
    if (state == RADIOLIB_ERR_NONE) {
     strcpy(outmessage, "[SX1280] Packet transmitted successfully!");
  } else {
   strcpy(outmessage, "[SX1280] Failed to transmit packet");
  }
  */
  strcpy(outmessage, "Advertisement outpdutotal created successfully! \r\n");
  // debug
  Serial.print("Header: BIN: 0b"); 
  Serial.print(outpdutotal[0], BIN);
  Serial.print(", 0b");    
  Serial.print(outpdutotal[1], BIN);
  Serial.print(",   HEX: 0x");    
  Serial.print(outpdutotal[0], HEX);
  Serial.print(", 0x");    
  Serial.print(outpdutotal[1], HEX);
  Serial.println();
  Serial.print("Payload:");  
  for (int i = 0; i < paylen; ++i) {
    Serial.print(" 0x");
    Serial.print(outpdutotal[i+2], HEX);
   }
  Serial.println();
  // end debug
}



void makebledat()  { // takes paylen, outpayload and other variables, and generates and sends a BLE data packet
// calculating the pdu header:
outpdutotal[0] = (outllid << 6);
if (outnesn) outpdutotal[0] += 0x20;
if (outsn) outpdutotal[0] += 0x10;
if (outmd) outpdutotal[0] += 0x08;
outpdutotal[0] += (outrfuc & 0b00000111);
if (paylen <= 31) {
// outpdutotal[1] = paylen << 3; 
outpdutotal[1] = (paylen+3) << 3; // maybe must add the length of MIC or CRC ???
}
else {
  strcpy(outmessage, "Incorrect payload length, must be within 6..37 for adv packets");
  return; }
outpdutotal[1] += (outrfud & 0b00000111);
// appending the payload to the pdu:
for (int i = 0; i < paylen; ++i) {
outpdutotal[i+2] = outpayload[i]; }
// transmitting BLE packet
/*  int state = radio.transmit(outpdutotal, paylen+2);
    if (state == RADIOLIB_ERR_NONE) {
     strcpy(outmessage, "[SX1280] Packet transmitted successfully!");
  } else {
    strcpy(outmessage, "[SX1280] Failed to transmit packet");
   } 
   */
  strcpy(outmessage, "Data outpdutotal created successfully! \r\n");
   // debug
  Serial.print("Header: BIN: 0b"); 
  Serial.print(outpdutotal[0], BIN);
  Serial.print(", 0b");    
  Serial.print(outpdutotal[1], BIN);
  Serial.print(",   HEX: 0x");
  Serial.print(outpdutotal[0], HEX);
  Serial.print(", 0x");    
  Serial.print(outpdutotal[1], HEX);
  Serial.println();
  Serial.print("Payload:");  
  for (int i = 0; i < paylen; ++i) {
    Serial.print(" 0x");
    Serial.print(outpdutotal[i+2], HEX);
   }
  Serial.println();
  // end debug
}



void spiupload() {
// uses twodigithextobytearray(); creates uploadbytearay from a sequence of 2dig hex number 
// set outmessptr to begin of "0xab 0xbc ...
if (strstr(outmessage, "spiupload ")) {
      strtokIndx = outmessptr + 10;
      if ((strlen(strtokIndx) <= numascihex) && (strlen(strtokIndx) >= 4 )) {
      // set strtokIndx to begin of "0xab 0xbc ...
      strtokIndx = strtok(outmessage, " "); // put it at the beginning and replace first space by \0
      strtokIndx = strtok(NULL, " "); // put it on the \0 = previous first space and replace next space already by \0
      twodigithextobytearray();
      xfersiz = arraysiz;
      for (i = 0; i < xfersiz; i++) {
      uploadbytearray[i] = resultbytearray[i];
      }
      Serial.print("uploadbytearray set to:");  
     for (i = 0; i < xfersiz; i++) {
       // appending also the numerical value, and converted to hex format
       Serial.print(" 0x");
       Serial.print(uploadbytearray[i], HEX);
      }
     Serial.println();
     //strcpy(outmessage, "(data was displayed by Serial.print instead of sprintf() to outmessage)");
     strcpy(outmessage, "\r\n");
     } else {
      strcpy(outmessage, "error: argument too short or too long");
    }
  } else {
     Serial.print("uploadbytearray:");  
     for (i = 0; i < xfersiz; i++) {
       // appending also the numerical value, and converted to hex format
       Serial.print(" 0x");
       Serial.print(uploadbytearray[i], HEX);
      }
     Serial.println();
     //strcpy(outmessage, "(data was displayed by Serial.print instead of sprintf() to outmessage)");
     strcpy(outmessage, "\r\n");
    }
} // end spiupload();
      
      

void spitransfer() {
// uploads uploadbytearray, receives downloadbytearray, and writes it in different formats hex, ascii, status
digitalWrite(NSS, LOW);
for (i = 0; i < xfersiz; i++) {
//inarray = SPI.transfer(outarray, strlen(outarray));
downloadbytearray[i]= SPI.transfer(uploadbytearray[i]);
}
digitalWrite(NSS, HIGH);
Serial.println("Done transacting SPI");  
Serial.println("Resulting downloadbytearray is: ");
Serial.print("Hex:");
for (i = 0; i < xfersiz; i++) {
Serial.print(" 0x"); Serial.print(downloadbytearray[i], HEX); 
}
Serial.println();
Serial.print("ASCII:");
for (i = 0; i < xfersiz; i++) {
Serial.print(" "); Serial.print((char)downloadbytearray[i]); 
}
Serial.println();
Serial.print("State:");
for (i = 0; i < xfersiz; i++) {
Serial.print(" "); Serial.print((downloadbytearray[i] & 0b11100000) >> 5); Serial.print(",");
Serial.print((downloadbytearray[i] & 0b00011100) >> 2);
}
Serial.println();
//strcpy(outmessage, "(data was displayed by Serial.print instead of sprintf() to outmessage)");
strcpy(outmessage, "");
}


// silent version that doesnt output anything
void silentspitransfer() {
// uploads uploadbytearray, receeives downloadbytearray
digitalWrite(NSS, LOW);
for (i = 0; i < xfersiz; i++) {
//inarray = SPI.transfer(outarray, strlen(outarray));
downloadbytearray[i]= SPI.transfer(uploadbytearray[i]);
}
digitalWrite(NSS, HIGH);
}


void hexascispitransfer() {
// uploads uploadbytearray, receives downloadbytearray, and writes it in formats hex and ascii only (no status)

digitalWrite(NSS, LOW);
for (i = 0; i < xfersiz; i++) {
//inarray = SPI.transfer(outarray, strlen(outarray));
downloadbytearray[i]= SPI.transfer(uploadbytearray[i]);
}
digitalWrite(NSS, HIGH);
// Serial.println("Done transacting SPI");  
// Serial.println("Resulting downloadbytearray is: ");
Serial.print("Hex:");
for (i = 3; i < xfersiz; i++) { // begin with i=3 to supress status bytes
Serial.print(" 0x"); Serial.print(downloadbytearray[i], HEX); 
}
Serial.println();
Serial.print("ASCII:");
for (i = 3; i < xfersiz; i++) { // begin with i=3 to supress status bytes
Serial.print(" "); Serial.print((char)downloadbytearray[i]); 
}
Serial.println();
}



// functions to set transceiver parameters into variables



void setpackettype() {
if (strstr(outmessage, "packettype ")) {
      strtokIndx = outmessptr + 11;
      if (strlen(strtokIndx) == 4) {
      // set strtokIndx to begin of "0xab 0xbc ...
      strtokIndx = strtok(outmessage, " "); // put it at the beginning and replace first space by \0
      strtokIndx = strtok(NULL, " "); // put it on the \0 = previous first space
      twodigithextobytearray();
      //paylen= arraysiz;
      // check here if 15 bytes entered:
      
      packetbytetype = resultbytearray[0];
      
      strcpy(outmessage, "packetbytetype set to:");  
      strcat(outmessage, " 0x");
      outmessptr += strlen(outmessage); // use pointer as cursor
      outmessptr += sprintf(outmessptr, "%02X", packetbytetype);
      outmessptr = outmessage; // reset to begin of char array
      
      // copy the colon separated version to the char array version of the variable
     strcpy(packetchartype, resultchararray);
     
     } else {
      strcpy(outmessage, "error: argument too short or too long ");
    }
  } else {
     strcpy(outmessage, "packetbytetype is:");  
     strcat(outmessage, " 0x");
     outmessptr += strlen(outmessage); // use pointer as cursor
     outmessptr += sprintf(outmessptr, "%02X", packetbytetype);
     outmessptr = outmessage; // reset to begin of char array
     }
     strcat(outmessage, "\r\n");
} // end setpackettype()




void setfreq()
{
  if (strstr(outmessage, "freq ")) {
    strtokIndx = outmessptr + 5;
    if (strlen(strtokIndx) <= 10) {
      strcpy(charfreq, strtokIndx);
      freq = atof(charfreq);
      strcpy(outmessage, "RF freq set to: ");
      //strcat(outmessage, charfreq);

      // new: append the numerical freq to output text to see if typed wrongly e.g. 2410,5 instead of decimal point.
      outmessptr += strlen(outmessage); // use pointer as cursor
      outmessptr += sprintf(outmessptr, "%f", freq);
      outmessptr = outmessage; // reset to begin of char array
      
      //setupble();
    } else {
      strcpy(outmessage, "error: argument too long");
    }
  } else {
     strcpy(outmessage, "freq is [MHz]: ");
    //strcat(outmessage, charfreq);

    // new: append the numerical freq to output text to see if typed wrongly e.g. 2410,5 instead of decimal point.
    outmessptr += strlen(outmessage); // use pointer as cursor
    outmessptr += sprintf(outmessptr, "%f", freq);
    outmessptr = outmessage; // reset to begin of char array
  }
  strcat(outmessage, "\r\n");
}



void settxparams() {
if (strstr(outmessage, "txparams ")) {
      strtokIndx = outmessptr + 9;
      if (strlen(strtokIndx) ==  9) {
      // set strtokIndx to begin of "0xab 0xbc ...
      strtokIndx = strtok(outmessage, " "); // put it at the beginning and replace first space by \0
      strtokIndx = strtok(NULL, " "); // put it on the \0 = previous first space
      twodigithextobytearray();
      //paylen= arraysiz;
      // check here if 15 bytes entered:
      for (i = 0; i < 2; i++) {
      txparbytearray[i] = resultbytearray[i];
      }
      strcpy(outmessage, "txparams set to:");  
     for (i = 0; i < 2; i++) {
       // appending also the numerical value, and converted to hex format
       strcat(outmessage, " 0x");
       outmessptr += strlen(outmessage); // use pointer as cursor
       outmessptr += sprintf(outmessptr, "%02X", txparbytearray[i]);
       outmessptr = outmessage; // reset to begin of char array
      }
     // copy the colon separated version to the char array version of the variable
     strcpy(txparchararray, resultchararray);
     } else {
      strcpy(outmessage, "error: argument too short or too long ");
    }
  } else {
     strcpy(outmessage, "txparams are:");  
     for (i = 0; i < 2; i++) {
       // appending also the numerical value, and converted to hex format
       strcat(outmessage, " 0x");
       outmessptr += strlen(outmessage); // use pointer as cursor
       outmessptr += sprintf(outmessptr, "%02X", txparbytearray[i]);
       outmessptr = outmessage; // reset to begin of char array
       }
     }
  strcat(outmessage, " \r\n");   
} // end settxparams()



void setpower()  // legacy
{
  if (strstr(outmessage, "power ")) {
    strtokIndx = outmessptr + 6;
    if (strlen(strtokIndx) <= 4) {
      strcpy(charpower, strtokIndx);
      //power = atoi(charpower);
      txparbytearray[0] = 18 + atoi(charpower);
      strcpy(outmessage, "RF power set to [dBm]: ");
      //strcat(outmessage, charpower);
      // new: append the numerical value to output text to see if converted correctly.
      outmessptr += strlen(outmessage); // use pointer as cursor
      outmessptr += sprintf(outmessptr, "%u", txparbytearray[0]-18);
      outmessptr = outmessage; // reset to begin of char array
      //setupble();
    } else {
      strcpy(outmessage, "error: argument too long");
    }
  } else {
     strcpy(outmessage, "power is [dBm]: ");
     // strcat(outmessage, charpower);
     // new: append the numerical value to output text to see if converted correctly.
      outmessptr += strlen(outmessage); // use pointer as cursor
      outmessptr += sprintf(outmessptr, "%u", txparbytearray[0]-18);
      outmessptr = outmessage; // reset to begin of char array
  }
}



void setsyncarray() {
// uses twodigithextobytearray(); creates syncbytearray from a sequence of 2dig hex number kindof "syncbytearray 0xbd 0x1e 0xea ..."
if (strstr(outmessage, "syncarray ")) {
      strtokIndx = outmessptr + 10;
      if (strlen(strtokIndx)== 74 ) {
      // set strtokIndx to begin of "0xab 0xbc ...
      strtokIndx = strtok(outmessage, " "); // put it at the beginning and replace first space by \0
      strtokIndx = strtok(NULL, " "); // put it on the \0 = previous first space
      twodigithextobytearray();
      //paylen= arraysiz;
      // check here if 15 bytes entered:
      for (i = 0; i < 15; i++) {
      syncbytearray[i] = resultbytearray[i];
      }
      strcpy(outmessage, "syncbytearray set to:");  
     for (i = 0; i < 15; i++) {
       // appending also the numerical value, and converted to hex format
       strcat(outmessage, " 0x");
       outmessptr += strlen(outmessage); // use pointer as cursor
       outmessptr += sprintf(outmessptr, "%02X", syncbytearray[i]);
       outmessptr = outmessage; // reset to begin of char array
      }
     // copy the colon separated version to the char array version of the variable
     strcpy(syncchararray, resultchararray);
      
     } else {
      strcpy(outmessage, "error: argument too short or too long ");
    }
  } else {
     strcpy(outmessage, "syncbytearray is:");  
     for (i = 0; i < 15; i++) {
       // appending also the numerical value, and converted to hex format
       strcat(outmessage, " 0x");
       outmessptr += strlen(outmessage); // use pointer as cursor
       outmessptr += sprintf(outmessptr, "%02X", syncbytearray[i]);
       outmessptr = outmessage; // reset to begin of char array
       }
     }
  strcat(outmessage, " \r\n");   
} // end setsyncarray()





/*
// obsolete
void setbitrate()
{
  if (strstr(outmessage, "bitrate ")) {
    strtokIndx = outmessptr + 8;
    if (strlen(strtokIndx) <= 4) {
      strcpy(charbitrate, strtokIndx);
      bitrate = atoi(charbitrate);
      strcpy(outmessage, "Bitrate set to: ");
      // strcat(outmessage, charbitrate);
      // new: append the numerical value to output text to see if converted correctly.
      outmessptr += strlen(outmessage); // use pointer as cursor
      outmessptr += sprintf(outmessptr, "%u", bitrate);
      outmessptr = outmessage; // reset to begin of char array
      //setupble();
    } else {
      strcpy(outmessage, "error: argument too long");
    }
  } else {
     strcpy(outmessage, "bitrate is [kBps]: ");
     // strcat(outmessage, charbitrate);
     // new: append the numerical value to output text to see if converted correctly.
      outmessptr += strlen(outmessage); // use pointer as cursor
      outmessptr += sprintf(outmessptr, "%u", bitrate);
      outmessptr = outmessage; // reset to begin of char array
  }
}


// obsolete
void setdeviation()
{
  if (strstr(outmessage, "deviation ")) {
    strtokIndx = outmessptr + 10;
    if (strlen(strtokIndx) <= 7) {
      strcpy(chardeviation, strtokIndx);
      deviation = atof(chardeviation);
      strcpy(outmessage, "Deviation set to: ");
      //strcat(outmessage, chardeviation);
      
      // new: append the numerical deviation to output text to see if typed wrongly e.g. 210,5 instead of decimal point.
      outmessptr += strlen(outmessage); // use pointer as cursor
      outmessptr += sprintf(outmessptr, "%f", deviation);
      outmessptr = outmessage; // reset to begin of char array
      //setupble();
    } else {
      strcpy(outmessage, "error: argument too long");
    }
  } else {
     strcpy(outmessage, "deviation is [kHz]: ");
     //strcat(outmessage, chardeviation);

     // new: append the numerical deviation to output text to see if typed wrongly e.g. 210,5 instead of decimal point.
      outmessptr += strlen(outmessage); // use pointer as cursor
      outmessptr += sprintf(outmessptr, "%f", deviation);
      outmessptr = outmessage; // reset to begin of char array
  }
}



// obsolete
void setshape()
{
  if (strstr(outmessage, "shape ")) {
    strtokIndx = outmessptr + 6;
    if (strlen(strtokIndx) == 1) {
      //strcpy(chardestlorad, strtokIndx);
      charshape = strtokIndx[0];
      strcpy(outmessage, "shaping set to: ");
      // cur_len = strlen(outmessage);
      // outmessage[cur_len] = charshape;
      // outmessage[cur_len + 1] = '\0';
      shape = charshape - '0';
      // new: append the numerical value to output text to see if converted correctly.
      outmessptr += strlen(outmessage); // use pointer as cursor
      outmessptr += sprintf(outmessptr, "%u", shape);
      outmessptr = outmessage; // reset to begin of char array
      //setupble();
    } else {
      strcpy(outmessage, "error: argument too short or long");
    }
  } else {
    strcpy(outmessage, "shape is [0, 2, 4]: ");
    // cur_len = strlen(outmessage);
    // outmessage[cur_len] = charshape;
    // outmessage[cur_len + 1] = '\0';
    // new: append the numerical value to output text to see if converted correctly.
      outmessptr += strlen(outmessage); // use pointer as cursor
      outmessptr += sprintf(outmessptr, "%u", shape);
      outmessptr = outmessage; // reset to begin of char array
  }
}
*/


void setmodparams() {
// uses twodigithextobytearray(); creates modparbytearray from a sequence of 2dig hex number kindof "modparams 0xbd 0x1e 0xea ..."
if (strstr(outmessage, "modparams ")) {
      strtokIndx = outmessptr + 10;
      if (strlen(strtokIndx) == 14 ) {
      // set strtokIndx to begin of "0xab 0xbc 0xcd ...
      strtokIndx = strtok(outmessage, " "); // put it at the beginning and replace first space by \0
      strtokIndx = strtok(NULL, " "); // put it on the \0 = previous first space
      twodigithextobytearray();
           
      for (i = 0; i < 3; i++) {
        modparbytearray[i] = resultbytearray[i];
      }
      strcpy(outmessage, "modparams set to:");  
     for (i = 0; i < 3; i++) {
       // appending also the numerical value, and converted to hex format
       strcat(outmessage, " 0x");
       outmessptr += strlen(outmessage); // use pointer as cursor
       outmessptr += sprintf(outmessptr, "%02X", modparbytearray[i]);
       outmessptr = outmessage; // reset to begin of char array
      }
     // copy the colon separated version to the char array version of the variable
     strcpy(modparchararray, resultchararray);
    } else {
      strcpy(outmessage, "error: argument too short or too long");
    }
  } else {
     strcpy(outmessage, "modparams are:");  
     for (i = 0; i < 3; i++) {
       // appending also the numerical value, and converted to hex format
       strcat(outmessage, " 0x");
       outmessptr += strlen(outmessage); // use pointer as cursor
       outmessptr += sprintf(outmessptr, "%02X", modparbytearray[i]);
       outmessptr = outmessage; // reset to begin of char array
       }
     }
  strcat(outmessage, " \r\n");   
} // end setmodparams()



void setpackparams() {
// uses twodigithextobytearray(); creates packparbytearray from a sequence of 2dig hex number kindof "packparams 0xbd 0x1e 0xea ..."
if (strstr(outmessage, "packparams ")) {
      strtokIndx = outmessptr + 11;
      if (strlen(strtokIndx) == 34 ) {
      // set strtokIndx to begin of "0xab 0xbc 0xcd ...
      strtokIndx = strtok(outmessage, " "); // put it at the beginning and replace first space by \0
      strtokIndx = strtok(NULL, " "); // put it on the \0 = previous first space
      twodigithextobytearray();
           
      for (i = 0; i < 7; i++) {
        packparbytearray[i] = resultbytearray[i];
      }
      strcpy(outmessage, "packparams set to:");  
     for (i = 0; i < 7; i++) {
       // appending also the numerical value, and converted to hex format
       strcat(outmessage, " 0x");
       outmessptr += strlen(outmessage); // use pointer as cursor
       outmessptr += sprintf(outmessptr, "%02X", packparbytearray[i]);
       outmessptr = outmessage; // reset to begin of char array
      }
     // copy the colon separated version to the char array version of the variable
     strcpy(packparchararray, resultchararray);
    } else {
      strcpy(outmessage, "error: argument too short or too long");
    }
  } else {
     strcpy(outmessage, "packparams are:");  
     for (i = 0; i < 7; i++) {
       // appending also the numerical value, and converted to hex format
       strcat(outmessage, " 0x");
       outmessptr += strlen(outmessage); // use pointer as cursor
       outmessptr += sprintf(outmessptr, "%02X", packparbytearray[i]);
       outmessptr = outmessage; // reset to begin of char array
       }
     }
  strcat(outmessage, " \r\n");   
} // end setpackparams()



void setirqmask() {
// uses twodigithextobytearray(); creates irqmaskbytearray from a sequence of 2dig hex number kindof "irqmask 0xbd 0x1e 0xea ..."
if (strstr(outmessage, "irqmask ")) {
      strtokIndx = outmessptr + 8;
      if (strlen(strtokIndx) == 19 ) {
      // set strtokIndx to begin of "0xab 0xbc 0xcd ...
      strtokIndx = strtok(outmessage, " "); // put it at the beginning and replace first space by \0
      strtokIndx = strtok(NULL, " "); // put it on the \0 = previous first space
      twodigithextobytearray();
           
      for (i = 0; i < 4; i++) {
        irqmaskbytearray[i] = resultbytearray[i];
      }
      strcpy(outmessage, "irqmask set to:");  
     for (i = 0; i < 4; i++) {
       // appending also the numerical value, and converted to hex format
       strcat(outmessage, " 0x");
       outmessptr += strlen(outmessage); // use pointer as cursor
       outmessptr += sprintf(outmessptr, "%02X", irqmaskbytearray[i]);
       outmessptr = outmessage; // reset to begin of char array
      }
     // copy the colon separated version to the char array version of the variable
     strcpy(irqmaskchararray, resultchararray);
    } else {
      strcpy(outmessage, "error: argument too short or too long");
    }
  } else {
     strcpy(outmessage, "irqmask is:");  
     for (i = 0; i < 4; i++) {
       // appending also the numerical value, and converted to hex format
       strcat(outmessage, " 0x");
       outmessptr += strlen(outmessage); // use pointer as cursor
       outmessptr += sprintf(outmessptr, "%02X", irqmaskbytearray[i]);
       outmessptr = outmessage; // reset to begin of char array
       }
     }
  strcat(outmessage, " \r\n");   
} // end setirqmask()



void setbuffbase() {
// uses twodigithextobytearray(); creates buffbasebytearray from a sequence of 2dig hex number kindof "buffbase 0xbd 0x1e 0xea ..."
if (strstr(outmessage, "buffbase ")) {
      strtokIndx = outmessptr + 9;
      if (strlen(strtokIndx) == 9 ) {
      // set strtokIndx to begin of "0xab 0xbc 0xcd ...
      strtokIndx = strtok(outmessage, " "); // put it at the beginning and replace first space by \0
      strtokIndx = strtok(NULL, " "); // put it on the \0 = previous first space
      twodigithextobytearray();
           
      for (i = 0; i < 2; i++) {
        buffbasebytearray[i] = resultbytearray[i];
      }
      strcpy(outmessage, "buffbase set to:");  
     for (i = 0; i < 2; i++) {
       // appending also the numerical value, and converted to hex format
       strcat(outmessage, " 0x");
       outmessptr += strlen(outmessage); // use pointer as cursor
       outmessptr += sprintf(outmessptr, "%02X", buffbasebytearray[i]);
       outmessptr = outmessage; // reset to begin of char array
      }
     // copy the colon separated version to the char array version of the variable
     strcpy(buffbasechararray, resultchararray);
    } else {
      strcpy(outmessage, "error: argument too short or too long");
    }
  } else {
     strcpy(outmessage, "buffbase is:");  
     for (i = 0; i < 2; i++) {
       // appending also the numerical value, and converted to hex format
       strcat(outmessage, " 0x");
       outmessptr += strlen(outmessage); // use pointer as cursor
       outmessptr += sprintf(outmessptr, "%02X", buffbasebytearray[i]);
       outmessptr = outmessage; // reset to begin of char array
       }
     }
  strcat(outmessage, " \r\n");   
} // end setbuffbase()



void setautotx() {
// uses twodigithextobytearray(); creates autotxbytearray from a sequence of 2dig hex number kindof "autotx 0xbd 0x1e 0xea ..."
if (strstr(outmessage, "autotx ")) {
      strtokIndx = outmessptr + 7;
      if (strlen(strtokIndx) == 9 ) {
      // set strtokIndx to begin of "0xab 0xbc 0xcd ...
      strtokIndx = strtok(outmessage, " "); // put it at the beginning and replace first space by \0
      strtokIndx = strtok(NULL, " "); // put it on the \0 = previous first space
      twodigithextobytearray();
           
      for (i = 0; i < 2; i++) {
        autotxbytearray[i] = resultbytearray[i];
      }
      strcpy(outmessage, "autotx set to:");  
     for (i = 0; i < 2; i++) {
       // appending also the numerical value, and converted to hex format
       strcat(outmessage, " 0x");
       outmessptr += strlen(outmessage); // use pointer as cursor
       outmessptr += sprintf(outmessptr, "%02X", autotxbytearray[i]);
       outmessptr = outmessage; // reset to begin of char array
      }
     // copy the colon separated version to the char array version of the variable
     strcpy(autotxchararray, resultchararray);
    } else {
      strcpy(outmessage, "error: argument too short or too long");
    }
  } else {
     strcpy(outmessage, "autotx is:");  
     for (i = 0; i < 2; i++) {
       // appending also the numerical value, and converted to hex format
       strcat(outmessage, " 0x");
       outmessptr += strlen(outmessage); // use pointer as cursor
       outmessptr += sprintf(outmessptr, "%02X", autotxbytearray[i]);
       outmessptr = outmessage; // reset to begin of char array
       }
     }
  strcat(outmessage, " \r\n");   
} // end setautotx()






// functions to apply the parameters to the transceiver

void setrfpackettype() {
uploadbytearray[0]=0x8a;
uploadbytearray[1]= packetbytetype; //0x04; //BLE p. 86
xfersiz = 2;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it
}


void setrffreq() {
Serial.print("Appling the frequency to the transceiver [MHz]: "); Serial.println(freq);
pllsteps = floor(freq *1e6 / 198.3642578125);
Serial.println("The divison gives: "); Serial.print(pllsteps); Serial.println(" PLL steps ");
uploadbytearray[0]=0x86;
uploadbytearray[3]= (pllsteps % 256);
pllsteps = pllsteps / 256;
uploadbytearray[2]= (pllsteps % 256);
pllsteps = pllsteps / 256;
uploadbytearray[1]= (pllsteps % 256);
xfersiz = 4;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it
}


void setrftxparams() {
uploadbytearray[0]=0x8e;
uploadbytearray[1]= txparbytearray[0]; // power + 18; //dBm + 18
uploadbytearray[2]= txparbytearray[1]; //ramptime 0x80 -> 10 us p. 88
xfersiz = 3;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it
}


/* // obsolete
void setaccessaddress() { // similar manipulation with div and mof as in setrffreq()
  // this is the famous sync word in other modes, in GFSK it has a even more MSB p.108
uploadbytearray[0]=0x18; //write register Opcode
uploadbytearray[1]=0x09;
uploadbytearray[2]=0xcf; //0x09cf memory location of MSB of access address of 4 byte BLE syncword
// now decompose the 4 byte variable into 4 bytes:
uploadbytearray[6]= (outaccadr % 256); //LSB
outaccadr = outaccadr / 256;
uploadbytearray[5]= (outaccadr % 256);
outaccadr = outaccadr / 256;
uploadbytearray[4]= (outaccadr % 256);
outaccadr = outaccadr / 256;
uploadbytearray[3]= (outaccadr % 256); //MSB
xfersiz = 7;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it
} */


void setrfsyncarray() { //replaces setaccessaddress; in BLE, 1st of 5 bytes is 0x00
 uploadbytearray[0]=0x18; //write register Opcode
uploadbytearray[1]=0x09;
uploadbytearray[2]=0xce; //0x09ce memory location of MSB of access address of general syncword
for (int i = 0; i < 15; ++i) {
  uploadbytearray[i+3] = syncbytearray[i];
}
xfersiz = 17;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it 
}


void setrfmodparams() { // p. 90, -> BLE/GFSK p. 104
uploadbytearray[0]=0x8b;
for (int i = 0; i < 3; ++i) {
  uploadbytearray[i+1] = modparbytearray[i];
}
//uploadbytearray[1]=0x45; // 1 Mbps, 1.2 MHz BW, p.113 BLE-specific
//uploadbytearray[2]=0x01; // Mod Ind 0.5 [0.35 .. 4] p. 105, p. 114 BLE-specific
//uploadbytearray[3]=0x20; //  filtering BT_0_5 p. 106, p. 114 BLE-specific
xfersiz = 4;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it
}



void setrfpackparams() { // p. 90, -> BLE/GFSK p. 114
uploadbytearray[0]=0x8c;
for (int i = 0; i < 7; ++i) {
  uploadbytearray[i+1] = packparbytearray[i];
}
/*
uploadbytearray[1]=0x20; // paylaod length max 37 bytes, p.114 BLE-specific
uploadbytearray[2]=0x10; // CRC 3 bytes p. 114 BLE-specific
uploadbytearray[3]=0x18; // 00001111 test payload, p. 114 BLE-specific
uploadbytearray[4]=0x00; // whitening enable, p. 114 BLE-specific
uploadbytearray[5]=0x00; // remaining packet params must be set to zero and sent to radio
uploadbytearray[6]=0x00;
uploadbytearray[7]=0x00;
*/
xfersiz = 8;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it
}



void setrfbuffbase() {
uploadbytearray[0]=0x8f;
for (int i = 0; i < 2; ++i) {
  uploadbytearray[i+1] = buffbasebytearray[i];
}
// uploadbytearray[1]=0x80; //tx
// uploadbytearray[2]=0x20; //rx
xfersiz = 3;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it
}



void setrfirqmask() {
uploadbytearray[0]=0x8d;
for (int i = 0; i <4 ; ++i) {
  uploadbytearray[i+1] = irqmaskbytearray[i];
}
xfersiz = 5;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it
}



void setrfautotx() { // activate automatic transmit mode after 125 us as on p. 84
uploadbytearray[0]=0x98;
uploadbytearray[1]= autotxbytearray[0]; // 0x00; // time MSB
uploadbytearray[2]= autotxbytearray[1]; //0x5c; // time LSB p.120: 125 us -33us (switch time) = 0x5c
xfersiz = 3;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it 
}



// direct radio control:

void clearirqstatus() {
uploadbytearray[0]=0x97;
xfersiz = 1;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it
}


void setstandby() {
uploadbytearray[0]=0x80;
uploadbytearray[1]=0x00; // stdby_rc p.78
xfersiz = 2;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it
// intvl =0; // stop periodic rx/tx user has to do himself

}


void settx() { // here no timeout
uploadbytearray[0]=0x83;
uploadbytearray[1]=0x00; // periodbase
uploadbytearray[2]=0x00; // periodbasecount
uploadbytearray[3]=0x00; // periodbasecount
xfersiz = 4;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it
// copy paste getstatus();  this avoids that the periodic status query in the intvl loop detects a packed receive
uploadbytearray[0]=0xc0; // p.72
uploadbytearray[1]=0x00;
xfersiz = 2;
//spiupload(); // displays whats in the uploadbytearray
silentspitransfer(); // and transmits it 
}


void setrx() { // here no timeout, single mode
uploadbytearray[0]=0x82;
uploadbytearray[1]=0x00; // periodbase
uploadbytearray[2]=0x00; // periodbasecount
uploadbytearray[3]=0x00; // periodbasecount
xfersiz = 4;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it
}


void setcw() {
uploadbytearray[0]=0x8a;
uploadbytearray[1]= 0x00; // packetbytetype GFSK default p. 86 because in BLE the cw mode doesnt work
xfersiz = 2;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it
uploadbytearray[0]=0xd1; // the actual thing: cw mode
xfersiz = 1;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it
}



void getrxbufferstatus() {
uploadbytearray[0]=0x17;
uploadbytearray[1]=0x00; // NOP
uploadbytearray[2]=0x00; // NOP
uploadbytearray[3]=0x00; // NOP
xfersiz = 4;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it
}



void getpacketstatus() {
uploadbytearray[0]=0x1d;
uploadbytearray[1]=0x00; // NOP
uploadbytearray[2]=0x00; // NOP
uploadbytearray[3]=0x00; // NOP
uploadbytearray[4]=0x00; // NOP
uploadbytearray[5]=0x00; // NOP
uploadbytearray[6]=0x00; // NOP
xfersiz = 7;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it
}



void setcontinuouspreamble() {
}



void setupble() { // calls the previous functions
setstandby();
setrfpackettype(); // attention this seems to disable the cw emission
setrffreq();
setrftxparams(); // although not mentioned on p. 104, also a good idea to set the power !
// it is under TX settings on p. 116 but we do it here because we dont do it before each transmit
setrfsyncarray();
delay(200);
setrfmodparams();
setrfpackparams();
setrfirqmask();
setrfbuffbase();
setrfautotx();
}


void sendoutpdutotal()  { // absolute minimalist version described p.116
//set standby
setstandby(); // attention this resets interval, if bothering eg by calling sendoutpdutotal() in the loop, then
// rather copy paste content of setstandby() without it
// write into uploadbytearray 0x1a write command, 0x80 tx offset, and copy of outpdutotal
uploadbytearray[0]=0x1a;
uploadbytearray[1]=0x80;
// outpdutotal has no explicit length but it is paylen+2
for (int i = 0; i < (paylen+2); ++i) {
uploadbytearray[i+2] = outpdutotal[i]; }
xfersiz = paylen + 4; // and with the 2 bytes opcode and offset, it is +4
spiupload();
spitransfer();

// clear irq
clearirqstatus();

// set TX  
settx();
}



void getstatus() {
uploadbytearray[0]=0xc0; // p.72
uploadbytearray[1]=0x00;
xfersiz = 2;
spiupload(); // displays whats in the uploadbytearray
spitransfer(); // and transmits it 
}



void stepfreq() {
  if (freq == 2402) //ch 37
   freq = 2426; // ch 38
   else if (freq == 2426) // ch 38
   freq = 2480; // ch 39
   else 
   freq = 2402; //ch37
}




void cmdparse() { //command parser
  
  if (strstr(outmessage, "help") == outmessptr) {
    helpscreen();
  }
  // else if (strstr(outmessage, "linkstrength") == outmessptr) {
  // linkstrength();
  // }
  else if (strstr(outmessage, "batlevel") == outmessptr) {
    batlevel();
  }
   else if (strstr(outmessage, "blemessage") == outmessptr) {
    setblemessage();
  }
  else if (strstr(outmessage, "blepayload") == outmessptr) {
    setblepayload();
  }
  /* else if (strstr(outmessage, "tncontext") == outmessptr) {
    settncontext();
  }
  else if (strstr(outmessage, "sercontext") == outmessptr) {
    setsercontext();
  }
  else if (strstr(outmessage, "lorcontext") == outmessptr) {
    setlorcontext();
  }
  else if (strstr(outmessage, "loclora") == outmessptr) {
    loclora();
  }
  else if (strstr(outmessage, "destlora") == outmessptr) {
    destlora();
  }
  else if (strstr(outmessage, "promiscuous") == outmessptr) {
    setpromisc();
  } */
  else if (strstr(outmessage, "packettype") == outmessptr) {
    setpackettype();
  }
  else if (strstr(outmessage, "freq") == outmessptr) {
    setfreq();
  }
  else if (strstr(outmessage, "txparams") == outmessptr) {
    settxparams();
  }
  else if (strstr(outmessage, "power") == outmessptr) {
    setpower();
  }
  else if (strstr(outmessage, "syncarray") == outmessptr) {
    setsyncarray();
  }
  else if (strstr(outmessage, "modparams") == outmessptr) {
    setmodparams();
  }
  else if (strstr(outmessage, "packparams") == outmessptr) {
    setpackparams();
  }
  else if (strstr(outmessage, "irqmask") == outmessptr) {
    setirqmask();
  }
  else if (strstr(outmessage, "buffbase") == outmessptr) {
    setbuffbase();
  }
   else if (strstr(outmessage, "autotx") == outmessptr) {
    setautotx();
  }

  else if (strstr(outmessage, "setrffreq") == outmessptr) {
    setrffreq();
  }
  else if (strstr(outmessage, "setrftxparams") == outmessptr) {
    setrftxparams();
  }
  else if (strstr(outmessage, "setupble") == outmessptr) {
  setupble();
  }
  else if (strstr(outmessage, "outpdutype") == outmessptr) {
    setoutpdutype();
  }
  else if (strstr(outmessage, "outtx") == outmessptr) {
    setouttx();
  }
   else if (strstr(outmessage, "outrx") == outmessptr) {
    setoutrx();
  }
  else if (strstr(outmessage, "outllid") == outmessptr) {
    setoutllid();
  }
  else if (strstr(outmessage, "outnesn") == outmessptr) {
    setoutnesn();
  }
  else if (strstr(outmessage, "outsn") == outmessptr) {
    setoutsn();
  }
  else if (strstr(outmessage, "outmd") == outmessptr) {
   setoutmd();
  }
   else if (strstr(outmessage, "intvl") == outmessptr) {
   setintvl();
  }
  else if (strstr(outmessage, "eepromstore") == outmessptr) {
    eepromstore();
  }
  else if (strstr(outmessage, "eepromretrieve") == outmessptr) {
    eepromretrieve();
  }
  else if (strstr(outmessage, "eepromdelete") == outmessptr) {
    eepromdelete();
  }
  else if (strstr(outmessage, "reboot") == outmessptr) {
    reboot();
  }
   else if (strstr(outmessage, "makebleadv") == outmessptr) {
    makebleadv();
  }
  else if (strstr(outmessage, "makebledat") == outmessptr) {
    makebledat();
  }
  else if (strstr(outmessage, "spimessage") == outmessptr) {
    setspimessage();
  }
   else if (strstr(outmessage, "spiupload") == outmessptr) {
    spiupload();
  }
   else if (strstr(outmessage, "spitransfer") == outmessptr) {
    spitransfer();
  }
   else if (strstr(outmessage, "sendoutpdutotal") == outmessptr) {
    sendoutpdutotal();
  }
  else if (strstr(outmessage, "clearirqstatus") == outmessptr) {
    clearirqstatus();
  }
  else if (strstr(outmessage, "setstandby") == outmessptr) {
    setstandby();
  }
  else if (strstr(outmessage, "settx") == outmessptr) {
    settx();
  }
  else if (strstr(outmessage, "setrx") == outmessptr) {
    setrx();
  }
  else if (strstr(outmessage, "setcw") == outmessptr) {
    setcw();
  }
   else if (strstr(outmessage, "getstatus") == outmessptr) {
    getstatus();
  }
  else if (strstr(outmessage, "txflag") == outmessptr) {
    settxflag();
  }
  else if (strstr(outmessage, "scanflag") == outmessptr) {
    setscanflag();
  }
   else strcat(outmessage, " - unknown command");
  }  // end cmdparse()




void setup() {

pinMode(NSS, OUTPUT);
pinMode(BUSY, INPUT);
pinMode(DIO1, INPUT);
digitalWrite(NSS, HIGH); // inverse logic, active low

SPI.begin();

// according to SX1281 datasheet we need SPI_MODE0, maximum is 18 MHz but we start with 1 MHz 
// SPI.beginTransaction(SPISettings(14000000, MSBFIRST, SPI_MODE0));
SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));


  
EEPROM.begin(nvlength);  // eeprom
Serial.begin(9600);
eepromretrieve(); // retrieve EEPROM data
publish(); // output acknowledgement of last function

// turning off wlan transceiver to save power and avoid interference
// copied from other sketches, e.g. LoRa_mqtt_terminal_gateway :
   Serial.println("WiFi Going into stealth mode to avoid interference with BLE at the same band");
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin(); // without a value it sleeps indefiniely and with value for a defined time, as ESP sleep ???
    delay(100); //Needed, at least in my tests WiFi doesn't power off without this for some reason




// initialize the transceiver; without this, it would return 7,7 as status when SPI transfer is attempted
pinMode(NRESET, OUTPUT);
digitalWrite(NRESET, LOW); // reset is inverse logic, active low
delay(1); // set it low for 1 ms
digitalWrite(NRESET, HIGH);
if (!digitalRead(BUSY)){
  Serial.println("entering of setup not verified");
}
lastSendTime = millis();

while (digitalRead(BUSY)){
  delay(1); //don't digital read too frequently
  // do nothing and wait 
  }
lastSendTime = millis() - lastSendTime;
Serial.print("Tansceiver was successfully reset in [ms] :"); Serial.println(lastSendTime);
lastSendTime =0; // reset the variable after abusing it..
digitalWrite(NSS, LOW);
SPI.transfer(0xc0); //GetStatus() just to tx something via SPI; // status codes on page 73 but here not used
digitalWrite(NSS, HIGH);
delay(5); //
digitalWrite(NSS, LOW);
SPI.transfer(0x97); //ClearIRQ() according to page 97
digitalWrite(NSS, HIGH);
delay(5); //
digitalWrite(NSS, LOW);
SPI.transfer(0x82); //setRX(), however without specifying continuous RX it seems to go back to standby RC 13
digitalWrite(NSS, HIGH);

helpscreen();

  // debug
  // Serial.print("paylen= ");  Serial.println(paylen);

// variables not initialized despite declaration...

/*
strcpy(blemessage, "hello");
strcpy(spimessage, "hello");
texttopayload();
texttoupload();
*/

Serial.println("Entering transceiver configuration with parameters from EEPROM or hardcoded if EEPROM absent");

delay(1000);
setupble();
Serial.println("Transceiver configuration complete ");
Serial.println();
delay(1000);

/*
sendoutpdutotal(); // so that the outpdutotal from the eeprom is in the buffer
// get status once so that the rx/tx done doesnt give a false packet received
//getstatus(); copy paste and disable ouputs 
uploadbytearray[0]=0xc0; // p.72
uploadbytearray[1]=0x00;
xfersiz = 2;
silentspitransfer(); // and transmits it 
*/

} // end setup()



void loop() {

if (intvl) // unequal zero also serves as flag enabling periodic execution of rx/tx
// setstandby() must also reset intvl to zero
if (millis() - lastSendTime > intvl) {

// check if something received and display it along with the present frequency
// if (higherstatus ==2 and lowerstatus ==6) // stdby and data avail to host

//getstatus(); copy paste and disable ouputs
uploadbytearray[0]=0xc0; // p.72
uploadbytearray[1]=0x00;
xfersiz = 2;
silentspitransfer(); // and transmits it 

// if packet received: see status explanation p. 73
if ((((downloadbytearray[0] & 0b11100000) >> 5) == 2) && (((downloadbytearray[0] & 0b00011100) >> 2) == 6)) { // begin if packet recvd

Serial.println("Packet received: ");

//getpacketstatus(); // p.93; mainly for RSSI here; copy paste content make silent:
uploadbytearray[0]=0x1d;
uploadbytearray[1]=0x00; // NOP
uploadbytearray[2]=0x00; // NOP
uploadbytearray[3]=0x00; // NOP
uploadbytearray[4]=0x00; // NOP
uploadbytearray[5]=0x00; // NOP
uploadbytearray[6]=0x00; // NOP
xfersiz = 7;
silentspitransfer(); // and transmits it

rssi = -1* downloadbytearray[2]; // 
//if (packetbytetype != 4) rssi = rssi/2; // means for BLE dont divide by 2 in contrast to all other modes !
rssi = rssi/2; // always divide by 2. then it is a seemingly huge power -35 dBm for devices at 1m distance, but reasonable 
// at 20 m instead of the unreasonable value of -180 (?) dBm at which reception was possible.
Serial.print("RSSI = "); Serial.print(rssi); Serial.println(" dBm");
Serial.print("freq = "); Serial.print(freq); Serial.println(" MHz ");

//getrxbufferstatus(); // get the length according p. 92; also offset but we use the one we define ourselves, ok because single rx
// ccopy paste content make silent:
uploadbytearray[0]=0x17;
uploadbytearray[1]=0x00; // NOP
uploadbytearray[2]=0x00; // NOP
uploadbytearray[3]=0x00; // NOP
xfersiz = 4;
silentspitransfer(); // and transmits it

rpaylen = downloadbytearray[2];
Serial.print("Length extracted from getrxbufferstatus : "); Serial.print(rpaylen); Serial.println(" characters");
// it yields erroneous values, maybe because the header is interpreted as data rather than advertisement ?

// now download the 2 header bytes only and extract the length by tentatively assuming it is an advertisement packet:
uploadbytearray[0]=0x1b;
uploadbytearray[1]=buffbasebytearray[1];
for (int i = 0; i < 3; ++i) { // append 3 0x00 (NOPs), 1 empty and 2 for the 2 header bytes
uploadbytearray[i+2] = 0x00; // NOP
}
xfersiz = 5; // opcode, offset, emptybyte, 2 NOPs
silentspitransfer();
rpaylen = (downloadbytearray[4] & 0b011111100) >> 2 ;
Serial.print("Length extracted from header : "); Serial.print(rpaylen); Serial.println(" characters");

// also extract the pdutype and output it:
rpdutype = (downloadbytearray[3] & 0b11110000) >> 4 ;
Serial.print("PDUtype extracted from header : "); Serial.println(rpdutype);

// with the known length, download the rest of pdu :
Serial.println("Reading PDU content : ");
uploadbytearray[0]=0x1b;
uploadbytearray[1]=buffbasebytearray[1] + 2; // buffer read offset shifted by 2
//append as many 0x00 s (NOPs) to spiupload as the length +1
for (int i = 0; i < rpaylen + 1; ++i) {
uploadbytearray[i+2] = 0x00; // NOP
}
xfersiz = rpaylen + 3;
hexascispitransfer(); // display hex and ascii but not interpret all bytes as if they were status


// and finally download the 3 CRC bytes :
Serial.println("Reading checksum CRC : ");
uploadbytearray[0]=0x1b;
uploadbytearray[1]=buffbasebytearray[1] + 2 + rpaylen; // buffer read offset shifted
for (int i = 0; i < 4; ++i) { // append 4 0x00 (NOPs), 1 empty and 3 for the 3 CRC bytes
uploadbytearray[i+2] = 0x00; // NOP
}
xfersiz = 6;
hexascispitransfer(); // display hex and ascii but not interpret all bytes as if they were status

Serial.println();

} //end if packet received: 



if (txflag) {

// setstandby(); copy paste make silent
uploadbytearray[0]=0x80;
uploadbytearray[1]=0x00; // stdby_rc p.78
xfersiz = 2;
silentspitransfer(); // and transmits it

// write into uploadbytearray 0x1a write command, 0x80 tx offset, and copy of outpdutotal
uploadbytearray[0]=0x1a;
uploadbytearray[1]=0x80;
// outpdutotal has no explicit length but it is paylen+2
for (int i = 0; i < (paylen+2); ++i) {
uploadbytearray[i+2] = outpdutotal[i]; }
xfersiz = paylen + 4; // and with the 2 bytes opcode and offset, it is +4
silentspitransfer();

// clearirqstatus(); copy paste make silent
uploadbytearray[0]=0x97;
xfersiz = 1;
silentspitransfer(); // and transmits it

// settx(); copy paste make silent
uploadbytearray[0]=0x83;
uploadbytearray[1]=0x00; // periodbase
uploadbytearray[2]=0x00; // periodbasecount
uploadbytearray[3]=0x00; // periodbasecount
xfersiz = 4;
silentspitransfer(); // and transmits it

delay(8); //wait some ms
}


if (scanflag) {
stepfreq(); // scans through advertisement channels; this one makes no output

//setrffreq(); copy paste make silent
pllsteps = floor(freq *1e6 / 198.3642578125);
uploadbytearray[0]=0x86;
uploadbytearray[3]= (pllsteps % 256);
pllsteps = pllsteps / 256;
uploadbytearray[2]= (pllsteps % 256);
pllsteps = pllsteps / 256;
uploadbytearray[1]= (pllsteps % 256);
xfersiz = 4;
silentspitransfer(); // and transmits it

}

//return to rx mode
// setrx(); copy paste make silent
uploadbytearray[0]=0x82;
uploadbytearray[1]=0x00; // periodbase
uploadbytearray[2]=0x00; // periodbasecount
uploadbytearray[3]=0x00; // periodbasecount
xfersiz = 4;
silentspitransfer(); // and transmits it

lastSendTime = millis();
} // end if millis() > lastSendTime

 


  // incoming data from serial
  while (Serial.available() && newdata == false) {
    lastchar = millis();
    rc = Serial.read();
    if (rc != '\n') {
      //if (rc != '\r')  //suppress carriage return !
      { message[ndx] = rc;
        ndx++; }
      if (ndx >= numascihex) {
        ndx = numascihex - 1; } }
    else {
      message[ndx] = '\0';
      ndx = 0;
      newdata = true; }
  }
  // put the 2s timeout in case the android terminal cannot terminate
  if (ndx != 0 && millis() - lastchar > 2000) { //if something received and not terminated after 2seconds, timeout
    message[ndx] = '\0';
    ndx = 0;
    newdata = true; }



 if (newdata) { // in this case, everything received from the console is a command. Payload is received by a setblemsg command
  // and transmitting is triggered by a subsequently sent sendbleadv or sendbledat command
 strcpy(outmessage, message); //here it is copied directly without searching for and stripping AT+ etc prefixes
 //Serial.println(message);
 cmdparse(); // proceed to command parsing
 publish(); // publishes outmessage containing command ack
 newdata = false;
 }
  
} // end loop
