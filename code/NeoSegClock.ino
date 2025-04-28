/*
this is version 6
to add:
api
web interface
audio -dfplayermini?
temp/humidity? via button?
color patterns?
*/
#define NEOSEGCLOCK_VERSION 6
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


// —————— Hardware Constants ——————
#define DATA_PIN           0                   // NeoPixel data pin
#define TONE_PIN           1                   // Tone data pin
#define PIXELS_PER_SEGMENT 5                    // pixels per 7-segment digit
#define NUM_DIGITS         4                    // HH:MM → 4 digits
#define NUM_SYMBOL_PIXELS  (2*4)                // +2 for colon, +1 indicator dot
#define NUM_PIXELS         ((PIXELS_PER_SEGMENT * NUM_DIGITS * 7) + NUM_SYMBOL_PIXELS)
#define MAX_EEPROM_ALARMS 10
#define EEPROM_SIZE (MAX_EEPROM_ALARMS * 7)
#define MDNS_NAME "NeoSegClock"

// —————— Time Sync Constants ——————
const char*        ntpServer       = "pool.ntp.org";
const long         gmtOffset_sec   = 0;
const int          daylightOffset_sec = 0;
unsigned long      lastSync        = 0;
const unsigned long syncInterval   = 3600UL * 1000UL;  // 1 hour

// —————— Objects & Globals ——————
Adafruit_NeoPixel strip(NUM_PIXELS, DATA_PIN, NEO_GRB + NEO_KHZ800);
NeoSegments        segs(strip, PIXELS_PER_SEGMENT);
RTC_DS1307         rtc;            // DS3231 not supported in Wokwi, but API-compatible
AsyncWebServer server(80);  // Create a web server on port 80
bool               rtcAvailable = false;

// —————— Wi-Fi Credentials ——————
const char* ssid     = MY_SSID;
const char* password = MY_PASS;

// —————— Alarm Definition ——————
struct Alarm {
  bool     enabled;
  uint8_t  hour, minute;
  uint32_t color;      // use strip.Color(r,g,b)
  uint8_t  toneID;     // 0–3 in this example
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
    setenv("TZ", "IST-2IDT,M3.4.4/26,M10.5.0", 1);
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
    size_t addr = i * 7;
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
    size_t addr = i * 7;
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

void handleHelp() {
  SerialPrintln("enter a command:");
  SerialPrintln("list, ?, add, remove, enable, disable, edit, color, time, set time, get color");
}

void handleAdd(String args) {
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
}

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
  else if (cmd == "remove")  handleRemove(args);
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
  else if (cmd == "remove")  handleRemove(args);
  else if (cmd == "enable")  handleEnableDisable(args, true);
  else if (cmd == "disable") handleEnableDisable(args, false);
  else if (cmd == "edit")    handleEdit(args);
  else if (cmd == "color")   handleSetColor(args);
  else if (cmd == "time")    handleShowTime();
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
  if (MDNS.begin(MDNS_NAME)) {
   Serial.print("✓ mDNS responder started: http://");
   Serial.print(MDNS_NAME);
   Serial.println(".local");
  } else {
    Serial.println("⚠ Failed to start mDNS responder");
  }
  //WebSerial.begin(&server);  // Start WebSerial with AsyncWebServer
  WebSerial.begin(&server, "/");
  WebSerial.onMessage(onWebSerialInput);  // Set up callback function

ArduinoOTA.setHostname(MDNS_NAME); // Optional: use same MDNS as WebSerial
ArduinoOTA.setPassword("1234");  // <-- your password

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

  // 3) Time system init
  setupTimeSystem();

  // 4) NeoSegments
  segs.begin();
  segs.setDigitColor(strip.Color(0,1,1));  // default red
  segs.registerSymbol(':', 2, true);
  segs.registerSymbol(';', 2, false);
  segs.registerSymbol('.', 1, true);

  // 5) Alarms
  EEPROM.begin(EEPROM_SIZE);
  initializeAlarms();  // replace with EEPROM load later
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
