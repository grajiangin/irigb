#include "server.h"
#include "ethernet.h"
#include <SPIFFS.h>
#include <time.h>

IRIGWebServer::IRIGWebServer() : server(nullptr), ws(nullptr), settings(nullptr), running(false), port(80) {
}

IRIGWebServer::~IRIGWebServer() {
    stop();
    if (ws) {
        delete ws;
        ws = nullptr;
    }
    if (server) {
        delete server;
        server = nullptr;
    }
}

bool IRIGWebServer::begin(Settings* settings) {
    this->settings = settings;
    // Initialize SPIFFS/LittleFS first
    if (!initFileSystem()) {
        Serial.println("Failed to initialize filesystem, attempting to format and recover...");

        // Try to format and reinitialize
        if (!formatFileSystem()) {
            Serial.println("Failed to format filesystem - server cannot start");
            return false;
        }

        // Try initialization again after formatting
        if (!initFileSystem()) {
            Serial.println("Failed to initialize filesystem even after formatting");
            return false;
        }

        Serial.println("Filesystem recovered successfully");
    }

    // Create AsyncWebServer instance
    server = new AsyncWebServer(port);
    if (!server) {
        Serial.println("Failed to create AsyncWebServer");
        return false;
    }

    // Create WebSocket instance
    ws = new AsyncWebSocket("/ws");
    if (!ws) {
        Serial.println("Failed to create WebSocket");
        return false;
    }

    // Setup WebSocket event handler
    using std::placeholders::_1;
    using std::placeholders::_2;
    using std::placeholders::_3;
    using std::placeholders::_4;
    using std::placeholders::_5;
    using std::placeholders::_6;
    ws->onEvent(std::bind(&IRIGWebServer::onWebSocketEvent, this, _1, _2, _3, _4, _5, _6));
    server->addHandler(ws);

    // Setup routes
    setupRoutes();

    Serial.println("Web server initialized successfully");
    return true;
}

void IRIGWebServer::start() {
    if (!server || running) {
        return;
    }

    server->begin();
    running = true;
    Serial.printf("Web server started on port %d\n", port);
    Serial.printf("Server address: http://%s\n", ETH.localIP().toString().c_str());
}

void IRIGWebServer::stop() {
    if (!server || !running) {
        return;
    }

    server->end();
    running = false;
    Serial.println("Web server stopped");
}

bool IRIGWebServer::isRunning() const {
    return running;
}

uint16_t IRIGWebServer::getPort() const {
    return port;
}

bool IRIGWebServer::initFileSystem() {
    // Use SPIFFS only
    Serial.println("Initializing SPIFFS...");
    if (SPIFFS.begin(true)) {
        // SPIFFS.begin() returned true, verify it's working
        File root = SPIFFS.open("/");
        if (root && root.isDirectory()) {
            Serial.println("SPIFFS initialized and verified successfully");

            // Check if index.html exists in SPIFFS
            if (SPIFFS.exists("/index.html")) {
                Serial.println("index.html found in SPIFFS");
                root.close();
                return true;
            } else {
                Serial.println("index.html not found in SPIFFS");
            }
            root.close();
        } else {
            Serial.println("SPIFFS initialization verification failed");
            SPIFFS.end(); // Clean up failed mount
        }
    } else {
        Serial.println("SPIFFS.begin() failed");
    }

    // If we get here, SPIFFS is not working properly
    Serial.println("Error: Could not initialize SPIFFS properly");
    Serial.println("This may indicate filesystem corruption.");
    return false;
}

bool IRIGWebServer::formatFileSystem() {
    Serial.println("Attempting to format and recover SPIFFS...");

    // Format SPIFFS
    Serial.println("Formatting SPIFFS...");
    if (SPIFFS.format()) {
        Serial.println("SPIFFS formatted successfully");
        if (SPIFFS.begin(true)) {
            File root = SPIFFS.open("/");
            if (root && root.isDirectory()) {
                Serial.println("SPIFFS reformatted and ready");
                root.close();
                return true;
            }
            root.close();
        }
    } else {
        Serial.println("SPIFFS format failed");
    }

    Serial.println("Error: Could not format SPIFFS");
    return false;
}

void IRIGWebServer::setupRoutes() {
    if (!server) {
        return;
    }

    // Serve static files from SPIFFS
    if (SPIFFS.begin(false)) { // Check if SPIFFS is mounted (don't format)
        if (SPIFFS.exists("/index.html")) {
            server->serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
            Serial.println("Serving static files from SPIFFS");
        }
    }

    // API endpoints
    server->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleGetConfig(request);
    });

    server->on("/api/config", HTTP_POST, [this](AsyncWebServerRequest *request) {
        handleSaveConfig(request);
    });

    // Root route handler (fallback)
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        handleRoot(request);
    });

    // 404 handler
    server->onNotFound([this](AsyncWebServerRequest *request) {
        handleNotFound(request);
    });

    Serial.println("Web server routes configured");
}

void IRIGWebServer::handleRoot(AsyncWebServerRequest *request) {
    // Serve index.html from SPIFFS
    if (SPIFFS.begin(false) && SPIFFS.exists("/index.html")) {
        Serial.println("Serving index.html from SPIFFS");
        request->send(SPIFFS, "/index.html", "text/html");
        return;
    }

    // If no HTML file found, send a simple response
    Serial.println("No index.html found, sending fallback page");
    String html = "<!DOCTYPE html><html><head><title>IRIG-B Server</title></head>";
    html += "<body><h1>IRIG-B Time Server</h1>";
    html += "<p>Server is running but no index.html found in filesystem.</p>";
    html += "<p>IP Address: " + ETH.localIP().toString() + "</p>";
    html += "<p>Check serial output for filesystem status.</p>";
    html += "</body></html>";

    request->send(200, "text/html", html);
}

void IRIGWebServer::handleNotFound(AsyncWebServerRequest *request) {
    String html = "<!DOCTYPE html><html><head><title>404 - Not Found</title></head>";
    html += "<body><h1>404 - Page Not Found</h1>";
    html += "<p>The requested page was not found.</p>";
    html += "<a href='/'>Go back to home</a>";
    html += "</body></html>";

    request->send(404, "text/html", html);
}

void IRIGWebServer::handleGetConfig(AsyncWebServerRequest *request) {
    if (!settings) {
        request->send(500, "application/json", "{\"error\":\"Settings not available\"}");
        return;
    }

    // Create JSON response from settings
    String json = "{";
    json += "\"dhcp\":" + String(settings->network.dhcp ? "true" : "false") + ",";
    json += "\"ip\":\"" + String(settings->network.ip) + "\",";
    json += "\"subnet\":\"" + String(settings->network.subnet) + "\",";
    json += "\"gateway\":\"" + String(settings->network.gateway) + "\",";
    json += "\"dns\":\"" + String(settings->network.dns) + "\",";
    json += "\"ntpServer\":\"" + String(settings->ntp.server) + "\",";
    json += "\"ntpPort\":" + String(settings->ntp.port) + ",";
    json += "\"timeOffset\":" + String(settings->ntp.timeOffset) + ",";
    json += "\"enabled\":" + String(settings->enabled ? "true" : "false") + ",";
    json += "\"channel_1_mode\":" + String(settings->channel_1_mode) + ",";
    json += "\"channel_2_mode\":" + String(settings->channel_2_mode) + ",";
    json += "\"channel_3_mode\":" + String(settings->channel_3_mode) + ",";
    json += "\"channel_4_mode\":" + String(settings->channel_4_mode) + ",";
    json += "\"channel_5_mode\":" + String(settings->channel_5_mode) + ",";
    json += "\"channel_6_mode\":" + String(settings->channel_6_mode) + ",";
    json += "\"channel_7_mode\":" + String(settings->channel_7_mode) + ",";
    json += "\"channel_8_mode\":" + String(settings->channel_8_mode);
    json += "}";

    request->send(200, "application/json", json);
}

void IRIGWebServer::handleSaveConfig(AsyncWebServerRequest *request) {
    if (!settings) {
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Settings not available\"}");
        return;
    }

    // Parse form data
    if (request->hasParam("dhcp", true)) {
        settings->network.dhcp = request->getParam("dhcp", true)->value() == "true";
        settings->network.ip = request->getParam("ip", true)->value();
        settings->network.subnet = request->getParam("subnet", true)->value();
        settings->network.gateway = request->getParam("gateway", true)->value();
        settings->network.dns = request->getParam("dns", true)->value();
        settings->ntp.server = request->getParam("ntpServer", true)->value();
        settings->ntp.port = request->getParam("ntpPort", true)->value().toInt();
        settings->enabled = request->getParam("enabled", true)->value() == "true";

        // Set network changes flag to trigger reconfiguration
        settings->setNetworkChangesFlag(true);

        if (settings->save()) {
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved\"}");
        } else {
            request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to save configuration\"}");
        }
    } else {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Missing parameters\"}");
    }
}

void IRIGWebServer::onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
            break;
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
        case WS_EVT_DATA: {
            // Handle incoming WebSocket data
            AwsFrameInfo *info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                data[len] = 0; // Null terminate the string
                String message = (char*)data;
                handleWebSocketMessage(client, message);
            }
            break;
        }
        case WS_EVT_PONG:
        case WS_EVT_ERROR:
            break;
    }
}

void IRIGWebServer::sendTimeUpdate(int hour, int minute, int second, int day) {
    if (!ws || ws->count() == 0) return;

    // Use provided time values, or get current time if not provided
    if (hour == -1 || minute == -1 || second == -1 || day == -1) {
        // Get current system time as fallback
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            hour = timeinfo.tm_hour;
            minute = timeinfo.tm_min;
            second = timeinfo.tm_sec;
            day = timeinfo.tm_mday;
        } else {
            // If no time available, don't send update
            return;
        }
    }

            // Apply time offset
            if (settings && settings->ntp.timeOffset != 0) {
                int32_t timeOffsetHours = settings->ntp.timeOffset;
                // Convert current time to total seconds since midnight
                int32_t totalSeconds = hour * 3600 + minute * 60 + second;
                
                // Apply offset (convert hours to seconds)
                totalSeconds += timeOffsetHours * 3600;
                
                // Handle day overflow/underflow
                while (totalSeconds >= 86400) {
                    totalSeconds -= 86400;
                    day++;
                }
                while (totalSeconds < 0) {
                    totalSeconds += 86400;
                    day--;
                }
                
                // Convert back to hours, minutes, seconds
                hour = totalSeconds / 3600;
                minute = (totalSeconds % 3600) / 60;
                second = totalSeconds % 60;
            }

    String timeData = "{";
    timeData += "\"type\":\"time\",";
    timeData += "\"hour\":" + String(hour) + ",";
    timeData += "\"minute\":" + String(minute) + ",";
    timeData += "\"second\":" + String(second) + ",";
    timeData += "\"day\":" + String(day);
    timeData += "}";

    ws->textAll(timeData);
}

void IRIGWebServer::update_led(bool enabled, bool ntp_sync) {
    if (!ws || ws->count() == 0) return;
    
    // Create JSON message for LED status update
    String json = "{";
    json += "\"type\":\"leds\",";
    json += "\"enabled\":" + String(enabled ? "true" : "false") + ",";
    json += "\"ntp_sync\":" + String(ntp_sync ? "true" : "false");
    json += "}";
    
    // Send to all connected WebSocket clients
    ws->textAll(json);
}

void IRIGWebServer::handleWebSocketMessage(AsyncWebSocketClient *client, String message) {
    Serial.println("WebSocket message received: " + message);

    // Parse JSON message
    if (message.startsWith("{")) {
        // Simple JSON parsing for key commands
        if (message.indexOf("\"action\":\"getConfig\"") >= 0) {
            handleGetConfigWebSocket(client);
        } else if (message.indexOf("\"action\":\"saveConfig\"") >= 0) {
            handleSaveConfigWebSocket(client, message);
        } else if (message.indexOf("\"action\":\"getTime\"") >= 0) {
            // Send current time immediately
            sendTimeUpdate();
        }
    }
}

void IRIGWebServer::handleGetConfigWebSocket(AsyncWebSocketClient *client) {
    if (!settings) {
        Serial.println("Settings not available");
        return;
    }

    // Create JSON response from settings
    String json = "{";
    json += "\"type\":\"config\",";
    json += "\"dhcp\":" + String(settings->network.dhcp ? "true" : "false") + ",";
    json += "\"ip\":\"" + String(settings->network.ip) + "\",";
    json += "\"subnet\":\"" + String(settings->network.subnet) + "\",";
    json += "\"gateway\":\"" + String(settings->network.gateway) + "\",";
    json += "\"dns\":\"" + String(settings->network.dns) + "\",";
    json += "\"ntpServer\":\"" + String(settings->ntp.server) + "\",";
    json += "\"ntpPort\":" + String(settings->ntp.port) + ",";
    json += "\"timeOffset\":" + String(settings->ntp.timeOffset) + ",";
    json += "\"enabled\":" + String(settings->enabled ? "true" : "false") + ",";
    json += "\"channel_1_mode\":" + String(settings->channel_1_mode) + ",";
    json += "\"channel_2_mode\":" + String(settings->channel_2_mode) + ",";
    json += "\"channel_3_mode\":" + String(settings->channel_3_mode) + ",";
    json += "\"channel_4_mode\":" + String(settings->channel_4_mode) + ",";
    json += "\"channel_5_mode\":" + String(settings->channel_5_mode) + ",";
    json += "\"channel_6_mode\":" + String(settings->channel_6_mode) + ",";
    json += "\"channel_7_mode\":" + String(settings->channel_7_mode) + ",";
    json += "\"channel_8_mode\":" + String(settings->channel_8_mode);
    json += "}";

    client->text(json);
}

void IRIGWebServer::handleSaveConfigWebSocket(AsyncWebSocketClient *client, String jsonData) {
    if (!settings) {
        Serial.println("Settings not available");
        return;
    }

    // Parse the JSON data and update settings
    if (jsonData.indexOf("\"dhcp\"") >= 0) {
        settings->network.dhcp = jsonData.indexOf("\"dhcp\":true") >= 0;
    }

    if (jsonData.indexOf("\"enabled\"") >= 0) {
        settings->enabled = jsonData.indexOf("\"enabled\":true") >= 0;
        Serial.printf("Enabled setting updated to: %s\n", settings->enabled ? "true" : "false");
    }

    // Extract values using string parsing
    String searchKeys[] = {"\"ip\"", "\"subnet\"", "\"gateway\"", "\"dns\"", "\"ntpServer\""};
    String* targetFields[] = {&settings->network.ip, &settings->network.subnet, &settings->network.gateway, &settings->network.dns, &settings->ntp.server};

    for (int i = 0; i < 5; i++) {
        int startPos = jsonData.indexOf(searchKeys[i]);
        if (startPos >= 0) {
            startPos = jsonData.indexOf("\"", startPos + searchKeys[i].length());
            if (startPos >= 0) {
                startPos++; // Skip the opening quote
                int endPos = jsonData.indexOf("\"", startPos);
                if (endPos > startPos) {
                    String value = jsonData.substring(startPos, endPos);
                    *targetFields[i] = value;
                }
            }
        }
    }

    // Handle ntpPort
    int portPos = jsonData.indexOf("\"ntpPort\"");
    if (portPos >= 0) {
        int colonPos = jsonData.indexOf(":", portPos);
        if (colonPos >= 0) {
            int commaPos = jsonData.indexOf(",", colonPos);
            if (commaPos == -1) commaPos = jsonData.indexOf("}", colonPos);
            if (commaPos > colonPos) {
                String portStr = jsonData.substring(colonPos + 1, commaPos);
                portStr.trim();
                settings->ntp.port = portStr.toInt();
            }
        }
    }

    // Handle time offset
    int timeOffsetPos = jsonData.indexOf("\"timeOffset\"");
    if (timeOffsetPos >= 0) {
        int colonPos = jsonData.indexOf(":", timeOffsetPos);
        if (colonPos >= 0) {
            int commaPos = jsonData.indexOf(",", colonPos);
            if (commaPos == -1) commaPos = jsonData.indexOf("}", colonPos);
            if (commaPos > colonPos) {
                String offsetStr = jsonData.substring(colonPos + 1, commaPos);
                offsetStr.trim();
                settings->ntp.timeOffset = offsetStr.toInt();
                settings->setNTPChangesFlag(true);
            }
        }
    }

    // Handle channel modes
    for (int i = 1; i <= 8; i++) {
        String channelKey = "\"channel_" + String(i) + "_mode\"";
        int channelPos = jsonData.indexOf(channelKey);
        if (channelPos >= 0) {
            int colonPos = jsonData.indexOf(":", channelPos);
            if (colonPos >= 0) {
                int commaPos = jsonData.indexOf(",", colonPos);
                if (commaPos == -1) commaPos = jsonData.indexOf("}", colonPos);
                if (commaPos > colonPos) {
                    String modeStr = jsonData.substring(colonPos + 1, commaPos);
                    modeStr.trim();
                    uint8_t mode = modeStr.toInt();
                    Serial.printf("Channel %d mode: %s -> %d\n", i, modeStr.c_str(), mode);
                    if (mode <= 8) { // Validate mode range (0-8)
                        switch (i) {
                            case 1: settings->channel_1_mode = mode; break;
                            case 2: settings->channel_2_mode = mode; break;
                            case 3: settings->channel_3_mode = mode; break;
                            case 4: settings->channel_4_mode = mode; break;
                            case 5: settings->channel_5_mode = mode; break;
                            case 6: settings->channel_6_mode = mode; break;
                            case 7: settings->channel_7_mode = mode; break;
                            case 8: settings->channel_8_mode = mode; break;
                        }
                        Serial.printf("Set channel %d mode to %d\n", i, mode);
                    } else {
                        Serial.printf("Invalid mode %d for channel %d (must be 0-8)\n", mode, i);
                    }
                }
            }
        }
    }

    // Set network changes flag to trigger reconfiguration if network settings were updated
    bool networkSettingsChanged = (jsonData.indexOf("\"dhcp\"") >= 0 ||
                                   jsonData.indexOf("\"ip\"") >= 0 ||
                                   jsonData.indexOf("\"subnet\"") >= 0 ||
                                   jsonData.indexOf("\"gateway\"") >= 0 ||
                                   jsonData.indexOf("\"dns\"") >= 0);
    if (networkSettingsChanged) {
        settings->setNetworkChangesFlag(true);
    }

    // Save settings to Preferences
    if (settings->save()) {
        String response = "{\"type\":\"configSaved\",\"success\":true,\"message\":\"Configuration saved successfully\"}";
        client->text(response);
        Serial.println("Configuration saved via WebSocket");
        
        // Immediately update LEDs to reflect the new settings
        update_led(settings->enabled, false); // ntp_sync will be updated in main loop
    } else {
        String response = "{\"type\":\"configSaved\",\"success\":false,\"message\":\"Failed to save configuration\"}";
        client->text(response);
        Serial.println("Failed to save configuration");
    }
}
