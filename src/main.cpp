#include <Arduino.h>
#include "driver/spi_master.h"
#include <esp_err.h>
#include "ESP32-ENC28J60.h"
#include "pins.h"

#include "display.h"
#include "ethernet.h"
#include "ntp.h"
#include "display.h"
Display display(DISP_DATA, DISP_CLK, DISP_STB);


extern void ntp_hanlder( NTPTime time)
{
  // Serial.printf("NTP: %d %d %d %d\n", time.day, time.hour, time.minute, time.second);
}

void setup() {
  Serial.begin(115200);
  init_ethernet();
  init_display();
  init_ntp();
}




bool sec_blink = false;
void loop() {
  // eth_status();
 delay(500);
 NTPTime time = ntp_get_time();
 Serial.printf("NTP: %d %d %d %d\n", time.day, time.hour, time.minute, time.second);
 display.print_display(time.day, time.hour, time.minute, time.second);
 display.set_minute_led(true);
 display.set_seconds_led(sec_blink);
 display.set_hour_led(true);
 sec_blink = !sec_blink;
 display.display();
 
}


