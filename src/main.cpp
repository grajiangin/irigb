#include <Arduino.h>
#include <WiFi.h>
#include "driver/spi_master.h"
#include <esp_err.h>
#include "ESP32-ENC28J60.h"
#include "pins.h"
#include "display.h"
#include "ethernet.h"
#include "ntp.h"
#include "server.h"
#include "settings.h"
#include "irigb.h"
// Timer for 0.5ms ISR
hw_timer_t *timer = NULL;

bool sec_blink = false;
uint32_t last_blink = 0;
uint32_t last_ntp_update = 0;
bool ntp_got_data = false;
Display display(DISP_DATA, DISP_CLK, DISP_STB);
IRIGWebServer webServer;
Settings settings;
bool wclk_state = false;
bool irig_available = false;
bool irig_enabled = false;
bool ntp_valid = false;

IRIGB irigb1(P1);
IRIGB irigb2(P2);
IRIGB irigb3(P3);
IRIGB irigb4(P4);
IRIGB irigb5(P5);
IRIGB irigb6(P6);
IRIGB irigb7(P7);
IRIGB irigb8(P8);

uint8_t bit_counter = 0;
void IRAM_ATTR onTimer()
{
  if (!irig_available)
    return;
  if (wclk_state)
  {
    irigb1.update(bit_counter);
    irigb2.update(bit_counter);
    irigb3.update(bit_counter);
    irigb4.update(bit_counter);
    irigb5.update(bit_counter);
    irigb6.update(bit_counter);
    irigb7.update(bit_counter);
    irigb8.update(bit_counter);
    bit_counter++;
    if (bit_counter >= 100)
    {
      bit_counter = 0;
    }
  }
  digitalWrite(WCLK, !wclk_state);
  wclk_state = !wclk_state;
  
}

extern void ntp_hanlder(NTPTime time)
{
  ntp_got_data = true;
  ntp_valid = true;
  irig_available = true;
}

void init_pins()
{
  pinMode(WCLK, OUTPUT);
  pinMode(P1, OUTPUT);
  pinMode(P2, OUTPUT);
  pinMode(P3, OUTPUT);
  pinMode(P4, OUTPUT);
  pinMode(P5, OUTPUT);
  pinMode(P6, OUTPUT);
  pinMode(P7, OUTPUT);
  pinMode(P8, OUTPUT);
}

void ntp_task(void *param)
{
  for (;;)
  {
    if (ntp_update())
    {
      irig_available = true;
    }
    if (irig_available)
    {
      NTPTime currentTime = ntp_get_time();
      IrigTime irigTime;
      irigTime.second = currentTime.second;
      irigTime.minute = currentTime.minute;
      irigTime.hour = currentTime.hour;
      irigTime.day = currentTime.day;
      irigTime.year = currentTime.year;
      irigb1.encodeTimeIntoBits(irigTime, (int)(settings.ntp.timeOffset * 3600));
      irigb2.encodeTimeIntoBits(irigTime, (int)(settings.ntp.timeOffset * 3600));
      irigb3.encodeTimeIntoBits(irigTime, (int)(settings.ntp.timeOffset * 3600));
      irigb4.encodeTimeIntoBits(irigTime, (int)(settings.ntp.timeOffset * 3600));
      irigb5.encodeTimeIntoBits(irigTime, (int)(settings.ntp.timeOffset * 3600));
      irigb6.encodeTimeIntoBits(irigTime, (int)(settings.ntp.timeOffset * 3600));
      irigb7.encodeTimeIntoBits(irigTime, (int)(settings.ntp.timeOffset * 3600));
      irigb8.encodeTimeIntoBits(irigTime, (int)(settings.ntp.timeOffset * 3600));
    }
    delay(300);
  }
}

void setup()
{
  Serial.begin(115200);
  init_pins();

  // Initialize 0.5ms timer ISR
  timer = timerBegin(0, 80, true);             // Timer 0, prescaler 80 (1MHz), count up
  timerAttachInterrupt(timer, &onTimer, true); // Attach ISR
  timerAlarmWrite(timer, 500, true);           // 500 ticks = 0.5ms (at 1MHz)
  timerAlarmEnable(timer);                     // Enable the timer
  // Load settings
  if (!settings.load())
  {
    Serial.println("Failed to load settings, using defaults");
  }
  init_display();
  display.print_display(0, 0, 0, 0);
  display.display();

  // Initialize ethernet with hardware
  if (!eth_init())
  {
    Serial.println("Failed to initialize ethernet hardware");
  }

  // Configure network using settings
  if (!eth_configure_network(&settings))
  {
    Serial.println("Failed to configure network");
  }

  // Start ethernet monitoring task
  if (!eth_start_monitoring(&settings))
  {
    Serial.println("Failed to start ethernet monitoring");
  }

  // Initialize web server with settings
  if (webServer.begin(&settings))
  {
    webServer.start();
  }
  else
  {
    Serial.println("Failed to initialize web server");
  }

  init_ntp();

  xTaskCreate(
      ntp_task,
      "ntp_task", // Task name
      4096,       // Stack size
      nullptr,    // Parameter
      1,          // Priority
      nullptr     // Task handle
  );
}

void loop()
{
  NTPTime time = ntp_get_time();
  webServer.sendTimeUpdate(time.hour, time.minute, time.second, time.day);
  irig_enabled = settings.enabled;
  if (ntp_valid)
  {
    display.print_display(time.day, time.hour, time.minute, time.second);
    display.set_seconds_led(sec_blink);
    display.set_minute_led(true);
    display.set_hour_led(true);
  }
  else
  {
    display.print_display(0, 0, 0, 0);
    display.set_seconds_led(false);
    display.set_minute_led(false);
    display.set_hour_led(false);
  }

  display.set_enabled_led(settings.enabled);
  display.set_network_led(eth_link_up());
  if (sec_blink)
  {
    webServer.update_led(settings.enabled, ntp_got_data);
    if (ntp_got_data)
    {
      ntp_got_data = false;
      display.set_ntp_led(true);
    }
    else
    {
      display.set_ntp_led(false);
    }
  }
  display.display();
  delay(300);
  if (millis() - last_blink > 500)
  {
    last_blink = millis();
    sec_blink = !sec_blink;
  }
}
