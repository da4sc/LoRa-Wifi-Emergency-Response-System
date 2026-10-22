#include <M5Unified.h>
#include <WiFi.h>
#include <HTTPClient.h>

// Set to 0 until the NEO-6M is physically connected.
// After wiring the GPS and installing TinyGPSPlus, change this to 1.
#define ENABLE_GPS 0

#if ENABLE_GPS
#include <TinyGPSPlus.h>
#endif

// ============================================================
// NESSO N1 EMERGENCY SYSTEM - V3 BATTERY TELEMETRY
//
// GREEN HARDWARE BUTTON:
//   SAFE                -> press -> SEND EMERGENCY
//   ACTIVE/ACKNOWLEDGED -> press -> CANCEL EMERGENCY
//
// DASHBOARD:
//   Acknowledge -> Nesso shows ACKNOWLEDGED automatically
//   Resolve     -> Nesso returns to SAFE automatically
//
// BATTERY:
//   - Battery percentage shown on Nesso
//   - Battery voltage shown on dashboard
//   - Charging state shown on Nesso + dashboard
//   - Telemetry sent every 10 seconds
// ============================================================


// ============================================================
// USER SETTINGS
// ============================================================

const char* WIFI_SSID =
    "Daksh iPhone";

const char* WIFI_PASSWORD =
    "n1_frndly_pass";

const char* DEVICE_ID =
    "NESSO-11545644";

// Keep the SAME working laptop IP you already use.
const char* SERVER_BASE =
    "http://172.20.10.3:5001";

const char* API_KEY =
    "NESSO_CAPSTONE_2026";


// ============================================================
// GPS - NEO-6M
//
// Nesso G6 = RX  <- GPS TX
// Nesso G7 = TX  -> GPS RX (optional for receive-only use)
//
// Robu's NEO-6M listing says default UART baud is 9600.
// ============================================================

const int GPS_RX_PIN = 6;
const int GPS_TX_PIN = 7;
const uint32_t GPS_BAUD = 9600;

// Demo fallback coordinates: approximate Palghar, Maharashtra.
// These are used ONLY when there is no real GPS fix.
const double FALLBACK_LATITUDE = 19.696700;
const double FALLBACK_LONGITUDE = 72.769900;

#if ENABLE_GPS
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
#endif


// ============================================================
// TIMING
// ============================================================

const unsigned long BUTTON_COOLDOWN_MS = 1200;
const unsigned long STATUS_POLL_MS = 2000;
const unsigned long TELEMETRY_INTERVAL_MS = 10000;
const unsigned long RECONNECT_INTERVAL_MS = 15000;

unsigned long lastButtonTime = 0;
unsigned long lastStatusPoll = 0;
unsigned long lastTelemetry = 0;
unsigned long lastReconnectAttempt = 0;


// ============================================================
// DEVICE STATE
// ============================================================

bool emergencyActive = false;

// SAFE / ACTIVE / ACKNOWLEDGED
String remoteStatus = "SAFE";


// ============================================================
// BATTERY HELPERS
// ============================================================

int getBatteryPercent() {

  int value =
    M5.Power.getBatteryLevel();

  // M5Unified returns -1 if the value is not ready/supported.
  if (value < 0) {
    return -1;
  }

  if (value > 100) {
    value = 100;
  }

  return value;
}


int getBatteryMv() {

  // M5Unified documents this value in millivolts.
  return M5.Power.getBatteryVoltage();
}


bool isBatteryCharging() {

  return
    M5.Power.isCharging()
    ==
    m5::Power_Class::is_charging;
}


String batteryPercentText() {

  int pct =
    getBatteryPercent();

  if (pct < 0) {
    return "--%";
  }

  return
    String(pct)
    +
    "%";
}


String powerStateText() {

  auto chargeState =
    M5.Power.isCharging();

  if (
    chargeState ==
    m5::Power_Class::is_charging
  ) {
    return "CHARGING";
  }

  if (
    chargeState ==
    m5::Power_Class::is_discharging
  ) {
    return "BATTERY";
  }

  return "UNKNOWN";
}


uint16_t batteryColor() {

  int pct =
    getBatteryPercent();

  if (pct < 0) {
    return TFT_WHITE;
  }

  if (pct <= 15) {
    return TFT_RED;
  }

  if (pct <= 30) {
    return TFT_YELLOW;
  }

  return TFT_GREEN;
}


// ============================================================
// GPS HELPERS
// ============================================================

void initGps() {
#if ENABLE_GPS
  gpsSerial.begin(
    GPS_BAUD,
    SERIAL_8N1,
    GPS_RX_PIN,
    GPS_TX_PIN
  );

  Serial.println(
    "GPS enabled on G6/G7 at 9600 baud."
  );
#else
  Serial.println(
    "GPS support compiled in, module currently disabled."
  );
#endif
}


void serviceGps() {
#if ENABLE_GPS
  while (
    gpsSerial.available()
  ) {
    gps.encode(
      gpsSerial.read()
    );
  }
#endif
}


bool gpsHasFix() {
#if ENABLE_GPS
  return
    gps.location.isValid()
    &&
    gps.location.age() < 15000;
#else
  return false;
#endif
}


double gpsLatitude() {
#if ENABLE_GPS
  if (gpsHasFix()) {
    return gps.location.lat();
  }
#endif

  return 0.0;
}


double gpsLongitude() {
#if ENABLE_GPS
  if (gpsHasFix()) {
    return gps.location.lng();
  }
#endif

  return 0.0;
}


int gpsSatellites() {
#if ENABLE_GPS
  if (gps.satellites.isValid()) {
    return (int)gps.satellites.value();
  }
#endif

  return 0;
}


String gpsFieldsJson() {

  String result;

  bool valid =
    gpsHasFix();

  result +=
    "\"gps_valid\":"
    +
    String(
      valid
        ? "true"
        : "false"
    )
    +
    ",";

  if (valid) {

    // Real NEO-6M location.
    result +=
      "\"latitude\":"
      +
      String(
        gpsLatitude(),
        6
      )
      +
      ",";

    result +=
      "\"longitude\":"
      +
      String(
        gpsLongitude(),
        6
      )
      +
      ",";

  } else {

    // No real fix: use clearly-labelled demo/fallback Palghar coordinates.
    // gps_valid stays false, so the dashboard knows this is NOT a real fix.
    result +=
      "\"latitude\":"
      +
      String(
        FALLBACK_LATITUDE,
        6
      )
      +
      ",";

    result +=
      "\"longitude\":"
      +
      String(
        FALLBACK_LONGITUDE,
        6
      )
      +
      ",";
  }

  result +=
    "\"satellites\":"
    +
    String(
      gpsSatellites()
    );

  return result;
}


void printGpsStatusToSerial() {

  Serial.print(
    "GPS: "
  );

#if ENABLE_GPS
  if (
    gpsHasFix()
  ) {

    Serial.print(
      gpsLatitude(),
      6
    );

    Serial.print(
      ", "
    );

    Serial.print(
      gpsLongitude(),
      6
    );

    Serial.print(
      " | satellites="
    );

    Serial.println(
      gpsSatellites()
    );

  } else {

    Serial.print(
      "waiting for fix"
    );

    Serial.print(
      " | satellites="
    );

    Serial.println(
      gpsSatellites()
    );
  }
#else
  Serial.println(
    "disabled until module is connected"
  );
#endif
}


// ============================================================
// DISPLAY
// ============================================================

void clearScreen() {
  M5.Display.fillScreen(TFT_BLACK);
}


void printCentered(
  const String& text,
  int y,
  uint16_t color,
  int size
) {

  M5.Display.setTextSize(size);
  M5.Display.setTextColor(
    color,
    TFT_BLACK
  );

  int width =
    M5.Display.textWidth(text);

  int x =
    (
      M5.Display.width()
      -
      width
    )
    /
    2;

  if (x < 0) {
    x = 0;
  }

  M5.Display.setCursor(
    x,
    y
  );

  M5.Display.print(
    text
  );
}


void printDeviceInfo() {

  M5.Display.setTextSize(1);

  M5.Display.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );

  M5.Display.setCursor(
    7,
    31
  );

  M5.Display.print("ID: ");
  M5.Display.println(DEVICE_ID);


  // Wi-Fi
  M5.Display.setCursor(
    7,
    47
  );

  M5.Display.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );

  M5.Display.print("Wi-Fi: ");

  if (
    WiFi.status() ==
    WL_CONNECTED
  ) {

    M5.Display.setTextColor(
      TFT_GREEN,
      TFT_BLACK
    );

    M5.Display.print("OK ");

    M5.Display.setTextColor(
      TFT_WHITE,
      TFT_BLACK
    );

    M5.Display.print(
      WiFi.RSSI()
    );

    M5.Display.println(
      " dBm"
    );

  } else {

    M5.Display.setTextColor(
      TFT_RED,
      TFT_BLACK
    );

    M5.Display.println(
      "DISCONNECTED"
    );
  }


  // Battery percentage
  M5.Display.setCursor(
    7,
    63
  );

  M5.Display.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );

  M5.Display.print(
    "Battery: "
  );

  M5.Display.setTextColor(
    batteryColor(),
    TFT_BLACK
  );

  M5.Display.println(
    batteryPercentText()
  );


  // Charging / battery
  M5.Display.setCursor(
    7,
    79
  );

  M5.Display.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );

  M5.Display.print(
    "Power: "
  );

  if (
    isBatteryCharging()
  ) {

    M5.Display.setTextColor(
      TFT_GREEN,
      TFT_BLACK
    );

  } else {

    M5.Display.setTextColor(
      TFT_WHITE,
      TFT_BLACK
    );
  }

  M5.Display.println(
    powerStateText()
  );
}


void showConnectingScreen() {

  clearScreen();

  printCentered(
    "NESSO EMERGENCY",
    8,
    TFT_CYAN,
    2
  );

  printCentered(
    "Connecting Wi-Fi...",
    55,
    TFT_YELLOW,
    1
  );

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );

  M5.Display.setCursor(
    7,
    82
  );

  M5.Display.print(
    "ID: "
  );

  M5.Display.println(
    DEVICE_ID
  );

  M5.Display.setCursor(
    7,
    100
  );

  M5.Display.print(
    "Battery: "
  );

  M5.Display.println(
    batteryPercentText()
  );
}


void showSafeScreen() {

  clearScreen();

  printCentered(
    "NESSO EMERGENCY",
    5,
    TFT_CYAN,
    2
  );

  printDeviceInfo();

  printCentered(
    "STATUS: SAFE",
    97,
    TFT_GREEN,
    2
  );

  printCentered(
    "GREEN button = SOS",
    121,
    TFT_WHITE,
    1
  );
}


void showSendingScreen() {

  clearScreen();

  printCentered(
    "EMERGENCY",
    24,
    TFT_RED,
    3
  );

  printCentered(
    "Sending request...",
    71,
    TFT_YELLOW,
    2
  );

  printCentered(
    "Battery " + batteryPercentText(),
    106,
    batteryColor(),
    1
  );
}


void showActiveScreen() {

  clearScreen();

  printCentered(
    "SOS ACTIVE",
    5,
    TFT_RED,
    2
  );

  printDeviceInfo();

  printCentered(
    "NOT ACKNOWLEDGED",
    97,
    TFT_YELLOW,
    2
  );

  printCentered(
    "GREEN = cancel SOS",
    121,
    TFT_WHITE,
    1
  );
}


void showAcknowledgedScreen() {

  clearScreen();

  printCentered(
    "SOS ACTIVE",
    5,
    TFT_RED,
    2
  );

  printDeviceInfo();

  printCentered(
    "ACKNOWLEDGED",
    97,
    TFT_GREEN,
    2
  );

  printCentered(
    "GREEN = cancel",
    121,
    TFT_WHITE,
    1
  );
}


void showResolvedScreen() {

  clearScreen();

  printCentered(
    "EMERGENCY",
    24,
    TFT_GREEN,
    2
  );

  printCentered(
    "RESOLVED",
    54,
    TFT_GREEN,
    3
  );

  printCentered(
    "Dashboard confirmed",
    96,
    TFT_WHITE,
    1
  );

  printCentered(
    "Battery " + batteryPercentText(),
    113,
    batteryColor(),
    1
  );
}


void showCancelledScreen() {

  clearScreen();

  printCentered(
    "EMERGENCY",
    24,
    TFT_GREEN,
    2
  );

  printCentered(
    "CANCELLED",
    54,
    TFT_GREEN,
    3
  );

  printCentered(
    "Cancellation confirmed",
    96,
    TFT_WHITE,
    1
  );

  printCentered(
    "Battery " + batteryPercentText(),
    113,
    batteryColor(),
    1
  );
}


void showFailedScreen(
  const String& message
) {

  clearScreen();

  printCentered(
    "REQUEST FAILED",
    20,
    TFT_RED,
    2
  );

  printCentered(
    message,
    61,
    TFT_WHITE,
    1
  );

  printCentered(
    "Check Wi-Fi / server",
    86,
    TFT_YELLOW,
    1
  );

  printCentered(
    "Battery " + batteryPercentText(),
    108,
    batteryColor(),
    1
  );
}


// ============================================================
// WI-FI
// ============================================================

bool connectWiFi() {

  if (
    WiFi.status() ==
    WL_CONNECTED
  ) {
    return true;
  }

  showConnectingScreen();

  Serial.println();
  Serial.println(
    "=============================="
  );

  Serial.print(
    "Connecting to Wi-Fi: "
  );

  Serial.println(
    WIFI_SSID
  );

  WiFi.disconnect();

  delay(300);

  WiFi.mode(
    WIFI_STA
  );

  delay(200);

  WiFi.setAutoReconnect(
    true
  );

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );

  unsigned long start =
    millis();

  while (
    WiFi.status() !=
    WL_CONNECTED
    &&
    millis() - start <
    20000
  ) {

    delay(500);

    Serial.print(".");
  }

  Serial.println();

  if (
    WiFi.status() ==
    WL_CONNECTED
  ) {

    Serial.println(
      "Wi-Fi CONNECTED"
    );

    Serial.print(
      "Nesso IP: "
    );

    Serial.println(
      WiFi.localIP()
    );

    Serial.print(
      "RSSI: "
    );

    Serial.print(
      WiFi.RSSI()
    );

    Serial.println(
      " dBm"
    );

    Serial.print(
      "Battery: "
    );

    Serial.print(
      batteryPercentText()
    );

    Serial.print(
      " | "
    );

    Serial.print(
      getBatteryMv()
    );

    Serial.print(
      " mV | "
    );

    Serial.println(
      powerStateText()
    );

    Serial.println(
      "=============================="
    );

    return true;
  }

  Serial.println(
    "Wi-Fi connection FAILED"
  );

  return false;
}


// ============================================================
// TELEMETRY
// ============================================================

String makeTelemetryPayload() {

  int batteryPct =
    getBatteryPercent();

  int batteryMv =
    getBatteryMv();

  bool charging =
    isBatteryCharging();

  String payload =
    "{"
    "\"device_id\":\""
    +
    String(DEVICE_ID)
    +
    "\",";

  payload +=
    "\"battery_pct\":"
    +
    String(batteryPct)
    +
    ",";

  payload +=
    "\"battery_mv\":"
    +
    String(batteryMv)
    +
    ",";

  payload +=
    "\"charging\":"
    +
    String(
      charging
        ? "true"
        : "false"
    )
    +
    ",";

  payload +=
    "\"wifi_rssi\":"
    +
    String(
      WiFi.status()
      ==
      WL_CONNECTED
        ? WiFi.RSSI()
        : 0
    )
    +
    ",";

  payload +=
    gpsFieldsJson();

  payload +=
    "}";

  return payload;
}


void sendTelemetry() {

  if (
    WiFi.status() !=
    WL_CONNECTED
  ) {
    return;
  }

  HTTPClient http;

  String url =
    String(SERVER_BASE)
    +
    "/api/telemetry";

  http.setTimeout(
    4000
  );

  http.begin(
    url
  );

  http.addHeader(
    "Content-Type",
    "application/json"
  );

  http.addHeader(
    "X-API-Key",
    API_KEY
  );

  String payload =
    makeTelemetryPayload();

  int httpCode =
    http.POST(
      payload
    );

  Serial.print(
    "Telemetry HTTP: "
  );

  Serial.print(
    httpCode
  );

  Serial.print(
    " | Battery: "
  );

  Serial.print(
    batteryPercentText()
  );

  Serial.print(
    " | "
  );

  Serial.print(
    getBatteryMv()
  );

  Serial.print(
    " mV | "
  );

  Serial.println(
    powerStateText()
  );

  http.end();
}


// ============================================================
// COMMON POST
// ============================================================

bool sendPost(
  const String& endpoint,
  const String& payload
) {

  if (
    WiFi.status() !=
    WL_CONNECTED
  ) {

    if (
      !connectWiFi()
    ) {

      showFailedScreen(
        "Wi-Fi unavailable"
      );

      delay(1800);

      return false;
    }
  }

  HTTPClient http;

  String url =
    String(SERVER_BASE)
    +
    endpoint;

  Serial.print(
    "POST: "
  );

  Serial.println(
    url
  );

  Serial.print(
    "Payload: "
  );

  Serial.println(
    payload
  );

  http.setTimeout(
    7000
  );

  http.begin(
    url
  );

  http.addHeader(
    "Content-Type",
    "application/json"
  );

  http.addHeader(
    "X-API-Key",
    API_KEY
  );

  int httpCode =
    http.POST(
      payload
    );

  Serial.print(
    "HTTP Response: "
  );

  Serial.println(
    httpCode
  );

  if (
    httpCode < 0
  ) {

    Serial.print(
      "HTTP Error: "
    );

    Serial.println(
      http.errorToString(
        httpCode
      )
    );
  }

  if (
    httpCode > 0
  ) {

    String response =
      http.getString();

    Serial.print(
      "Server: "
    );

    Serial.println(
      response
    );
  }

  bool success =
    httpCode >= 200
    &&
    httpCode < 300;

  http.end();

  return success;
}


// ============================================================
// SEND SOS
// ============================================================

void sendEmergency() {

  Serial.println();
  Serial.println(
    "*** SENDING EMERGENCY ***"
  );

  showSendingScreen();

  String payload =
    "{"
    "\"device_id\":\""
    +
    String(DEVICE_ID)
    +
    "\","
    "\"status\":\"EMERGENCY\","
    "\"wifi_rssi\":"
    +
    String(WiFi.RSSI())
    +
    ","
    "\"battery_pct\":"
    +
    String(getBatteryPercent())
    +
    ","
    "\"battery_mv\":"
    +
    String(getBatteryMv())
    +
    ","
    "\"charging\":"
    +
    String(
      isBatteryCharging()
        ? "true"
        : "false"
    )
    +
    ",";

  payload +=
    gpsFieldsJson();

  payload +=
    "}";

  bool success =
    sendPost(
      "/api/emergency",
      payload
    );

  if (
    success
  ) {

    emergencyActive =
      true;

    remoteStatus =
      "ACTIVE";

    Serial.println(
      "EMERGENCY SENT SUCCESSFULLY"
    );

    showActiveScreen();

    lastStatusPoll = 0;

  } else {

    emergencyActive =
      false;

    remoteStatus =
      "SAFE";

    Serial.println(
      "EMERGENCY SEND FAILED"
    );

    showFailedScreen(
      "SOS was NOT sent"
    );

    delay(2200);

    showSafeScreen();
  }
}


// ============================================================
// CANCEL SOS
// ============================================================

void cancelEmergency() {

  Serial.println();
  Serial.println(
    "*** CANCELLING EMERGENCY ***"
  );

  String payload =
    "{"
    "\"device_id\":\""
    +
    String(DEVICE_ID)
    +
    "\""
    "}";

  bool success =
    sendPost(
      "/api/cancel",
      payload
    );

  if (
    success
  ) {

    emergencyActive =
      false;

    remoteStatus =
      "SAFE";

    Serial.println(
      "EMERGENCY CANCELLED"
    );

    showCancelledScreen();

    delay(1700);

    showSafeScreen();

  } else {

    emergencyActive =
      true;

    Serial.println(
      "CANCEL FAILED - SOS REMAINS ACTIVE"
    );

    showFailedScreen(
      "Cancel NOT confirmed"
    );

    delay(2000);

    if (
      remoteStatus ==
      "ACKNOWLEDGED"
    ) {

      showAcknowledgedScreen();

    } else {

      showActiveScreen();
    }
  }
}


// ============================================================
// DASHBOARD STATUS POLLING
// ============================================================

void pollDashboardStatus() {

  if (
    WiFi.status() !=
    WL_CONNECTED
  ) {
    return;
  }

  HTTPClient http;

  String url =
    String(SERVER_BASE)
    +
    "/api/device-status/"
    +
    String(DEVICE_ID);

  http.setTimeout(
    5000
  );

  http.begin(
    url
  );

  http.addHeader(
    "X-API-Key",
    API_KEY
  );

  int httpCode =
    http.GET();

  if (
    httpCode != 200
  ) {

    Serial.print(
      "Status poll failed. HTTP: "
    );

    Serial.println(
      httpCode
    );

    http.end();

    return;
  }

  String response =
    http.getString();

  http.end();

  Serial.print(
    "Dashboard status: "
  );

  Serial.println(
    response
  );


  // ACKNOWLEDGED
  if (
    response.indexOf(
      "ACKNOWLEDGED"
    ) >= 0
  ) {

    bool changed =
      !emergencyActive
      ||
      remoteStatus !=
      "ACKNOWLEDGED";

    emergencyActive =
      true;

    remoteStatus =
      "ACKNOWLEDGED";

    if (
      changed
    ) {

      Serial.println(
        ">>> SOS ACKNOWLEDGED BY DASHBOARD"
      );

      showAcknowledgedScreen();
    }

    return;
  }


  // ACTIVE
  if (
    response.indexOf(
      "\"ACTIVE\""
    ) >= 0
    ||
    response.indexOf(
      ":\"ACTIVE\""
    ) >= 0
  ) {

    bool changed =
      !emergencyActive
      ||
      remoteStatus !=
      "ACTIVE";

    emergencyActive =
      true;

    remoteStatus =
      "ACTIVE";

    if (
      changed
    ) {

      Serial.println(
        ">>> SOS ACTIVE / NOT ACKNOWLEDGED"
      );

      showActiveScreen();
    }

    return;
  }


  // SAFE
  if (
    response.indexOf(
      "\"SAFE\""
    ) >= 0
    ||
    response.indexOf(
      ":\"SAFE\""
    ) >= 0
  ) {

    bool wasEmergency =
      emergencyActive;

    emergencyActive =
      false;

    remoteStatus =
      "SAFE";

    if (
      wasEmergency
    ) {

      Serial.println(
        ">>> EMERGENCY RESOLVED BY DASHBOARD"
      );

      showResolvedScreen();

      delay(1700);

      showSafeScreen();

    } else {

      showSafeScreen();
    }
  }
}


// ============================================================
// GREEN HARDWARE BUTTON
// ============================================================

void handleGreenButton() {

  unsigned long now =
    millis();

  if (
    now - lastButtonTime
    <
    BUTTON_COOLDOWN_MS
  ) {
    return;
  }

  lastButtonTime =
    now;

  if (
    emergencyActive
  ) {

    cancelEmergency();

  } else {

    sendEmergency();
  }
}


// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(
    115200
  );

  delay(500);

  auto cfg =
    M5.config();

  M5.begin(
    cfg
  );

  initGps();

  M5.Display.setRotation(
    1
  );

  M5.Display.setBrightness(
    180
  );

  bool connected =
    connectWiFi();

  if (
    connected
  ) {

    // Send battery data immediately after connecting.
    sendTelemetry();

    // Restore real emergency state.
    pollDashboardStatus();

  } else {

    showFailedScreen(
      "Wi-Fi not connected"
    );

    delay(1800);

    showSafeScreen();
  }

  Serial.println();
  Serial.println(
    "================================="
  );

  Serial.println(
    " Nesso N1 Emergency System Ready"
  );

  Serial.println(
    "================================="
  );

  Serial.print(
    "Device ID: "
  );

  Serial.println(
    DEVICE_ID
  );

  Serial.print(
    "Battery: "
  );

  Serial.print(
    batteryPercentText()
  );

  Serial.print(
    " | Voltage: "
  );

  Serial.print(
    getBatteryMv()
  );

  Serial.print(
    " mV | Power: "
  );

  Serial.println(
    powerStateText()
  );

  printGpsStatusToSerial();
}


// ============================================================
// LOOP
// ============================================================

void loop() {

  serviceGps();

  M5.update();


  // GREEN HARDWARE BUTTON
  // If your green physical button is BtnB on your unit,
  // change ONLY BtnA to BtnB.
  if (
    M5.BtnA.wasPressed()
  ) {

    Serial.println();
    Serial.println(
      "GREEN BUTTON PRESSED"
    );

    handleGreenButton();
  }


  // Emergency state synchronization.
  if (
    WiFi.status() ==
    WL_CONNECTED
    &&
    millis() - lastStatusPoll
    >=
    STATUS_POLL_MS
  ) {

    lastStatusPoll =
      millis();

    pollDashboardStatus();
  }


  // Battery / Wi-Fi telemetry.
  if (
    WiFi.status() ==
    WL_CONNECTED
    &&
    millis() - lastTelemetry
    >=
    TELEMETRY_INTERVAL_MS
  ) {

    lastTelemetry =
      millis();

    sendTelemetry();

    // Refresh text so battery percentage can change on-screen.
    if (
      remoteStatus ==
      "ACKNOWLEDGED"
    ) {

      showAcknowledgedScreen();

    } else if (
      emergencyActive
    ) {

      showActiveScreen();

    } else {

      showSafeScreen();
    }
  }


  // Wi-Fi recovery.
  if (
    WiFi.status() !=
    WL_CONNECTED
    &&
    millis() - lastReconnectAttempt
    >=
    RECONNECT_INTERVAL_MS
  ) {

    lastReconnectAttempt =
      millis();

    Serial.println(
      "Wi-Fi lost. Reconnecting..."
    );

    if (
      connectWiFi()
    ) {

      sendTelemetry();
      pollDashboardStatus();

    } else {

      if (
        emergencyActive
      ) {

        if (
          remoteStatus ==
          "ACKNOWLEDGED"
        ) {

          showAcknowledgedScreen();

        } else {

          showActiveScreen();
        }

      } else {

        showSafeScreen();
      }
    }
  }


  delay(10);
}
