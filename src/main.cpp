#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Ethernet.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <ArduinoMDNS.h>
#include <EthernetUdp.h>
#include <NTPClient.h>
#include <TimeLib.h>

// Configuration
const int NUM_BATTERIES = 10;
const int ANALOG_PINS[16] = {A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15};
const float BATTERY_VOLTAGE_MAX = 12.0; // Maximum battery voltage being monitored
const float ARDUINO_REF_VOLTAGE = 5.0;  // Arduino analog reference voltage
const unsigned long LOG_INTERVAL = 60000; // Log every minute
const unsigned long DISPLAY_UPDATE = 2000; // Update display every 2 seconds
const int SD_CS_PIN = 4; // SD card CS pin (default for Ethernet Shield)

// Time configuration
const long TIMEZONE_OFFSET = -4 * 3600; // UTC offset in seconds (EST = -5 hours)
const char* NTP_SERVER = "pool.ntp.org";
const unsigned long NTP_UPDATE_INTERVAL = 3600000; // Update every hour

// Hardware setup
LiquidCrystal_I2C lcd(0x27, 16, 2);
byte mac[] = {0xA8, 0x61, 0x0A, 0xAE, 0x34, 0xF2};
EthernetServer server(80);
EthernetUDP udp;
EthernetUDP ntpUDP;
MDNS mdns(udp);
NTPClient timeClient(ntpUDP, NTP_SERVER, 0, NTP_UPDATE_INTERVAL);

// Network settings
String deviceId = "3572";  // Set your custom device identifier here
String mdnsHostname = "";
IPAddress assignedIP;

// Battery monitoring
struct Battery {
  int analogPin;
  int rawValue;
  float voltage;
  float percentage;
  bool isHealthy;
  unsigned long lastUpdate;
};

Battery batteries[NUM_BATTERIES];
unsigned long lastLogTime = 0;
unsigned long lastDisplayUpdate = 0;
int currentDisplayBattery = 0;

// Status LEDs
const int RED_LED = 12;
const int GREEN_LED = 13;
bool ledState = false;
unsigned long lastLedUpdate = 0;

// Function declarations
void readBatteries();
void updateDisplay();
void updateStatusLEDs(unsigned long currentTime);
void logBatteryData(unsigned long timestamp);
void handleWebRequests();
void sendDashboard(EthernetClient& client);
void sendCurrentData(EthernetClient& client);
void sendHistoryData(EthernetClient& client);
void send404(EthernetClient& client);

// Time function declarations
String getUTCTimeString();
String getLocalTimeString();
String getUSLocalTimeString();
String getDateTimeForCSV();
void initializeNTP();
unsigned long getUTCTimestamp();

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }

  // Initialize LEDs
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);

  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Battery Monitor");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");

  // Initialize battery structures
  for (int i = 0; i < NUM_BATTERIES; i++) {
    batteries[i].analogPin = ANALOG_PINS[i];
    batteries[i].rawValue = 0;
    batteries[i].voltage = 0.0;
    batteries[i].percentage = 0.0;
    batteries[i].isHealthy = true;
    batteries[i].lastUpdate = 0;
  }

  // Initialize SD card with detailed diagnostics
  Serial.print("Initializing SD card on CS pin ");
  Serial.print(SD_CS_PIN);
  Serial.print("...");

  lcd.setCursor(0, 1);
  lcd.print("Init SD card... ");

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println(" FAILED!");
    Serial.println("SD card troubleshooting:");
    Serial.println("1. Check card is inserted properly");
    Serial.println("2. Check card is formatted (FAT16/FAT32)");
    Serial.println("3. Check wiring to CS pin 4");
    Serial.println("4. Try different SD card");

    lcd.setCursor(0, 1);
    lcd.print("SD Card Failed! ");
    delay(3000);
  } else {
    Serial.println(" Success!");

    // Test SD card read/write capability
    Serial.print("Testing SD card write access...");
    File testFile = SD.open("test.txt", FILE_WRITE);
    if (testFile) {
      testFile.println("SD test");
      testFile.close();
      Serial.println(" Write OK");
      SD.remove("test.txt"); // Clean up test file
    } else {
      Serial.println(" Write FAILED!");
      Serial.println("SD card is read-only or corrupted");
    }

    // Create header in log file if it doesn't exist
    if (!SD.exists("battery.csv")) {
      Serial.print("Creating new log file...");
      File logFile = SD.open("battery.csv", FILE_WRITE);
      if (logFile) {
        logFile.print("DateTime_UTC,");
        for (int i = 0; i < NUM_BATTERIES; i++) {
          logFile.print("Battery");
          logFile.print(i + 1);
          logFile.print("_Raw,Battery");
          logFile.print(i + 1);
          logFile.print("_Voltage,Battery");
          logFile.print(i + 1);
          logFile.print("_Percentage");
          if (i < NUM_BATTERIES - 1) logFile.print(",");
        }
        logFile.println();
        logFile.close();
        Serial.println(" Success!");
      } else {
        Serial.println(" FAILED!");
        Serial.println("Cannot create log file - check SD card");
      }
    } else {
      Serial.println("Log file already exists");
    }

    lcd.setCursor(0, 1);
    lcd.print("SD Card Ready!  ");
    delay(1000);
  }

  // Set mDNS hostname using custom device ID
  mdnsHostname = "battery-monitor-" + deviceId;

  // Initialize Ethernet with DHCP
  Serial.print("Getting IP via DHCP...");
  lcd.setCursor(0, 1);
  lcd.print("Getting IP...   ");

  if (Ethernet.begin(mac) == 0) {
    Serial.println("DHCP failed! Using fallback IP");
    IPAddress fallbackIP(192, 168, 1, 177);
    Ethernet.begin(mac, fallbackIP);
  }

  assignedIP = Ethernet.localIP();
  Serial.print("IP address: ");
  Serial.println(assignedIP);

  // Start web server
  server.begin();

  // Initialize mDNS
  Serial.print("Starting mDNS as: ");
  Serial.print(mdnsHostname);
  Serial.println(".local");

  if (mdns.begin(assignedIP, mdnsHostname.c_str())) {
    mdns.addServiceRecord(mdnsHostname.c_str(), 80, MDNSServiceTCP, "\\x0dBattery Monitor");
    Serial.println("mDNS responder started");

    lcd.setCursor(0, 1);
    lcd.print(mdnsHostname.substring(0, 16));
    delay(3000);

    lcd.setCursor(0, 1);
    lcd.print(assignedIP);
    delay(2000);
  } else {
    Serial.println("mDNS failed to start");
    lcd.setCursor(0, 1);
    lcd.print("mDNS Failed!    ");
    delay(2000);
  }

  // Initialize NTP
  initializeNTP();

  lcd.setCursor(0, 1);
  lcd.print("Ready!          ");
  delay(1000);
}

void loop() {
  unsigned long currentTime = millis();

  // Process mDNS
  mdns.run();

  // Update NTP client
  timeClient.update();

  // Read battery values
  readBatteries();

  // Update display
  if (currentTime - lastDisplayUpdate >= DISPLAY_UPDATE) {
    updateDisplay();
    lastDisplayUpdate = currentTime;
  }

  // Update status LEDs
  updateStatusLEDs(currentTime);

  // Log data to SD card
  if (currentTime - lastLogTime >= LOG_INTERVAL) {
    logBatteryData(currentTime);
    lastLogTime = currentTime;
  }

  // Handle web requests
  handleWebRequests();

  delay(100);
}

void readBatteries() {
  for (int i = 0; i < NUM_BATTERIES; i++) {
    batteries[i].rawValue = analogRead(batteries[i].analogPin);
    // Calculate actual voltage based on voltage divider
    // Assumes voltage divider scales battery voltage to Arduino's 0-5V range
    float scaledVoltage = (batteries[i].rawValue * ARDUINO_REF_VOLTAGE) / 1023.0;
    batteries[i].voltage = scaledVoltage * (BATTERY_VOLTAGE_MAX / ARDUINO_REF_VOLTAGE);

    // Calculate percentage based on typical 12V battery range (10V=0%, 12.6V=100%)
    float minVoltage = BATTERY_VOLTAGE_MAX * 0.83; // 10V for 12V battery
    float maxVoltage = BATTERY_VOLTAGE_MAX * 1.05; // 12.6V for 12V battery
    batteries[i].percentage = constrain(map(batteries[i].voltage * 100, minVoltage * 100, maxVoltage * 100, 0, 100), 0, 100);

    // Consider below 20% (approximately 10.5V for 12V battery) as unhealthy
    batteries[i].isHealthy = batteries[i].percentage > 20;
    batteries[i].lastUpdate = millis();
  }
}

void updateDisplay() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Bat");
  lcd.print(currentDisplayBattery + 1);
  lcd.print(": ");
  lcd.print(batteries[currentDisplayBattery].voltage, 2);
  lcd.print("V");

  lcd.setCursor(0, 1);
  lcd.print(batteries[currentDisplayBattery].percentage);
  lcd.print("% ");
  lcd.print(batteries[currentDisplayBattery].isHealthy ? "OK" : "LOW");

  // Cycle through batteries
  currentDisplayBattery = (currentDisplayBattery + 1) % NUM_BATTERIES;
}

void updateStatusLEDs(unsigned long currentTime) {
  bool anyUnhealthy = false;
  for (int i = 0; i < NUM_BATTERIES; i++) {
    if (!batteries[i].isHealthy) {
      anyUnhealthy = true;
      break;
    }
  }

  if (anyUnhealthy) {
    // Blink red LED for warnings
    if (currentTime - lastLedUpdate >= 500) {
      ledState = !ledState;
      digitalWrite(RED_LED, ledState ? HIGH : LOW);
      digitalWrite(GREEN_LED, LOW);
      lastLedUpdate = currentTime;
    }
  } else {
    // Solid green for all healthy
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH);
  }
}

void logBatteryData(unsigned long timestamp) {
  // Check if SD card is still available
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD card no longer accessible");
    return;
  }

  File logFile = SD.open("battery.csv", FILE_WRITE);
  if (logFile) {
    size_t bytesWritten = 0;

    // Write timestamp
    bytesWritten += logFile.print(getDateTimeForCSV());
    bytesWritten += logFile.print(",");

    // Write battery data
    for (int i = 0; i < NUM_BATTERIES; i++) {
      bytesWritten += logFile.print(batteries[i].rawValue);
      bytesWritten += logFile.print(",");
      bytesWritten += logFile.print(batteries[i].voltage, 3);
      bytesWritten += logFile.print(",");
      bytesWritten += logFile.print(batteries[i].percentage, 1);
      if (i < NUM_BATTERIES - 1) bytesWritten += logFile.print(",");
    }
    bytesWritten += logFile.println();

    logFile.flush(); // Force write to SD card
    logFile.close();

    if (bytesWritten > 0) {
      Serial.print("Data logged (");
      Serial.print(bytesWritten);
      Serial.println(" bytes)");
    } else {
      Serial.println("Warning: No data written to SD card");
    }
  } else {
    Serial.println("ERROR: Cannot open battery.csv for writing");
    Serial.println("Possible causes:");
    Serial.println("- SD card removed or corrupted");
    Serial.println("- SD card full");
    Serial.println("- File system error");
  }
}

void handleWebRequests() {
  EthernetClient client = server.available();
  if (client) {
    String request = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        request += c;

        if (c == '\n' && request.endsWith("\r\n\r\n")) {
          break;
        }
      }
    }

    // Parse request
    if (request.indexOf("GET / ") >= 0) {
      sendDashboard(client);
    } else if (request.indexOf("GET /api/current") >= 0) {
      sendCurrentData(client);
    } else if (request.indexOf("GET /api/history") >= 0) {
      sendHistoryData(client);
    } else {
      send404(client);
    }

    client.stop();
  }
}

void sendDashboard(EthernetClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();

  client.println("<!DOCTYPE html>");
  client.println("<html>");
  client.println("<head>");
  client.println("<title>Battery Monitor Dashboard</title>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  client.println("<style>");
  client.println("body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }");
  client.println(".container { max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }");
  client.println(".battery-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin: 20px 0; }");
  client.println(".battery-card { border: 2px solid #ddd; border-radius: 8px; padding: 15px; text-align: center; }");
  client.println(".healthy { border-color: #4CAF50; background: #f8fff8; }");
  client.println(".warning { border-color: #ff9800; background: #fff8f0; }");
  client.println(".critical { border-color: #f44336; background: #fff0f0; }");
  client.println(".voltage { font-size: 24px; font-weight: bold; margin: 10px 0; }");
  client.println(".percentage { font-size: 18px; color: #666; }");
  client.println("h1 { text-align: center; color: #333; }");
  client.println("</style>");
  client.println("</head>");
  client.println("<body>");
  client.println("<div class='container'>");
  client.println("<h1>Battery Monitor Dashboard</h1>");
  client.print("<p style='text-align: center; color: #666;'>Device: ");
  client.print(mdnsHostname);
  client.print(".local | IP: ");
  client.print(assignedIP);
  client.println("</p>");
  client.println("<p id='datetime' style='text-align: center; color: #888; font-size: 14px;'></p>");
  client.println("<div class='battery-grid' id='batteryGrid'>");
  client.println("</div>");
  client.println("</div>");

  client.println("<script>");
  client.println("function updateDashboard() {");
  client.println("  fetch('/api/current')");
  client.println("    .then(response => response.json())");
  client.println("    .then(data => {");
  client.println("      const grid = document.getElementById('batteryGrid');");
  client.println("      grid.innerHTML = '';");
  client.println("      data.batteries.forEach((battery, index) => {");
  client.println("        const card = document.createElement('div');");
  client.println("        card.className = 'battery-card ' + (battery.percentage > 50 ? 'healthy' : battery.percentage > 20 ? 'warning' : 'critical');");
  client.println("        card.innerHTML = `");
  client.println("          <h3>Battery ${index + 1}</h3>");
  client.println("          <div class='voltage'>${battery.voltage.toFixed(2)}V</div>");
  client.println("          <div class='percentage'>${battery.percentage.toFixed(1)}%</div>");
  client.println("          <div>Raw: ${battery.raw}</div>");
  client.println("        `;");
  client.println("        grid.appendChild(card);");
  client.println("      });");
  client.println("      // Update datetime display");
  client.println("      if (data.datetime) {");
  client.println("        document.getElementById('datetime').textContent = 'Last updated: ' + data.datetime;");
  client.println("      }");
  client.println("    });");
  client.println("}");
  client.println("updateDashboard();");
  client.println("setInterval(updateDashboard, 2000);");
  client.println("</script>");
  client.println("</body>");
  client.println("</html>");
}

void sendCurrentData(EthernetClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();

  client.print("{\"timestamp\":");
  client.print(getUTCTimestamp());
  client.print(",\"datetime\":\"");
  client.print(getUSLocalTimeString());
  client.print("\",\"batteries\":[");

  for (int i = 0; i < NUM_BATTERIES; i++) {
    client.print("{");
    client.print("\"id\":");
    client.print(i + 1);
    client.print(",\"raw\":");
    client.print(batteries[i].rawValue);
    client.print(",\"voltage\":");
    client.print(batteries[i].voltage, 3);
    client.print(",\"percentage\":");
    client.print(batteries[i].percentage, 1);
    client.print(",\"healthy\":");
    client.print(batteries[i].isHealthy ? "true" : "false");
    client.print("}");
    if (i < NUM_BATTERIES - 1) client.print(",");
  }

  client.println("]}");
}

void sendHistoryData(EthernetClient& client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: application/json");
  client.println("Connection: close");
  client.println();

  client.println("{\"history\":[");

  File logFile = SD.open("battery.csv");
  if (logFile) {
    String line;
    bool firstLine = true;
    bool firstDataLine = true;

    while (logFile.available()) {
      line = logFile.readStringUntil('\n');
      line.trim();

      if (firstLine) {
        firstLine = false;
        continue; // Skip header
      }

      if (line.length() > 0) {
        if (!firstDataLine) client.print(",");

        int commaIndex = line.indexOf(',');
        String timestamp = line.substring(0, commaIndex);
        String data = line.substring(commaIndex + 1);

        client.print("{\"timestamp\":\"");
        client.print(timestamp);
        client.print("\",\"data\":[");

        int startIndex = 0;
        int batteryIndex = 0;

        while (startIndex < (int)data.length() && batteryIndex < NUM_BATTERIES) {
          if (batteryIndex > 0) client.print(",");

          // Raw value
          int nextComma = data.indexOf(',', startIndex);
          String rawValue = data.substring(startIndex, nextComma);
          startIndex = nextComma + 1;

          // Voltage
          nextComma = data.indexOf(',', startIndex);
          String voltage = data.substring(startIndex, nextComma);
          startIndex = nextComma + 1;

          // Percentage
          nextComma = data.indexOf(',', startIndex);
          if (nextComma == -1) nextComma = data.length();
          String percentage = data.substring(startIndex, nextComma);
          startIndex = nextComma + 1;

          client.print("{\"raw\":");
          client.print(rawValue);
          client.print(",\"voltage\":");
          client.print(voltage);
          client.print(",\"percentage\":");
          client.print(percentage);
          client.print("}");

          batteryIndex++;
        }

        client.print("]}");
        firstDataLine = false;
      }
    }
    logFile.close();
  }

  client.println("]}");
}

void send404(EthernetClient& client) {
  client.println("HTTP/1.1 404 Not Found");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<h1>404 - Not Found</h1>");
}

// Time functions implementation
void initializeNTP() {
  Serial.print("Initializing NTP client...");
  lcd.setCursor(0, 1);
  lcd.print("Syncing time... ");

  timeClient.begin();
  timeClient.update();

  int attempts = 0;
  while (!timeClient.isTimeSet() && attempts < 10) {
    delay(1000);
    timeClient.update();
    attempts++;
    Serial.print(".");
  }

  if (timeClient.isTimeSet()) {
    Serial.println(" Success!");
    setTime(timeClient.getEpochTime());
    lcd.setCursor(0, 1);
    lcd.print("Time synced!    ");
  } else {
    Serial.println(" Failed!");
    lcd.setCursor(0, 1);
    lcd.print("Time sync failed");
  }
  delay(2000);
}

unsigned long getUTCTimestamp() {
  if (timeClient.isTimeSet()) {
    return timeClient.getEpochTime();
  }
  return 0;
}

String getUTCTimeString() {
  if (!timeClient.isTimeSet()) {
    return "Time not synced";
  }

  unsigned long utc = timeClient.getEpochTime();
  tmElements_t tm;
  breakTime(utc, tm);

  char buffer[32];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d UTC",
          tmYearToCalendar(tm.Year), tm.Month, tm.Day,
          tm.Hour, tm.Minute, tm.Second);

  return String(buffer);
}

String getLocalTimeString() {
  if (!timeClient.isTimeSet()) {
    return "Time not synced";
  }

  unsigned long local = timeClient.getEpochTime() + TIMEZONE_OFFSET;
  tmElements_t tm;
  breakTime(local, tm);

  char buffer[32];
  sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
          tmYearToCalendar(tm.Year), tm.Month, tm.Day,
          tm.Hour, tm.Minute, tm.Second);

  return String(buffer);
}

String getUSLocalTimeString() {
  if (!timeClient.isTimeSet()) {
    return "Time not synced";
  }

  unsigned long local = timeClient.getEpochTime() + TIMEZONE_OFFSET;
  tmElements_t tm;
  breakTime(local, tm);

  // Convert to 12-hour format
  int hour12 = tm.Hour;
  String ampm = "AM";
  if (hour12 == 0) {
    hour12 = 12;
  } else if (hour12 > 12) {
    hour12 -= 12;
    ampm = "PM";
  } else if (hour12 == 12) {
    ampm = "PM";
  }

  char buffer[32];
  sprintf(buffer, "%02d/%02d/%04d %d:%02d:%02d %s",
          tm.Month, tm.Day, tmYearToCalendar(tm.Year),
          hour12, tm.Minute, tm.Second, ampm.c_str());

  return String(buffer);
}

String getDateTimeForCSV() {
  if (!timeClient.isTimeSet()) {
    return "1970-01-01T00:00:00Z";
  }

  unsigned long utc = timeClient.getEpochTime();
  tmElements_t tm;
  breakTime(utc, tm);

  char buffer[32];
  sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02dZ",
          tmYearToCalendar(tm.Year), tm.Month, tm.Day,
          tm.Hour, tm.Minute, tm.Second);

  return String(buffer);
}