#include <Arduino.h>
#include "driver/spi_master.h"
#include <esp_err.h>
#include "ESP32-ENC28J60.h"
#include "pins.h"

#include "display.h"
#include "ethernet.h"

Display display(DISP_DATA, DISP_CLK, DISP_STB);


void setup() {
  Serial.begin(115200);
  init_ethernet();
}


void loop() {
  eth_status();
 delay(1000);
}


