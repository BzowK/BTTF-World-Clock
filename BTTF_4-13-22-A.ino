/*==========================================
  - 4-13-22-A
    - Complete
      - NTP Working
      - WebSerial Working (http://ESP32IP/WebSerial)
      - ArduinoOTA Working
      - Displays correct date/time
      - Code cleaned up
    - To Do
      - Import + parse iCalendar
      - Configure Red / Orange displays with iCal data

==========================================
*/

#include "SevenSegmentTM1637.h"
#include "SevenSegmentExtended.h"
#include <WiFi.h>
#include <EasyNTPClient.h>
#include <TimeLib.h>                 
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSerial.h>

#define STASSID "Kellermanns"
#define STAPSK  "gateway56"
const char* ssid     = STASSID;
const char* password = STAPSK;

// WebSerial
AsyncWebServer server(80);

void recvMsg(uint8_t *data, size_t len){
  WebSerial.println("Received Data...");
  String d = "";
  for(int i=0; i < len; i++){
    d += char(data[i]);
  }
  WebSerial.println(d);
}

// NTP
#define NTP_SERVER   "ch.pool.ntp.org"
#define TZ           -6                  // (utc+) TZ in hours
#define TZ_SEC       ((TZ)*3600)        // remark: European DST will be calculated in the code
#define UTC_OFFSET + TZ
WiFiUDP udp;
EasyNTPClient ntpClient(udp, NTP_SERVER, TZ_SEC);  //calls NTP for current timezone; summertime_EU() does the rest

void capturetime();
void showtime(byte m [4], int w);
void showdigit(byte i, byte digit, int w);
void go_online();
void get_NTP_time();
boolean summertime_EU(int year, byte month, byte day, byte hour, byte tzHours);

byte momentdisplay [4];                  // time shown in display
byte moment [4];                         // to store actual time in 4 digits
unsigned long timestamp;                 // timestamp to measure time past since last update on NTP server

int backlight = 25; // The brightness setting
int maxbacklight = 50; // The brightness setting
int year_red = 2128; // The year you wish to see on the top section
int year_orange = 1976; // The year you wish to see on the bottom section

// Red Clock Pin
const byte PIN_CLK_Red = 18;   // define CLK pin  
// Green Clock Pin
const byte PIN_CLK_Green = 15;   // define CLK pin 
// Orange Clock Pin
const byte PIN_CLK_Orange = 14;   // define CLK pin 

// GREEN Displays - (NTP Time)
const byte PIN_DIO_G1 = 2;
SevenSegmentExtended      green1(PIN_CLK_Green, PIN_DIO_G1);
const byte PIN_DIO_G2 = 4;
SevenSegmentTM1637       green2(PIN_CLK_Green, PIN_DIO_G2);
const byte PIN_DIO_G3 = 16;
SevenSegmentExtended     green3(PIN_CLK_Green, PIN_DIO_G3);
int greenAM = 17;
int greenPM = 5;

// RED Displays - (Top Clock)
const byte PIN_DIO_R1 = 19;
SevenSegmentExtended      red1(PIN_CLK_Red, PIN_DIO_R1);
const byte PIN_DIO_R2 = 21;
SevenSegmentTM1637       red2(PIN_CLK_Red, PIN_DIO_R2);
const byte PIN_DIO_R3 = 23;
SevenSegmentExtended     red3(PIN_CLK_Red, PIN_DIO_R3);
int redAM = 22;
int redPM = 12;

// ORANGE Displays - (Bottom Clock)
const byte PIN_DIO_O1 = 27;   // define DIO pin (any digital pin)
SevenSegmentExtended      orange1(PIN_CLK_Orange, PIN_DIO_O1);
const byte PIN_DIO_O2 = 26;   
SevenSegmentTM1637        orange2(PIN_CLK_Orange, PIN_DIO_O2);
const byte PIN_DIO_O3 = 33;   
SevenSegmentExtended       orange3(PIN_CLK_Orange, PIN_DIO_O3); 
int orangeAM = 32;
int orangePM = 35;

/* Useful Constants */
#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24L)

int minutesBetweenDataRefresh = 15;  // Time in minutes between data refresh (default 15 minutes)
boolean IS_24HOUR = false; // 23:00 millitary 24 hour clock
int minutesBetweenRefreshing = 1; 
//boolean flashOnSeconds = true; // when true the : character in the time will flash on and off as a seconds indicator

// LED Settings
const int offset = 1;
int refresh = 0;

// OTA
boolean ENABLE_OTA = true;    // this will allow you to load firmware to the device over WiFi (see OTA for ESP8266)
String OTA_Password = "";     // Set an OTA password here -- leave blank if you don't want to be prompted for password
#define HOSTNAME "BTTFClock"


void setup() {
  Serial.begin(115200);
  
  // Go Online
  go_online();
                       
  // WebSerial  
  WebSerial.begin(&server);
  WebSerial.msgCallback(recvMsg);
  server.begin();

  // NTP
  get_NTP_time();                        // get precise time from time server
  delay(10);
  Serial.println();

  // Booting up the displays
  pinMode(PIN_CLK_Red, OUTPUT);
  pinMode(PIN_CLK_Green, OUTPUT);
  pinMode(PIN_CLK_Orange, OUTPUT);
  pinMode(PIN_DIO_O1, OUTPUT);
  pinMode(PIN_DIO_O2, OUTPUT);
  pinMode(PIN_DIO_O3, OUTPUT);
  pinMode(PIN_DIO_G1, OUTPUT); 
  pinMode(PIN_DIO_G2, OUTPUT);
  pinMode(PIN_DIO_G3, OUTPUT); 
  pinMode(PIN_DIO_R1, OUTPUT);
  pinMode(PIN_DIO_R2, OUTPUT);
  pinMode(PIN_DIO_R3, OUTPUT);
  pinMode(greenAM, OUTPUT);
  pinMode(greenPM, OUTPUT);
  pinMode(redAM, OUTPUT);
  pinMode(redPM, OUTPUT);
  pinMode(orangeAM, OUTPUT);
  pinMode(orangePM, OUTPUT);

  orange1.begin();          
  orange2.begin();
  orange3.begin();              
  green1.begin();
  green2.begin();
  green3.begin();            
  red1.begin();
  red2.begin();
  red3.begin();            
  orange1.setBacklight(backlight);  // set the brightness to 100 %
  orange1.setColonOn(true); 
  orange2.setBacklight(backlight);
  orange2.setColonOn(0); // Switch off ":" for Orange "year"
  orange3.setBacklight(backlight);  
  green1.setBacklight(maxbacklight);
  green1.setColonOn(true); 
  green2.setBacklight(maxbacklight);
  green2.setColonOn(0); // Switch off ":" for Green "year"
  green3.setBacklight(maxbacklight);
  red1.setColonOn(true); 
  red1.setBacklight(backlight);
  red2.setBacklight(backlight);
  red3.setBacklight(backlight);  
  red2.setColonOn(0); // Switch off ":" for Red "year"
  Serial.println();

  // ArduinoOTA
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // NTP
  if ((now() - timestamp) > 43200) {     // 43200 = 12 hours
    get_NTP_time();                      // get all 12 hours a time update from NTP Server --> avoiding a constant read from time server
  }

  //ArduinoOTA
  ArduinoOTA.handle();

  // Serial / WebSerial for Troubleshooting
  Serial.print("Hour: ");
  Serial.println(hour(now()));
  Serial.print("Minute: ");
  Serial.println(minute(now()));
  Serial.print("Month: ");
  Serial.println(month(now()));
  Serial.print("Year: ");
  Serial.println(year(now()));
  Serial.print("Day: ");
  Serial.println(day(now()));
  Serial.println("*****");
  WebSerial.print("Hour: ");
  WebSerial.println(hour(now()));
  WebSerial.print("Minute: ");
  WebSerial.println(minute(now()));
  WebSerial.print("Month: ");
  WebSerial.println(month(now()));
  WebSerial.print("Year: ");
  WebSerial.println(year(now()));
  WebSerial.print("Day: ");
  WebSerial.println(day(now()));
  WebSerial.println("*****");
  
  // Green Clock AM / PM
  if(hour(now())>=13){
    digitalWrite(greenAM,0);
    digitalWrite(greenPM,1);
  } else if (hour(now())==12){ 
    digitalWrite(greenAM,0);
    digitalWrite(greenPM,1);
  }
    else  {
    digitalWrite(greenAM,1);
    digitalWrite(greenPM,0);
  }

  // Red Clock AM / PM
  if(hour(now())>=13){
    digitalWrite(redAM,0);
    digitalWrite(redPM,1);
  } else if (hour(now())==12){ 
    digitalWrite(redAM,0);
    digitalWrite(redPM,1);
  }
    else  {
    digitalWrite(redAM,1);
    digitalWrite(redPM,0);
  }

  // Orange Clock AM / PM
  if(hour(now())>=13){
    digitalWrite(orangeAM,0);
    digitalWrite(orangePM,1);
  } else if (hour(now())==12){ 
    digitalWrite(orangeAM,0);
    digitalWrite(orangePM,1);
  }
    else  {
    digitalWrite(orangeAM,1);
    digitalWrite(orangePM,0);
  }

  //Powersave
  if ((hour(now()) == 22) && (minute(now()) == 30)) { // Turns displays off at given time
    green1.off();
    green2.off();
    green3.off();
    red1.off();
    red2.off();
    red3.off();
    orange1.off();
    orange2.off();
    orange3.off();
    Serial.println("Powersave: On"); 
    WebSerial.println("Powersave: On");
  }
  else if ((hour(now()) == 9) && (minute(now()) == 30)){ // Turns displays on at given time
    green1.on();
    green2.on();
    green3.on();
    red1.on();
    red2.on();
    red3.on();
    orange1.on();
    orange2.on();
    orange3.on();
    Serial.println("Powersave: Off");  
    WebSerial.println("Powersave: Off");  
  }
      
  // Red Displays
  red3.printTime(10, 21, true);                         
  red2.print(year_red, true);                           
  red1.printTime(hour(now()), minute(now()), true);        

    
  // Green Displays - NTP Time
  green3.printTime(month(now()), day(now()), true); 
  green2.print(year(now()));
  green1.printTime(hour(now()), minute(now()), true);
    
  // Orange Displays                     
  orange1.printTime(11, 25, true);                        // November 25th                
  orange2.print(year_orange, true);                       // 1976
  orange3.printTime(hour(now()), minute(now()), true);  // Orange Timezone if can get it to work
}

// Function to Connect to Wifi
void go_online() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("---> Connecting to WiFi ");
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    i++;
    if (i > 20) {
      Serial.println("Could not connect to WiFi!");
      Serial.println("Doing a reset now and retry a connection from scratch.");
      go_online();                            // try again
    }
    Serial.print(".");
  }
  Serial.println("Wifi connected ok.");
}

// Function to get NTP Time
void get_NTP_time() {
  Serial.println("---> Now reading time from NTP Server");
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected! Doing that now again.");
    go_online();
  }
  int i = 0;
  while (!ntpClient.getUnixTime()) {
    delay(500);
    i++;
    if (i > 20) {
      Serial.println("Could not connect to NTP server!");
      Serial.println("Doing a reset now and retry a connection from scratch.");
      delay(5000);                            // wait 5 secs and then do a reset
    }
    Serial.print(".");
  }

  setTime(ntpClient.getUnixTime());           // get UNIX timestamp (seconds from 1.1.1970 on)

  if (summertime_EU(year(now()), month(now()), day(now()), hour(now()), 1)) {
    adjustTime(3600);                         // adding one hour
  }
  timestamp = now();
  Serial.println("Time obtained!");
  
} 

boolean summertime_EU(int year, byte month, byte day, byte hour, byte tzHours)
// European Daylight Savings Time calculation by "jurs" for German Arduino Forum
// input parameters: "normal time" for year, month, day, hour and tzHours (0=UTC, 1=MEZ)
// return value: returns true during Daylight Saving Time, false otherwise
{
  if (month < 3 || month > 10) return false; // keine Sommerzeit in Jan, Feb, Nov, Dez
  if (month > 3 && month < 10) return true;  // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep
  if (month == 3 && (hour + 24 * day) >= (1 + tzHours + 24 * (31 - (5 * year / 4 + 4) % 7)) || month == 10 && (hour + 24 * day) < (1 + tzHours + 24 * (31 - (5 * year / 4 + 1) % 7)))
    return true;
  else
    return false;
} 
