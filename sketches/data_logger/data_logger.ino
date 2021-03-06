#include <avr/sleep.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <chibi.h>
#include <Wire.h>
#include <SPI.h>
#include <SdFat.h>
#include <pcf2127.h>
#include <stdint.h>
#include <OneWire.h>

#include <src/chb_eeprom.h>

typedef struct{
  int32_t sensor_id;
  int32_t value;
} reading_t;



typedef struct{
  reading_t vsol;
  reading_t vbat;
  reading_t temperatureGround;
  reading_t temperatureAir;
  reading_t humidity;
  reading_t distance_to_water_surface;
  int32_t count;
  int32_t signal_strength;
  char timestamp[19];
  int32_t node_id;
} awarenode_packet_t;



awarenode_packet_t main_packet;



#define EDGE_ID BROADCAST_ADDR

#define RTC_CLOCK_SOURCE 0b11 // Selects the clock source. 0b11 selects 1/60Hz clock.
#define RTC_SLEEP 30 // Number of timer clock cycles before interrupt is generated.

#define SBUF_SIZE 200 //Note that due to inefficient implementation of sd_write() the memory footprint is actually twice of this value.

#define EEPROM_CONF_ADDR 0x12

#define DATECODE "31-07-2017"
#define TITLE "SABOTEN 900 MHz Long Range\nDATALOGGER\n"
#define ADCREFVOLTAGE 3.3
#define FILENAME "AWANODE.TXT"

#define OneWireSensor 1
#define SONAR 0

int hgmPin = 22;
int sdCsPin = 15;
int rtcCsPin = 28;
int ledPin = 18;
int sdDetectPin = 19;
int vbatPin = 31;
int vsolPin = 29;
int oneWirePin = 8;
int DH11sensorPin = 5;


#if SONAR
int sonarTriggerPin = 5;
int sonarEchoPin = 7;
int sonarAwakePin = 10;
#endif

int debugModePin = 4;
int burstModePin = 3;


int debug_mode = 0;

// Pins that will not interfer with the SPI: 2 to 5, 7 to 10 + 14 and 15

unsigned char buf[100];

// this is for printf
static FILE uartout = {0};

PCF2127 pcf(0, 0, 0, rtcCsPin);
SdFat sd;
SdFile myFile;

#ifdef OneWireSensor
OneWire OW_temperature_probe(oneWirePin);
#endif






void setup()
{
  // fill in the UART file descriptor with pointer to writer.
  fdev_setup_stream (&uartout, uart_putchar, NULL, _FDEV_SETUP_WRITE);

  // The uart is the standard output device STDOUT.
  stdout = &uartout ;

  // set up high gain mode pin
  pinMode(hgmPin, OUTPUT);
  digitalWrite(hgmPin, LOW);

  // set up rtc chip select
  pinMode(rtcCsPin, OUTPUT);
  digitalWrite(rtcCsPin, HIGH);

  pinMode(sdCsPin, OUTPUT);
  digitalWrite(sdCsPin, HIGH);

  // set up sd card detect
  pinMode(sdDetectPin, INPUT);
  digitalWrite(sdDetectPin, HIGH);

  // set up battery monitoring
  pinMode(vbatPin, INPUT);

  // set up LED
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  //delay(300);
  //digitalWrite(ledPin, LOW);

#if SONAR
  // set up the sonar
  pinMode(sonarTriggerPin, OUTPUT);
  pinMode(sonarEchoPin, INPUT);
  pinMode(sonarAwakePin, OUTPUT);
  digitalWrite(sonarAwakePin, HIGH);
#endif

  // DH11 sensor
  pinMode(DH11sensorPin, INPUT);

  // set up the burst and debug mode pin
  pinMode(debugModePin, INPUT_PULLUP);
  pinMode(burstModePin, INPUT_PULLUP);


  // Initialize the chibi command line and set the speed to 57600 bps
  chibiCmdInit(57600);


//  pcf.enableMinuteInterrupt();
//  pcf.enableSecondInterrupt();
//  pcf.setInterruptToPulse();
  attachInterrupt(2, rtcInterrupt, FALLING);


  // Reads the node and sensors ID from the EEPROM
  cmdReadConf(0,0);

  // set up chibi regs for external P/A
  //chibiRegWrite(0x4, 0xA0);

  // datecode version
  printf(TITLE);
  printf("Datecode: %s\n\r", DATECODE);



  // check for SD and init
  uint8_t sdDetect;
  sdDetect = digitalRead(sdDetectPin);
  if (sdDetect == 0)
  {
    // init the SD card
    if (!sd.begin(sdCsPin))
    {

      Serial.println("Card failed, or not present");
      sd.initErrorPrint();
    }
    printf("SD Card is initialized.\n");
  }
  else
  {
    printf("No SD card detected.\n");
  }



  // This is where you declare the commands for the command line.
  // The first argument is the alias you type in the command line. The second
  // argument is the name of the function that the command will jump to.

  chibiCmdAdd("getsaddr", cmdGetShortAddr);  // set the short address of the node
  chibiCmdAdd("setsaddr", cmdSetShortAddr);  // get the short address of the node
  chibiCmdAdd("send", cmdSend);   // send the string typed into the command line
  chibiCmdAdd("send2", cmd_tx2);
  chibiCmdAdd("rd", cmd_reg_read);   // send the string typed into the command line
  chibiCmdAdd("wr", cmd_reg_write);   // send the string typed into the command line
  chibiCmdAdd("slr", cmdSleepRadio);
  chibiCmdAdd("slm", cmdSleepMcu);
  chibiCmdAdd("time", cmdWriteTime);
  chibiCmdAdd("date", cmdWriteDate);
  chibiCmdAdd("rdt", cmdReadDateTime);
  chibiCmdAdd("bat", cmdVbatRead);
  chibiCmdAdd("sol", cmdVsolRead);
  chibiCmdAdd("tmp", cmdReadTemp);
  chibiCmdAdd("start", cmdStartCycle);
  chibiCmdAdd("rconf", cmdReadConf);
  chibiCmdAdd("wconf", cmdWriteConf);
  chibiCmdAdd("sdw", cmdSdWrite);
  chibiCmdAdd("sdr", cmdSdRead);
  chibiCmdAdd("sdc", cmdSdClear);
  chibiCmdAdd("sdd", cmdDumpData);


  // high gain mode
  digitalWrite(hgmPin, HIGH);
  if(digitalRead(debugModePin)==LOW){
    debug_mode = 1;
#if SONAR
    digitalWrite(sonarAwakePin, HIGH);
#endif
  }

  // Initialize the chibi wireless stack
  chibiSetShortAddr(main_packet.node_id);
  chibiInit();
  chibiSetShortAddr(main_packet.node_id);


}

/**************************************************************************/
// Loop
/**************************************************************************/
void loop()
{
  if(debug_mode==1){
    if(digitalRead(debugModePin)==HIGH){
      debug_mode = 0;
    }
    chibiCmdPoll();
    return;
  }

  #if SONAR
  digitalWrite(sonarAwakePin, HIGH);
  #endif

#ifdef OneWireSensor
  get_temp_ow(main_packet.temperatureGround.value);
#else

#endif
  get_temp(main_packet.temperatureAir.value, main_packet.humidity.value);
  get_vbat(main_packet.vbat.value);
  get_vsol(main_packet.vsol.value);
  #if SONAR
  //get_sonar(main_packet.distance_to_water_surface.value);
  delay(1000);
  get_sonar(main_packet.distance_to_water_surface.value);
  /*if(r.sonar.value>10){
    digitalWrite(sonarAwakePin, LOW);
  }
  else{
    digitalWrite(sonarAwakePin, HIGH);
  }*/
#endif
  main_packet.count++;
  get_timestamp(main_packet.timestamp);

  char sbuf[SBUF_SIZE];
  sprintf(sbuf, "Port: %d Bit: %d",digitalPinToPort(hgmPin), digitalPinToBitMask(hgmPin));
  Serial.println(sbuf);


  sprintf(sbuf, "Node_id: %d, count: %d, timestamp: %19s, id %d: %dC (temperatureG), id %d: %dC (temperatureA), id %d: %d (humidity), id %d: %dmV (battery), id %d: %dmV (solar), id %d: %d cm (water level)",
                (int) main_packet.node_id,
                (int) main_packet.count,
                (int) main_packet.timestamp,
                (int) main_packet.temperatureGround.sensor_id, (int) main_packet.temperatureGround.value,
                (int) main_packet.temperatureAir.sensor_id, (int) main_packet.temperatureAir.value,
                (int) main_packet.humidity.sensor_id, (int) main_packet.humidity.value,
                (int) main_packet.vbat.sensor_id, (int) main_packet.vbat.value,
                (int) main_packet.vsol.sensor_id, (int) main_packet.vsol.value,
                (int) main_packet.distance_to_water_surface.sensor_id, (int) main_packet.distance_to_water_surface.value);
  Serial.println(sbuf);
  //chibiTx(EDGE_ID, (unsigned char*)(&main_packet), sizeof(main_packet));

    // Write to SD card
  sprintf(sbuf, "%s|%d|%d=%d;%d=%d;%d=%d;%d=%d;%d=%d;\n",
          (int)main_packet.timestamp,
          (int)main_packet.count,
          (int)main_packet.temperatureGround.sensor_id, (int) main_packet.temperatureGround.value,
          (int)main_packet.temperatureAir.sensor_id, (int) main_packet.temperatureAir.value,
          (int)main_packet.humidity.sensor_id, (int) main_packet.humidity.value,
          (int)main_packet.vbat.sensor_id, (int)main_packet.vbat.value,
          (int)main_packet.vsol.sensor_id, (int)main_packet.vsol.value);
  SdWriteBuf(sbuf);
  Serial.println(sbuf);

  //Check for radio commands
  /*if (chibiDataRcvd() == true)
  {
    int rssi, src_addr, len;
    len = chibiGetData(buf);
    if (len == 0) {
      Serial.println("Null packet received");
      return;
    }
    if (strcmp(buf, "DUMPDATA")==0)
    {
        cmdDumpData();
    }

    // retrieve the data and the signal strength
    rssi = chibiGetRSSI();
    src_addr = chibiGetSrcAddr();
  }*/

  cmdDumpData();

  free(sbuf);
  sleep_mcu();
}


void rtcInterrupt(){
  Serial.println("Interrupt");
  detachInterrupt(2);
}

void sleep_radio(){
  digitalWrite(hgmPin, LOW);
  // set up chibi regs to turn off external P/A
  chibiRegWrite(0x4, 0x20);
  chibiSleepRadio(1);
}

void wakeup_radio(){
  chibiSleepRadio(0);
  digitalWrite(hgmPin, HIGH);
  // set up chibi regs to turn on external P/A
  chibiRegWrite(0x4, 0xA0);


}

void sleep_mcu(){
  attachInterrupt(2, rtcInterrupt, FALLING);
#if SONAR
  digitalWrite(sonarAwakePin, LOW);
  pinMode(sonarTriggerPin, INPUT);
#endif

#if OneWireSensor
  pinMode(oneWirePin, INPUT);
#endif
  pinMode(debugModePin, INPUT);
  pinMode(burstModePin, INPUT);

  delay(1000);

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_radio();
  sleep_enable();        // setting up for sleep ...

  ADCSRA &= ~(1 << ADEN);    // Disable ADC
  Serial.println("Going to sleep");
  delay(1000);
  digitalWrite(ledPin, LOW);


  if(digitalRead(burstModePin)==HIGH){
    // Every 30 minutes
    pcf.runWatchdogTimer(0b11,30);
    Serial.println("Sleeping for 30 minutes");
    Serial.flush();
  }
  else{
    // Every 10 seconds
    pcf.runWatchdogTimer(0b10,10);
    Serial.println("Sleeping for 10 seconds");
    Serial.flush();
  }

  //pcf.runWatchdogTimer(RTC_CLOCK_SOURCE, RTC_SLEEP);

  sleep_mode();
  /* ....ZZzzzzZZzzzZZZzz....*/
  sleep_disable();
  Serial.println("Awake");
  digitalWrite(ledPin, HIGH);

#if SONAR
  pinMode(sonarTriggerPin, OUTPUT);
  digitalWrite(sonarAwakePin, HIGH);
#endif

  wakeup_radio();
  ADCSRA |= (1 << ADEN); // Enable ADC
  // Let the sonar "boot up"
  delay(5000);
  pinMode(debugModePin, INPUT_PULLUP);
  pinMode(burstModePin, INPUT_PULLUP);
#if OneWireSensor
  pinMode(oneWirePin, INPUT_PULLUP);
#endif
}


void get_timestamp(char tbuf[19])
{
  uint8_t hours, minutes, seconds;
  uint8_t year, month, day, weekday;

  pcf.readTime(&hours, &minutes, &seconds);
  pcf.readDate(&year, &month, &day, &weekday);

//  char tbuf[19];
  sprintf(tbuf, "%u-%u-%u %u:%u:%u", year, month, day, hours, minutes, seconds);
  Serial.println(tbuf);
//  Serial.println("Year: %d, Month: %d, Day: %d, Weekday: %d Hours: %d, Minutes: %d, Seconds: %d\n", year, month, day, weekday, hours, minutes, seconds);
}

void init_datetime(int arg_cnt, char **args)
{
  uint8_t year, month, day, weekday;

  year = chibiCmdStr2Num(args[1], 10);
  month = chibiCmdStr2Num(args[2], 10);
  day = chibiCmdStr2Num(args[3], 10);
  weekday = chibiCmdStr2Num(args[4], 10);
  pcf.writeDate(2016, 1, 24, 6);

  pcf.readDate(&year, &month, &day, &weekday);
  printf("Year: %d, Month: %d, Day: %d, Weekday: %d\n", year, month, day, weekday);
}



/**************************************************************************/
// USER FUNCTIONS
/**************************************************************************/

/**************************************************************************/
/*!
    Get short address of device from EEPROM
    Usage: getsaddr
*/
/**************************************************************************/
void cmdGetShortAddr(int arg_cnt, char **args)
{
  int val;

  val = chibiGetShortAddr();
  Serial.print("Short Address: "); Serial.println(val, HEX);
}

/**************************************************************************/
/*!
    Write short address of device to EEPROM
    Usage: setsaddr <addr>
*/
/**************************************************************************/
void cmdSetShortAddr(int arg_cnt, char **args)
{
  int val;

  val = chibiCmdStr2Num(args[1], 16);
  chibiSetShortAddr(val);
}

/**************************************************************************/
/*!
    Instructs the board to leave the debug mode and start the regular power
    cycle
    Usage: start
*/
/**************************************************************************/
void cmdStartCycle(int arg_cnt, char **args)
{
  debug_mode = 0;
}

/**************************************************************************/
/*!
    Transmit data to another node wirelessly using Chibi stack. Currently
    only handles ASCII string payload
    Usage: send <addr> <string...>
*/
/**************************************************************************/
void cmdSend(int arg_cnt, char **args)
{
    byte data[100];
    int addr, len;

    // convert cmd line string to integer with specified base
    addr = chibiCmdStr2Num(args[1], 16);

    // concatenate strings typed into the command line and send it to
    // the specified address
    len = strCat((char *)data, 2, arg_cnt, args);
    chibiTx(addr, data,len);
}

/**************************************************************************/
/*!
    Reads the configuration of the node's ids in the eeprom and prints it.
    Each id is a 32 unsigned integer, taking 4 bytes.
    Usage: rconf
*/
/**************************************************************************/
void cmdReadConf(int arg_cnt, char** args)
{
  uint16_t addr = EEPROM_CONF_ADDR;
  chb_eeprom_read(addr, (U8 *)(&(main_packet.node_id)), 4); addr+=4;
  chb_eeprom_read(addr, (U8 *)(&(main_packet.vsol.sensor_id)), 4); addr+=4;
  chb_eeprom_read(addr, (U8 *)(&(main_packet.vbat.sensor_id)), 4); addr+=4;
  chb_eeprom_read(addr, (U8 *)(&(main_packet.temperatureGround.sensor_id)), 4); addr+=4;
  chb_eeprom_read(addr, (U8 *)(&(main_packet.humidity.sensor_id)), 4); addr+=4;
  chb_eeprom_read(addr, (U8 *)(&(main_packet.distance_to_water_surface.sensor_id)), 4); addr+=4;
  char sbuf[SBUF_SIZE];
  sprintf(sbuf, "Node_id: %d\n\r  vsol_id: %d\n\r  vbat_id: %d\n\r  temperature_id: %d\n\r  humidity_id: %d\n\r  sonar_id: %d",
                (int) main_packet.node_id,
                (int) main_packet.vsol.sensor_id,
                (int) main_packet.vbat.sensor_id,
                (int) main_packet.temperatureGround.sensor_id,
                (int) main_packet.humidity.sensor_id,
                (int) main_packet.distance_to_water_surface.sensor_id);
  Serial.println(sbuf);
}

/**************************************************************************/
/*!
    Writes the configuration of the node's ids in the eeprom and prints it.
    Each id is a 32 unsigned integer, taking 4 bytes.
    Usage: wconf node_id vsol_id vbat_id temperature_id humidity_id sonar_id
*/
/**************************************************************************/
void cmdWriteConf(int arg_cnt, char** args)
{
  main_packet.node_id =           strtol(args[1], NULL, 10);
  main_packet.vsol.sensor_id =    strtol(args[2], NULL, 10);
  main_packet.vbat.sensor_id =    strtol(args[3], NULL, 10);
  main_packet.temperatureGround.sensor_id =               strtol(args[4], NULL, 10);
  main_packet.humidity.sensor_id =                  strtol(args[5], NULL, 10);
  main_packet.distance_to_water_surface.sensor_id = strtol(args[6], NULL, 10);
  uint16_t addr = EEPROM_CONF_ADDR;
  chb_eeprom_write(addr, (U8 *)(&(main_packet.node_id)), 4); addr+=4;
  chb_eeprom_write(addr, (U8 *)(&(main_packet.vsol.sensor_id)), 4); addr+=4;
  chb_eeprom_write(addr, (U8 *)(&(main_packet.vbat.sensor_id)), 4); addr+=4;
  chb_eeprom_write(addr, (U8 *)(&(main_packet.temperatureGround.sensor_id)), 4); addr+=4;
  chb_eeprom_write(addr, (U8 *)(&(main_packet.humidity.sensor_id)), 4); addr+=4;
  chb_eeprom_write(addr, (U8 *)(&(main_packet.distance_to_water_surface.sensor_id)), 4); addr+=4;
}

/**************************************************************************/
/*!

*/
/**************************************************************************/
// Gives a mV result
bool get_vbat(int32_t &battery)
{
  unsigned int vbat = analogRead(vbatPin);
  battery = (int32_t)(((vbat/1023.0) * ADCREFVOLTAGE) * 2000);
  return true;
}

void cmdVbatRead(int arg_cnt, char **args)
{
  int32_t b;
  get_vbat(b);
  Serial.print("Battery voltage: "); Serial.println((float)(b/1000.0), 1);
}

/**************************************************************************/
/*!

*/
/**************************************************************************/
// gives a mV result
bool get_vsol(int32_t &solar)
{
  unsigned int vsol = analogRead(vsolPin);
  solar = (int32_t)(((vsol/1023.0) * ADCREFVOLTAGE) * 2000);
}

void cmdVsolRead(int arg_cnt, char **args)
{
  int32_t s;
  get_vsol(s);
  Serial.print("Solar voltage: "); Serial.println((float)(s/1000.0), 1);
}


/**************************************************************************/
/*!
    Read radio registers via SPI.
    Usage: rd <addr>
*/
/**************************************************************************/
void cmd_reg_read(int arg_cnt, char **args)
{
    uint8_t addr, val;

    addr = strtol(args[1], NULL, 16);
    val = chibiRegRead(addr);

    sprintf((char *)buf, "Reg Read: %04X, %02X.\n", addr, val);
    Serial.print((char *)buf);
}

/**************************************************************************/
/*!
    Write radio registers via SPI
    Usage: wr <addr> <val>
*/
/**************************************************************************/
void cmd_reg_write(int arg_cnt, char **args)
{
    uint8_t addr, val;

    addr = strtol(args[1], NULL, 16);
    val = strtol(args[2], NULL, 16);

    chibiRegWrite(addr, val);
    sprintf((char *)buf, "Write: %04X, %02X.\n", addr, val);
    Serial.print((char *)buf);

    val = chibiRegRead(addr);
    sprintf((char *)buf, "Readback: %04X, %02X.\n", addr, val);
    Serial.print((char *)buf);
}


/**************************************************************************/
//
/**************************************************************************/
void cmdSleepMcu(int arg_cnt, char **args)
{
  printf("Sleeping MCU\n");
  delay(100);

  // set pullups on inputs
//  pinMode(sdCsPin, INPUT);
//nn  digitalWrite(sdCsPin, HIGH);

//  pinMode(sdDetectPin, INPUT);
  digitalWrite(sdDetectPin, LOW);

  digitalWrite(ledPin, LOW);

  attachInterrupt(2, rtcInterrupt, FALLING);
  delay(100);

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  chibiSleepRadio(0);
  sleep_enable();        // setting up for sleep ...

  ADCSRA &= ~(1 << ADEN);    // Disable ADC
  sleep_mode();

  sleep_disable();
  ADCSRA |= (1 << ADEN);
  digitalWrite(ledPin, HIGH);
  chibiSleepRadio(1);
}

/**************************************************************************/
//
/**************************************************************************/
void cmdSleepRadio(int arg_cnt, char **args)
{
  int val = strtol(args[1], NULL, 10);

  if (val)
  {
    digitalWrite(hgmPin, LOW);

    // set up chibi regs to turn off external P/A
    chibiRegWrite(0x4, 0x20);
  }
  else
  {
    digitalWrite(hgmPin, HIGH);

    // set up chibi regs to turn on external P/A
    chibiRegWrite(0x4, 0xA0);
  }

  // turn on/off radio
  chibiSleepRadio(val);
}

/**************************************************************************/
//
/**************************************************************************/


void cmdReadDateTime(int arg_cnt, char **args)
{
  uint8_t hours, minutes, seconds;
  uint8_t year, month, day, weekday;

  pcf.readTime(&hours, &minutes, &seconds);
  pcf.readDate(&year, &month, &day, &weekday);

  printf("Year: %d, Month: %d, Day: %d, Weekday: %d Hours: %d, Minutes: %d, Seconds: %d\n", year, month, day, weekday, hours, minutes, seconds);
}

/**************************************************************************/
//
/**************************************************************************/
void cmdWriteTime(int arg_cnt, char **args)
{
  uint8_t hours, minutes, seconds;

  hours = chibiCmdStr2Num(args[1], 10);
  minutes = chibiCmdStr2Num(args[2], 10);
  seconds = chibiCmdStr2Num(args[3], 10);
  pcf.writeTime(hours, minutes, seconds);

  pcf.readTime(&hours, &minutes, &seconds);
  printf("Hours: %d, Minutes: %d, Seconds: %d\n", hours, minutes, seconds);
}

/**************************************************************************/
//
/**************************************************************************/
void cmdWriteDate(int arg_cnt, char **args)
{
  uint8_t year, month, day, weekday;

  year = chibiCmdStr2Num(args[1], 10);
  month = chibiCmdStr2Num(args[2], 10);
  day = chibiCmdStr2Num(args[3], 10);
  weekday = chibiCmdStr2Num(args[4], 10);
  pcf.writeDate(year, month, day, weekday);

  pcf.readDate(&year, &month, &day, &weekday);
  printf("Year: %d, Month: %d, Day: %d, Weekday: %d\n", year, month, day, weekday);
}

#if SONAR
bool get_sonar(int32_t &distance){
  int32_t duration;
  digitalWrite(sonarTriggerPin, LOW);
  delayMicroseconds(2);
  digitalWrite(sonarTriggerPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(sonarTriggerPin, LOW);
  duration = pulseIn(sonarEchoPin, HIGH);

  distance = (duration/2) / 29.1;
  if (distance >= 200){
    distance = 200;
  }
  if (distance < 0){
    distance = 0;
  }

  /*digitalWrite(ledPin, HIGH);
  delay(distance*10);
  digitalWrite(ledPin, LOW);*/

}
#endif

#ifdef OneWireSensor
bool get_temp_ow(int32_t &temperature){
    byte addr[8];
    byte present = 0;
    byte data[12];

    if ( !OW_temperature_probe.search(addr)) {
      Serial.println("No more addresses.");
      Serial.println();
      OW_temperature_probe.reset_search();
      delay(250);
      temperature=0;
      OW_temperature_probe.search(addr);
    }
    Serial.print("ROM =");
    for( int i = 0; i < 8; i++) {
      Serial.write(' ');
      Serial.print(addr[i], HEX);
    }

    OW_temperature_probe.reset();
    OW_temperature_probe.select(addr);
    OW_temperature_probe.write(0x44, 1);
    delay(1000);
    present = OW_temperature_probe.reset();
    OW_temperature_probe.select(addr);
    OW_temperature_probe.write(0xBE);         // Read Scratchpad
    Serial.print("  Data = ");
    Serial.print(present, HEX);
    Serial.print(" ");
    for ( int i = 0; i < 9; i++) {           // we need 9 bytes
      data[i] = OW_temperature_probe.read();
      Serial.print(data[i], HEX);
      Serial.print(" ");
    }
    Serial.print(" CRC=");
    Serial.print(OneWire::crc8(data, 8), HEX);
    Serial.println();

    int16_t raw = (data[1] << 8) | data[0];
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
    float celsius = (float)raw / 16.0;
    Serial.print("  Temperature = ");
    Serial.println(celsius);
    temperature = (int)(celsius*100);
    return true;
}
#endif

bool get_temp(int32_t &temperature, int32_t& humidity){

    // It's ugly, isn't it?
    int pin = DH11sensorPin;
    int ret = 0;
    uint8_t bits[5];
    const int DHTLIB_TIMEOUT = (F_CPU/40000);

    // INIT BUFFERVAR TO RECEIVE DATA
    uint8_t mask = 128;
    uint8_t idx = 0;

    uint8_t bit = digitalPinToBitMask(pin);
    uint8_t port = digitalPinToPort(pin);
    volatile uint8_t *PIR = portInputRegister(port);

    // EMPTY BUFFER
    for (uint8_t i = 0; i < 5; i++) bits[i] = 0;

    // REQUEST SAMPLE
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW); // T-be
    delay(18);  //delay(wakeupDelay);
    digitalWrite(pin, HIGH);   // T-go
    delayMicroseconds(40);
    pinMode(pin, INPUT);

    // GET ACKNOWLEDGE or TIMEOUT
    uint16_t loopCntLOW = DHTLIB_TIMEOUT;
    while ((*PIR & bit) == LOW )  // T-rel
    {
        if (--loopCntLOW == 0) return false;
    }

    uint16_t loopCntHIGH = DHTLIB_TIMEOUT;
    while ((*PIR & bit) != LOW )  // T-reh
    {
        if (--loopCntHIGH == 0) return false;
    }

    // READ THE OUTPUT - 40 BITS => 5 BYTES
    for (uint8_t i = 40; i != 0; i--)
    {
        loopCntLOW = DHTLIB_TIMEOUT;
        while ((*PIR & bit) == LOW )
        {
            if (--loopCntLOW == 0) return false;
        }

        uint32_t t = micros();

        loopCntHIGH = DHTLIB_TIMEOUT;
        while ((*PIR & bit) != LOW )
        {
            if (--loopCntHIGH == 0) return false;
        }

        if ((micros() - t) > 40)
        {
            bits[idx] |= mask;
        }
        mask >>= 1;
        if (mask == 0)   // next byte?
        {
            mask = 128;
            idx++;
        }
    }
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
    humidity = bits[0];
    temperature = bits[2];
    return true;
}



/**************************************************************************/
/*!
    Reads temperature and humidity from the sensor and displays it
    Usage: tmp
*/
/**************************************************************************/
void cmdReadTemp(int arg_cnt, char **args)
{
  int32_t t, h;
  get_temp(t, h);

    Serial.print("Humidity: ");
    Serial.println(h);
    Serial.print("Temperature: ");
    Serial.println(t);
}

/**************************************************************************/
/*!
    Transmit data to another node wirelessly using Chibi stack.
    Usage: send <addr> <string...>
*/
/**************************************************************************/
void cmd_tx2(int arg_cnt, char **args)
{
    unsigned char data[6];
    int i, addr, len;

    addr = chibiCmdStr2Num(args[1], 16);

    for (i=0; i<1000; i++)
    {
      sprintf((char *)data, "%04d", i);
      data[4] = '\0';
      //data[0] = i;
      //data[1] = i+1;
      chibiTx(addr, data, 5);
      delay(10);
    }
}


/**************************************************************************/
/*!
    Concatenate multiple strings from the command line starting from the
    given index into one long string separated by spaces.
*/
/**************************************************************************/
int strCat(char *buf, unsigned char index, char arg_cnt, char **args)
{
    uint8_t i, len;
    char *data_ptr;

    data_ptr = buf;
    for (i=0; i<arg_cnt - index; i++)
    {
        len = strlen(args[i+index]);
        strcpy((char *)data_ptr, (char *)args[i+index]);
        data_ptr += len;
        *data_ptr++ = ' ';
    }
    *data_ptr++ = '\0';

    return data_ptr - buf;
}

/**************************************************************************/
// This is to implement the printf function from within arduino
/**************************************************************************/
static int uart_putchar (char c, FILE *stream)
{
    Serial.write(c);
    return 0;
}



/**************************************************************************/
/*!

*/
/**************************************************************************/
void cmdSdWrite(int arg_cnt, char **args)
{
    char data[1000];

    // concatenate strings typed into the command line and send it to
    // the specified address
    strCat(data, 1, arg_cnt, args);

    SdWriteBuf(data);
}

/**************************************************************************/
/*!

*/
/**************************************************************************/
void SdWriteBuf(char *data)
{
    if (myFile.open(FILENAME, O_RDWR | O_CREAT | O_AT_END))

    {
      myFile.write(data);
      myFile.close();
    }
    else
    {
      printf("Error opening dataFile\n");
    }
}


/**************************************************************************/
/*!

*/
/**************************************************************************/
bool SdReadLine(SdFile& file, char* buf, int maxlen=200)
{
    char* cur = buf;
    int ind=0;
    while(1)
    {
        int16_t r = file.read();
        if(r==-1)
            return false;
        *cur = (uint8_t)(r);
        if(*cur=='\n')
        {
            *cur=0;
            return true;
        }
        cur++;
        ind++;
        if(ind==maxlen) return false;
    }
}


void cmdSdRead()
{
    if (myFile.open(FILENAME, O_READ))
    {
        bool ok=true;
        while(ok)
        {
            char data[201];
            ok = SdReadLine(myFile, (char *)data);
            if(ok) Serial.println((char *)data);
        }
        myFile.close();
    }
    else
    {
      printf("Error opening dataFile\n");
    }
}


/**************************************************************************/
/*!

*/
/**************************************************************************/
void cmdSdClear()
{
    if (myFile.open(FILENAME, O_RDWR | O_CREAT | O_TRUNC))
    {
      myFile.close();
    }
    else
    {
      printf("Error opening dataFile\n");
    }
}

/**************************************************************************/
/*!

*/
/**************************************************************************/

void cmdDumpData()
{
    Serial.println("Starting data dump");
    if (myFile.open(FILENAME, O_READ))
    {
        bool ok=true;
        while(ok)
        {
            char data[201];
            ok = SdReadLine(myFile, (char *)data);
            if(ok){
                chibiTx(EDGE_ID, (unsigned char*)(&data), strlen(data)+1);
                Serial.println((char *)data);
            }
        }
        myFile.close();
    }
    else
    {
      printf("Error opening dataFile\n");
    }
}
