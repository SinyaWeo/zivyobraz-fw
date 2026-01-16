#ifndef DISPLAY_H
#define DISPLAY_H

#include <epd_driver.h>
#include <Arduino.h>

#include "utils.h"

// Enum for 16 levels of gray (0 = black, 15 = white)
enum class Color : uint16_t {
  Black = 0,
  Gray01 = 15,
  Gray02 = 31,
  Gray03 = 47,
  Gray04 = 63,
  Gray05 = 79,
  Gray06 = 95,
  Gray07 = 111,
  Gray08 = 127,
  Gray09 = 143,
  Gray10 = 159,
  Gray11 = 175,
  Gray12 = 191,
  Gray13 = 207,
  Gray14 = 223,
  White = 255
};

namespace Display
{

void init();
void powerOnAndInit();
void clear();
void setRotation(uint8_t rotation);

// Display info
uint16_t getWidth();
uint16_t getHeight();
uint16_t getResolutionX();
uint16_t getResolutionY();
uint16_t getNumberOfPages();

// Drawing functions
void drawPixel(int xCord, int yCord, Color color);
void drawQrCode(const char *qrStr, int qrSize, int yCord, int xCord, byte qrSizeMulti = 1);
void setTextPos(GFXfont *font, const String &text, int xCord, int yCord);
void centeredText(GFXfont *font, const String &text, int xCord, int yCord);

// Page functions
Rect_t setToFullWindow();
Rect_t setToPartialWindow(int xCord, int yCord, int width, int height);
bool supportsPartialRefresh();
void setToFirstPage();
bool setToNextPage();
void enableLightSleepDuringRefresh(bool enable);

// Direct streaming functions (for row-by-row streaming mode)
bool supportsDirectStreaming();
void initDirectStreaming(bool partialRefresh = false, uint16_t maxRowCount = 0);
void writeRowsDirect(uint16_t yStart, uint16_t rowCount, const uint8_t *blackData, const uint8_t *colorData);
void finishDirectStreaming();
void refreshDisplay();

// Error screens
void showNoWiFiError(uint64_t sleepSeconds, const String &wikiUrl);
void showWiFiError(const String &hostname, const String &password, const String &urlWeb, const String &wikiUrl);
}
#endif // DISPLAY_H