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
#include "irig.h"
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
uint8_t wclk_count = 0;
uint8_t count_10ms = 0;
uint8_t bit_index = 0;
bool irig_available = false;
bool irig_enabled = false;

bool ntp_valid = false;

struct IrigBits
{
  uint8_t bits[105];
  uint8_t format;
  uint8_t pin;
};

IrigBits bits[8];
bool pin_state[8];

// ISR function called every 0.5ms
void IRAM_ATTR onTimer()
{

  // if(wclk_state)
  // {
  //   timerAlarmWrite(timer, 600, true);  
  // }
  // else
  // {
  //   timerAlarmWrite(timer, 400, true);  
  // }


  if (wclk_state)
  {
    if (irig_available)
    {
      for (int i = 0; i < 8; i++)
      {
        uint8_t bit = bits[i].bits[bit_index];
        uint8_t format = bits[i].format;
        uint8_t pin = bits[i].pin;
        if (format == 8)
        {
          // digitalWrite(pin, LOW);
          pin_state[i] = false;
          continue;
        }
        if (bit == 0)
        {
          // digitalWrite(pin, count_10ms < 2 ? HIGH : LOW);
          pin_state[i] = count_10ms < 2 ;
        }
        else if (bit == 1)
        {
          // digitalWrite(pin, count_10ms < 5 ? HIGH : LOW);
          pin_state[i] = count_10ms < 5 ;
        }
        else if (bit == 2)
        {
          // digitalWrite(pin, count_10ms < 8 ? HIGH : LOW);
          pin_state[i] = count_10ms < 8 ;
        }
      }
      count_10ms++;
      if (count_10ms >= 10)
      {
        count_10ms = 0;
        bit_index++;
        if (bit_index >= 100)
        {
          bit_index = 0;
          irig_available = false;
        }
      }
    }
    else
    {
      count_10ms = 0;
      bit_index = 0;
    }
  }
  
  digitalWrite(P1, pin_state[0]);
  digitalWrite(P2, pin_state[1]);
  digitalWrite(P3, pin_state[2]);
  digitalWrite(P4, pin_state[3]);
  digitalWrite(P5, pin_state[4]);
  digitalWrite(P6, pin_state[5]);
  digitalWrite(P7, pin_state[6]);
  digitalWrite(P8, pin_state[7]);
  digitalWrite(WCLK, !wclk_state);
  wclk_state = !wclk_state;
  
}

extern void ntp_hanlder(NTPTime time)
{
  ntp_got_data = true;
  ntp_valid = true;
  // for(int i=0;i<8;i++)
  // {
  //   bits[i].bits[0]=1;
  // }
  // irig_available = true;
  if (irig_enabled)
  {
    encodeTimeIntoBits(bits[0].bits, time, settings.channel_1_mode);
    encodeTimeIntoBits(bits[1].bits, time, settings.channel_2_mode);
    encodeTimeIntoBits(bits[2].bits, time, settings.channel_3_mode);
    encodeTimeIntoBits(bits[3].bits, time, settings.channel_4_mode);
    encodeTimeIntoBits(bits[4].bits, time, settings.channel_5_mode);
    encodeTimeIntoBits(bits[5].bits, time, settings.channel_6_mode);
    encodeTimeIntoBits(bits[6].bits, time, settings.channel_7_mode);
    encodeTimeIntoBits(bits[7].bits, time, settings.channel_8_mode);
    irig_available = true;
  }
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
  bits[0].pin = P1;
  bits[1].pin = P2;
  bits[2].pin = P3;
  bits[3].pin = P4;
  bits[4].pin = P5;
  bits[5].pin = P6;
  bits[6].pin = P7;
  bits[7].pin = P8;
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
}
int ip_count=0;
void loop()
{
  NTPTime time = ntp_get_time();
  bits[0].format = settings.channel_1_mode;
  bits[1].format = settings.channel_2_mode;
  bits[2].format = settings.channel_3_mode;
  bits[3].format = settings.channel_4_mode;
  bits[4].format = settings.channel_5_mode;
  bits[5].format = settings.channel_6_mode;
  bits[6].format = settings.channel_7_mode;
  bits[7].format = settings.channel_8_mode;
  webServer.sendTimeUpdate(time.hour, time.minute, time.second, time.day);
  irig_enabled = settings.enabled;
  if(ntp_valid)
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
