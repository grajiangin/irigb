#ifndef SERVER_H
#define SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <AsyncWebSocket.h>
#include "settings.h"

class IRIGWebServer {
public:
    IRIGWebServer();
    ~IRIGWebServer();

    // Initialize the web server
    bool begin(Settings* settings);

    // Start the web server
    void start();

    // Stop the web server
    void stop();

    // Get server status
    bool isRunning() const;

    // Get server port
    uint16_t getPort() const;

    // Send time update via WebSocket (public for main.cpp access)
    void sendTimeUpdate(int hour = -1, int minute = -1, int second = -1, int day = -1);
    
    // Send LED status update via WebSocket (public for main.cpp access)
    void update_led(bool enabled, bool ntp_sync);

private:
    AsyncWebServer* server;
    AsyncWebSocket* ws;
    Settings* settings;
    bool running;
    uint16_t port;

    // Initialize SPIFFS/LittleFS
    bool initFileSystem();

    // Format filesystem if corrupted
    bool formatFileSystem();

    // Setup web routes
    void setupRoutes();

    // Handle root page
    void handleRoot(AsyncWebServerRequest *request);

    // Handle 404 errors
    void handleNotFound(AsyncWebServerRequest *request);

    // Handle API endpoints (legacy, now using WebSocket)
    void handleGetConfig(AsyncWebServerRequest *request);
    void handleSaveConfig(AsyncWebServerRequest *request);

    // WebSocket event handler
    void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);

    // WebSocket message handlers
    void handleWebSocketMessage(AsyncWebSocketClient *client, String message);
    void handleGetConfigWebSocket(AsyncWebSocketClient *client);
    void handleSaveConfigWebSocket(AsyncWebSocketClient *client, String jsonData);

};

#endif // SERVER_H
