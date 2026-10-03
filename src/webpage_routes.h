#ifndef WEBPAGE_ROUTES_H
#define WEBPAGE_ROUTES_H

#pragma once
#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <FS.h>
#include <SD.h>
#include <esp_mac.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>

#define MAX_SYSTEM_NODES 16

enum PacketType {
    PACKET_REGISTRATION_REQ,
    PACKET_REGISTRATION_ACK,
    PACKET_TELEMETRY
};

struct __attribute__((__packed__)) TelemetryPacket {
    uint8_t packetType;       
    uint8_t dynamicNodeId;    
    float temperature;        
    uint8_t macAddr[6];       // FIXED: Added array boundary limit to prevent memory overlapping
    uint32_t timestamp;       
};

struct RegistrationEntry {
    uint8_t mac[6];           // FIXED: Added array boundary limit matching network interfaces
    bool isActive = false;
};
extern RegistrationEntry clientRegistry[MAX_SYSTEM_NODES];

// Structure to hold single node metrics in the server memory matrix
struct NodeData {
    float temperature = 0.0f;
    uint32_t lastSeenMillis = 0;
    bool isOnline = false;
};
extern NodeData systemMatrix[MAX_SYSTEM_NODES];

struct NodeStats {
    uint32_t sampleCount = 0;   
    float rollingMean = 0.0f;   
    float accumulatedM2 = 0.0f; 
};

extern int global_counter;
extern const char* counter_file;
// Share tracking states globally across files
extern uint8_t broadcastMacAddress[];
// Runtime identity storage (Replaces the rigid platformio.ini macros)
extern int myRuntimeNodeId;
extern bool amIServerNode;
extern String myRuntimeHostname;

// Function Prototypes (Promises to the compiler that these functions exist externally)
void init_resilient_esp_now();
void on_data_sent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status);
void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len);
void broadcast_telemetry(float currentTemperature);
void update_system_matrix(uint8_t nodeId, float temp);
void handle_api_system_temp(AsyncWebServerRequest *request);

void load_global_counter();
void save_global_counter();

void say_hello(void);
void mcu_dir(fs::FS &fs, const char * dir_name, uint8_t levels);
void webpage_serve_html(AsyncWebServerRequest *request, fs::LittleFSFS &local_filesystem);
void webpage_led(AsyncWebServer &server, const int led_pin);
void webpage_file_list(AsyncWebServerRequest *request, fs::FS &fs_instance);
String add_commas_to_string(long value);
void handle_sd_files(AsyncWebServerRequest *request);
bool init_sd(int chip_select_pin);
String get_unique_id();
void init_webpage_routes(AsyncWebServer &server, fs::FS &sd_instance, fs::FS &fs_instance);
String format_with_commas(long value);

// ----- Keep as the last line for the include guard ------------------------
#endif // WEBPAGE_ROUTES_H
