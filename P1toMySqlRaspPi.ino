/* Arduino 'slimme meter' P1-port reader.
 
 This sketch reads data from a Dutch smart meter that is equipped with a P1-port.
 Connect 'RTS' from meter to Arduino pin 5
 Connect 'GND' from meter to Arduino GND
 Connect 'RxD' from meter to Arduino pin 0 (RX)
 
 Baudrate 115200, 8N1.
 BS170 transistor & 10k resistor is needed to make data readable if meter spits out inverted data
 
 A .php file is requested (with consumption numbers in the GET request) every minute (interval set at line #52)
 created by 'ThinkPad' @ Tweakers.net, september 2014
 
 http://gathering.tweakers.net/forum/list_messages/1601301
 */

#include <AltSoftSerial.h>
#include <SPI.h>
#include <Ethernet.h>
// AltSoftSerial always uses these pins:
//
// Board          Transmit  Receive   PWM Unusable
// -----          --------  -------   ------------
// Teensy 2.0         9        10       (none)
// Teensy++ 2.0      25         4       26, 27
// Arduino Uno        9         8         10
// Arduino Mega      46        48       44, 45
// Wiring-S           5         6          4
// Sanguino          13        14         12

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0x30, 0x32, 0x31};
IPAddress ip(192,168,2,176);
IPAddress server(192,168,2,29);
EthernetClient client;
AltSoftSerial altSerial;

const int requestPin =  5;         
char input; // incoming serial data (byte)
bool readnextLine = false;
#define BUFSIZE 75
char buffer[BUFSIZE]; //Buffer for serial data to find \n .
int bufpos = 0;
long mEVLT = 0; //Meter reading Electrics - consumption low tariff
long mEVHT = 0; //Meter reading Electrics - consumption high tariff
long mEAV = 0;  //Meter reading Electrics - Actual consumption
long mELLT = 0; //Meter reading Electrics - consumption low tariff
long mELHT = 0; //Meter reading Electrics - consumption high tariff
long mEAL = 0;  //Meter reading Electrics - Actual consumption
long mG = 0;   //Meter reading Gas

long lastTime = 0;        // will store last time 
long interval = 60000;           // interval at which to blink (milliseconds)

void setup() {
  Serial.begin(115200);
  altSerial.begin(9600);
  delay(1000);
  Ethernet.begin(mac, ip);
  delay(1000);
  pinMode(4, OUTPUT);                  // SD select pin
  digitalWrite(4, HIGH);               // Explicitly disable SD

  //Set RTS pin high, so smart meter will start sending telegrams
  pinMode(requestPin, OUTPUT);
  digitalWrite(requestPin, HIGH);
}

void loop() {

  decodeTelegram();

  if(millis() - lastTime > interval) {
    lastTime = millis();   
    //send data to PHP/MySQL
    httpRequest();
    //Reset variables to zero for next run
    mEVLT = 0;
    mEVHT = 0;
    mEAV = 0;
    mG = 0;
    //Stop Ethernet
    client.stop();
  }
} //Einde loop

void decodeTelegram() {
  long tl = 0;
  long tld =0;

  if (altSerial.available()) {
    input = altSerial.read();
    char inChar = (char)input;
    // Fill buffer up to and including a new line (\n)
    buffer[bufpos] = input&127;
    bufpos++;

    if (input == '\n') { // We received a new line (data up to \n)
      if (sscanf(buffer,"1-0:1.8.1(%ld.%ld" ,&tl, &tld)==2){
        tl *= 1000;
        tl += tld;
        mEVLT = tl;
      }

      // 1-0:1.8.2 = Elektra verbruik hoog tarief (DSMR v4.0)
      if (sscanf(buffer,"1-0:1.8.2(%ld.%ld" ,&tl, &tld)==2){
        tl *= 1000;
        tl += tld;
        mEVHT = tl;
      }

      // 1-0:1.7.0 = Electricity consumption actual usage (DSMR v4.0)
      if (sscanf(buffer,"1-0:1.7.0(%ld.%ld" ,&tl , &tld) == 2)
      { 
        mEAV = (tl*1000)+(tld*10);
      }     
      
      if (sscanf(buffer,"1-0:2.8.1(%ld.%ld" ,&tl, &tld)==2){
        tl *= 1000;
        tl += tld;
        mELLT = tl;
      }

      // 1-0:1.8.2 = Elektra verbruik hoog tarief (DSMR v4.0)
      if (sscanf(buffer,"1-0:2.8.2(%ld.%ld" ,&tl, &tld)==2){
        tl *= 1000;
        tl += tld;
        mELHT = tl;
      }

      // 1-0:1.7.0 = Electricity consumption actual usage (DSMR v4.0)
      if (sscanf(buffer,"1-0:2.7.0(%ld.%ld" ,&tl , &tld) == 2)
      { 
        mEAL = (tl*1000)+(tld*10);
      }
            
      
      
      // 0-1:24.2.1 = Gas (DSMR v4.0) on Kaifa MA105 meter
      if (strncmp(buffer, "0-1:24.2.1(m3)", strlen("0-1:24.2.1(m3)")) == 0) {
    
      // 0-1:24.2.1 = Gas (DSMR v4.0) on Kaifa MA105 meter
      // if (strncmp(buffer, "0-1:24.2.1", strlen("0-1:24.2.1")) == 0) {
        if (sscanf(strrchr(buffer, '(') + 1, "%d.%d", &tl, &tld) == 2) {
          mG = (tl*1000)+tld; 
        }
      }

      // Empty buffer again (whole array)
      for (int i=0; i<75; i++)
      { 
        buffer[i] = 0;
      }
      bufpos = 0;
    }
  } //Einde 'if AltSerial.available'
} //Einde 'decodeTelegram()' functie

void httpRequest() {
  // if there's a successful connection:
  
  if (client.connect(server, 80)) {
    client.print("GET /p1.php?mEVLT=");
    client.print(mEVLT);
    client.print("&mEVHT=");
    client.print(mEVHT);
    client.print("&mEAV=");
    client.print(mEAV);
    client.print("&mELLT=");
    client.print(mELLT);
    client.print("&mELHT=");
    client.print(mELHT);
    client.print("&mEAL=");
    client.print(mEAL);    
    client.print("&mG=");
    client.print(mG);
    client.println(" HTTP/1.1");
    client.println("Host: 192.168.2.29");
    client.println("User-Agent: arduino-ethernet");
    client.println("Connection: close");
    client.println();
    //Request complete; empty recieve buffer
    while (client.available()) { //data available
      char c = client.read(); //gets byte from ethernet buffer
    }
    client.println();
  } 
  else {
    client.stop();
  }
}
