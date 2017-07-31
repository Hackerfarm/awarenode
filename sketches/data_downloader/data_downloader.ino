#include <chibi.h>
#include <Wire.h>
#include <Ethernet.h>
#include <stdint.h>

bool received_data = false;

#define ADCREFVOLTAGE 3.3
#define NODE_ID 300
#define RX_BUFSIZE 1000

#define RX_BUFSIZE 1000

#define NODE_ID 333

int ledPin = 4;

unsigned char buf[RX_BUFSIZE];

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 177);

void setup() {
  Serial.begin(57600);
  // ethernet chip select pin
  // do not remove or the radio won't initialize correctly
  pinMode(31, OUTPUT);
  digitalWrite(31, HIGH);

  Serial.println("Data Downloader Arashi v0.1");
  Serial.println("Init chibi stack");


  chibiInit();
  chibiSetShortAddr(NODE_ID);
  Serial.println("Init chibi stack complete");
  pinMode(ledPin, OUTPUT);
  Ethernet.begin(mac, ip);
}

/**************************************************************************/
// Loop
/**************************************************************************/
void loop()
{

  if(!received_data)
  {
      chibiTx(BROADCAST_ADDR, "DUMPDATA", strlen("DUMPDATA")+1);
      delay(100);
  }

  if (chibiDataRcvd() == true)
  { 
    int rssi, src_addr, len;
    len = chibiGetData(buf);
    if (len == 0) {
      Serial.println("Null packet received");
      return;
    }
    Serial.println((char *)(buf));
    received_data = true;
  }
}


