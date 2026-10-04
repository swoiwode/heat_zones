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

// =========================================================================
// COMPILE-TIME WIFI CREDENTIAL INJECTION
// Values are securely extracted straight from your platformio.ini variables
// =========================================================================
const char* wifi_ssid     = WIFI_SSID;
const char* wifi_password = WIFI_SECRET_KEY;
// =========================================================================


AsyncWebServer server(80);

// 1. Declare the Event Source stream endpoint globally
AsyncEventSource events("/events");
unsigned long last_time = 0;

char output_buffer[128] = "Hello, World!"; // Buffer for incoming data
// Global variables for thread communication
volatile bool new_value_available = false;
String shared_input_message = "";
int colon_pos = 0;
String input_cmd = "";
String input_data = "";
const char* local_ntp = "10.0.0.1";   // Local gateway/NTP server IP
struct tm timeinfo;
char timestamp[64];
const int output_pin = 23;

unsigned long last_update_time = 0;
int seconds_since_last_save = 0;
const int save_interval_seconds = 60; // Set to 10 or 60 depending on preference
unsigned long lastTxTime = 0;
const unsigned long txInterval = 2000; // Broadcast telemetry every 2000ms (2 seconds)

// Pull the statistical storage matrix allocated in your routes source file
extern NodeStats statisticalMatrix[MAX_SYSTEM_NODES];
// ----------------------------------------------------------

// Explicitly define the standard DevKit I2C pins
#define I2C_SDA 4
#define I2C_SCL 5
#define SD_CS 16
#define SD_SCK 6
#define SD_MOSI 7
#define SD_MISO 2

// Declaration for an SSD1306 display connected to I2C (SDA, SCL)
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // See datasheet for Address; 0x3D for some, 0x3C for most
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
Adafruit_SSD1306 ssd1306(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Adafruit_MCP9808 mcp9808 = Adafruit_MCP9808();

void setup() {
    Serial.begin(115200);
    
    // FIXED: Non-blocking boot guard allows standalone operation without a terminal open
    unsigned long serialTimeout = millis();
    while (!Serial) {
        if (millis() - serialTimeout > 2000) {
            break; // Force-exit the lock after 2 seconds if no computer is listening
        }
        delay(10); 
    }
    
    // Needs some delay to enable initial Serial.printf, 1000 is not enough
    delay(2000);

    // pinMode(RGB_BUILTIN, OUTPUT);
    rgbLedWrite(RGB_BUILTIN, 8, 4, 0); // Amber
    Wire.begin(I2C_SDA, I2C_SCL);
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    pinMode(output_pin, OUTPUT);

    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if (!ssd1306.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        rgbLedWrite(RGB_BUILTIN, 8, 0, 0); // red
        for (;;); // Don't proceed, loop forever
    }
    Serial.println(F(" *** SSD1306 display successfully initialized!"));
    ssd1306.clearDisplay();
    ssd1306.display();
    ssd1306.setTextSize(3);
    ssd1306.setTextColor(SSD1306_WHITE);
    ssd1306.setCursor(0, 16);
    ssd1306.println(F("MCP9808"));
    ssd1306.display();

    // The default I2C address for MCP9808 is 0x18
    if (!mcp9808.begin(0x18)) {
      Serial.println("Error: Could not find MCP9808 sensor. Check your wiring!");
      rgbLedWrite(RGB_BUILTIN, 8, 0, 0); // red
      for (;;); // Don't proceed, loop forever
    }
    // Set the resolution mode (Optional)
    // 0 - 0.5°C, ~30 ms. Fastest reading; lowest resolution. Great for rapid tracking where precision matters less.
    // 1 - 0.25°C, ~65 ms. Good middle ground for responsiveness.
    // 2 - 0.125°C, ~130 ms. High resolution.
    // 3 - 0.0625°C, ~250 ms. Default mode. Maximum possible resolution and precision; slowest conversion time.
    // mcp9808.setResolution(3); 
  
    // Wake up the sensor (required if it was previously shut down)
    mcp9808.wake(); 
    Serial.println(F(" *** MCP9808 Sensor successfully initialized!"));

    init_sd(SD_CS);
    Serial.printf(" *** SD Card Available Space: %.2f GB\n",
                  ((double)(SD.totalBytes() - SD.usedBytes()) / 1e9));

    if (!LittleFS.begin()) {
        rgbLedWrite(RGB_BUILTIN, 8, 0, 0); // Red
        Serial.println(F("An error occurred while mounting LittleFS\n"));
        for (;;); // Don't proceed, loop forever
    }
    Serial.println(F("LittleFS mounted successfully."));
    load_global_counter();

    mcu_dir(LittleFS, "/", 3);

    // Connect to Wi-Fi
    Serial.printf("Connecting to %s ", wifi_ssid);
    // WiFi.begin(wifi_ssid, wifi_password);
    // while (WiFi.status() != WL_CONNECTED) {
    //     delay(500);
    //     Serial.print(F("."));
    // }
    // Serial.printf("\nWi-Fi connected, ip: %s\n", WiFi.localIP().toString().c_str());

    // --------------------------------------------------------------------------
    // 1. Prepare the radio architecture for concurrent Wi-Fi + ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(); // Clear any stale connections from previous reboots

    // 2. Initialize ESP-NOW while the radio interface is idle
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] Failed to bind ESP-NOW architecture.");
        return;
    }
    
    // Register the data tracking callbacks
    esp_now_register_send_cb(on_data_sent);
    esp_now_register_recv_cb(on_data_recv);

    // Set channel to 0 so ESP-NOW automatically follows your router's channel later
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastMacAddress, 6);
    peerInfo.channel = WiFi.channel(); 
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("[ERROR] Failed to map universal broadcast peer pairing.");
        return;
    }
    Serial.println("[SUCCESS] ESP-NOW Layer bound to radio framework.");

    // 3. Fire up the physical connection to the network matrix
    WiFi.begin(wifi_ssid, wifi_password);
    
    Serial.print("Synchronizing with network matrix");
    unsigned long startAttempt = millis();
    
    // Wait for the router to assign a valid DHCP lease
    while ((WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) && (millis() - startAttempt < 8000)) {
        delay(500);
        Serial.print(".");
    }

    // 4. Verify routing status and assign browser tab dynamic configurations
    if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0)) {
        // =========================================================================
        // REMOVED INLINE ROLE CONFLICT FILTER
        // Identity strings are now handled safely at compile time by platformio.ini
        // =========================================================================
        
        // Push the hostname attributes directly into the active network layer
        WiFi.setHostname(myRuntimeHostname.c_str());

        // Initialize the mDNS responder for browser URL discovery
        if (!MDNS.begin(myRuntimeHostname.c_str())) {
            rgbLedWrite(RGB_BUILTIN, 8, 0, 0); // Red
            Serial.println(F("Error setting up mDNS!"));
            while (1) {
                delay(1000);
            }
        }

        Serial.printf("\n[SYNC] Node Registered: %s (ID: %d)\n", myRuntimeHostname.c_str(), myRuntimeNodeId);
        Serial.printf("mDNS Active: http://%s.local\n", myRuntimeHostname.c_str());
        Serial.print("Network Route: http://");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n[ERROR] Core network matrix timeout. Check SSID credentials.");
    }

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        webpage_serve_html(request, LittleFS);
    });

    webpage_led(server, LED_BUILTIN);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        request->send(LittleFS, "/index.html", "text/html");
    });

    server.on("/sd_files", HTTP_GET, handle_sd_files);

    server.on("/files", HTTP_GET, [](AsyncWebServerRequest* request) {
        webpage_file_list(request, LittleFS);
    });

    server.on("/view", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (request->hasParam("file")) {
            String file_path = request->getParam("file")->value();

            if (SD.exists(file_path)) {
                // 1. Prepare the response stream directly from the SD card.
                // Leaving the 3rd parameter empty lets the server auto-detect text/images/code.
                AsyncWebServerResponse* response = request->beginResponse(SD, file_path, String());

                // 2. Extract just the raw file name (removing any leading slash)
                String clean_name = file_path;
                if (clean_name.startsWith("/")) {
                    clean_name = clean_name.substring(1);
                }

                // 3. THE FIX: Force the browser to read the real filename on right-click save
                // "inline" means it still displays perfectly inside the new browser window
                response->addHeader("Content-Disposition", "inline; filename=\"" + clean_name + "\"");

                // 4. Send the configured response payload out
                request->send(response);
                return;
            } else {
                request->send(404, "text/plain", "File Not Found on SD Card");
                return;
            }
        }
        request->send(400, "text/plain", "Bad Request: Missing 'file' parameter");
    });

    init_webpage_routes(server, SD, LittleFS);

    server.serveStatic("/", LittleFS, "/");

    server.on("/update", HTTP_GET, [](AsyncWebServerRequest* request) {
        String response_message = "No content data received";

        if (request->hasParam("value")) {
            // 1. Save data to global variable
            shared_input_message = request->getParam("value")->value();
            // 2. Set flag to true
            new_value_available = true;

            response_message = "ESP32 received: " + shared_input_message;
        }
        request->send(200, "text/plain", response_message);
    });

    server.addHandler(&events);

    server.begin();
    Serial.println(F("HTTP Web Server running."));

    Serial.printf("Unique Device ID: 0x%s\n", get_unique_id().c_str());

    // Point directly to your local gateway IP address
    configTime(0, 0, local_ntp);
    // This string explicitly defines "PST" for standard and "PDT" for daylight savings
    setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
    tzset();

    if (getLocalTime(&timeinfo)) {
        snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        snprintf(timestamp, sizeof(timestamp), "[UNSYNCED]");
    }
    Serial.printf("ESP32 Web Server started on, %s.\n", timestamp);

    /*
    // 1. Create a text buffer string to hold the output
    char formattedTime[64];

    // 2. Use strftime to convert %Z and %z into readable text inside the buffer
    // %Z = Abbreviation (e.g. EST) | %z = Numeric offset (e.g. -0500)
    strftime(formattedTime, sizeof(formattedTime), "%Z", &timeinfo);

    // 3. Now use standard printf to display the string buffer using %s
    Serial.printf("%s\n", formattedTime);
    */
    rgbLedWrite(RGB_BUILTIN, 0, 8, 0); // Green
    say_hello();
}

void loop() {
    analogWrite(output_pin, 50);

    if (new_value_available) {
        new_value_available = false; // Reset the flag
        shared_input_message.trim(); // Remove any leading/trailing whitespace

        // Serial.printf("Input command: %s\n", shared_input_message.c_str());
        colon_pos = shared_input_message.indexOf(':');
        input_cmd = shared_input_message.substring(0, colon_pos);
        input_cmd.trim();
        input_cmd.toUpperCase();
        Serial.printf("command: %s\n", input_cmd.c_str());
        input_data = shared_input_message.substring(colon_pos + 1);
        input_data.trim();

        Serial.printf("data: %s\n", input_data.c_str());

        if (input_cmd == "COUNTER_START") {
            global_counter = input_data.toInt();
            Serial.printf("Updated global_counter and added commas: %s\n",
                          add_commas_to_string(global_counter).c_str());
        } else if (input_cmd == "LED") {
            input_data.toUpperCase();
            if (input_data == "ON") {
                // do action
                Serial.println(F("Turning LED ON"));
                rgbLedWrite(RGB_BUILTIN, 33, 32, 32); // White

            } else if (input_data == "OFF") {
                // do action
                Serial.println(F("Turning LED OFF"));
                rgbLedWrite(RGB_BUILTIN, 0, 8, 0); // Green
            }
        } else if (input_cmd == "SD_DELETE") {
            String del_file = "/" + input_data;
            Serial.printf("Deleting SD Card file: %s\n", del_file.c_str());
            SD.remove(del_file);
            Serial.printf("SD Deleted %s\n", del_file.c_str());
        } else if (input_cmd == "SD_MOUNT") {
            init_sd(SD_CS);
            Serial.println(F("SD Card mounted"));
            Serial.printf(" *** SD Card Available Space: %.2f GB\n",
                          ((double)(SD.totalBytes() - SD.usedBytes()) / 1e9));
        } else if (input_cmd == "SD_REMOVE") {
            SD.end(); // Unmount the SD card
            Serial.println(F("SD Card can be removed"));
            Serial.printf(" *** SD Card Available Space: %.2f GB\n",
                          ((double)(SD.totalBytes() - SD.usedBytes()) / 1e9));
        } else if (input_cmd == "CLEAR") {
            Serial.println(F("CLEAR"));
            events.send("[CLEAR_LOG_TRIGGER]", "log_update", millis());
        } else if (input_cmd == "COUNTER_SAVE") {
            Serial.println(F("COUNTER_SAVE"));
            save_global_counter(); // Instantly commits data to LittleFS
            events.send("System state saved to internal flash memory.", "output_update", millis());
        } else {
            Serial.printf("Unknown command: %s\n", input_cmd.c_str());
        }
    }

    // =====================================================================
    // SUB-NODE TELEMETRY LOOP: Transmits live data every 2 seconds
    // =====================================================================
    static unsigned long lastTxTime = 0; 
    if (!amIServerNode && (millis() - lastTxTime >= 2000)) {
        lastTxTime = millis();
        // Automatically fetches and broadcasts your active mcp9808 reading
        broadcast_telemetry(mcp9808.readTempC());
    }

    // Send a non-blocking background update to all clients every 1 second
    if ((millis() - last_time) > 1000) {
        last_time = millis();

        // FIXED: Force the Master Server (Node 00) to populate its own matrix cache line
        if (amIServerNode) {
            float localTemp = mcp9808.readTempC();
            update_system_matrix(0, localTemp);

            // --- ACCUMULATE SERVER LOCAL METRICS INTO HOURLY STATS ---
            NodeStats &serverStats = statisticalMatrix[0];
            serverStats.sampleCount++;

            // --- FIXED: LATCH LOCAL SERVER EXTREME THERMAL BOUNDS ---
            if (localTemp < serverStats.minTemp) serverStats.minTemp = localTemp;
            if (localTemp > serverStats.maxTemp) serverStats.maxTemp = localTemp;
            // ---------------------------------------------------------

            float delta = localTemp - serverStats.rollingMean;
            serverStats.rollingMean += delta / serverStats.sampleCount;
            float delta2 = localTemp - serverStats.rollingMean;
            serverStats.accumulatedM2 += delta * delta2;
            // ---------------------------------------------------------
        }

        if (getLocalTime(&timeinfo)) {
            snprintf(timestamp, sizeof(timestamp), "%02d:%02d:%02d %04d-%02d-%02d",
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
        } else {
            snprintf(timestamp, sizeof(timestamp), "[UNSYNCED]");
        }

        snprintf(output_buffer, sizeof(output_buffer),
                 "%.4f°C %s 0x%s %.2f GB",
                 mcp9808.readTempC(),
                 timestamp,
                 get_unique_id().c_str(),
                 ((double)(SD.totalBytes() - SD.usedBytes()) / 1e9));
        events.send(String(output_buffer).c_str(), "output_update", millis());

        // OLED Display Logic (Maintained exactly as requested)
        ssd1306.clearDisplay();
        ssd1306.setCursor(0, 16);
        ssd1306.printf("%.4f", mcp9808.readTempC());
        ssd1306.display();

        // Serial.printf("%s\n", output_buffer);
        global_counter++;

        // =========================================================================
        // HOURLY STATISTICAL TIMING LOOP INTEGRATION (3,600 Seconds)
        // =========================================================================
        if (amIServerNode) {
            static unsigned long lastHourlyFlushTime = 0;
            // const unsigned long HOURLY_INTERVAL = 3600000UL;
            const unsigned long HOURLY_INTERVAL = 60000UL;

            if (millis() - lastHourlyFlushTime >= HOURLY_INTERVAL) {
                lastHourlyFlushTime = millis();

                // --- NEW: CAPTURE NETWORK TIME METRICS FROM SYSTEM ---
                time_t now;
                struct tm timeinfo;
                char dateBuffer[12] = "0000-00-00"; // Safeguard defaults
                char timeBuffer[12] = "00:00:00";

                time(&now);
                if (localtime_r(&now, &timeinfo)) {
                    strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", &timeinfo);
                    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);
                }
                // ----------------------------------------------------

                Serial.println("\n================================================================================================================");
                Serial.println("[STATS LOG] 5-Second Fast-Poll Interval Hit -> Executing Crunch Routine");
                Serial.println("Date, Time, Local Network Name, Node ID, Sample Count, Success %, Min Temp (C), Max Temp (C), Avg Temp (C), Std Dev (C)");
                Serial.println("================================================================================================================");

                // =========================================================================
                // 📍 SELF-HEALING STORAGE RECOVERY PIPELINE ACTIVE
                // =========================================================================
                // Re-verifies file presence right before writing; heals headers if missing!
                initialize_csv_log("/telemetry_log.csv");

                // Open the SD card file target for data appending
                File sdLogFile = SD.open("/telemetry_log.csv", FILE_APPEND);
                if (!sdLogFile) {
                    Serial.println(F("[SD-LOG] ERROR: Failed to open telemetry_log.csv for data writing!"));
                }
                // =========================================================================

                for (int i = 0; i < MAX_SYSTEM_NODES; i++) {
                    NodeStats &stats = statisticalMatrix[i];
                    
                    // --- FIXED: DYNAMIC LOCAL HOSTNAME STRING ENGINE ---
                    char hostname[24];
                    if (i == 0) {
                        snprintf(hostname, sizeof(hostname), "HZ-SERVER.local");
                    } else {
                        snprintf(hostname, sizeof(hostname), "HZ-NODE-%02d.local", i);
                    }
                    // ----------------------------------------------------
                    
                    // --- FIXED: CALIBRATE SUCCESS MATH DENOMINATOR TO 5-SECONDS ---
                    float successRate = (stats.sampleCount / (HOURLY_INTERVAL / 1000.0f)) * 100.0f;
                    if (successRate > 100.0f) successRate = 100.0f; 
                    // ----------------------------------------------------

                    if (stats.sampleCount > 0) {
                        float calculatedAverage = stats.rollingMean;
                        float calculatedStdDev = 0.0f;
                        
                        if (stats.sampleCount > 1) {
                            float variance = stats.accumulatedM2 / (stats.sampleCount - 1);
                            calculatedStdDev = sqrt(variance);
                        }
                        
                        // Prints cleanly to your Serial Terminal Monitor
                        Serial.printf("[HOURLY-METRICS], %s, %s, %-17s, NODE_%02d, %5lu, %5.1f%%, %10.2f, %10.2f, %12.4f, %10.4f\n", 
                                      dateBuffer, timeBuffer, hostname, i, stats.sampleCount, 
                                      successRate, stats.minTemp, stats.maxTemp, calculatedAverage, calculatedStdDev);

                        // 📍 Writes the identical data row row safely to the SD card media
                        if (sdLogFile) {
                            sdLogFile.printf("%s,%s,%s,NODE_%02d,%lu,%.1f,%.2f,%.2f,%.4f,%.4f\n", 
                                            dateBuffer, timeBuffer, hostname, i, stats.sampleCount, 
                                            successRate, stats.minTemp, stats.maxTemp, calculatedAverage, calculatedStdDev);
                        }
                    } else {
                        // Prints empty slot line to Serial Terminal Monitor
                        Serial.printf("[HOURLY-METRICS], %s, %s, %-17s, NODE_%02d,     0,   0.0%%,     --.--,     --.--,      --.----,    --.----\n", 
                                      dateBuffer, timeBuffer, hostname, i);

                        // 📍 Writes the clean empty placeholder row straight to SD card storage
                        if (sdLogFile) {
                            sdLogFile.printf("%s,%s,%s,NODE_%02d,0,0.0,--.--,--.--,--.----,--.----\n", 
                                            dateBuffer, timeBuffer, hostname, i);
                        }
                    }
                    
                    // Reset running variables back to zero for the next clean fast block
                    stats.sampleCount = 0;
                    stats.rollingMean = 0.0f;
                    stats.accumulatedM2 = 0.0f;
                    stats.minTemp = 999.0f;  // Resets high bound marker
                    stats.maxTemp = -999.0f; // Resets low bound marker
                }

                // =========================================================================
                // 📍 CLOSE FILE HANDLE TO FLUSH ALL BUFFERS SAFELY TO DISK
                // =========================================================================
                if (sdLogFile) {
                    sdLogFile.close();
                    Serial.println(F("[SD-LOG] Hourly metrics block safely synchronized to SD storage media."));
                }
                // =========================================================================

                Serial.println("================================================================================================================\n");
            }
        }
     }
}

