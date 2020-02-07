

/*  
 Test the M5.Lcd.print() viz embedded M5.Lcd.write() function

 This sketch used font 2, 4, 7

 Make sure all the display driver and pin comnenctions are correct by
 editting the User_Setup.h file in the TFT_eSPI library folder.

 #########################################################################
 ###### DON'T FORGET TO UPDATE THE User_Setup.h FILE IN THE LIBRARY ######
 #########################################################################
 */

#include <M5Stack.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <Syslog.h>
#include "Adafruit_Sensor.h"
#include <Adafruit_BMP280.h>
#include "DHT12.h"
#include <Wire.h>     //The DHT12 uses I2C comunication.
#include <HTTPClient.h>
#include <TimeLib.h>
#include <NTPClient.h>
DHT12 dht12;          //Preset scale CELSIUS and ID 0x5c.
Adafruit_BMP280 bme;
WiFiServer telnet(8899);
HTTPClient http;


const char* ssid = "SSID";
const char* password = "Password";
const char* NTPServer = "10.10.11.1";
#define SYSLOG_SERVER "10.10.11.120"
#define SYSLOG_PORT 5141
#define DEVICE_HOSTNAME "Thermostat"
#define APP_NAME "M5Stack Thermostat"
WiFiUDP udpClient;
Syslog syslog(udpClient, SYSLOG_SERVER, SYSLOG_PORT, DEVICE_HOSTNAME, APP_NAME, LOG_KERN);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTPServer);

//Build a list of Variables
float Temperature;
float DegC;
float Hum;
float Press;
float checksum;
float lastChecksum;
float dispPress; // Display pressure
int DBHigh; // High side of deadband
int DBLow; // Low side of deadband
int currentTemp; //Current temp in Integer form
int temperatureSPF; //Temperature set point in Degrees F
int temperatureDBF; //Temperature dead band in Degrees F
int systemStatus; //place to store what is going on
int Epoch;
int elapsed_update;
int last_update = -60000;
int PDT;
int PST;
int Day;
int Month;
int Year;
int Hour;
int Minute;
int Weekday;
int Statetime;
int Elapsedstate;
int testsp;
byte laststate;
byte currentstate;
bool Hold = false;
bool DST = false;
bool sensorcheck = false;
String inData;
String conradOn = "http://10.10.11.66/Low";
String conradOff = "http://10.10.11.66/off";
String westingHouse = "http://10.10.11.2/api/HueAPIKey/lights/11/state";
String Sunday = "0:05:00:70,1:10:00:65,2:12:00:70,3:22:00:65";
String Monday = "0:04:00:70,1:08:00:65,2:18:00:70,3:22:00:65";
String Tuesday = "0:04:00:70,1:08:00:65,2:18:00:70,3:22:00:65";
String Wednesday = "0:04:00:70,1:08:00:65,2:18:00:70,3:22:00:65";
String Thursday = "0:04:00:70,1:08:00:65,2:18:00:70,3:22:00:65";
String Friday = "0:04:00:70,1:08:00:65,2:18:00:70,3:22:00:65";
String Saturday = "0:05:00:70,1:10:00:65,2:12:00:70,3:22:00:65";

void setup() {
  M5.begin();
  Wire.begin();
  WiFi.begin(ssid, password);
  WiFi.mode(WIFI_STA);
  // M5.Lcd.setRotation(2);
  // Wire.begin();
  telnet.begin();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");  
  }
  while (!bme.begin(0x76)){  
      syslog.log(LOG_INFO, "Could not find a valid BMP280 sensor, check wiring!");
  }
  syslog.log(LOG_INFO, "Thermostat Startup");
  timeClient.begin();
  PDT = -25200;
  PST = -3600;
  timeClient.setTimeOffset(PDT);  
  temperatureSPF = 70;
  temperatureDBF = 5;
  checkSensor();
  pinMode(15,OUTPUT);
  m5.Speaker.mute();
  dacWrite(25, 0);
}

WiFiClient RemoteClient;

 
void CheckForConnections()
{
  if (telnet.hasClient())
  {
    // If we are already connected to another computer, 
    // then reject the new connection. Otherwise accept
    // the connection. 
    if (RemoteClient.connected() > 2)
    {
      syslog.log(LOG_INFO, "Client Rejected");
      telnet.available().stop();
    }
    else
    {
      RemoteClient = telnet.available();
    }
  }
}

void EchoReceivedData()
{
  while (RemoteClient.connected() && RemoteClient.available())
  {
      char thisChar = RemoteClient.read();
      inData += thisChar;       
       if (inData == "up"){ tempUp(); inData = ""; }
       else if (( inData == "dn")){ tempDn(); inData = ""; }
       else if (( inData == "t")){ RemoteClient.println(Temperature); RemoteClient.stop(); inData = ""; }
       else if (( inData == "s")){ RemoteClient.println(temperatureSPF); RemoteClient.stop(); inData = ""; }
       else if (( inData == "f")){ DST = true; RemoteClient.stop(); inData = ""; }
       else if (( inData == "b")){ DST = false; RemoteClient.stop(); inData = ""; }
       else if (( inData == "u")){ Hold = false; RemoteClient.stop(); inData = ""; }
       else if (( inData == "q")){ error(); inData = ""; }        
  }
}

void error() {
  RemoteClient.stop();
  syslog.log(LOG_INFO, "Rejected TCP Client");
}

void tempUp() {
  Hold = true;
  temperatureSPF = temperatureSPF + 1;
  // Set up some limits. These are hard coded for security reasons  
  if (temperatureSPF < 55 || temperatureSPF > 75){ temperatureSPF = 65; }
  syslog.log(LOG_INFO, String(temperatureSPF));
  RemoteClient.stop();
}

void tempDn() {
  Hold = true;
  temperatureSPF = temperatureSPF - 1;
  // Set up some limits. These are hard coded for security reasons  
  if (temperatureSPF < 55 || temperatureSPF > 75){ temperatureSPF = 65; }
  syslog.log(LOG_INFO, String(temperatureSPF));
  RemoteClient.stop();
}

void reportTemp() {
  RemoteClient.println(temperatureSPF);
  delay(400);
  RemoteClient.stop();
}

//Celsius to Fahrenheit conversion
double Fahrenheit(double celsius) {
        return 1.8 * celsius + 32;
}

void checkSensor() {
  Temperature = Fahrenheit(dht12.readTemperature());
  Hum = dht12.readHumidity();
  Press = bme.readPressure();
}

void lcdBright() {
  M5.Lcd.setBrightness(10);
}

byte TempControl(int x, int y, int z){
// x is ambient temperature
// y is setpoint
// z is dead band
if (y < 55 || y > 72) y = 65;
int dbhi = y + z; //create integer dead band for the heat Ceiling
int dblo = y; //create integer dead band for the heat Floor
byte enableSP = false; //build an enable byte
byte fallingEdge = false; //rising edge
byte fire = false; //build a turn on byte
byte lastState; //last state of the fire command
byte risingEdge; //falling edge
byte enableDB; //Dead band enable
if (x < y){ //allow the enable byte to go high
 enableSP = true;
} else { //once the temperature rises to the high dead band reset everything (turn off heat)
  enableSP = false;
  fire = false;
  fallingEdge = false;
  risingEdge = false;
}
if (x < dblo && lastState == false){ //we dropped below the dead band and are not rising in temp
  fallingEdge = true;
}
if (x > dblo && lastState == true){ //we rise above the low deadband and we are giving it juice
  risingEdge = true;
}
if (fallingEdge == true || risingEdge == true){
  enableDB = true;
}
if (enableDB == true && enableSP == true){
  fire = true; 
}
lastState = fire;
return fire;
}

String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

int schedule(int DOW, int CHR, int CMIN)
{
  String Wake;
  String Leave;
  String Return;
  String Sleep;
  int WHR;
  int LHR;
  int RHR;
  int SHR;
  int WMIN;
  int LMIN;
  int RMIN;
  int SMIN;
  int WINT;
  int LINT;
  int RINT;
  int SINT;
  int CINT;
  switch(DOW){
    case 1:
    Wake = getValue(Sunday,',',0);
    Leave = getValue(Sunday,',',1);
    Return = getValue(Sunday,',',2);
    Sleep = getValue(Sunday,',',3);
    break;
    case 2:
    Wake = getValue(Monday,',',0);
    Leave = getValue(Monday,',',1);
    Return = getValue(Monday,',',2);
    Sleep = getValue(Monday,',',3);
    break;
    case 3:
    Wake = getValue(Tuesday,',',0);
    Leave = getValue(Tuesday,',',1);
    Return = getValue(Tuesday,',',2);
    Sleep = getValue(Tuesday,',',3);
    break;
    case 4:
    Wake = getValue(Wednesday,',',0);
    Leave = getValue(Wednesday,',',1);
    Return = getValue(Wednesday,',',2);
    Sleep = getValue(Wednesday,',',3);
    break;
    case 5:
    Wake = getValue(Thursday,',',0);
    Leave = getValue(Thursday,',',1);
    Return = getValue(Thursday,',',2);
    Sleep = getValue(Thursday,',',3);
    break;
    case 6:
    Wake = getValue(Friday,',',0);
    Leave = getValue(Friday,',',1);
    Return = getValue(Friday,',',2);
    Sleep = getValue(Friday,',',3);
    break;
    case 7:
    Wake = getValue(Saturday,',',0); //0:05:00:70
    Leave = getValue(Saturday,',',1); //1:10:00:65
    Return = getValue(Saturday,',',2); //2:12:00:70
    Sleep = getValue(Saturday,',',3); //3:22:00:65
    break;
  }
  WHR = atoi(getValue(Wake, ':', 1).c_str());
  LHR = atoi(getValue(Leave, ':', 1).c_str());
  RHR = atoi(getValue(Return, ':', 1).c_str());
  SHR = atoi(getValue(Sleep, ':', 1).c_str());
  WMIN = atoi(getValue(Wake, ':', 2).c_str());
  LMIN = atoi(getValue(Leave, ':', 2).c_str());
  RMIN = atoi(getValue(Return, ':', 2).c_str());
  SMIN = atoi(getValue(Sleep, ':', 2).c_str());
  WINT = (WHR * 100) + WMIN;
  LINT = (LHR * 100) + LMIN;
  RINT = (RHR * 100) + RMIN;
  SINT = (SHR * 100) + SMIN;
  CINT = (CHR * 100) + CMIN;
  if (CINT >= WINT && CINT < LINT){ return atoi(getValue(Wake, ':', 3).c_str()); }
  if (CINT >= LINT && CINT < RINT){ return atoi(getValue(Leave, ':', 3).c_str()); }
  if (CINT >= RINT && CINT < SINT){ return atoi(getValue(Return, ':', 3).c_str()); }
  if (CINT >= SINT || CINT < WINT){ return atoi(getValue(Sleep, ':', 3).c_str()); }
}
void loop() {
  
  DBHigh = temperatureSPF + temperatureDBF;
  DBLow = temperatureSPF - temperatureDBF + 5;
  currentTemp = (int) Temperature;
  dispPress = Press / 1000.0; //comvert Pa into kPa 
  elapsed_update = millis();
  if(elapsed_update - last_update >= 60000){
    syslog.log(LOG_INFO, "Thermostat still alive!");
    while(!timeClient.update()) {
      timeClient.forceUpdate();
    }
    checkSensor();
    Epoch = timeClient.getEpochTime();
    if(DST == false){ Epoch = Epoch + PST; }
    Year = year(Epoch);
    Month = month(Epoch);
    Day = day(Epoch);
    Weekday = weekday(Epoch);
    Hour = hour(Epoch);
    Minute = minute(Epoch);
    last_update = millis();
    sensorcheck == false;
    if (Hold == false){ temperatureSPF = schedule(Weekday, Hour, Minute);}
  }
  if(elapsed_update - last_update > 30000 && sensorcheck == false) { checkSensor(); sensorcheck = true;}
  checksum = Minute + dispPress + temperatureSPF;
  currentstate = TempControl(currentTemp, temperatureSPF, temperatureDBF);
  if (currentstate == laststate && Statetime == 0){ Statetime = millis(); }
  Elapsedstate = millis() - Statetime;
  if (currentstate != laststate && Elapsedstate >= 300000){
    Statetime = 0;
    lcdBright();
    if (laststate == HIGH) {
     syslog.log(LOG_INFO, "Furnace Off!");
     http.begin(conradOff);
     http.GET();
     http.end();
     http.begin(westingHouse);
     http.addHeader("Content-Type", "text/plain");
     http.PUT("{\"on\":false}");
     http.end();
     systemStatus = 2; 
     digitalWrite(15, LOW);   
    }
    if (laststate == LOW) {
     syslog.log(LOG_INFO, "Furnace On!");
     http.begin(conradOn);
     http.GET();
     http.end();
     http.begin(westingHouse);
     http.addHeader("Content-Type", "text/plain");
     http.PUT("{\"on\":true}");
     http.end();
     systemStatus = 1;
     digitalWrite(15, HIGH); 
    }
    laststate = currentstate;
  }
  CheckForConnections();
  EchoReceivedData();
  M5.update();
  if (M5.BtnA.wasReleased()) {
    tempUp();
    M5.Lcd.setBrightness(200);
  } else if (M5.BtnB.wasReleased()) {
    tempDn();
    M5.Lcd.setBrightness(200);
  } else if (M5.BtnC.wasReleased()) {
    if (Hold == true){ Hold = false;}
    else if ((Hold == false)){ Hold = true;}
    lastChecksum = 0;
    M5.Lcd.setBrightness(200);
  } 
  // Only refresh the screen if something important has changed.
  if (checksum != lastChecksum){
    //Place any screen updating in this block of code. To refresh the screen
    //outside this block of code, set lastChecksum to Zero.
    lastChecksum = checksum;
    // Fill screen with Black
    M5.Lcd.fillScreen(TFT_BLACK);
    
    // Set "cursor" at top left corner of display (0,0) and select font 2
    // (cursor will move to next line automatically during printing with 'M5.Lcd.println'
    //  or stay on the line is there is room for the text with M5.Lcd.print)
    M5.Lcd.setCursor(0, 0, 2);
    // Set the font colour to be white with a black background, set text size multiplier to 1
    M5.Lcd.setTextColor(TFT_WHITE);  
    M5.Lcd.setTextSize(1);
    // We can now plot text on screen using the "print" class
    M5.Lcd.print("Temperature:        ");
    M5.Lcd.print("     ");
    M5.Lcd.print(ssid);
    M5.Lcd.print(" : ");
    M5.Lcd.println(WiFi.localIP());
    // Set the font colour based on the temperature
    if (currentTemp > DBHigh + 2){M5.Lcd.setTextColor(TFT_RED);}
    else if ((currentTemp < DBLow - 2)){M5.Lcd.setTextColor(TFT_BLUE);}
    else {M5.Lcd.setTextColor(TFT_YELLOW);} 
    M5.Lcd.setTextFont(7);
    M5.Lcd.print(Temperature);
    M5.Lcd.setTextFont(4);
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.print("    RSSI:  ");
    M5.Lcd.setTextFont(7);
    // Set the font colour based on the RSSI
    if (WiFi.RSSI() > -50){M5.Lcd.setTextColor(TFT_GREEN);}
    else if ((WiFi.RSSI() < -50) && (WiFi.RSSI() > -75)){M5.Lcd.setTextColor(TFT_YELLOW);}
    else {M5.Lcd.setTextColor(TFT_RED);}
    M5.Lcd.println(WiFi.RSSI());
    
    // Display the Current setpoint and deadband
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextFont(2);
    M5.Lcd.print("Setpoint:");
    M5.Lcd.print("          ");
    M5.Lcd.println("RH%:");
    M5.Lcd.setTextColor(TFT_YELLOW); 
    M5.Lcd.setTextFont(7);
    M5.Lcd.print(temperatureSPF);
    M5.Lcd.print("  ");
    M5.Lcd.setTextColor(TFT_LIGHTGREY); 
    M5.Lcd.println(Hum);
     
    //Deadband
    M5.Lcd.setTextColor(TFT_WHITE);
    M5.Lcd.setTextFont(2);
    M5.Lcd.println("DeadBand:         High:                 Low:");
    M5.Lcd.setTextColor(TFT_YELLOW); 
    M5.Lcd.setTextFont(7);
    M5.Lcd.print(temperatureDBF);
    M5.Lcd.print("      ");
    M5.Lcd.setTextColor(TFT_RED);
    M5.Lcd.print(DBHigh);
    M5.Lcd.print("      ");
    M5.Lcd.setTextColor(TFT_BLUE);
    M5.Lcd.println(DBLow);

    //Pressure
    M5.Lcd.setTextFont(2);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.print(dispPress);
    M5.Lcd.println(" kPa");

    //Time
    if (Hold == true){M5.Lcd.setTextColor(TFT_RED);}
    else if ((Hold == false)){M5.Lcd.setTextColor(TFT_GREEN);}
    M5.Lcd.print("Year: ");
    M5.Lcd.print(Year);
    if (Month < 10){ M5.Lcd.print("0"); }
    M5.Lcd.print(Month);
    if (Day < 10){ M5.Lcd.print("0"); }
    M5.Lcd.print(Day);
    M5.Lcd.print(" DOW: ");
    M5.Lcd.print(Weekday);
    M5.Lcd.print(" Time: ");
    if (Hour < 10){ M5.Lcd.print("0"); }
    M5.Lcd.print(Hour);
    M5.Lcd.print(":");
    if (Minute < 10){ M5.Lcd.print("0"); }
    M5.Lcd.println(Minute);
    if (systemStatus == 1) {
      M5.Lcd.setTextColor(TFT_RED);
      M5.Lcd.print("Heat On!");
    }
    else if ((systemStatus == 2)) {
      M5.Lcd.setTextColor(TFT_GREEN);
      M5.Lcd.print("Heat Off!");
    }
    else {
      M5.Lcd.setTextColor(TFT_YELLOW);
      M5.Lcd.print("UNKNOWN STATE!!");
    }
    }
  
  
  
}
