// // Optimal 4-space indent logging pattern
//  Void log_telemetry(const char* logMessage) {
//     File logFile = SD.open("/telemetry.csv", FILE_WRITE);
//     if (logFile) {
//         logFile.println(logMessage);
//         logFile.flush(); // Force write to physical sectors
//         logFile.close(); // Safely close handle
//     } else {
//         Serial.println("[ERROR] Failed to open telemetry log file.");
//     }
//  }
#include "webpage_routes.h"
#include "esp_wifi.h"

int global_counter = 0;
const char* counter_file_path = "/counter.dat";
const char* counter_file = "/counter.dat"; 
static File uploadFile;
uint8_t broadcastMacAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
// Automatically calculate identities right at boot based on PlatformIO environment flags
bool amIServerNode = (atoi(NODE_NUMBER) == 0);
int myRuntimeNodeId = atoi(NODE_NUMBER);
String myRuntimeHostname = (atoi(NODE_NUMBER) == 0) ? "HZ-SERVER" : "HZ-NODE-" + String(NODE_NUMBER);
NodeData systemMatrix[MAX_SYSTEM_NODES];
RegistrationEntry clientRegistry[MAX_SYSTEM_NODES];

void say_hello(void) {
  Serial.printf("Hello, World!\n");
}

void init_resilient_esp_now() {
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
      Serial.println("[ERROR] Failed to bind ESP-NOW architecture.");
      return;
  }

  esp_now_register_send_cb(on_data_sent);
  esp_now_register_recv_cb(on_data_recv);

  esp_wifi_set_ps(WIFI_PS_NONE);

  // =========================================================================
  // UNIFIED COMPILE-TIME PARSER: Hard-lock integer assignments via C standard
  // =========================================================================
  // atoi directly extracts the numbers from the unquoted platform literal tokens
  myRuntimeNodeId = atoi(NODE_NUMBER); 
  myRuntimeHostname = "HZ-NODE-" + String(myRuntimeNodeId);
  // =========================================================================

  // 2. UNIFIED IDENTITY ASSIGNMENT: Evaluate role based on clean integer indices
  if (myRuntimeNodeId == 0) {
      amIServerNode = true;
      myRuntimeHostname = "HZ-SERVER";
      
      // Open the server's firewall layer to listen to ALL sub-node unicast frames cleanly
      esp_now_peer_info_t peerInfo = {};
      memset(&peerInfo, 0, sizeof(peerInfo));
      memset(peerInfo.peer_addr, 0, 6); // Wildcard address opens listener pipelines
      peerInfo.channel = WiFi.channel();
      peerInfo.encrypt = false;

      if (!esp_now_is_peer_exist(peerInfo.peer_addr)) {
          esp_now_add_peer(&peerInfo);
      }
      Serial.println("[INIT] Codebase booted as MASTER SERVER. Open listener mapped.");
  } 
  else {
      amIServerNode = false;
      
      // Sub-nodes explicitly pair with Node 00's fixed broadcast target signature
      esp_now_peer_info_t peerInfo = {};
      memset(&peerInfo, 0, sizeof(peerInfo));
      memcpy(peerInfo.peer_addr, broadcastMacAddress, 6);
      peerInfo.channel = WiFi.channel();
      peerInfo.encrypt = false;

      if (!esp_now_is_peer_exist(peerInfo.peer_addr)) {
          esp_now_add_peer(&peerInfo);
      }
      Serial.printf("[INIT] Codebase booted as SUB-NODE %02d. Senders layer mapped.\n", myRuntimeNodeId);
  }

  WiFi.setHostname(myRuntimeHostname.c_str());
}

void update_system_matrix(uint8_t nodeId, float temp) {
  if (nodeId < MAX_SYSTEM_NODES) {
      systemMatrix[nodeId].temperature = temp;
      systemMatrix[nodeId].lastSeenMillis = millis();
      systemMatrix[nodeId].isOnline = true; // Hard-lock the cache line active
  }
}

void on_data_sent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
    if (status != ESP_NOW_SEND_SUCCESS) {
        Serial.println("[ESP-NOW] Broadcast telemetry failed to clear radio.");
    }
}

void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  TelemetryPacket packet;
  if (len == sizeof(packet)) {
      memcpy(&packet, incomingData, sizeof(packet));

      // -----------------------------------------------------------------
      // SERVER MODE: Manage Dynamic Registration & Data Matrix Mapping
      // -----------------------------------------------------------------
      if (amIServerNode) {
          // Process registrations FIRST based strictly on packet type, bypassing slot index checks
          if (packet.packetType == PACKET_REGISTRATION_REQ) {
              int assignedId = -1;

              // Step A: Check if this transmitter MAC address is already in our table
              for (int i = 1; i < MAX_SYSTEM_NODES; i++) {
                  if (clientRegistry[i].isActive && memcmp(clientRegistry[i].mac, recv_info->src_addr, 6) == 0) {
                      assignedId = i;
                      break;
                  }
              }

              // Step B: If it's a newly discovered chip, deal out the next available slot
              if (assignedId == -1) {
                  for (int i = 1; i < MAX_SYSTEM_NODES; i++) {
                      if (!clientRegistry[i].isActive) {
                          memcpy(clientRegistry[i].mac, recv_info->src_addr, 6);
                          clientRegistry[i].isActive = true;
                          assignedId = i;
                          break;
                      }
                  }
              }

              // Step C: Send the direct confirmation packet back out to the sender's MAC
              if (assignedId != -1) {
                  TelemetryPacket ackPacket;
                  ackPacket.packetType = PACKET_REGISTRATION_ACK;
                  ackPacket.dynamicNodeId = assignedId;
                  ackPacket.temperature = 0.0f;
                  memcpy(ackPacket.macAddr, recv_info->src_addr, 6);
                  ackPacket.timestamp = millis();

                  esp_now_send(recv_info->src_addr, (uint8_t *)&ackPacket, sizeof(ackPacket));
                  Serial.printf("[SERVER] Dynamic handshaking complete -> Assigned Slot %02d\n", assignedId);
              }
              return; // Safely exit early from the registration transaction frame
          }
          
          // Handle regular temperature transmission packets
          if (packet.packetType == PACKET_TELEMETRY && packet.dynamicNodeId < MAX_SYSTEM_NODES) {
              update_system_matrix(packet.dynamicNodeId, packet.temperature);
              Serial.printf("[SERVER] Intercepted payload from Node %02d -> Temp: %.4f C\n", 
                            packet.dynamicNodeId, packet.temperature);
          }
      }
      // -----------------------------------------------------------------
      // TRANSMITTER MODE: Process Assignment Confirmations
      // -----------------------------------------------------------------
      else {
          if (packet.packetType == PACKET_REGISTRATION_ACK) {
              uint8_t myMac[6];
              esp_read_mac(myMac, ESP_MAC_WIFI_STA);
              
              // Confirm the incoming server message was targeted to our exact hardware signature
              if (memcmp(packet.macAddr, myMac, 6) == 0) {
                  myRuntimeNodeId = packet.dynamicNodeId;
                  myRuntimeHostname = "HZ-NODE-0" + String(myRuntimeNodeId);
                  
                  WiFi.setHostname(myRuntimeHostname.c_str());
                  Serial.printf("[TX-SYNC] Handshake locked! Claiming dynamic profile: %s\n", 
                                myRuntimeHostname.c_str());
              }
          }
      }
  }
}

void broadcast_telemetry(float currentTemperature) {
    // Extract the raw compile-time integer value directly inside the function scope
    int compileTimeId = atoi(NODE_NUMBER);

    // Master Server (Node 00) never needs to transmit radio telemetry packets to itself
    if (compileTimeId == 0) {
        return; 
    }

    // THROTTLE LAYER: Restrict transmitter bursts to a clean 1-second interval
    static unsigned long lastBroadcastMillis = 0;
    if (millis() - lastBroadcastMillis < 1000) {
        return; 
    }
    lastBroadcastMillis = millis();

    TelemetryPacket packet;
    
    // Explicitly target your broadcast/unicast pipeline layout
    esp_now_peer_info_t peerInfo;
    if (esp_now_get_peer(broadcastMacAddress, &peerInfo) == ESP_OK) {
        if (peerInfo.channel != WiFi.channel()) {
            peerInfo.channel = WiFi.channel();
            esp_now_mod_peer(&peerInfo);
        }
    }

    // Pack the structure using the hardlocked compile-time ID token
    packet.packetType = PACKET_TELEMETRY;
    packet.dynamicNodeId = compileTimeId; // Hardlocked directly to your platformio.ini flag!
    packet.temperature = currentTemperature;
    packet.timestamp = millis();

    esp_err_t result = esp_now_send(broadcastMacAddress, (uint8_t *)&packet, sizeof(packet));
    
    if (result == ESP_OK) {
        Serial.printf("[TX] Node %02d sent Temp: %.4f C on Channel %d\n", 
                      compileTimeId, currentTemperature, WiFi.channel());
    } else {
        Serial.println("[ERROR] Failed to push telemetry packet to ESP-NOW radio queue.");
    }
}

void load_global_counter() {
  if (!LittleFS.exists(counter_file_path)) {
    Serial.println("Counter file not found. Starting from 0.");
    global_counter = 0;
    return;
  }

  File file = LittleFS.open(counter_file_path, FILE_READ);
  if (!file) {
    Serial.println("Failed to open counter file for LittleFS!");
    return;
  }

  String content = "";
  while (file.available()) {
    content += (char)file.read();
  }
  file.close();

  global_counter = content.toInt();
  Serial.printf("Counter successfully restored from LittleFS: %d\n", global_counter);
}

void save_global_counter() {
  File file = LittleFS.open(counter_file_path, FILE_WRITE);
  if (!file) {
    Serial.println("Error: Failed to open counter file for writing!");
    return;
  }

  if (file.print(global_counter)) {
    Serial.printf("Counter successfully saved to LittleFS: %d\n", global_counter);
  } else {
    Serial.println("Write failed!");
  }

  file.close();
}

void init_webpage_routes(AsyncWebServer &server, fs::FS &sd_instance, fs::FS &fs_instance) {
  // =========================================================================
  // CHUNK 2: Live Multi-Zone Matrix JSON API Endpoint
  // =========================================================================
  // server.on("/api/system_temp", HTTP_GET, [](AsyncWebServerRequest *request) {
  //     String jsonOutput = "[\n";
  //     unsigned long currentMillis = millis();

  //     for (int i = 0; i < MAX_SYSTEM_NODES; i++) {
  //         // Flag a background transmitter offline if it misses its 10-second check-in
  //         if (i != myRuntimeNodeId && systemMatrix[i].isOnline && (currentMillis - systemMatrix[i].lastSeenMillis > 10000)) {
  //             systemMatrix[i].isOnline = false;
  //         }

  //         jsonOutput += "    {\n";
  //         jsonOutput += "        \"node_id\": " + String(i) + ",\n";
  //         jsonOutput += "        \"temperature\": " + String(systemMatrix[i].temperature, 4) + ",\n";
  //         jsonOutput += "        \"online\": " + String(systemMatrix[i].isOnline ? "true" : "false") + "\n";
  //         jsonOutput += "    }";
          
  //         if (i < MAX_SYSTEM_NODES - 1) {
  //             jsonOutput += ",\n";
  //         } else {
  //             jsonOutput += "\n";
  //         }
  //     }
  //     jsonOutput += "]";
      
  //     request->send(200, "application/json", jsonOutput);
  // });

  // =========================================================================
  // Live Multi-Zone Matrix JSON API Endpoint (Forced Local Server Route)
  // =========================================================================
  server.on("/api/system_temp", HTTP_GET, [](AsyncWebServerRequest *request) {
      String jsonOutput = "[\n";
      unsigned long currentMillis = millis();

      for (int i = 0; i < MAX_SYSTEM_NODES; i++) {
          // Force the active server node (Slot 0) to ALWAYS stay flagged online
          if (i == 0 && amIServerNode) {
              systemMatrix[i].isOnline = true;
          } 
          // External sub-nodes: Flag offline if they haven't checked in for 10 seconds
          else if (i != myRuntimeNodeId && systemMatrix[i].isOnline && (currentMillis - systemMatrix[i].lastSeenMillis > 10000)) {
              systemMatrix[i].isOnline = false;
          }

          jsonOutput += "    {\n";
          jsonOutput += "        \"node_id\": " + String(i) + ",\n";
          jsonOutput += "        \"temperature\": " + String(systemMatrix[i].temperature, 4) + ",\n";
          jsonOutput += "        \"online\": " + String(systemMatrix[i].isOnline ? "true" : "false") + "\n";
          jsonOutput += "    }";
          
          if (i < MAX_SYSTEM_NODES - 1) {
              jsonOutput += ",\n";
          } else {
              jsonOutput += "\n";
          }
      }
      jsonOutput += "]";
      
      request->send(200, "application/json", jsonOutput);
  });

  // Local structure definition to encapsulate request tracking state cleanly
  struct UploadState {
    File file;
    bool aborted = false;
  };

  // =========================================================================
  // 1. HOME ROUTE
  // =========================================================================
  server.on("/", HTTP_GET, [&fs_instance](AsyncWebServerRequest *request) {
    const char* path = "/index.html";
    if (!fs_instance.exists(path)) {
      request->send(404, "text/plain", "File Not Found inside LittleFS");
      return;
    }
    request->send(fs_instance, path, "text/html", false, [](const String& var) -> String {
      if (var == "BOARD_HOSTNAME") return String(WiFi.getHostname());
      return String();
    });
  });

  // =========================================================================
  // 2. CHECK ROUTE
  // =========================================================================
  server.on("/check-file", HTTP_GET, [&sd_instance](AsyncWebServerRequest *request) {
    if (!request->hasParam("name")) {
      request->send(400, "text/plain", "Missing name parameter");
      return;
    }
    String filename = request->getParam("name")->value();
    if (!filename.startsWith("/")) filename = "/" + filename;

    if (sd_instance.exists(filename)) {
      request->send(200, "text/plain", "true");
    } else {
      request->send(200, "text/plain", "false");
    }
  });

  // Inside your file serving route in webpage_routes.cpp:
  server.on("/download", HTTP_GET, [&sd_instance](AsyncWebServerRequest *request) {
    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Missing file parameter");
      return;
    }

    String filename = request->getParam("file")->value();
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }

    if (!sd_instance.exists(filename)) {
      request->send(404, "text/plain", "File Not Found");
      return;
    }

    // PASSING 'true' AS THE LAST ARGUMENT FORCES THE DOWNLOAD PARAMETER!
    // Syntax: send(File system, Path, Content Type, Download parameter [true = force download filename])
    request->send(sd_instance, filename, "text/plain", true);
  });

  // =========================================================================
  // 3. UPLOAD ROUTE (Callbacks defined cleanly as local auto variables)
  // =========================================================================

 // A. Define what happens when the total upload finishes
  auto on_upload_complete = [](AsyncWebServerRequest *request) {
    UploadState *state = (UploadState *)request->_tempObject;
    if (state && state->aborted) {
      request->send(507, "text/plain", "Error: Insufficient Storage on SD Card");
    } else {
      request->send(200, "text/plain", "Upload Complete");
    }
    if (state) {
      delete state;
      request->_tempObject = nullptr;
    }
  };

  // B. Define how raw byte chunks are actively written to hardware
  auto on_incoming_chunk = [&sd_instance](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    UploadState *state = (UploadState *)request->_tempObject;
    if (!state) {
      state = new UploadState();
      request->_tempObject = state;
    }

    if (!index) {
      size_t total_file_size = request->contentLength();
      size_t free_space_on_sd = SD.totalBytes() - SD.usedBytes();

      if (total_file_size > free_space_on_sd) {
        Serial.printf("[WARN] Upload aborted. File (%u B) exceeds Free Space (%u B)\n", total_file_size, free_space_on_sd);
        state->aborted = true;
        return;
      }

      if (!filename.startsWith("/")) filename = "/" + filename;
      Serial.printf("[Web Server] Starting Upload: %s\n", filename.c_str());

      state->file = sd_instance.open(filename, FILE_WRITE);
      if (!state->file) {
        Serial.println("[ERROR] Failed to open target file on SD Card for writing.");
        state->aborted = true;
      }
    }

    if (!state->aborted && state->file && len) {
      state->file.write(data, len);
    }

    if (final && state->file) {
      state->file.close();
      if (!state->aborted) {
        Serial.printf("[Web Server] File Upload complete. Bytes written: %u\n", index + len);
      }
    }
  };

  // C. Register routes with zero visual inline nesting
  server.on("/upload", HTTP_POST, on_upload_complete, on_incoming_chunk);
}

void handle_sd_files(AsyncWebServerRequest *request) {
  File root = SD.open("/");
  if (!root) {
    request->send(500, "text/plain", "Failed to open SD card root directory.");
    return;
  }

  // 1. Change content type to text/html to let the browser parse links
  AsyncResponseStream *response = request->beginResponseStream("text/html");
  // Add a basic monospaced body wrapper for clean alignment
  response->print("<html><body style='font-family: monospace; line-height: 1.5;'>");
  response->print("<h2>SD Card files in /</h2>");

  File entry = root.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      const char* name = entry.name();
      size_t bytes = entry.size();

      // Handle optional leading slash formatting depending on core versions
      String filename_str = String(name);
      String url_path = filename_str.startsWith("/") ? filename_str : "/" + filename_str;
      String display_name = filename_str.startsWith("/") ? filename_str.substring(1) : filename_str;

      // Format file size into a human-readable string (KB or MB)
      String size_str;
      if (bytes >= 1024 * 1024) {
        size_str = String((float)bytes / (1024.0 * 1024.0), 2) + " MB";
      } else {
        size_str = String((float)bytes / 1024.0, 1) + " KB";
      }

      // 2. Format as a clickable link pointing to a viewing route with size appended
      response->printf("<a href='/view?file=%s' target='_blank'>%s</a> (%s)<br>\n",
        url_path.c_str(),
        display_name.c_str(),
        size_str.c_str()
      );
    }
    entry.close();
    entry = root.openNextFile();
  }
  root.close();
  response->print("</body></html>");

  // Ship remaining data packets to the browser
  request->send(response);
}

void mcu_dir(fs::FS &fs, const char * dir_name, uint8_t levels) {
  Serial.printf("Files in: %s\n", dir_name);

  File root = fs.open(dir_name, "r");
  if (!root) {
    Serial.printf("Failed to open directory\n");
    return;
  }
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) {
      Serial.printf(" * %s (%d blocks)\n", file.name(), file.size());
    }
    file = root.openNextFile();
  }
  root.close(); // Clean up the root file system handle
}

void webpage_serve_html(AsyncWebServerRequest *request, fs::LittleFSFS &local_filesystem) {
    const char* path = "/index.html";

    if (!local_filesystem.exists(path)) {
        request->send(404, "text/plain", "Index HTML Missing from Flash");
        return;
    }

    File file = local_filesystem.open(path, "r");
    if (!file) {
        request->send(500, "text/plain", "Failed to open HTML template.");
        return;
    }

    String htmlContent = file.readString();
    file.close();

    htmlContent.replace("%BOARD_HOSTNAME%", myRuntimeHostname.c_str());
    request->send(200, "text/html", htmlContent);
}

void webpage_led(AsyncWebServer &server, const int led_pin) {
  pinMode(led_pin, OUTPUT);

  server.on("/led", HTTP_GET, [led_pin](AsyncWebServerRequest *request) {
    if (request -> hasArg("state")) {
      String state = request ->arg("state");
      if (state == "on") {
        digitalWrite(led_pin, HIGH);
        request -> send(200, "text/plain", "LED ON");
      } else if (state == "off") {
        digitalWrite(led_pin, LOW);
        request -> send(200, "text/plain", "LED OFF");
      } else {
        request -> send(400, "text/plain", "Invalid State Value");
      }
    } else {
      request -> send(400, "text/plain", "Missing State Parameter");
    }
  });
}

void webpage_file_list(AsyncWebServerRequest *request, fs::FS &fs) {
    File root = fs.open("/", "r");
    if (!root || !root.isDirectory()) {
        request->send(500, "text/plain", "Failed to open directory");
        return;
    }

    String json = "[";
    File file = root.openNextFile();
    while (file) {
      // Serial.printf("File: %s %d\n", file.name(), file.isDirectory());
      if (!file.isDirectory()) {
        String file_name = String(file.name());
        // Check if the file matches any excluded extensions
        if (!file_name.endsWith(".html") && !file_name.endsWith(".foo")) {
            if (json != "[") {
                json += ",";
            }
            json += "\"" + file_name + "\"";
        }
      }
      file = root.openNextFile();
    }
    json += "]";

    request->send(200, "application/json", json);
}

String add_commas_to_string(long value) {
  String original = String(value);
  String formatted = "";

  // Handle negative numbers smoothly
  bool is_negative = value < 0;
  if (is_negative) {
    original.remove(0, 1); // Temporarily strip the minus sign
  }

  int len = original.length();

  // Loop through characters backwards to place commas every 3 digits
  for (int i = 0; i < len; i++) {
    if (i > 0 && i % 3 == 0) {
      formatted = "," + formatted;
    }
    formatted = original[len - 1 - i] + formatted;
  }

  // Re-attach the minus sign if the number was negative
  if (is_negative) {
    formatted = "-" + formatted;
  }

  return formatted;
}

bool init_sd(int chip_select_pin) {
  Serial.println("Attempting to initialize SD card...");
  if (SD.begin(chip_select_pin)) {
    Serial.println("✅ SD Card Successfully Initialized / Re-inserted!");
    return true;

    // Optional: Print card type or size here to verify it works
    uint8_t cardType = SD.cardType();
    if(cardType == CARD_NONE){
        Serial.println("No SD card type recognized");
    }
  } else {
    Serial.println("❌ No SD card found.");
    return false;
  }
}

String get_unique_id() {
  uint8_t mac[6];
  char macStr[13]; // 12 hex characters + null terminator

  // Fetch the factory-programmed base MAC address
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) == ESP_OK) {
    snprintf(macStr, sizeof(macStr), "%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(macStr);
  }

  return "UNKNOWN_ESP32";
}

String format_with_commas(long value) {
  String original = String(value);
  String formatted = "";

  // Handle negative numbers smoothly
  bool is_negative = value < 0;
  if (is_negative) {
    original.remove(0, 1); // Temporarily strip the minus sign
  }

  int len = original.length();

  // Loop through characters backwards to place commas every 3 digits
  for (int i = 0; i < len; i++) {
    if (i > 0 && i % 3 == 0) {
      formatted = "," + formatted;
    }
    formatted = original[len - 1 - i] + formatted;
  }

  // Re-attach the minus sign if the number was negative
  if (is_negative) {
    formatted = "-" + formatted;
  }

  return formatted;
}
