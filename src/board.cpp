#include "board.h"

#include "display.h"
#include "logger.h"
#include "sensor.h"
#include "epd_driver.h"

// TWI/I2C library
#include <Wire.h>

#include <esp_adc_cal.h>
#include <soc/adc_channel.h>

namespace Board
{

void setupHW()
{
  // Initialize display
  // Display::init();
  Display::init();
}

void setEPaperPowerOn(bool on)
{
  if (on)
    epd_poweron();
  else
    epd_poweroff();
}

void enterDeepSleepMode(uint64_t sleepDuration)
{
  #if defined(EXT_BUTTON)
  esp_sleep_enable_ext1_wakeup(1ULL << EXT_BUTTON, ESP_EXT1_WAKEUP_ANY_LOW);
  #endif
  delay(100);
  esp_deep_sleep_start();
}

float getBatteryVoltage()
{
  float volt;

  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize((adc_unit_t)ADC_UNIT_1, (adc_atten_t)ADC_ATTEN_DB_2_5,
                                                          (adc_bits_width_t)ADC_WIDTH_BIT_12, 1100, &adc_chars);

  float measurement = (float)analogRead(vBatPin);
  volt = (float)(measurement / 4095.0) * 7.05;

  Logger::log<Logger::Topic::BATTERY>("Voltage: {} V\n", volt);
  return volt;
}

unsigned long checkButtonPressDuration()
{
#ifdef EXT_BUTTON
  pinMode(EXT_BUTTON, INPUT_PULLUP);

  // Check if button is pressed (LOW = pressed with pull-up)
  if (digitalRead(EXT_BUTTON) == HIGH)
    return 0; // Button not pressed

  Logger::log<Logger::Topic::BTN>("Press detected at boot, measuring duration...\n");

  unsigned long pressStart = millis();
  const unsigned long maxWaitTime = 10000;

  // Wait while button is pressed (LOW)
  while (digitalRead(EXT_BUTTON) == LOW)
  {
    delay(50);

    if (millis() - pressStart > maxWaitTime)
      break;
  }

  unsigned long pressDuration = millis() - pressStart;
  Logger::log<Logger::Topic::BTN>("Press duration: {} ms\n", pressDuration);
  return pressDuration;
#else
  // EXT_BUTTON not defined for this board
  return 0;
#endif
}

float getCPUTemperature()
{
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3
  return temperatureRead();
#else
  return 0.0f;
#endif
}

ResetReason getResetReason()
{
  esp_reset_reason_t reason = esp_reset_reason();

  switch (reason)
  {
    case ESP_RST_POWERON:
      return ResetReason::POWERON;
    case ESP_RST_EXT:
      return ResetReason::EXT;
    case ESP_RST_SW:
      return ResetReason::SW;
    case ESP_RST_PANIC:
      return ResetReason::PANIC;
    case ESP_RST_INT_WDT:
      return ResetReason::INT_WDT;
    case ESP_RST_TASK_WDT:
      return ResetReason::TASK_WDT;
    case ESP_RST_WDT:
      return ResetReason::WDT;
    case ESP_RST_DEEPSLEEP:
      return ResetReason::DEEPSLEEP;
    case ESP_RST_BROWNOUT:
      return ResetReason::BROWNOUT;
    case ESP_RST_SDIO:
      return ResetReason::SDIO;
    default:
      return ResetReason::UNKNOWN;
  }
}

const char *getResetReasonString()
{
  esp_reset_reason_t reason = esp_reset_reason();
  switch (reason)
  {
    case ESP_RST_POWERON:
      return "poweron";
    case ESP_RST_EXT:
      return "external";
    case ESP_RST_SW:
      return "software";
    case ESP_RST_PANIC:
      return "panic";
    case ESP_RST_INT_WDT:
      return "int_watchdog";
    case ESP_RST_TASK_WDT:
      return "task_watchdog";
    case ESP_RST_WDT:
      return "watchdog";
    case ESP_RST_DEEPSLEEP:
      return "deepsleep";
    case ESP_RST_BROWNOUT:
      return "brownout";
    case ESP_RST_SDIO:
      return "sdio";
    default:
      return "unknown";
  }
}
} // namespace Board
