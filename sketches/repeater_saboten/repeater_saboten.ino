#include <chibi.h>
#include <Wire.h>
#include <stdint.h>

int hgmPin = 22;
int ledPin = 18;
int vbatPin = 31;
int vsolPin = 29;
int led_state = 1;

#define ADCREFVOLTAGE 3.3
#define NODE_ID 300
#define RX_BUFSIZE 1000

unsigned char buf[RX_BUFSIZE];

void setup()
{    
	// set up high gain mode pin
  pinMode(hgmPin, OUTPUT);
  digitalWrite(hgmPin, LOW);
  
  // set up battery monitoring
  pinMode(vbatPin, INPUT);
  
  // set up LED
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // Initialize the chibi command line and set the speed to 57600 bps
  chibiCmdInit(57600);
  
  // Initialize the chibi wireless stack
  chibiInit();
  
  // datecode version
  Serial.println("Repeater Saboten v0.1");

  // high gain mode
  digitalWrite(hgmPin, HIGH);

  
}

/**************************************************************************/
// Loop
/**************************************************************************/
void loop()
{
  if (chibiDataRcvd() == true)
  { 
    int rssi, src_addr, len;
    len = chibiGetData(buf);
    if (len == 0) {
      Serial.println("Null packet received");
      return;
    }
    
    // retrieve the data and the signal strength
    /*rssi = chibiGetRSSI();
    src_addr = chibiGetSrcAddr();
    Serial.print("Signal strength: ");
    Serial.print(rssi);
    Serial.print("\t");*/
    if (len)
    {
      Serial.print("Received a packet of length ");
      Serial.println(len);
      chibiTx(BROADCAST_ADDR, (unsigned char*)(&buf), len);

    }
  }
}


