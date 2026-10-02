# Project Profile: ESP32-C6 Multi-Node Shared Duty Node Network

## Coding Standards
* Variable and function naming: snake_case.
* Tab Indentation: Strict 4-space indentation.
* Frameworks: Arduino ESP32 Core, ESPAsyncWebServer, SPI SD, LittleFS.
* Separation of Concerns: No HTML inline strings in C++ files. HTML lives 
  purely on LittleFS data files.

## Web Architecture Status
* UI Assets: Stored in `data/index.html` on LittleFS. Fits a tight container 
  (450px) with custom styling.
* Layout: Symmetrical grid layout containing:
  - Terminal text line input (`userInput`).
  - Send button (`btn-send`).
  - Green counter box (`liveCounter`, width: 427px).
  - Async file browser upload zone with an explicit `73px` wide, `42px` high 
    green "SD files" anchor button.
  - Independent scrollable output box (`liveScrollLog`, width: 427px, 
    height: 310px).
* Web Stream Channels:
  1. `output_update`: Updates the real-time numeric counter display.
  2. `log_update`: Streams system telemetry rows. If it contains the 
     `[CLEAR_LOG_TRIGGER]` flag, JavaScript flushes the 250-line maximum 
     rolling log window queue instantly.
* Upload Routine: Thread-safe multipart file stream handler implemented inside 
  C++ routing function `init_webpage_routes`. Leverages `request->_tempObject` 
  allocations to dynamically intercept file sizes against remaining SD 
  sectors (`SD.totalBytes() - SD.usedBytes()`) to prevent storage overflow 
  crashes early at chunk 0.
* File Viewing: Managed via a `/view` endpoint route using dynamic manually 
  attached `Content-Disposition: inline; filename="..."` header responses 
  to force browser right-click context paths to honor original SD item 
  names instead of default naming masks.

## Core Objective: Multi-Node Expansion
* Topology: 4-Node distributed mesh exchanging frames over ESP-NOW.
* Resiliency: Dynamic Server Election. Node 1 is primary. If Node 1 heartbeat 
  expires, Backup 1 (Node 2) configures its Wi-Fi Station profile, boots the 
  AsyncWebServer, and hosts the clean interface dashboard.
* Content Generation Payload: MCP9808 Sensor modules tracking localized 
  Temperature.

## Maintenance Protocol
* Whenever hardware peripherals are added or removed, UI elements change, or network/endpoint contracts are altered, keep this blueprint updated to reflect the active design.
