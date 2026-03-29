/*
 * =============================================================================
 * MediVend - ESP32 Medicine Dispenser Controller
 * =============================================================================
 * 
 * This sketch runs on an ESP32 DevKit board and receives dispense commands
 * from the MediScan Mart web application after successful payment.
 * 
 * HARDWARE SETUP:
 *   - ESP32 DevKit (ESP32-WROOM-32 or similar)
 *   - Motor/Relay connected to GPIO 13 (configurable below)
 *   - Power supply as needed for motor
 * 
 * LIBRARIES REQUIRED (Install via Arduino Library Manager):
 *   - ArduinoJson (by Benoit Blanchon) - Version 7.x
 *   - WiFi (built-in with ESP32 board package)
 *   - WebServer (built-in with ESP32 board package)
 * 
 * BOARD SETUP:
 *   - In Arduino IDE: Tools > Board > ESP32 Arduino > ESP32 Dev Module
 *   - Install ESP32 board package if not already installed:
 *     File > Preferences > Additional Board Manager URLs:
 *     https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
 * 
 * Author: MediScan Mart Team
 * Date: January 2026
 * =============================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// =============================================================================
// CONFIGURATION - MODIFY THESE VALUES FOR YOUR SETUP
// =============================================================================

// Wi-Fi Credentials (CHANGE THESE TO YOUR NETWORK)
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// =============================================================================
// MEDICINE SLOT CONFIGURATION - 3 SEPARATE MOTORS/RELAYS
// =============================================================================

// GPIO pin assignments for each medicine slot
// NOTE: Avoid GPIO 13 - it can cause issues on some ESP32 boards (onboard LED)
const int PIN_PARACETAMOL = 27;   // Slot 1: Paracetamol (changed from 13)
const int PIN_AMOXICILLIN = 12;   // Slot 2: Amoxicillin  
const int PIN_CETIRIZINE = 14;    // Slot 3: Cetirizine

// =============================================================================
// MOTOR CONTROL CONFIGURATION (OPTIMIZED for Precision Dispensing)
// =============================================================================

// PWM Configuration - Lower frequency = smoother at low speeds
const int PWM_FREQUENCY = 500;      // PWM frequency in Hz (was 1000)
const int PWM_RESOLUTION = 8;       // 8-bit resolution (0-255)

// Speed profiles - REDUCED for controlled single-rotation
int targetSpeed = 120;              // Default ~47% (adjustable via calibration)
const int CRAWL_SPEED = 80;         // ~30% - Very slow for precise positioning
const int NORMAL_SPEED = 120;       // ~47% - Standard dispensing (was 180)
const int FAST_SPEED = 150;         // ~60% - Maximum recommended
const int MIN_SPEED = 0;            // Starting speed

// Timing configuration - TUNED for ONE spring rotation
int rampUpTimeMs = 300;             // Soft acceleration (adjustable)
int runTimeMs = 500;                // Main dispense time (CRITICAL - tune this!)
int rampDownTimeMs = 250;           // Soft deceleration
int pauseBetweenMs = 400;           // Let spring settle
const int RAMP_STEP_DELAY_MS = 10;  // PWM ramp smoothness (was 15)

// PWM Channels for each motor (ESP32 has 16 channels: 0-15)
const int PWM_CHANNEL_1 = 0;  // Paracetamol
const int PWM_CHANNEL_2 = 1;  // Amoxicillin
const int PWM_CHANNEL_3 = 2;  // Cetirizine

// =============================================================================
// CALIBRATION MODE
// =============================================================================
bool calibrationMode = false;
int selectedSlot = 1;  // 1=Paracetamol, 2=Amoxicillin, 3=Cetirizine

// Server Configuration
const int SERVER_PORT = 80;

// =============================================================================
// GLOBAL VARIABLES
// =============================================================================

WebServer server(SERVER_PORT);

// Status tracking
bool isDispensing = false;
int totalDispensed = 0;

// =============================================================================
// SETUP FUNCTION
// =============================================================================

void setup() {
  // Initialize Serial Monitor for debugging
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("==============================================");
  Serial.println("   MediVend ESP32 Dispenser Controller");
  Serial.println("   3-SLOT MEDICINE DISPENSER");
  Serial.println("   >> SMOOTH MOTOR CONTROL (Anti-Vibration) <<");
  Serial.println("==============================================");
  Serial.println();

  // Initialize PWM channels for smooth motor control
  // Using ESP32 LEDC (LED Control) peripheral for PWM
  Serial.println("[INIT] Configuring PWM for smooth motor control...");
  
  // Setup PWM channel for Paracetamol (Slot 1)
  ledcSetup(PWM_CHANNEL_1, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcAttachPin(PIN_PARACETAMOL, PWM_CHANNEL_1);
  ledcWrite(PWM_CHANNEL_1, 0);  // Start with motor OFF
  
  // Setup PWM channel for Amoxicillin (Slot 2)
  ledcSetup(PWM_CHANNEL_2, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcAttachPin(PIN_AMOXICILLIN, PWM_CHANNEL_2);
  ledcWrite(PWM_CHANNEL_2, 0);  // Start with motor OFF
  
  // Setup PWM channel for Cetirizine (Slot 3)
  ledcSetup(PWM_CHANNEL_3, PWM_FREQUENCY, PWM_RESOLUTION);
  ledcAttachPin(PIN_CETIRIZINE, PWM_CHANNEL_3);
  ledcWrite(PWM_CHANNEL_3, 0);  // Start with motor OFF
  
  Serial.println("[INIT] Motor PWM channels configured:");
  Serial.println("       Slot 1 (Paracetamol): GPIO " + String(PIN_PARACETAMOL) + " -> PWM Ch " + String(PWM_CHANNEL_1));
  Serial.println("       Slot 2 (Amoxicillin): GPIO " + String(PIN_AMOXICILLIN) + " -> PWM Ch " + String(PWM_CHANNEL_2));
  Serial.println("       Slot 3 (Cetirizine):  GPIO " + String(PIN_CETIRIZINE) + " -> PWM Ch " + String(PWM_CHANNEL_3));
  Serial.println("[INIT] PWM Frequency: " + String(PWM_FREQUENCY) + " Hz");
  Serial.println("[INIT] Max Speed: " + String((targetSpeed * 100) / 255) + "% (adjustable via calibration)");

  // Connect to Wi-Fi
  connectToWiFi();

  // Setup HTTP server routes
  setupServerRoutes();

  // Start server
  server.begin();
  Serial.println("[SERVER] HTTP server started on port " + String(SERVER_PORT));
  Serial.println();
  printReadyMessage();
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  server.handleClient();
  
  // Check for serial calibration commands
  if (Serial.available() > 0) {
    handleSerialCommand();
  }
  
  delay(2);  // Small delay for stability
}

// =============================================================================
// WI-FI CONNECTION
// =============================================================================

void connectToWiFi() {
  Serial.print("[WIFI] Connecting to: ");
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("[WIFI] Connected successfully!");
    Serial.print("[WIFI] IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println("[WIFI] ERROR: Failed to connect!");
    Serial.println("[WIFI] Please check SSID and password, then restart.");
  }
}

// =============================================================================
// HTTP SERVER ROUTES
// =============================================================================

void setupServerRoutes() {
  // Health check endpoint
  server.on("/", HTTP_GET, handleRoot);
  
  // Status endpoint
  server.on("/status", HTTP_GET, handleStatus);
  
  // Main dispense endpoint
  server.on("/dispense", HTTP_POST, handleDispense);
  
  // Handle CORS preflight requests
  server.on("/dispense", HTTP_OPTIONS, handleCORS);
  
  // 404 handler
  server.onNotFound(handleNotFound);
  
  Serial.println("[SERVER] Routes configured:");
  Serial.println("         GET  /         - Health check");
  Serial.println("         GET  /status   - Dispenser status");
  Serial.println("         POST /dispense - Dispense medicines");
}

// =============================================================================
// ROUTE HANDLERS
// =============================================================================

// Root endpoint - simple health check
void handleRoot() {
  addCORSHeaders();
  String response = "{ \"device\": \"MediVend ESP32 Dispenser\", \"status\": \"online\", \"version\": \"1.0.0\" }";
  server.send(200, "application/json", response);
  Serial.println("[HTTP] GET / - Health check requested");
}

// Status endpoint - current dispenser state
void handleStatus() {
  addCORSHeaders();
  
  JsonDocument doc;
  doc["online"] = true;
  doc["isDispensing"] = isDispensing;
  doc["totalDispensed"] = totalDispensed;
  doc["slots"]["Paracetamol"] = PIN_PARACETAMOL;
  doc["slots"]["Amoxicillin"] = PIN_AMOXICILLIN;
  doc["slots"]["Cetirizine"] = PIN_CETIRIZINE;
  doc["ipAddress"] = WiFi.localIP().toString();
  
  String response;
  serializeJson(doc, response);
  
  server.send(200, "application/json", response);
  Serial.println("[HTTP] GET /status - Status requested");
}

// Handle CORS preflight OPTIONS request
void handleCORS() {
  addCORSHeaders();
  server.send(204);  // No content
  Serial.println("[HTTP] OPTIONS /dispense - CORS preflight");
}

// Main dispense handler
void handleDispense() {
  addCORSHeaders();
  
  Serial.println();
  Serial.println("================================================");
  Serial.println("[DISPENSE] Received dispense request");
  Serial.println("================================================");

  // Check if already dispensing
  if (isDispensing) {
    Serial.println("[ERROR] Already dispensing! Request rejected.");
    sendErrorResponse(503, "Dispenser busy - already dispensing");
    return;
  }

  // Check for request body
  if (!server.hasArg("plain")) {
    Serial.println("[ERROR] No request body received!");
    sendErrorResponse(400, "Missing request body");
    return;
  }

  String requestBody = server.arg("plain");
  Serial.println("[DISPENSE] Request body:");
  Serial.println(requestBody);

  // Parse JSON
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, requestBody);

  if (error) {
    Serial.print("[ERROR] JSON parse failed: ");
    Serial.println(error.c_str());
    sendErrorResponse(400, "Invalid JSON format");
    return;
  }

  // Extract and validate payment status
  const char* paymentStatus = doc["paymentStatus"];
  const char* paymentId = doc["paymentId"];
  
  if (!paymentStatus) {
    Serial.println("[ERROR] Missing paymentStatus field!");
    sendErrorResponse(400, "Missing paymentStatus field");
    return;
  }

  Serial.print("[DISPENSE] Payment ID: ");
  Serial.println(paymentId ? paymentId : "N/A");
  Serial.print("[DISPENSE] Payment Status: ");
  Serial.println(paymentStatus);

  // CRITICAL: Verify payment was successful
  if (strcmp(paymentStatus, "success") != 0) {
    Serial.println("[ERROR] Payment status is not 'success' - DISPENSING BLOCKED");
    sendErrorResponse(402, "Payment not verified - dispensing blocked");
    return;
  }

  Serial.println("[DISPENSE] Payment verified: SUCCESS ✓");

  // Extract items array
  JsonArray items = doc["items"];
  if (items.isNull() || items.size() == 0) {
    Serial.println("[ERROR] No items array or empty items!");
    sendErrorResponse(400, "Missing or empty items array");
    return;
  }

  // Calculate total tablets to dispense
  int totalTablets = 0;
  Serial.println("[DISPENSE] Items to dispense:");
  
  for (JsonObject item : items) {
    const char* name = item["name"];
    int qty = item["qty"];
    totalTablets += qty;
    
    Serial.print("  - ");
    Serial.print(name ? name : "Unknown");
    Serial.print(": ");
    Serial.print(qty);
    Serial.println(" tablets");
  }

  Serial.print("[DISPENSE] Total tablets to dispense: ");
  Serial.println(totalTablets);

  // Start dispensing - process each medicine separately
  isDispensing = true;
  int dispensedCount = 0;
  
  for (JsonObject item : items) {
    const char* name = item["name"];
    int qty = item["qty"];
    
    if (name && qty > 0) {
      dispensedCount += dispenseMedicine(name, qty);
    }
  }
  
  isDispensing = false;
  totalDispensed += dispensedCount;

  // Send success response
  Serial.println("[DISPENSE] Dispensing complete!");
  Serial.print("[DISPENSE] Total dispensed: ");
  Serial.print(dispensedCount);
  Serial.println(" tablets");
  Serial.println("================================================");
  Serial.println();

  // Build response
  JsonDocument responseDoc;
  responseDoc["success"] = true;
  responseDoc["dispensedCount"] = dispensedCount;
  responseDoc["message"] = "Dispensing complete";
  
  String response;
  serializeJson(responseDoc, response);
  
  server.send(200, "application/json", response);
}

// 404 Not Found handler
void handleNotFound() {
  addCORSHeaders();
  sendErrorResponse(404, "Endpoint not found");
  Serial.print("[HTTP] 404 - Unknown endpoint: ");
  Serial.println(server.uri());
}

// =============================================================================
// DISPENSING LOGIC - PER MEDICINE SLOT
// =============================================================================

// Get the GPIO pin for a specific medicine
int getMedicinePin(const char* medicineName) {
  if (strcmp(medicineName, "Paracetamol") == 0) {
    return PIN_PARACETAMOL;
  } else if (strcmp(medicineName, "Amoxicillin") == 0) {
    return PIN_AMOXICILLIN;
  } else if (strcmp(medicineName, "Cetirizine") == 0) {
    return PIN_CETIRIZINE;
  }
  return -1;  // Unknown medicine
}

// Get PWM channel for a medicine
int getPWMChannel(const char* medicineName) {
  if (strcmp(medicineName, "Paracetamol") == 0) {
    return PWM_CHANNEL_1;
  } else if (strcmp(medicineName, "Amoxicillin") == 0) {
    return PWM_CHANNEL_2;
  } else if (strcmp(medicineName, "Cetirizine") == 0) {
    return PWM_CHANNEL_3;
  }
  return -1;
}

// Soft Start: Gradually increase motor speed to prevent torque shock
void softStart(int channel) {
  Serial.println("    [PWM] Soft start (ramping up)...");
  int steps = rampUpTimeMs / RAMP_STEP_DELAY_MS;
  if (steps < 1) steps = 1;
  int increment = targetSpeed / steps;
  if (increment < 1) increment = 1;
  
  for (int speed = MIN_SPEED; speed <= targetSpeed; speed += increment) {
    ledcWrite(channel, speed);
    delay(RAMP_STEP_DELAY_MS);
  }
  ledcWrite(channel, targetSpeed);  // Ensure we reach target
}

// Soft Stop: Gradually decrease motor speed to prevent recoil
void softStop(int channel) {
  Serial.println("    [PWM] Soft stop (ramping down)...");
  int steps = rampDownTimeMs / RAMP_STEP_DELAY_MS;
  if (steps < 1) steps = 1;
  int decrement = targetSpeed / steps;
  if (decrement < 1) decrement = 1;
  
  for (int speed = targetSpeed; speed >= MIN_SPEED; speed -= decrement) {
    ledcWrite(channel, speed);
    delay(RAMP_STEP_DELAY_MS);
  }
  ledcWrite(channel, 0);  // Ensure complete stop
}

// Run motor at constant speed
void runMotor(int channel, int duration) {
  Serial.print("    [PWM] Running at ");
  Serial.print((targetSpeed * 100) / 255);
  Serial.print("% power for ");
  Serial.print(duration);
  Serial.println("ms");
  
  ledcWrite(channel, targetSpeed);
  delay(duration);
}

// Dispense tablets for a specific medicine with SMOOTH MOTOR CONTROL
int dispenseMedicine(const char* medicineName, int count) {
  int pin = getMedicinePin(medicineName);
  int channel = getPWMChannel(medicineName);
  
  if (pin == -1 || channel == -1) {
    Serial.print("[WARNING] Unknown medicine: ");
    Serial.println(medicineName);
    return 0;
  }
  
  Serial.println();
  Serial.print("[SLOT] Dispensing ");
  Serial.print(medicineName);
  Serial.print(" from GPIO ");
  Serial.print(pin);
  Serial.print(" (PWM Channel ");
  Serial.print(channel);
  Serial.println(")");
  Serial.println("[SLOT] Using SMOOTH motor control (anti-vibration)");
  
  int dispensed = 0;
  
  for (int i = 0; i < count; i++) {
    Serial.print("  [MOTOR] Tablet ");
    Serial.print(i + 1);
    Serial.print(" of ");
    Serial.println(count);
    
    // === SMOOTH MOTOR CONTROL SEQUENCE ===
    // Phase 1: Soft Start (gradual acceleration)
    softStart(channel);
    
    // Phase 2: Run at constant speed
    runMotor(channel, runTimeMs);
    
    // Phase 3: Soft Stop (gradual deceleration)
    softStop(channel);
    
    // Phase 4: Settling pause (let spring stabilize)
    Serial.println("    [PAUSE] Letting spring settle...");
    delay(pauseBetweenMs);
    
    dispensed++;
  }
  
  Serial.print("[SLOT] ");
  Serial.print(medicineName);
  Serial.println(" complete.");
  return dispensed;
}

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

void addCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void sendErrorResponse(int code, const char* message) {
  JsonDocument doc;
  doc["success"] = false;
  doc["error"] = message;
  
  String response;
  serializeJson(doc, response);
  
  server.send(code, "application/json", response);
}

void printReadyMessage() {
  Serial.println("==============================================");
  Serial.println("   DISPENSER READY FOR COMMANDS");
  Serial.println("==============================================");
  Serial.println();
  Serial.println("Configure your web app with this IP address:");
  Serial.print("   >>> ");
  Serial.print(WiFi.localIP());
  Serial.println(" <<<");
  Serial.println();
  Serial.println("Test endpoints:");
  Serial.print("   Health: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
  Serial.print("   Status: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/status");
  Serial.println();
  printCalibrationHelp();
}

void printCalibrationHelp() {
  Serial.println("==============================================");
  Serial.println("   CALIBRATION MODE - Serial Commands");
  Serial.println("==============================================");
  Serial.println();
  Serial.println("  QUICK TEST (Direct - No Selection Needed):");
  Serial.println("    a  - Test Paracetamol (Slot 1)");
  Serial.println("    b  - Test Amoxicillin (Slot 2)");
  Serial.println("    c  - Test Cetirizine (Slot 3)");
  Serial.println();
  Serial.println("  SLOT SELECTION (for adjustments):");
  Serial.println("    1  - Select Slot 1 for +/- adjustment");
  Serial.println("    2  - Select Slot 2 for +/- adjustment");
  Serial.println("    3  - Select Slot 3 for +/- adjustment");
  Serial.println("    t  - Test currently selected slot");
  Serial.println();
  Serial.println("  OTHER:");
  Serial.println("    m  - Manual motor pulse (100ms)");
  Serial.println("    s  - STOP all motors");
  Serial.println();
  Serial.println("  SPEED ADJUSTMENT:");
  Serial.println("    +  - Increase speed by 10");
  Serial.println("    -  - Decrease speed by 10");
  Serial.println("    f  - FAST (150)  n  - NORMAL (120)  c  - CRAWL (80)");
  Serial.println();
  Serial.println("  TIMING ADJUSTMENT:");
  Serial.println("    [ ] - Run time -/+ 50ms");
  Serial.println("    { } - Pause -/+ 50ms");
  Serial.println();
  Serial.println("  INFO:  p - Print settings   h - Help");
  Serial.println();
  Serial.print("Current slot: ");
  Serial.println(selectedSlot);
  Serial.println();
  Serial.println("Waiting for commands...");
  Serial.println();
}

void printCurrentSettings() {
  Serial.println();
  Serial.println("=== CURRENT MOTOR SETTINGS ===");
  Serial.print("  Selected Slot: ");
  Serial.print(selectedSlot);
  Serial.println(selectedSlot == 1 ? " (Paracetamol)" : (selectedSlot == 2 ? " (Amoxicillin)" : " (Cetirizine)"));
  Serial.print("  Target Speed: ");
  Serial.print(targetSpeed);
  Serial.print(" (");
  Serial.print((targetSpeed * 100) / 255);
  Serial.println("%)");
  Serial.print("  Ramp Up Time: ");
  Serial.print(rampUpTimeMs);
  Serial.println(" ms");
  Serial.print("  Run Time: ");
  Serial.print(runTimeMs);
  Serial.println(" ms");
  Serial.print("  Ramp Down Time: ");
  Serial.print(rampDownTimeMs);
  Serial.println(" ms");
  Serial.print("  Pause Between: ");
  Serial.print(pauseBetweenMs);
  Serial.println(" ms");
  Serial.print("  Total Cycle: ~");
  Serial.print(rampUpTimeMs + runTimeMs + rampDownTimeMs + pauseBetweenMs);
  Serial.println(" ms");
  Serial.println("==============================");
  Serial.println();
}

int getSelectedChannel() {
  switch (selectedSlot) {
    case 1: return PWM_CHANNEL_1;
    case 2: return PWM_CHANNEL_2;
    case 3: return PWM_CHANNEL_3;
    default: return PWM_CHANNEL_1;
  }
}

// Test a specific slot with timing report
void testSlot(int channel, const char* slotName) {
  unsigned long startTime = millis();
  Serial.print("    Testing ");
  Serial.print(slotName);
  Serial.println("...");
  
  softStart(channel);
  runMotor(channel, runTimeMs);
  softStop(channel);
  delay(pauseBetweenMs);
  
  Serial.print("[DONE] ");
  Serial.print(slotName);
  Serial.print(" cycle completed in ");
  Serial.print(millis() - startTime);
  Serial.println(" ms");
}

void handleSerialCommand() {
  char cmd = Serial.read();
  
  // Ignore newlines and carriage returns
  if (cmd == '\n' || cmd == '\r') return;
  
  Serial.print("[CMD] Received: '");
  Serial.print(cmd);
  Serial.println("'");
  
  int channel = getSelectedChannel();
  
  switch (cmd) {
    // === DIRECT SLOT TESTS (a/b/c) - Test specific slot without changing selection ===
    case 'a':
    case 'A':
      Serial.println("[TEST] Testing SLOT 1 (Paracetamol)...");
      testSlot(PWM_CHANNEL_1, "Paracetamol");
      break;
    case 'b':
    case 'B':
      Serial.println("[TEST] Testing SLOT 2 (Amoxicillin)...");
      testSlot(PWM_CHANNEL_2, "Amoxicillin");
      break;
    case 'c':
    case 'C':
      Serial.println("[TEST] Testing SLOT 3 (Cetirizine)...");
      testSlot(PWM_CHANNEL_3, "Cetirizine");
      break;
      
    // === SLOT SELECTION (for speed/timing adjustments) ===
    case '1':
      selectedSlot = 1;
      Serial.println("[SLOT] Selected: Paracetamol (GPIO 27) - Now use +/- to adjust speed");
      break;
    case '2':
      selectedSlot = 2;
      Serial.println("[SLOT] Selected: Amoxicillin (GPIO 12) - Now use +/- to adjust speed");
      break;
    case '3':
      selectedSlot = 3;
      Serial.println("[SLOT] Selected: Cetirizine (GPIO 14) - Now use +/- to adjust speed");
      break;
      
    // === TEST CURRENT SLOT ===
    case 't':
    case 'T':
      Serial.print("[TEST] Running dispense on CURRENT SLOT ");
      Serial.print(selectedSlot);
      Serial.println("...");
      testSlot(channel, selectedSlot == 1 ? "Paracetamol" : (selectedSlot == 2 ? "Amoxicillin" : "Cetirizine"));
      break;
      
    case 'm':
    case 'M':
      Serial.println("[MANUAL] Quick motor pulse (100ms)...");
      ledcWrite(channel, targetSpeed);
      delay(100);
      ledcWrite(channel, 0);
      Serial.println("[MANUAL] Done");
      break;
      
    case 's':
    case 'S':
      Serial.println("[STOP] Motor stopped!");
      ledcWrite(PWM_CHANNEL_1, 0);
      ledcWrite(PWM_CHANNEL_2, 0);
      ledcWrite(PWM_CHANNEL_3, 0);
      break;
      
    // === SPEED ADJUSTMENT ===
    case '+':
    case '=':
      targetSpeed = min(255, targetSpeed + 10);
      Serial.print("[SPEED] Increased to: ");
      Serial.print(targetSpeed);
      Serial.print(" (");
      Serial.print((targetSpeed * 100) / 255);
      Serial.println("%)");
      break;
      
    case '-':
    case '_':
      targetSpeed = max(30, targetSpeed - 10);
      Serial.print("[SPEED] Decreased to: ");
      Serial.print(targetSpeed);
      Serial.print(" (");
      Serial.print((targetSpeed * 100) / 255);
      Serial.println("%)");
      break;
      
    case 'f':
    case 'F':
      targetSpeed = FAST_SPEED;
      Serial.println("[SPEED] Set to FAST (150 / 60%)");
      break;
      
    case 'n':
    case 'N':
      targetSpeed = NORMAL_SPEED;
      Serial.println("[SPEED] Set to NORMAL (120 / 47%)");
      break;
      
    case 'c':
    case 'C':
      targetSpeed = CRAWL_SPEED;
      Serial.println("[SPEED] Set to CRAWL (80 / 30%)");
      break;
      
    // === TIMING ADJUSTMENT ===
    case '[':
      runTimeMs = max(100, runTimeMs - 50);
      Serial.print("[TIMING] Run time: ");
      Serial.print(runTimeMs);
      Serial.println(" ms");
      break;
      
    case ']':
      runTimeMs = min(2000, runTimeMs + 50);
      Serial.print("[TIMING] Run time: ");
      Serial.print(runTimeMs);
      Serial.println(" ms");
      break;
      
    case '{':
      pauseBetweenMs = max(100, pauseBetweenMs - 50);
      Serial.print("[TIMING] Pause: ");
      Serial.print(pauseBetweenMs);
      Serial.println(" ms");
      break;
      
    case '}':
      pauseBetweenMs = min(2000, pauseBetweenMs + 50);
      Serial.print("[TIMING] Pause: ");
      Serial.print(pauseBetweenMs);
      Serial.println(" ms");
      break;
      
    // === INFO ===
    case 'p':
    case 'P':
      printCurrentSettings();
      break;
      
    case 'h':
    case 'H':
    case '?':
      printCalibrationHelp();
      break;
      
    default:
      Serial.println("[CMD] Unknown command. Press 'h' for help.");
      break;
  }
}
