# 🔋 Battery Monitor System

A comprehensive Arduino-based battery monitoring system that tracks up to 10 batteries simultaneously, providing real-time monitoring, data logging, and web-based dashboard access.

## ✨ Features

### 🔌 Multi-Battery Monitoring
- **Configurable battery count** - Monitor 1-10 batteries (easily expandable to 16)
- **Real-time voltage readings** - Accurate 12V battery monitoring with voltage divider support
- **Battery health assessment** - Automatic health status based on voltage levels
- **Percentage calculation** - Smart percentage calculation based on typical 12V battery ranges

### 🌐 Network Connectivity
- **DHCP Support** - Automatically gets IP address from your router
- **mDNS Service Discovery** - Access via `battery-monitor-3572.local`
- **Fallback IP** - Uses static IP (192.168.1.177) if DHCP fails
- **Web Dashboard** - Real-time battery status via web browser

### 🕐 Time Synchronization
- **NTP Time Sync** - Automatic time synchronization from public NTP servers
- **Timezone Support** - Configurable timezone offset (currently set to EDT UTC-4)
- **Multiple Time Formats** - UTC for logging, US format for display
- **Real-time Clock** - Maintains accurate time for data logging

### 💾 Data Logging
- **SD Card Logging** - Automatic CSV logging every minute
- **UTC Timestamps** - ISO 8601 format timestamps for universal compatibility
- **Historical Data** - Complete battery history for trend analysis
- **CSV Format** - Easy import into Excel, Python, or other analysis tools

### 📱 User Interfaces

#### LCD Display (16x2)
- **Rotating Display** - Cycles through all batteries every 2 seconds
- **Battery Status** - Shows voltage, percentage, and health status
- **System Status** - Displays initialization progress and network info

#### Web Dashboard
- **Real-time Updates** - Auto-refreshing every 2 seconds
- **Color-coded Status** - Green (healthy), Orange (warning), Red (critical)
- **Device Information** - Shows hostname, IP address, and last update time
- **Responsive Design** - Works on desktop, tablet, and mobile devices

#### Visual Indicators
- **Status LEDs** - Green (all healthy) or blinking red (warning)
- **Smart Alerts** - Blink rate indicates battery health status

## 🔧 Hardware Requirements

### Core Components
- **Arduino Mega 2560** - Main microcontroller
- **Ethernet Shield v2** - Network connectivity and SD card storage
- **32GB SD Card** - Data logging storage (FAT32 format recommended)
- **16x2 LCD Display** - Local status display (I2C interface)

### Battery Monitoring
- **Voltage Divider Circuits** - Scale 12V batteries to 0-5V Arduino range
- **Up to 10 Batteries** - Connected to analog pins A0-A9
- **Status LEDs** - Connected to digital pins 12 (red) and 13 (green)

### Network Setup
- **Ethernet Connection** - Via Ethernet Shield
- **Router with DHCP** - For automatic IP assignment
- **Internet Access** - For NTP time synchronization

## ⚙️ Configuration

### Battery Settings
```cpp
const int NUM_BATTERIES = 10;              // Number of batteries to monitor
const float BATTERY_VOLTAGE_MAX = 12.0;    // Maximum battery voltage
const float ARDUINO_REF_VOLTAGE = 5.0;     // Arduino reference voltage
```

### Time Settings
```cpp
const long TIMEZONE_OFFSET = -4 * 3600;    // UTC offset in seconds (EDT)
const char* NTP_SERVER = "pool.ntp.org";   // NTP server address
```

### Device Identity
```cpp
String deviceId = "3572";                  // Custom device identifier
```

### Logging Intervals
```cpp
const unsigned long LOG_INTERVAL = 60000;    // Log every minute
const unsigned long DISPLAY_UPDATE = 2000;   // Update display every 2 seconds
```

## 🌐 Web API Endpoints

### Main Dashboard
- **URL**: `http://battery-monitor-3572.local/` or `http://[ip-address]/`
- **Description**: Interactive web dashboard with real-time battery status

### Current Data API
- **URL**: `/api/current`
- **Format**: JSON
- **Description**: Real-time battery data with timestamps
```json
{
  "timestamp": 1727388645,
  "datetime": "09/26/2024 3:30:45 PM",
  "batteries": [
    {
      "id": 1,
      "raw": 512,
      "voltage": 12.34,
      "percentage": 85.2,
      "healthy": true
    }
  ]
}
```

### Historical Data API
- **URL**: `/api/history`
- **Format**: JSON
- **Description**: Complete battery history from SD card
```json
{
  "history": [
    {
      "timestamp": "2024-09-26T20:30:45Z",
      "data": [
        {"raw": 512, "voltage": 12.34, "percentage": 85.2}
      ]
    }
  ]
}
```

## 📊 Battery Health Logic

### Voltage Ranges (12V Batteries)
- **100%**: 12.6V (fully charged)
- **50%**: 11.8V (half charged)
- **20%**: 10.5V (low battery warning)
- **0%**: 10.0V (discharged)

### Health Status
- **Healthy** (Green): Above 20% charge
- **Warning** (Blinking Red): Below 20% charge
- **Critical** (Red): Immediate attention needed

## 💾 Data Storage

### SD Card Format
- **File**: `battery.csv`
- **Format**: CSV with headers
- **Timestamp**: ISO 8601 UTC format
- **Columns**: DateTime_UTC, Battery1_Raw, Battery1_Voltage, Battery1_Percentage, ...

### Sample CSV Data
```csv
DateTime_UTC,Battery1_Raw,Battery1_Voltage,Battery1_Percentage
2024-09-26T20:30:45Z,512,12.340,85.2
2024-09-26T20:31:45Z,510,12.315,84.8
```

## 🚀 Getting Started

### 1. Hardware Setup
1. Connect Arduino Mega 2560 to Ethernet Shield v2
2. Insert formatted 32GB SD card into shield
3. Connect I2C LCD display (SDA/SCL pins)
4. Wire voltage divider circuits for each battery
5. Connect status LEDs to pins 12 and 13

### 2. Software Installation
1. Install PlatformIO extension in VS Code
2. Clone this repository
3. Open project in PlatformIO
4. Configure settings in `main.cpp`
5. Build and upload to Arduino

### 3. Network Configuration
1. Connect Ethernet cable to shield
2. Power on Arduino - it will get IP via DHCP
3. Check Serial Monitor for assigned IP address
4. Access web dashboard via `battery-monitor-3572.local`

## 🔧 Troubleshooting

### SD Card Issues
- Ensure card is formatted as FAT32
- Check card size (32GB or smaller recommended)
- Verify card is properly inserted
- Try different SD card if problems persist

### Network Issues
- Check Ethernet cable connection
- Verify router DHCP is enabled
- Check Serial Monitor for network status
- Try accessing via IP address if mDNS fails

### Battery Readings
- Verify voltage divider ratios (12V → 5V)
- Check analog pin connections (A0-A9)
- Calibrate voltage references if needed
- Test with known battery voltages

## 📈 Future Enhancements

- [ ] Email/SMS alerts for low batteries
- [ ] Historical data graphing on web dashboard
- [ ] Battery trend analysis and predictions
- [ ] Remote configuration via web interface
- [ ] Mobile app integration
- [ ] Multiple device coordination

## 📝 License

This project is open source and available under the MIT License.

## 🤝 Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for bugs and feature requests.

---

**Device ID**: battery-monitor-3572.local
**Default IP**: 192.168.1.177 (if DHCP fails)
**Timezone**: EDT (UTC-4)
**Logging Interval**: 60 seconds