#include <Wire.h>
#include <VL53L0X.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// ================= USER CONFIGURATION =================
const char* ssid = "TP-Link_E41B";             
const char* password = "03876794"; 

// QLab Computer IP
const char* udpAddress = "0.0.0.0";     
const int udpPort = 53535; // Port to Send TO and Listen ON
// ======================================================

VL53L0X sensor;
WiFiUDP udp;
char packetBuffer[255]; // Buffer to hold incoming packet

// XIAO ESP32C6 I2C Pins
#define SDA_PIN D4
#define SCL_PIN D5

// Variable to lock the script after firing
bool taskCompleted = false;

void setup() {
  Serial.begin(115200);
  delay(2000); 

  // --- WiFi Setup ---
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("My IP: ");
  Serial.println(WiFi.localIP());

  // --- Start UDP Listening ---
  udp.begin(udpPort);
  Serial.printf("Listening on UDP port %d\n", udpPort);

  // --- Sensor Setup ---
  Wire.begin(SDA_PIN, SCL_PIN);
  sensor.setTimeout(500);

  if (!sensor.init()) {
    Serial.println("Failed to detect sensor! Check wiring.");
    while (1) {}
  }

  sensor.startContinuous();
  Serial.println("System Ready. Waiting for trigger...");
}

void loop() {
  // 1. ALWAYS CHECK FOR INCOMING RESET COMMANDS
  checkResetCommand();

  // 2. SENSOR LOGIC
  if (!taskCompleted) {
    
    uint16_t distance = sensor.readRangeContinuousMillimeters();

    // Print distance for debugging
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" mm");

    // 3. TRIGGER LOGIC
    // CRITICAL FIX: Only trigger if distance is > 350 AND valid (< 8000)
    // 8190 or 8191 are error codes; we must ignore them.
    if (distance > 350 && distance < 8000) {
      
      // Send the message
      sendUdpMessage("/cue/SAFE/go");
      
      // Lock the system
      taskCompleted = true;
      
      Serial.println("-----------------------------------");
      Serial.println("TRIGGER SENT. SYSTEM LOCKED.");
      Serial.println("Waiting for 'RESETSAFE' UDP command...");
      Serial.println("-----------------------------------");
      
      sensor.stopContinuous(); 
    }
  }
  
  delay(50); 
}

void sendUdpMessage(const char* msg) {
  Serial.print("Sending UDP: ");
  Serial.println(msg);
  
  udp.beginPacket(udpAddress, udpPort);
  udp.print(msg);
  udp.endPacket();
}
void checkResetCommand() {
  int packetSize = udp.parsePacket();
  
  if (packetSize) {
    int len = udp.read(packetBuffer, 255);
    if (len > 0) packetBuffer[len] = 0;

    Serial.print("Received UDP: ");
    Serial.println(packetBuffer);

    if (strstr(packetBuffer, "RESETSAFE") != NULL) {
      Serial.println("!!! RESET COMMAND RECEIVED !!!");
      
      // 1. Start the sensor
      sensor.startContinuous();
      
      // 2. STABILIZATION DELAY
      // We block here for 2 seconds to let the sensor warm up and settle.
      // During this time, the loop logic cannot run, so no false triggers can happen.
      Serial.println("Stabilizing sensor (2 seconds)...");
      delay(5000); 

      // 3. Re-Arm the system
      taskCompleted = false;
      Serial.println("System Re-Armed and Active.");
    }
  }
}