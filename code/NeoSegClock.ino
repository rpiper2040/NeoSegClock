/*
to add:
ap mode
api - settings, factory, reset,
web interface
audio -dfplayermini?
temp/humidity? via button?
color patterns?
*/
#define NEOSEGCLOCK_VERSION 6.2 //working on api
//added route with a few cmds added list handle that returns
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_NeoPixel.h>
#include <NeoSegments.h>
#include <my_secrets.h>
#include <WiFi.h>
#include <time.h>
#include <vector>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <WebSerialLite.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include "index_html.h"


// —————— Hardware Constants ——————
#define DATA_PIN           0                   // NeoPixel data pin
#define TONE_PIN           1                   // Tone data pin
#define PIXELS_PER_SEGMENT 5                    // pixels per 7-segment digit
#define NUM_DIGITS         4                    // HH:MM → 4 digits
#define NUM_SYMBOL_PIXELS  (2*4)                // +2 for colon, +1 indicator dot
#define NUM_PIXELS         ((PIXELS_PER_SEGMENT * NUM_DIGITS * 7) + NUM_SYMBOL_PIXELS)
#define SETTINGS_SIZE       300
#define MAX_EEPROM_ALARMS   10
#define ALARM_SIZE          7
#define ALARMS_OFFSET       SETTINGS_SIZE
#define EEPROM_SIZE         512
#define MDNS_NAME           "NeoSegClock"
#define OTA_PASS            "1234"
#define SETTINGS_VERSION 1
// —————— Time Sync Constants ——————
const char*        ntpServer       = "pool.ntp.org";
const long         gmtOffset_sec   = 0;
const int          daylightOffset_sec = 0;
unsigned long      lastSync        = 0;
const unsigned long syncInterval   = 3600UL * 1000UL;  // 1 hour
char*              ntpEnv          = "IST-2IDT,M3.4.4/26,M10.5.0";

// —————— Objects & Globals ——————
Adafruit_NeoPixel strip(NUM_PIXELS, DATA_PIN, NEO_GRB + NEO_KHZ800);
NeoSegments        segs(strip, PIXELS_PER_SEGMENT);
RTC_DS1307         rtc;            // DS3231 not supported in Wokwi, but API-compatible
AsyncWebServer server(80);  // Create a web server on port 80
bool               rtcAvailable = false;

// —————— Wi-Fi Credentials ——————
const char* ssid     = MY_SSID;
const char* password = MY_PASS;





struct ClockSettings {
  uint8_t version;             // New field for versioning
  bool use24Hour;
  char ntpEnv[32];
  char wifiSSID[32];
  char wifiPassword[32];
  uint8_t defaultR;
  uint8_t defaultG;
  uint8_t defaultB;
  //MDNS_NAME
};

// —————— Alarm Definition ——————
struct Alarm {
  bool     enabled;
  uint8_t  hour, minute;
  uint32_t color;      // use strip.Color(r,g,b)
  uint8_t  toneID;     // 0–4 in this example
  bool     triggered;  // to fire only once per minute
};

// —————— Global Alarms List ——————
std::vector<Alarm> alarms;

// —————— Initialize Some Example Alarms ——————
void initializeAlarms() {
  loadAlarmsFromEEPROM();
  if (alarms.empty()) {
    Serial.println("No alarms in EEPROM. Loading defaults.");
    alarms.push_back({ true, 19, 15, strip.Color(0,10,0), 0, false });
    alarms.push_back({ true,  6, 40, strip.Color(10,10,0), 1, false });
    alarms.push_back({ true,  7, 10, strip.Color(10,10,10), 2, false });
    saveAlarmsToEEPROM();
  }
}

// —————— Time Sync Routines ——————
void setupTimeSystem() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    setenv("TZ", ntpEnv, 1);
    tzset();
  struct tm tinfo;
  if (getLocalTime(&tinfo, 5000)) {
    Serial.println("✔ NTP sync succeeded at startup.");
    if (rtcAvailable) {
      rtc.adjust(DateTime(
        tinfo.tm_year + 1900,
        tinfo.tm_mon  + 1,
        tinfo.tm_mday,
        tinfo.tm_hour,
        tinfo.tm_min,
        tinfo.tm_sec
      ));
      Serial.println("→ RTC updated from NTP.");
    }
  }
  else if (rtcAvailable) {
    Serial.println("⚠ NTP failed; falling back to RTC.");
    DateTime now = rtc.now();
    struct timeval tv = { now.unixtime(), 0 };
    settimeofday(&tv, nullptr);
    Serial.println("→ System time set from RTC.");
  }
  else {
    Serial.println("⚠ No time source available at startup!");
  }
}

void periodicTimeSync() {
  if (millis() - lastSync < syncInterval) return;
  lastSync = millis();

  struct tm tinfo;
  if (getLocalTime(&tinfo, 5000)) {
    Serial.println("✔ Periodic NTP sync succeeded.");
    if (rtcAvailable) {
      rtc.adjust(DateTime(
        tinfo.tm_year + 1900,
        tinfo.tm_mon  + 1,
        tinfo.tm_mday,
        tinfo.tm_hour,
        tinfo.tm_min,
        tinfo.tm_sec
      ));
      Serial.println("→ RTC updated from NTP.");
    }
  } else {
    Serial.println("⚠ Periodic NTP sync failed.");
  }
}

// —————— Unified Time Access ——————
DateTime getTime() {
  struct tm tinfo;
  if (getLocalTime(&tinfo)) {
    return DateTime(
      tinfo.tm_year + 1900,
      tinfo.tm_mon  + 1,
      tinfo.tm_mday,
      tinfo.tm_hour,
      tinfo.tm_min,
      tinfo.tm_sec
    );
  }
  else if (rtcAvailable) {
    Serial.println("⚠ System time read failed; using RTC.");
    return rtc.now();
  }
  else {
    Serial.println("⚠ No valid time source; returning 2000-01-01 00:00:00");
    return DateTime(2000,1,1,0,0,0);
  }
}

// —————— Display Current Time on NeoSegments ——————
void displayTime() {
  DateTime now = getTime();
  char colonChar = (now.second() % 2 == 0) ? ':' : ';';  // blink
  char buf[6];
  sprintf(buf, "%02d%c%02d", now.hour(), colonChar, now.minute());
  segs.setString(buf);
  segs.update();
}

// —————— Check & Trigger Alarms ——————
void triggerAlarm(const Alarm &alarm) {
  segs.setDigitColor(alarm.color);
  segs.update();
  switch (alarm.toneID) {
    case 1: tone(TONE_PIN, 440, 500); break;  // A4
    case 2: tone(TONE_PIN, 523, 500); break;  // C5
    case 3: tone(TONE_PIN, 659, 500); break;  // E5
    default: break;                     // silent or 0
  }
}

void checkAlarms() {
  DateTime now = getTime();
  for (auto &alarm : alarms) {
    bool match = alarm.enabled
              && now.hour()   == alarm.hour
              && now.minute() == alarm.minute;
    if (match && !alarm.triggered) {
      triggerAlarm(alarm);
      alarm.triggered = true;
    }
    else if (!match) {
      alarm.triggered = false;
    }
  }
}

void saveAlarmsToEEPROM() {
  for (size_t i = 0; i < alarms.size() && i < MAX_EEPROM_ALARMS; ++i) {
    const Alarm& a = alarms[i];
    size_t addr = ALARMS_OFFSET + i * ALARM_SIZE;
    EEPROM.write(addr++, a.enabled);
    EEPROM.write(addr++, a.hour);
    EEPROM.write(addr++, a.minute);
    EEPROM.write(addr++, (a.color >> 16) & 0xFF); // R
    EEPROM.write(addr++, (a.color >> 8) & 0xFF);  // G
    EEPROM.write(addr++, a.color & 0xFF);         // B
    EEPROM.write(addr++, a.toneID);
  }
  EEPROM.commit();
}

void loadAlarmsFromEEPROM() {
  alarms.clear();
  for (int i = 0; i < MAX_EEPROM_ALARMS; ++i) {
    size_t addr = ALARMS_OFFSET + i * ALARM_SIZE;
    bool enabled = EEPROM.read(addr++);
    uint8_t hour = EEPROM.read(addr++);
    uint8_t minute = EEPROM.read(addr++);
    uint8_t r = EEPROM.read(addr++);
    uint8_t g = EEPROM.read(addr++);
    uint8_t b = EEPROM.read(addr++);
    uint8_t toneID = EEPROM.read(addr++);
    if (hour > 23 || minute > 59) continue;  // skip invalid
    alarms.push_back({ enabled, hour, minute, strip.Color(r, g, b), toneID, false });
  }
}

//settings:
ClockSettings settings;

bool isSettingsValid(const ClockSettings &s) {
  return s.version == SETTINGS_VERSION &&
         s.ntpEnv[0] != '\0' &&
         strlen(s.ntpEnv) < sizeof(s.ntpEnv);
}

void setDefaultSettings() {
  SerialPrintln("setting defaults");
  settings.version = SETTINGS_VERSION;
  settings.use24Hour = false;
  // Use strncpy to prevent buffer overflow and ensure null termination
  strncpy(settings.ntpEnv, "UTC0", sizeof(settings.ntpEnv) - 1);
  settings.ntpEnv[sizeof(settings.ntpEnv) - 1] = '\0'; // Ensure null termination
  strncpy(settings.wifiSSID, "wifissid", sizeof(settings.wifiSSID) - 1);
  settings.wifiSSID[sizeof(settings.wifiSSID) - 1] = '\0'; // Ensure null termination
  strncpy(settings.wifiPassword, "password", sizeof(settings.wifiPassword) - 1);
  settings.wifiPassword[sizeof(settings.wifiPassword) - 1] = '\0'; // Ensure null termination
  settings.defaultR = 255;
  settings.defaultG = 255;
  settings.defaultB = 255;
}


void loadSettingsFromEEPROM() {
  int addr = 0;
  EEPROM.get(addr, settings);
  if (!isSettingsValid(settings)) {
    SerialPrintln("invalid settings");
    setDefaultSettings();
    saveSettingsToEEPROM();
  }
}

void saveSettingsToEEPROM() {
  int addr = 0;
  EEPROM.put(addr, settings);
  EEPROM.commit();
}

void SerialPrint(const String &msg) {
  Serial.print(msg);
  WebSerial.print(msg);
}

void SerialPrintln(const String &msg) {
  Serial.println(msg);
  WebSerial.println(msg);
}

void SerialPrintf(const char *format, ...) {
  char buffer[256];  // Adjust size as needed
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  Serial.print(buffer);
  WebSerial.print(buffer);
}
// —————— Command Handlers ——————
void handleList() {
  SerialPrintln("=== Alarms ===");
  for (size_t i = 0; i < alarms.size(); ++i) {
    auto &a = alarms[i];
    SerialPrintf("[%d] %02d:%02d | Color: #%06X | Tone: %d | Enabled: %s\n",
      i, a.hour, a.minute, a.color, a.toneID, a.enabled ? "Yes" : "No");
  }
}
String twoDigits(int number) {
  if (number < 10) return "0" + String(number);
  return String(number);
}

DynamicJsonDocument getAlarmsAsJson() {
  DynamicJsonDocument doc(4096);
  JsonArray alarmsArray = doc.to<JsonArray>();
  for (size_t i = 0; i < alarms.size(); ++i) {
    JsonObject alarm = alarmsArray.createNestedObject();
    alarm["index"] = i;
    alarm["hour"] = alarms[i].hour;
    alarm["minute"] = alarms[i].minute;
    char hexColor[8];
    sprintf(hexColor, "#%06x", alarms[i].color);
    alarm["color"] = String(hexColor);
    alarm["tone"] = alarms[i].toneID;
    alarm["enabled"] = alarms[i].enabled;
  }
  return doc;
}


void handleHelp() {
  SerialPrintln("enter a command:");
  SerialPrintln("list, ?, add, remove, enable, disable, edit, color, time, set time, get color");
}

/*void handleAdd(String args) {
  int hh, mm, r, g, b, toneID;
  if (sscanf(args.c_str(), "%d %d %d %d %d %d", &hh, &mm, &r, &g, &b, &toneID) == 6) {
    alarms.push_back({ true, (uint8_t)hh, (uint8_t)mm,
                       strip.Color(r, g, b),
                       (uint8_t)toneID, false });
    saveAlarmsToEEPROM();
    SerialPrintln("Alarm added.");
  } else {
    SerialPrintln("Invalid format. Use: add HH MM R G B Tone");
  }
}*/
int handleAdd(String args) {
  int hh, mm, r, g, b, toneID;
  if (sscanf(args.c_str(), "%d %d %d %d %d %d", &hh, &mm, &r, &g, &b, &toneID) != 6) {
    SerialPrintln("Invalid format. Use: add HH MM R G B Tone");
    return 2;  // Invalid format
  }
  if (hh < 0 || hh > 23 || mm < 0 || mm > 59 ||
      r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 ||
      toneID < 0 || toneID > 4) {
    SerialPrintln("Invalid values. Check hour, minute, color range, tone.");
    return 3;  // Invalid values
  }
  alarms.push_back({ true, (uint8_t)hh, (uint8_t)mm,
                     strip.Color(r, g, b),
                     (uint8_t)toneID, false });
  saveAlarmsToEEPROM();
  SerialPrintln("Alarm added.");
  return 0;  // OK
}
/*
void handleRemove(String args) {
  int idx;
  if (sscanf(args.c_str(), "%d", &idx) == 1 && idx >= 0 && idx < (int)alarms.size()) {
    alarms.erase(alarms.begin() + idx);
    saveAlarmsToEEPROM();
    SerialPrintln("Alarm removed.");
  } else {
    SerialPrintln("Invalid index.");
  }
}
*/
int handleRemove(int index) {
  if (index < 0 || index >= alarms.size()) {
    SerialPrintln("Invalid index.");
    return 2; // Error code for invalid index
  }
  alarms.erase(alarms.begin() + index);
  saveAlarmsToEEPROM();
  SerialPrintln("Alarm removed.");
  return 0; // OK
}

void handleEnableDisable(String args, bool enable) {
  int idx;
  if (sscanf(args.c_str(), "%d", &idx) == 1 && idx >= 0 && idx < (int)alarms.size()) {
    alarms[idx].enabled = enable;
    saveAlarmsToEEPROM();
    SerialPrintf("Alarm %s.\n", enable ? "enabled" : "disabled");
  } else {
    SerialPrintln("Invalid index.");
  }
}

void handleEdit(String args) {
  int idx, hh, mm, r, g, b, toneID;
  if (sscanf(args.c_str(), "%d %d %d %d %d %d %d", &idx, &hh, &mm, &r, &g, &b, &toneID) == 7
      && idx >= 0 && idx < (int)alarms.size()) {
    auto &a = alarms[idx];
    a.hour = hh;
    a.minute = mm;
    a.color = strip.Color(r, g, b);
    a.toneID = toneID;
    saveAlarmsToEEPROM();
    SerialPrintf("Alarm %d updated.\n", idx);
  } else {
    SerialPrintln("Invalid format. Use: edit N HH MM R G B Tone");
  }
}

void handleSetColor(String args) {
  int r, g, b;
  if (sscanf(args.c_str(), "%d %d %d", &r, &g, &b) == 3
      && r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
    uint32_t col = strip.Color(r, g, b);
    segs.setDigitColor(col);
    SerialPrintf("Digit color set to RGB(%d,%d,%d)\n", r, g, b);
  } else {
    SerialPrintln("Invalid color values. Use: color R G B");
  }
}

void handleShowTime() {
  DateTime now = getTime();
  SerialPrintf("Current time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
}

void handleSetTime(String args) {
  int hh, mm, ss = 0;
  int count = sscanf(args.c_str(), "%d %d %d", &hh, &mm, &ss);
  if (count >= 2 && hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59 && ss >= 0 && ss <= 59) {
    time_t nowSec = time(nullptr);
    struct tm *now = localtime(&nowSec);
    now->tm_hour = hh;
    now->tm_min = mm;
    now->tm_sec = ss;
    time_t newTime = mktime(now);
    struct timeval tv = { newTime, 0 };
    settimeofday(&tv, nullptr);

    if (rtcAvailable) {
      rtc.adjust(DateTime(now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, hh, mm, ss));
      SerialPrintln("Time set manually and updated RTC.");
    } else {
      SerialPrintln("Time set manually.");
    }
  } else {
    SerialPrintln("Invalid format. Use: set time HH MM [SS]");
  }
}

void handleGetColor() {
  uint32_t col = segs.getDigitColor();
  uint8_t r = (col >> 16) & 0xFF;
  uint8_t g = (col >> 8) & 0xFF;
  uint8_t b = col & 0xFF;
  SerialPrintf("Current digit color: RGB(%d, %d, %d)\n", r, g, b);
}




void apiGetColor(AsyncWebServerRequest *request) {
  uint32_t color = segs.getDigitColor();  // Get the color as a uint32_t value
  uint8_t r = (color >> 16) & 0xFF;  // Extract red component
  uint8_t g = (color >> 8) & 0xFF;   // Extract green component
  uint8_t b = color & 0xFF;           // Extract blue component
  char hexColor[8];
  snprintf(hexColor, sizeof(hexColor), "#%02X%02X%02X", r, g, b);
  String response = "{\"status\":\"ok\",\"color\":\"" + String(hexColor) + "\"}";
  request->send(200, "application/json", response);
}

void apiSetColor(AsyncWebServerRequest *request) {
  if (!request->hasParam("color")) {
    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing color parameter\"}");
    return;
  }
  String hex = request->getParam("color")->value();
  if (hex.startsWith("#")) {
    hex = hex.substring(1);
  }
  if (hex.length() == 3) {
    hex = String(hex[0]) + hex[0] + String(hex[1]) + hex[1] + String(hex[2]) + hex[2];
  }
  if (hex.length() != 6) {
    request->send(400, "application/json", R"({"status":"error","message":"Invalid color format"})");
    return;
  }
  uint8_t r = strtol(hex.substring(0, 2).c_str(), nullptr, 16);
  uint8_t g = strtol(hex.substring(2, 4).c_str(), nullptr, 16);
  uint8_t b = strtol(hex.substring(4, 6).c_str(), nullptr, 16);
  uint32_t color = strip.Color(r, g, b);
  segs.setDigitColor(color);
  request->send(200, "application/json", "{\"status\":\"ok\"}");
}

void apiGetSettings(AsyncWebServerRequest *request) {
  DynamicJsonDocument doc(512);
  doc["version"] = settings.version;
  doc["use24Hour"] = settings.use24Hour;
  doc["ntpEnv"] = settings.ntpEnv;
  doc["wifiSSID"] = settings.wifiSSID;
  doc["wifiPassword"] = settings.wifiPassword;
  doc["defaultR"] = settings.defaultR;
  doc["defaultG"] = settings.defaultG;
  doc["defaultB"] = settings.defaultB;
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}


void apiSetSettings(AsyncWebServerRequest *request) {
  bool changed = false;
if (request->hasParam("version")) {
   String verStr = request->getParam("version")->value();
    int verInt = verStr.toInt();
   if (verInt >= 0 && verInt <= 255) {
     settings.version = (uint8_t)verInt;
      changed = true;
    } else {
      request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid version number, must be 0-255\"}");
      return;
    }
  }
  if (request->hasParam("use24Hour")) {
    String use24 = request->getParam("use24Hour")->value();
    if (use24 == "true") {
      settings.use24Hour = true;
      changed = true;
    } else if (use24 == "false") {
      settings.use24Hour = false;
      changed = true;
    } else {
      request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid use24Hour value, expected true or false\"}");
      return;
    }
  }
  if (request->hasParam("ntpEnv")) {
    String ntp = request->getParam("ntpEnv")->value();
    strncpy(settings.ntpEnv, ntp.c_str(), sizeof(settings.ntpEnv) - 1);
    settings.ntpEnv[sizeof(settings.ntpEnv) - 1] = '\0';
    changed = true;
  }
  if (request->hasParam("wifiSSID")) {
    String ssid = request->getParam("wifiSSID")->value();
    strncpy(settings.wifiSSID, ssid.c_str(), sizeof(settings.wifiSSID) - 1);
    settings.wifiSSID[sizeof(settings.wifiSSID) - 1] = '\0';
    changed = true;
  }
  if (request->hasParam("wifiPassword")) {
    String pass = request->getParam("wifiPassword")->value();
    if (pass.length() > 31) {
      request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"WiFi password too long, max 31 characters\"}");
      return;
    }
    strncpy(settings.wifiPassword, pass.c_str(), sizeof(settings.wifiPassword) - 1);
    settings.wifiPassword[sizeof(settings.wifiPassword) - 1] = '\0';
    changed = true;
  }
  if (request->hasParam("defaultColor")) {
    String colorStr = request->getParam("defaultColor")->value();
    if (colorStr.length() == 7 && colorStr.charAt(0) == '#') {
      unsigned int color = strtoul(colorStr.substring(1).c_str(), NULL, 16);
      settings.defaultR = (color >> 16) & 0xFF;
      settings.defaultG = (color >> 8) & 0xFF;
      settings.defaultB = color & 0xFF;
      changed = true;
    } else {
      request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid color format, expected #RRGGBB\"}");
      return;
    }
  }
  if (!changed) {
    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"At least one setting must be provided\"}");
    return;
  }
  saveSettingsToEEPROM();
  request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Settings updated successfully\"}");
}


void apiGetTime(AsyncWebServerRequest *request) {
  DateTime now = getTime();
  DynamicJsonDocument doc(128);
  doc["status"] = "ok";
  char buf[9];
  sprintf(buf, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  doc["time"] = buf;
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void apiSetTime(AsyncWebServerRequest *request) {
  if (!request->hasParam("hh") || !request->hasParam("mm") || !request->hasParam("ss")) {
    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing hh/mm/ss parameter\"}");
    return;
  }
  int hh = request->getParam("hh")->value().toInt();
  int mm = request->getParam("mm")->value().toInt();
  int ss = request->getParam("ss")->value().toInt();
  hh = constrain(hh, 0, 23);
  mm = constrain(mm, 0, 59);
  ss = constrain(ss, 0, 59);
  struct timeval tv;
  gettimeofday(&tv, nullptr); // Get current date for date portion
  struct tm *t = localtime(&tv.tv_sec);
  t->tm_hour = hh;
  t->tm_min = mm;
  t->tm_sec = ss;
  time_t newTime = mktime(t);
  tv.tv_sec = newTime;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Time set\"}");
  if (rtcAvailable) {
    rtc.adjust(DateTime(t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, hh, mm, ss));
  }
}

void apiList(AsyncWebServerRequest *request){
  DynamicJsonDocument doc(2048);
  doc["status"] = "ok";
  doc["response"] = getAlarmsAsJson(); // <--- real JsonArray here
  String output;
  serializeJson(doc, output);
  request->send(200, "application/json", output);
}

void apiAdd(AsyncWebServerRequest *request){
  if (!request->hasParam("args")) {
    request->send(400, "application/json", "{\"status\":\"error\",\"code\":1,\"message\":\"Missing args parameter\"}");
    return;
  }
  String args = request->getParam("args")->value();
  int result = handleAdd(args);  // Now returns 0/1/2/3
  if (result == 0) {
    DynamicJsonDocument doc(512);
    doc["status"] = "ok";
    doc["message"] = "Alarm added.";
    doc["code"] = 0;
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  } else {
    DynamicJsonDocument errorDoc(512);
    errorDoc["status"] = "error";
    errorDoc["code"] = result;
    if (result == 2) errorDoc["message"] = "Invalid format. Expected: HH MM R G B Tone";
    else if (result == 3) errorDoc["message"] = "Invalid values. Check hour, minute, RGB 0-255, Tone 0-4.";
    else errorDoc["message"] = "Unknown error.";
    String output;
    serializeJson(errorDoc, output);
    request->send(400, "application/json", output);
  }
}

void apiRemove(AsyncWebServerRequest *request) {
  if (!request->hasParam("index")) {
    request->send(400, "application/json", "{\"status\":\"error\",\"code\":1,\"message\":\"Missing index parameter\"}");
    return;
  }
  int index = request->getParam("index")->value().toInt();
  int result = handleRemove(index);
  if (result == 0) {
    DynamicJsonDocument doc(512);
    doc["status"] = "ok";
    doc["message"] = "Alarm removed.";
    doc["code"] = 0;
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  } else {
    DynamicJsonDocument errorDoc(512);
    errorDoc["status"] = "error";
    errorDoc["code"] = result;
    if (result == 2) errorDoc["message"] = "Invalid index.";
    else errorDoc["message"] = "Unknown error.";
    String output;
    serializeJson(errorDoc, output);
    request->send(400, "application/json", output);
  }
}

void apiEdit(AsyncWebServerRequest *request) {
   if (!request->hasParam("index")) {
    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing index parameter\"}");
    return;
  }
  int index = request->getParam("index")->value().toInt();
  if (index < 0 || index >= alarms.size()) {
    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid index\"}");
    return;
  }
  bool changed = false;
  if (request->hasParam("time")) {
    String timeStr = request->getParam("time")->value();
    int hh, mm;
    if (sscanf(timeStr.c_str(), "%d:%d", &hh, &mm) == 2) {
      alarms[index].hour = (uint8_t)hh;
      alarms[index].minute = (uint8_t)mm;
      changed = true;
    } else {
      request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid time format, expected HH:MM\"}");
      return;
    }
  }
  if (request->hasParam("color")) {
    String colorStr = request->getParam("color")->value();
    int r, g, b;
    if (sscanf(colorStr.c_str(), "%d,%d,%d", &r, &g, &b) == 3) {
      alarms[index].color = strip.Color(r, g, b);
      changed = true;
    } else {
      request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid color format, expected R,G,B\"}");
      return;
    }
  }
  if (request->hasParam("tone")) {
    int toneID = request->getParam("tone")->value().toInt();
    if (toneID >= 0 && toneID <= 4) {
      alarms[index].toneID = (uint8_t)toneID;
      changed = true;
    } else {
      request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid tone, expected 0-4\"}");
      return;
    }
  }
  if (request->hasParam("enable")) {
    String enableStr = request->getParam("enable")->value();
    if (enableStr == "true") {
      alarms[index].enabled = true;
      changed = true;
    } else if (enableStr == "false") {
      alarms[index].enabled = false;
      changed = true;
    } else {
      request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Invalid enable value, expected true or false\"}");
      return;
    }
  }
  if (!changed) {
    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"At least one editable parameter (time, color, tone, enable) is required\"}");
    return;
  }
  saveAlarmsToEEPROM();
  request->send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Alarm updated successfully\"}");
}

// —————— Handle Serial UI ——————
void onWebSerialInput(uint8_t *data, size_t len) {
  String input = "";
  for (size_t i = 0; i < len; i++) {
    input += (char)data[i];
  }
  input.trim();
  int spaceIdx = input.indexOf(' ');
  String cmd = input;
  String args = "";
  if (spaceIdx != -1) {
    cmd = input.substring(0, spaceIdx);
    args = input.substring(spaceIdx + 1);
  }
  if (cmd == "list")         handleList();
  else if (cmd == "?")       handleHelp();
  else if (cmd == "add")     handleAdd(args);
  else if (cmd == "remove")  handleRemove(args.toInt());
  else if (cmd == "enable")  handleEnableDisable(args, true);
  else if (cmd == "disable") handleEnableDisable(args, false);
  else if (cmd == "edit")    handleEdit(args);
  else if (cmd == "color")   handleSetColor(args);
  else if (cmd == "time")    handleShowTime();
  else if (cmd == "set") {
    if (args.startsWith("time")) {
      handleSetTime(args.substring(5));  // skip "time "
    } else {
      WebSerial.println("Unknown set command. Use: set time HH MM [SS]");
    }
  }
  else if (cmd == "get") {
    if (args == "color") {
      handleGetColor();
    } else {
      WebSerial.println("Unknown get command. Use: get color");
    }
  }
  else {
    WebSerial.println("Invalid command.");
  }
}


void handleSerialInput() {
  if (!Serial.available()) return;
  String input = Serial.readStringUntil('\n');
  input.trim();

  int spaceIdx = input.indexOf(' ');
  String cmd = input;
  String args = "";

  if (spaceIdx != -1) {
    cmd = input.substring(0, spaceIdx);
    args = input.substring(spaceIdx + 1);
  }

  if (cmd == "list")         handleList();
  else if (cmd == "?")       handleHelp();
  else if (cmd == "add")     handleAdd(args);
  else if (cmd == "remove")  handleRemove(args.toInt());
  else if (cmd == "enable")  handleEnableDisable(args, true);
  else if (cmd == "disable") handleEnableDisable(args, false);
  else if (cmd == "edit")    handleEdit(args);
  else if (cmd == "color")   handleSetColor(args);
  else if (cmd == "time")    handleShowTime();
  else if (cmd == "set24")    Serial.println(settings.use24Hour=true);
  else if (cmd == "set12")    Serial.println(settings.use24Hour=false);
  else if (cmd == "setpass1")    {strncpy(settings.wifiPassword, "1", sizeof(settings.wifiPassword) - 1);
settings.wifiPassword[sizeof(settings.wifiPassword) - 1] = '\0'; 
saveSettingsToEEPROM();}
  else if (cmd == "setpass2")    {
    strncpy(settings.wifiPassword, "2222", sizeof(settings.wifiPassword) - 1);
    settings.wifiPassword[sizeof(settings.wifiPassword) - 1] = '\0';
    saveSettingsToEEPROM();}
  else if (cmd == "set") {
    if (args.startsWith("time")) {
      handleSetTime(args.substring(5));  // skip "time "
    } else {
      Serial.println("Unknown set command. Use: set time HH MM [SS]");
    }
  }
  else if (cmd == "get") {
    if (args == "color") {
      handleGetColor();
    } else {
      Serial.println("Unknown get command. Use: get color");
    }
  }
  else {
    Serial.println("Invalid command.");
  }
}


// —————— Arduino Setup ——————
void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin();

  EEPROM.begin(EEPROM_SIZE);
  loadSettingsFromEEPROM();  
  initializeAlarms();

  // 1) RTC
  if (rtc.begin()) {
    rtcAvailable = rtc.isrunning();
    Serial.printf("RTC %sfound, %srunning.\n",
      rtcAvailable ? "" : "not ",
      rtcAvailable ? "" : "not "
    );
  } else {
    rtcAvailable = false;
    Serial.println("RTC not found; continuing without it.");
  }

  // 2) Wi-Fi connect
  Serial.print("Connecting Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("✓ Wi-Fi connected. IP address: %s\n", WiFi.localIP().toString().c_str());
  //mDNS
  if (MDNS.begin(MDNS_NAME)) {
   Serial.print("✓ mDNS responder started: http://");
   Serial.print(MDNS_NAME);
   Serial.println(".local");
  } else {
    Serial.println("⚠ Failed to start mDNS responder");
  }
  //WebSerialLite
  WebSerial.begin(&server);
  WebSerial.onMessage(onWebSerialInput);  // Set up callback function
//ArduinoOTA
  ArduinoOTA.setHostname(MDNS_NAME);
  ArduinoOTA.setPassword(OTA_PASS); 
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH) type = "sketch";
      else type = "filesystem";
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nUpdate complete");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
     Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
      if      (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
     else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
  ArduinoOTA.begin();
  Serial.println("✓ Arduino OTA Ready");
  server.begin();            // Start the web server




//add here the /api route??
server.on("/api", HTTP_GET, [](AsyncWebServerRequest *request) {
  if (!request->hasParam("cmd")) {
    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing cmd parameter\"}");
    return;
  }
  String cmd = request->getParam("cmd")->value();
  if (cmd == "list") apiList(request);
  else if (cmd == "add") apiAdd(request);
  else if (cmd == "remove") apiRemove(request);
  else if (cmd == "edit") apiEdit(request);
  else if (cmd == "getColor") apiGetColor(request);
  else if (cmd == "setColor") apiSetColor(request);
  else if (cmd == "getTime") apiGetTime(request);
  else if (cmd == "setTime") apiSetTime(request);
  else if (cmd == "getSettings") apiGetSettings(request);
  else if (cmd == "setSettings") apiSetSettings(request);
  else if (cmd == "reset")  ESP.restart();
  else {
    request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Unknown command\"}");
    return;
  }
});
server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
  request->send_P(200, "text/html", index_html);
});

  // 3) Time system init
  setupTimeSystem();

  // 4) NeoSegments
  segs.begin();
  segs.setDigitColor(strip.Color(1,0,1));  // default red
  const uint8_t C_segments[7] = {1,1,0,0,1,1,0}; 
  const uint8_t F_segments[7] = {1,1,0,1,1,0,0}; 
  segs.registerDigit('F', F_segments);
  segs.registerDigit('C', C_segments);
  segs.registerSymbol(':', 2, true);
  segs.registerSymbol(';', 2, false);
  segs.registerSymbol('.', 1, true);

  SerialPrintln("Welcome to NeoSegClock! Enter '?' for commands.");
}

// —————— Main Loop ——————
unsigned long lastUpdate      = 0;
const unsigned long updateInterval = 1000;

void loop() {
  periodicTimeSync();

  unsigned long now = millis();
  if (now - lastUpdate >= updateInterval) {
    lastUpdate = now;
    displayTime();
    checkAlarms();
  } 
ArduinoOTA.handle();
  handleSerialInput();
}
