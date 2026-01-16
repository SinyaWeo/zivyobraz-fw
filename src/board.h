#ifndef BOARD_H
#define BOARD_H

#include <Arduino.h>

#include "utils.h"

#define EXT_BUTTON 21
#define vBatPin 14
#define BOARD_MAX_PAGE_BUFFER_SIZE (48 * 1024)

enum class ResetReason : uint8_t
{
  UNKNOWN = 0,
  POWERON = 1,   // Power-on reset or flash upload
  EXT = 2,       // External reset (reset button)
  SW = 3,        // Software reset
  PANIC = 4,     // Software panic/exception
  INT_WDT = 5,   // Interrupt watchdog
  TASK_WDT = 6,  // Task watchdog
  WDT = 7,       // Other watchdog
  DEEPSLEEP = 8, // Wake from deep sleep
  BROWNOUT = 9,  // Brownout reset
  SDIO = 10      // Reset over SDIO
};

namespace Board
{
void setupHW();
void setEPaperPowerOn(bool on);
void enterDeepSleepMode(uint64_t sleepDuration);

float getBatteryVoltage();
float getCPUTemperature();
ResetReason getResetReason();
const char *getResetReasonString();

unsigned long checkButtonPressDuration();

} // namespace Board

#endif // BOARD_H
