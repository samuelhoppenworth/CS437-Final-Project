#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BH1750.h>

// ========== DISPLAY SETTINGS ==========
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1  // No reset pin
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ========== LIGHT SENSOR ==========
BH1750 lightMeter;
float lightLevel = 0;  // Light level in lux

// Light level thresholds (in lux)
const int LIGHT_LOW = 1000;      // Below this = low light
const int LIGHT_GOOD = 10000;    // Above this = good sunlight
// 0-1000 = Low, 1000-10000 = Moderate, 10000+ = Good sunlight

// ========== WIFI SETTINGS ==========
const char* ssid = "DeanUnit414";      // Change to match your iPhone name exactly
const char* password = "AF7AQG6P";   // Your hotspot password

// ========== PIN SETTINGS ==========
const int relayPin = 18;       // GPIO18 for relay (digital output - works fine)
const int sensorPin = 34;      // VP pin - MUST be GPIO 32-39 for ADC1 (works with WiFi)
const int waterLevelPin = 23;  // GPIO23 - HIGH = water OK, LOW = water low

// ========== VARIABLES ==========
float sensorValue = 0;
float moisturePercent = 0;
bool pumpState = false;     // false = OFF, true = ON
bool manualMode = true;     // Start in MANUAL mode so auto doesn't override
bool waterLevelLow = false; // true when water supply is low

// Auto mode status tracking
String autoStatus = "IDLE";      // IDLE, PUMPING, WAITING
int waitSecondsLeft = 0;         // Seconds remaining in wait period

// Swapped back to ACTIVE LOW
#define RELAY_ON LOW
#define RELAY_OFF HIGH

// ========== SENSOR CALIBRATION ==========
// Calibrate these based on YOUR sensor readings:
const int SENSOR_WET = 900;    // Raw value when sensor is in water (100% moisture)
const int SENSOR_DRY = 3500;   // Raw value when sensor is in dry air (0% moisture)
const int DRY_THRESHOLD_PERCENT = 50;  // Pump activates when moisture drops below 50%

// Create web server on port 80
WebServer server(80);

// ========== HTML PAGE ==========
String getHTML() {
  String pumpStatus = pumpState ? "ON" : "OFF";
  String pumpColor = pumpState ? "#4CAF50" : "#f44336";
  String modeStatus = manualMode ? "MANUAL" : "AUTO";
  String modeColor = manualMode ? "#ff9800" : "#2196F3";
  
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="refresh" content="3">
  <title>Plant Watering System</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;700&display=swap');
    
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }
    
    body {
      font-family: 'Outfit', sans-serif;
      min-height: 100vh;
      background: linear-gradient(135deg, #1a1a2e 0%, #16213e 50%, #0f3460 100%);
      color: #e8e8e8;
      padding: 20px;
    }
    
    .container {
      max-width: 420px;
      margin: 0 auto;
    }
    
    h1 {
      text-align: center;
      font-size: 1.8em;
      font-weight: 700;
      margin-bottom: 30px;
    }
    
    h1 .title-text {
      background: linear-gradient(90deg, #00d9ff, #00ff88);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      background-clip: text;
    }
    
    h1 .emoji {
      -webkit-text-fill-color: initial;
      margin-right: 8px;
    }
    
    .card {
      background: rgba(255, 255, 255, 0.05);
      backdrop-filter: blur(10px);
      border-radius: 20px;
      padding: 25px;
      margin-bottom: 20px;
      border: 1px solid rgba(255, 255, 255, 0.1);
      box-shadow: 0 8px 32px rgba(0, 0, 0, 0.3);
    }
    
    .card-title {
      font-size: 0.85em;
      text-transform: uppercase;
      letter-spacing: 2px;
      color: #888;
      margin-bottom: 15px;
    }
    
    .moisture-display {
      text-align: center;
    }
    
    .moisture-value {
      font-size: 4em;
      font-weight: 700;
      color: #00ff88;
      text-shadow: 0 0 30px rgba(0, 255, 136, 0.5);
    }
    
    .moisture-bar {
      height: 12px;
      background: rgba(255, 255, 255, 0.1);
      border-radius: 6px;
      margin-top: 20px;
      overflow: hidden;
    }
    
    .moisture-fill {
      height: 100%;
      background: linear-gradient(90deg, #f44336, #ff9800, #4CAF50);
      border-radius: 6px;
      transition: width 0.5s ease;
    }
    
    .raw-value {
      font-size: 0.9em;
      color: #666;
      margin-top: 10px;
    }
    
    .status-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 15px 0;
      border-bottom: 1px solid rgba(255, 255, 255, 0.05);
    }
    
    .status-row:last-child {
      border-bottom: none;
    }
    
    .status-label {
      font-size: 1em;
      color: #aaa;
    }
    
    .status-value {
      font-size: 1.1em;
      font-weight: 600;
      padding: 6px 16px;
      border-radius: 20px;
    }
    
    a {
      text-decoration: none;
    }
    
    .btn {
      display: block;
      width: 100%;
      padding: 18px;
      font-size: 1.1em;
      font-weight: 600;
      font-family: 'Outfit', sans-serif;
      border: none;
      border-radius: 15px;
      cursor: pointer;
      transition: all 0.3s ease;
      text-transform: uppercase;
      letter-spacing: 1px;
      margin-bottom: 12px;
    }
    
    .btn-pump {
      background: linear-gradient(135deg, #00d9ff, #0099cc);
      color: #fff;
      box-shadow: 0 4px 20px rgba(0, 217, 255, 0.4);
    }
    
    .btn-pump:hover {
      transform: translateY(-2px);
      box-shadow: 0 6px 25px rgba(0, 217, 255, 0.5);
    }
    
    .btn-mode {
      background: rgba(255, 255, 255, 0.1);
      color: #e8e8e8;
      border: 1px solid rgba(255, 255, 255, 0.2);
    }
    
    .btn-mode:hover {
      background: rgba(255, 255, 255, 0.15);
    }
    
    .status-animation {
      position: relative;
    }
    
    .status-animation.pumping {
      animation: pulse-green 1s infinite;
    }
    
    .status-animation.waiting {
      animation: pulse-orange 2s infinite;
    }
    
    @keyframes pulse-green {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.6; }
    }
    
    @keyframes pulse-orange {
      0%, 100% { transform: scale(1); }
      50% { transform: scale(1.05); }
    }
    
    .pulse-ring {
      position: absolute;
      top: 50%;
      left: 50%;
      transform: translate(-50%, -50%);
      width: 80px;
      height: 80px;
      border: 3px solid #4CAF50;
      border-radius: 50%;
      animation: ring-pulse 1s infinite;
    }
    
    @keyframes ring-pulse {
      0% { transform: translate(-50%, -50%) scale(0.8); opacity: 1; }
      100% { transform: translate(-50%, -50%) scale(1.5); opacity: 0; }
    }
    
    .plant-icon {
      font-size: 3em;
      text-align: center;
      margin-bottom: 10px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1><span class="emoji">&#127807;</span><span class="title-text">Plant Watering System</span></h1>
    
    <div class="card">
      <div class="card-title">Soil Moisture</div>
      <div class="moisture-display">
        <div class="moisture-value">)rawliteral";
  
  html += String(moisturePercent, 0);
  html += R"rawliteral(%</div>
        <div class="moisture-bar">
          <div class="moisture-fill" style="width: )rawliteral";
  html += String(moisturePercent, 0);
  html += R"rawliteral(%;"></div>
        </div>
        <div class="raw-value">Raw sensor: )rawliteral";
  html += String(sensorValue, 0);
  html += R"rawliteral(</div>
      </div>
    </div>
    
    <div class="card">
      <div class="card-title">System Status</div>
      <div class="status-row">
        <span class="status-label">Pump</span>
        <span class="status-value" style="background: )rawliteral";
  html += pumpColor;
  html += R"rawliteral(; color: #fff;">)rawliteral";
  html += pumpStatus;
  html += R"rawliteral(</span>
      </div>
      <div class="status-row">
        <span class="status-label">Mode</span>
        <span class="status-value" style="background: )rawliteral";
  html += modeColor;
  html += R"rawliteral(; color: #fff;">)rawliteral";
  html += modeStatus;
  html += R"rawliteral(</span>
      </div>
      <div class="status-row">
        <span class="status-label">Water Supply</span>
        <span class="status-value" style="background: )rawliteral";
  html += waterLevelLow ? "#f44336" : "#4CAF50";
  html += R"rawliteral(; color: #fff;">)rawliteral";
  html += waterLevelLow ? "LOW!" : "OK";
  html += R"rawliteral(</span>
      </div>
      <div class="status-row">
        <span class="status-label">Sunlight</span>
        <span class="status-value" style="background: )rawliteral";
  // Color based on light level
  if (lightLevel >= LIGHT_GOOD) {
    html += "#4CAF50";  // Green - good
  } else if (lightLevel >= LIGHT_LOW) {
    html += "#ff9800";  // Orange - moderate
  } else {
    html += "#f44336";  // Red - low
  }
  html += R"rawliteral(; color: #fff;">)rawliteral";
  if (lightLevel >= LIGHT_GOOD) {
    html += "GOOD";
  } else if (lightLevel >= LIGHT_LOW) {
    html += "MODERATE";
  } else {
    html += "LOW";
  }
  html += R"rawliteral(</span>
      </div>
    </div>
    
    <div class="card">
      <div class="card-title">Light Level</div>
      <div class="moisture-display">
        <div class="moisture-value" style="font-size: 2.5em;">)rawliteral";
  html += String((int)lightLevel);
  html += R"rawliteral(</div>
        <div class="raw-value">lux</div>
      </div>
    </div>
    )rawliteral";
  
  // Show warning banner if water is low
  if (waterLevelLow) {
    html += R"rawliteral(
    <div class="card" style="background: rgba(244, 67, 54, 0.3); border: 2px solid #f44336;">
      <div style="text-align: center; color: #ff6b6b;">
        <div style="font-size: 2em;">&#9888;</div>
        <div style="font-size: 1.2em; font-weight: 600;">WATER SUPPLY LOW</div>
        <div style="font-size: 0.9em; color: #aaa;">Please refill the water reservoir</div>
      </div>
    </div>
    )rawliteral";
  }
  
  // Auto mode status card
  if (!manualMode) {
    html += R"rawliteral(
    <div class="card">
      <div class="card-title">Auto Mode Status</div>
      <div style="text-align: center; padding: 10px;">
    )rawliteral";
    
    if (autoStatus == "PUMPING") {
      html += R"rawliteral(
        <div class="status-animation pumping">
          <div style="font-size: 2em;">&#128167;</div>
          <div style="font-size: 1.3em; color: #4CAF50; font-weight: 600;">PUMPING WATER</div>
          <div class="pulse-ring"></div>
        </div>
      )rawliteral";
    } else if (autoStatus == "WAITING") {
      html += R"rawliteral(
        <div class="status-animation waiting">
          <div style="font-size: 2em;">&#9203;</div>
          <div style="font-size: 1.3em; color: #ff9800; font-weight: 600;">WAITING</div>
          <div style="font-size: 2em; color: #00d9ff; margin-top: 5px;">)rawliteral";
      html += String(waitSecondsLeft);
      html += R"rawliteral(s</div>
          <div style="font-size: 0.9em; color: #888;">until next check</div>
        </div>
      )rawliteral";
    } else {
      html += R"rawliteral(
        <div style="font-size: 2em;">&#9989;</div>
        <div style="font-size: 1.2em; color: #4CAF50;">SOIL MOISTURE OK</div>
        <div style="font-size: 0.9em; color: #888;">No watering needed</div>
      )rawliteral";
    }
    
    html += R"rawliteral(
      </div>
    </div>
    )rawliteral";
  }
  
  html += R"rawliteral(
    <div class="card">
      <div class="card-title">Controls</div>
      <a href="/toggle"><button class="btn btn-pump">&#128167; Toggle Pump</button></a>
      <a href="/mode"><button class="btn btn-mode">&#128260; Switch Mode</button></a>
    </div>
  </div>
</body>
</html>
)rawliteral";
  
  return html;
}

// ========== WEB HANDLERS ==========
void handleRoot() {
  server.send(200, "text/html", getHTML());
}

void handleToggle() {
  manualMode = true;  // Switch to manual when button pressed
  pumpState = !pumpState;
  digitalWrite(relayPin, pumpState ? RELAY_ON : RELAY_OFF);
  Serial.print("TOGGLE pressed! Pump is now: ");
  Serial.println(pumpState ? "ON" : "OFF");
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleMode() {
  manualMode = !manualMode;
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleNotFound() {
  server.send(404, "text/plain", "Not Found");
}

// ========== SETUP ==========
void setup() {
  Serial.begin(115200);
  
  // Initialize OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 display not found!");
  } else {
    Serial.println("OLED display initialized!");
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Plant Watering");
    display.println("System Starting...");
    display.display();
  }
  
  // Initialize BH1750 light sensor
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("BH1750 light sensor initialized!");
  } else {
    Serial.println("BH1750 light sensor not found!");
  }
  
  // Setup pins
  pinMode(relayPin, OUTPUT);
  pinMode(sensorPin, INPUT);
  pinMode(waterLevelPin, INPUT);
  digitalWrite(relayPin, RELAY_OFF);
  
  // Connect to WiFi
  Serial.println();
  Serial.println("==== WiFi Debug Info ====");
  Serial.print("Looking for network: [");
  Serial.print(ssid);
  Serial.println("]");
  Serial.println("=========================");
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  // Scan for available networks
  Serial.println("Scanning for WiFi networks...");
  int n = WiFi.scanNetworks();
  Serial.print("Found ");
  Serial.print(n);
  Serial.println(" networks:");
  for (int i = 0; i < n; i++) {
    Serial.print("  ");
    Serial.print(i + 1);
    Serial.print(") ");
    Serial.print(WiFi.SSID(i));
    Serial.print(" (Signal: ");
    Serial.print(WiFi.RSSI(i));
    Serial.println(" dBm)");
  }
  Serial.println();
  
  Serial.print("Attempting to connect to: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("===== SUCCESS! CONNECTED! =====");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.println("================================");
  } else {
    Serial.println("===== CONNECTION FAILED =====");
    Serial.print("Status: ");
    Serial.println(WiFi.status());
    Serial.println("1=NO_SSID_AVAIL, 4=CONNECT_FAILED, 6=DISCONNECTED");
    Serial.println("Make sure hotspot name matches EXACTLY (case-sensitive)!");
    Serial.println("=============================");
  }
  // Setup web server routes
  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);
  server.on("/mode", handleMode);
  server.onNotFound(handleNotFound);
  
  server.begin();
  Serial.println("Web server started!");
}

// ========== LOOP ==========
void loop() {
  // Handle web requests
  server.handleClient();
  
  // Read moisture sensor
  sensorValue = analogRead(sensorPin);
  
  // Read water level sensor (HIGH = water OK, LOW = water low)
  waterLevelLow = digitalRead(waterLevelPin) == LOW;
  
  // Read light sensor
  lightLevel = lightMeter.readLightLevel();
  
  // Calculate moisture % using calibration values
  // Map: SENSOR_WET (900) = 100%, SENSOR_DRY (3500) = 0%
  moisturePercent = 100.0 - (((sensorValue - SENSOR_WET) / (float)(SENSOR_DRY - SENSOR_WET)) * 100.0);
  if (moisturePercent < 0) moisturePercent = 0;
  if (moisturePercent > 100) moisturePercent = 100;
  
  // Auto mode: control pump based on moisture with timed cycles
  static unsigned long pumpStartTime = 0;    // When pump started
  static unsigned long lastWaterTime = 0;    // When last watering cycle ended
  static bool isWatering = false;            // Currently in a watering cycle?
  
  const unsigned long PUMP_DURATION = 1500;  // Run pump for 1.5 seconds
  const unsigned long WAIT_DURATION = 60000; // Wait 60 seconds before re-evaluating
  
  if (!manualMode) {
    unsigned long currentTime = millis();
    
    if (isWatering) {
      // Currently watering - check if 1.5 seconds have passed
      autoStatus = "PUMPING";
      waitSecondsLeft = 0;
      if (currentTime - pumpStartTime >= PUMP_DURATION) {
        // Stop pump after 1.5 seconds
        pumpState = false;
        digitalWrite(relayPin, RELAY_OFF);
        isWatering = false;
        lastWaterTime = currentTime;
        Serial.println("AUTO: Pump stopped. Waiting 60 seconds...");
      }
    } else {
      // Not currently watering - check if we should start
      if (moisturePercent >= DRY_THRESHOLD_PERCENT) {
        // Soil is wet enough (above 50%) - do nothing
        autoStatus = "IDLE";
        waitSecondsLeft = 0;
        pumpState = false;
        digitalWrite(relayPin, RELAY_OFF);
      } else if (currentTime - lastWaterTime >= WAIT_DURATION || lastWaterTime == 0) {
        // Soil is dry AND 60 seconds have passed (or first run)
        autoStatus = "PUMPING";
        pumpState = true;
        digitalWrite(relayPin, RELAY_ON);
        isWatering = true;
        pumpStartTime = currentTime;
        Serial.println("AUTO: Soil dry! Pumping for 1.5 seconds...");
      } else {
        // Soil is dry but still waiting
        autoStatus = "WAITING";
        unsigned long elapsed = currentTime - lastWaterTime;
        waitSecondsLeft = (WAIT_DURATION - elapsed) / 1000;
      }
    }
  } else {
    autoStatus = "MANUAL";
    waitSecondsLeft = 0;
  }
  
  // Update display and serial output (every ~1 second)
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000) {
    lastPrint = millis();
    
    // Update OLED display
    display.clearDisplay();
    display.setTextSize(1);  // Small text (6x8 pixels per char)
    
    // Line 1: Title
    display.setCursor(0, 0);
    display.print("== PLANT MONITOR ==");
    
    // Line 2: Moisture
    display.setCursor(0, 10);
    display.print("Moisture: ");
    display.print((int)moisturePercent);
    display.print("%");
    
    // Line 3: Light
    display.setCursor(0, 20);
    display.print("Light: ");
    display.print((int)lightLevel);
    display.print(" lux");
    
    // Line 4: Light status
    display.setCursor(0, 30);
    display.print("Sun: ");
    if (lightLevel >= LIGHT_GOOD) {
      display.print("GOOD");
    } else if (lightLevel >= LIGHT_LOW) {
      display.print("MODERATE");
    } else {
      display.print("LOW");
    }
    
    // Line 5: Pump status
    display.setCursor(0, 40);
    display.print("Pump: ");
    display.print(pumpState ? "ON" : "OFF");
    display.print(" [");
    display.print(manualMode ? "MANUAL" : "AUTO");
    display.print("]");
    
    // Line 6: Water supply
    display.setCursor(0, 50);
    display.print("Water: ");
    display.print(waterLevelLow ? "!! LOW !!" : "OK");
    
    display.display();
    
    // Serial debug
    Serial.print("Pump: ");
    Serial.print(pumpState ? "ON" : "OFF");
    Serial.print(" | Mode: ");
    Serial.print(manualMode ? "MANUAL" : "AUTO");
    Serial.print(" | Moisture: ");
    Serial.print((int)moisturePercent);
    Serial.println("%");
  }
  delay(100);  // Small delay for web responsiveness
}