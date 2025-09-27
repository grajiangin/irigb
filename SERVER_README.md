# Web Server Module

This project includes a web server module that serves HTML pages from SPIFFS/LittleFS filesystem using the ESPAsyncWebServer library.

## Features

- Asynchronous web server for better performance
- Serves static files from SPIFFS/LittleFS
- Beautiful HTML interface showing IRIG-B time server status
- Automatic fallback between LittleFS and SPIFFS
- Easy integration with existing ethernet setup

## Files

- `lib/server/server.h` - Server class header
- `lib/server/server.cpp` - Server implementation
- `data/index.html` - Main web interface
- `upload_data.py` - Script to upload HTML files to ESP32 filesystem

## Dependencies

The following dependency has been added to `platformio.ini`:

```
me-no-dev/ESP Async WebServer@^1.2.3
```

## Usage

### 1. Upload HTML Files to ESP32

Before running the server, you need to upload the HTML files to the ESP32's filesystem:

```bash
# Make the upload script executable (first time only)
chmod +x upload_data.py

# Upload data directory to ESP32 SPIFFS/LittleFS
python3 upload_data.py
```

Or using PlatformIO directly:

```bash
pio run --target uploadfs
```

### 2. Server Integration

The server is automatically initialized in `main.cpp` after ethernet setup. The server will:

- Start on port 80
- Serve files from SPIFFS/LittleFS root directory
- Default to `index.html` for the root path
- Show server address in serial output

### 3. Accessing the Web Interface

Once the ESP32 has an IP address (via DHCP), you can access the web interface at:

```
http://[ESP32_IP_ADDRESS]/
```

For example: `http://192.168.1.100/`

## Web Interface Features

The included `index.html` provides:

- **Current Time Display**: Shows local system time
- **System Status LEDs**:
  - Network status (green/red)
  - NTP synchronization status (green/orange)
  - IRIG-B signal status (green/red)
- **Network Information**: IP address, MAC address, uptime
- **Auto-refresh**: Data updates every 30 seconds
- **Manual refresh**: Button to refresh status immediately

## API

### IRIGWebServer Class

```cpp
#include "server.h"

// Create server instance
IRIGWebServer webServer;

// Initialize server (must be called before start)
bool success = webServer.begin();

// Start the server
webServer.start();

// Stop the server
webServer.stop();

// Check if server is running
bool running = webServer.isRunning();

// Get server port
uint16_t port = webServer.getPort();
```

## Filesystem

The server supports both LittleFS (preferred) and SPIFFS filesystems:

- **LittleFS**: Better for ESP32-S3, supports directories
- **SPIFFS**: Fallback filesystem if LittleFS fails
- **Data Directory**: Place HTML/CSS/JS files in the `data/` directory
- **Upload**: Use `pio run --target uploadfs` or the provided script

## Troubleshooting

### Server won't start
- Check serial output for initialization errors
- Ensure ethernet is connected and has an IP address
- Verify SPIFFS/LittleFS was uploaded with HTML files

### Files not found (404 errors)
- Upload HTML files to ESP32 filesystem using `upload_data.py`
- Check that `data/index.html` exists
- Verify filesystem initialization in serial logs

### Cannot access web interface
- Check ESP32 IP address in serial output
- Ensure ethernet cable is connected
- Verify firewall/antivirus isn't blocking the connection
- Try accessing from the same network segment

### Upload fails
- Ensure ESP32 is connected via USB
- Check that PlatformIO can detect the board
- Try resetting the ESP32 before upload

## Customization

### Adding New Routes

To add custom routes, modify `server.cpp` in the `setupRoutes()` method:

```cpp
server->on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Handle API request
    String json = "{\"status\":\"ok\"}";
    request->send(200, "application/json", json);
});
```

### Modifying HTML

Edit `data/index.html` to customize the web interface. The page includes:

- Responsive design with glassmorphism effects
- Real-time status updates
- LED status indicators
- Network information display

### Adding More Files

Add additional HTML, CSS, or JS files to the `data/` directory. They will be automatically served by the static file handler.

## PlatformIO Configuration

The server requires the following board configuration in `platformio.ini`:

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
```

## Dependencies

- ESPAsyncWebServer: For asynchronous HTTP handling
- ESP32-ENC28J60: For ethernet connectivity (already included)
- Arduino framework: Base functionality
