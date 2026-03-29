# MediVend ESP32 Dispenser Controller

Arduino sketch for ESP32 that receives dispense commands from the MediScan Mart web app.

## Quick Start

### 1. Install Arduino IDE & ESP32 Board

1. Download [Arduino IDE](https://www.arduino.cc/en/software)
2. Go to **File → Preferences**
3. Add this URL to "Additional Board Manager URLs":
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Go to **Tools → Board → Boards Manager**
5. Search "ESP32" and install **esp32 by Espressif Systems**

### 2. Install Required Library

- Go to **Sketch → Include Library → Manage Libraries**
- Search "ArduinoJson" by Benoit Blanchon
- Install **version 7.x**

### 3. Configure Wi-Fi

Open `esp32_dispenser.ino` and modify these lines:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";      // Your Wi-Fi name
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";  // Your Wi-Fi password
```

### 4. Upload to ESP32

1. Connect ESP32 via USB
2. Select **Tools → Board → ESP32 Dev Module**
3. Select correct **Tools → Port** (e.g., COM3)
4. Click **Upload** (→ button)

### 5. Get IP Address

1. Open **Tools → Serial Monitor**
2. Set baud rate to **115200**
3. Press **EN/Reset** button on ESP32
4. Note the IP address displayed (e.g., `192.168.1.105`)

## Hardware Wiring

| ESP32 Pin | Connection |
|-----------|------------|
| GPIO 13   | Motor/Relay IN |
| GND       | Motor/Relay GND |
| 5V/VIN    | Motor power (if needed) |

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/` | Health check |
| GET | `/status` | Dispenser status |
| POST | `/dispense` | Dispense medicines |

### POST /dispense

**Request Body:**
```json
{
  "paymentId": "pay_xxxx",
  "paymentStatus": "success",
  "items": [
    { "name": "Paracetamol", "qty": 3 }
  ]
}
```

**Response:**
```json
{
  "success": true,
  "dispensedCount": 3,
  "message": "Dispensing complete"
}
```

## Testing with curl

```bash
curl -X POST http://192.168.1.105/dispense \
  -H "Content-Type: application/json" \
  -d '{"paymentId":"test123","paymentStatus":"success","items":[{"name":"Test","qty":2}]}'
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Can't connect to Wi-Fi | Check SSID/password, ensure 2.4GHz network |
| Upload fails | Hold BOOT button during upload, try different USB cable |
| No serial output | Check baud rate (115200), press EN button |
| CORS errors | Already handled in code, check if ESP32 IP is correct |
