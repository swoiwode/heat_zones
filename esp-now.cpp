#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_MCP9808.h>
#include <Adafruit_Sensor.h>
#include <SPI.h>
#include <SD.h>
#include <time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "webpage_routes.h"

// Include ESP-NOW library
#include <esp_now.h>

// Define MAC addresses of the nodes
uint8_t node1_addr[] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX};
uint8_t node2_addr[] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX};
uint8_t node3_addr[] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX};
uint8_t node4_addr[] = {0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX};

// Callback function for when data is sent
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("Sent successfully");
  } else {
    Serial.println("Send failed");
  }
}

// Function to handle incoming data
void onDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
  if (len == sizeof(uint8_t)) {
    uint8_t *incomingDataPointer = (uint8_t *)incomingData;
    if (*incomingDataPointer == 0x01) {
      // Heartbeat received from Node 1
      Serial.println("Heartbeat received from Node 1");
    }
  }
}

// Web server setup
AsyncWebServer server(80);

// MCP9808 sensor initialization
Adafruit_MCP9808 mcp = Adafruit_MCP9808();

void setup() {
  Serial.begin(115200);

  // Initialize WiFi
  WiFi.mode(WIFI_STA);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register send callback function
  esp_now_register_send_cb(onDataSent);

  // Register receive callback function
  esp_now_register_recv_cb(onDataRecv);

  // Add peer nodes
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, node1_addr, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  memcpy(peerInfo.peer_addr, node2_addr, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  memcpy(peerInfo.peer_addr, node3_addr, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  memcpy(peerInfo.peer_addr, node4_addr, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // Set up web server routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/index.html", "text/html");
  });

  // Start web server
  server.begin();

  // Initialize other components (e.g., MCP9808 sensor)
  if (!mcp.begin(0x18)) {
    Serial.println("Could not find MCP9808 chip");
    while (1) delay(1000);
  }

  // Initialize SD card
  if (!SD.begin(5)) {
    Serial.println("SD card initialization failed!");
    return;
  }

  // Initialize OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Hello, World!");
  display.display();

  // Initialize other components (e.g., MCP9808 sensor)
  // ...
}

void loop() {
  // Send heartbeat to Node 1
  uint8_t heartbeat[1] = {0x01};
  esp_err_t result = esp_now_send(node1_addr, heartbeat, sizeof(heartbeat));
  if (result == ESP_OK) {
    Serial.println("Heartbeat sent successfully");
  } else {
    Serial.println("Error sending heartbeat");
  }

  // Check if Node 1's heartbeat expires
  // If Node 1's heartbeat expires, Node 2 takes over
  // This is a simplified example, actual implementation may vary
  static unsigned long lastHeartbeatTime = 0;
  unsigned long currentTime = millis();
  if (currentTime - lastHeartbeatTime > 5000) {
    Serial.println("Node 1 heartbeat expired, Node 2 takes over");
    // Configure Wi-Fi Station profile, boot AsyncWebServer, etc.
  }

  delay(1000);
}

// Add your existing code here, ensuring it's properly integrated