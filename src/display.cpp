#include "display.h"

#include "board.h"
#include "pixel_packer.h"
#include "logger.h"

// ESP32 sleep functions for light sleep during display refresh
#include <esp_sleep.h>
#include <cstddef>
#include <cstdint>

// Calculate optimal page height based on buffer size, display dimensions, and bits per pixel
// Returns full height if it fits in buffer, otherwise the maximum height that fits
#define CALC_PAGE_HEIGHT(height, width, bpp)                                                                           \
  (((BOARD_MAX_PAGE_BUFFER_SIZE * 8) / ((width) * (bpp)) >= (height))                                                  \
     ? (height)                                                                                                        \
     : ((BOARD_MAX_PAGE_BUFFER_SIZE * 8) / ((width) * (bpp))))

#include <epd_driver.h>
#include <ed047tc1.h>

// Font
// #include <gfxfont.h>
// #include "fonts/OpenSansSB_12px.h"
#include "fonts/OpenSansSB14px.h"
#include "fonts/OpenSansSB16px.h"
#include "fonts/OpenSansSB18px.h"
#include "fonts/OpenSansSB20px.h"
#include "fonts/OpenSansSB24px.h"

const GFXfont *font14px = &OpenSansSB14px;
const GFXfont *font16px = &OpenSansSB16px;
const GFXfont *font18px = &OpenSansSB18px;
const GFXfont *font20px = &OpenSansSB20px;
const GFXfont *font24px = &OpenSansSB24px;
const GFXfont* defaultFont = &OpenSansSB16px;

#include <qrcode.h>

#define DISPLAY_RESOLUTION_X EPD_WIDTH
#define DISPLAY_RESOLUTION_Y EPD_HEIGHT

namespace Display
{

// Framebuffer pointer for display operations
uint8_t *framebuffer = nullptr;

static struct
{
  uint8_t *data = nullptr;
  size_t size = 0;

  bool allocate(size_t bytes)
  {
    release();
    data = (uint8_t *)malloc(bytes);
    if (data)
      size = bytes;
    return data != nullptr;
  }

  void release()
  {
    if (data)
    {
      free(data);
      data = nullptr;
      size = 0;
    }
  }

  bool canFit(size_t bytes) const { return data && size >= bytes; }
} bwConversionBuffer;

// Track whether partial refresh is requested for direct streaming mode
static bool directStreamingPartialRefresh = false;

FontProperties defaultFontProps = {
  0,
  15,
  0,
  0
};

FontProperties invertedFontProps = {
  15,
  0,
  0,
  0
};

DrawMode_t defaultDrawMode = DrawMode_t::BLACK_ON_WHITE;
DrawMode_t invertedDrawMode = DrawMode_t::WHITE_ON_BLACK;

void init()
{
  Logger::log<Logger::Level::DEBUG, Logger::Topic::DISP>("Initializing display...\n");
  if (framebuffer)
  {
    Logger::log<Logger::Level::DEBUG, Logger::Topic::DISP>("Display already initialized\n");
    return;
  }
  epd_init();
  // Allocate framebuffer
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (!framebuffer)
  {
    Logger::log<Logger::Level::ERROR, Logger::Topic::DISP>("Failed to allocate framebuffer memory\n");
  } else {
    Logger::log<Logger::Level::DEBUG, Logger::Topic::DISP>("Framebuffer allocated: {} bytes\n", BOARD_MAX_PAGE_BUFFER_SIZE);
  }
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  // clear();
}

uint16_t getResolutionX(){
  return DISPLAY_RESOLUTION_X;
};
uint16_t getResolutionY(){
  return DISPLAY_RESOLUTION_Y;
};

uint16_t getWidth(){
  return getResolutionX();
};
uint16_t getHeight(){
  return getResolutionY();
};

void powerOnAndInit()
{
  init();
  epd_clear();
  Board::setEPaperPowerOn(true);
  delay(500);

}

void clear()
{
  Logger::log<Logger::Level::DEBUG, Logger::Topic::DISP>("Clearing display...\n");

  epd_clear();
  delay(100);
  // Disable power supply for ePaper
  Board::setEPaperPowerOn(false);

  Logger::log<Logger::Level::DEBUG, Logger::Topic::DISP>("Display cleared.\n");
}

void drawPixel(int xCord, int yCord, Color color) { epd_draw_pixel(xCord, yCord, static_cast<uint8_t>(color), framebuffer); }

void drawQrCode(const char *qrStr, int qrSize, int yCord, int xCord, byte qrSizeMulti)
{
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(qrSize)];
  qrcode_initText(&qrcode, qrcodeData, qrSize, ECC_LOW, qrStr);

  int qrSizeTemp = (4 * qrSize) + 17;
  // QR Code Starting Point
  int offset_x = xCord - (qrSizeTemp * 2);
  int offset_y = yCord - (qrSizeTemp * 2);

  for (int y = 0; y < qrcode.size; y++)
  {
    for (int x = 0; x < qrcode.size; x++)
    {
      int newX = offset_x + (x * qrSizeMulti);
      int newY = offset_y + (y * qrSizeMulti);

      epd_fill_rect(newX, newY, qrSizeMulti, qrSizeMulti, qrcode_getModule(&qrcode, x, y) ? static_cast<uint8_t>(Color::Black) : static_cast<uint8_t>(Color::White), framebuffer);
    }
  }
}

void setTextPos(GFXfont *font, const String &text, int xCord, int yCord)
{
  int x = 0, y = 0, x1, y1, w, h;
  get_text_bounds(font, text.c_str(), 0, 0, &x1, &y1, &w, &h, &defaultFontProps);
  int cursor_x = xCord;
  int cursor_y = yCord + (h / 2);
  writeln(font,text.c_str(), &cursor_x, &cursor_y, framebuffer);
  Logger::log<Logger::Level::DEBUG, Logger::Topic::DISP>("Text '{}' drawn at ({}, {})\n", text.c_str(), cursor_x, cursor_y);
}

void centeredText(const GFXfont *font, const String &text, int xCord, int yCord, DrawMode_t mode, FontProperties *fontProps)
{
  int x = 0, y = 0, x1, y1, w, h;
  get_text_bounds(font, text.c_str(), &x, &y, &x1, &y1, &w, &h, fontProps);
  int cursor_x = xCord - (w / 2);
  int cursor_y = yCord + (h / 2);
  write_mode(font, text.c_str(), &cursor_x, &cursor_y, framebuffer, mode, fontProps);
  Logger::log<Logger::Level::DEBUG, Logger::Topic::DISP>("Centered text '{}' drawn at ({}, {})\n", text.c_str(), cursor_x, cursor_y);

}

Rect_t setToFullWindow()
{
  directStreamingPartialRefresh = false;
  return epd_full_screen();
}

Rect_t setToPartialWindow(int xCord, int yCord, int width, int height)
{
  directStreamingPartialRefresh = true;
  Rect_t area = {xCord, yCord, width, height};
  return area;
}

bool supportsPartialRefresh()
{
  // Note: For grayscale displays, partial refresh will use BW mode (reduced quality but no flash)
  return true;
}

void setToFirstPage() {}

bool setToNextPage() { return true; }

// Busy callback for light sleep during display refresh
void busyCallbackLightSleep(const void *)
{
  // Enter light sleep for short periods while display is refreshing
  // Wake up after 100ms to check BUSY status again
  esp_sleep_enable_timer_wakeup(100 * 1000);
  esp_light_sleep_start();
}

void enableLightSleepDuringRefresh(bool enable)
{
  if (enable)
  {
    Logger::log<Logger::Level::DEBUG, Logger::Topic::DISP>("Enabling light sleep during display refresh\n");
    // display.epd2.setBusyCallback(busyCallbackLightSleep, nullptr);
  }
  else
  {
    // display.epd2.setBusyCallback(nullptr, nullptr);
  }
}

///////////////////////////////////////////////
// Direct Streaming Functions
///////////////////////////////////////////////

bool supportsDirectStreaming()
{
  // All display types now support direct streaming
  // BW, Grayscale, 3C, 4C, and 7C displays support the setPaged()/writeNative()/refresh() API
  return true;
}

void initDirectStreaming(bool partialRefresh, uint16_t maxRowCount)
{
  Logger::log<Logger::Level::DEBUG, Logger::Topic::DISP>("Initializing direct streaming mode\n");
  if (partialRefresh)
  {
    Logger::log<Logger::Level::DEBUG, Logger::Topic::DISP>("Partial refresh mode requested\n");
  }

  // Initialize display
  // When partialRefresh is true, we pass initial=false to the library
  // This sets _initial_refresh=false, allowing partial refresh to work
  // (otherwise the library forces a full refresh on first update)

  // init();

  // Set partial refresh flag based on parameter
  directStreamingPartialRefresh = partialRefresh;

  // Set full window for direct writes
  // FIXME
  // display.setFullWindow();
}

void writeRowsDirect(uint16_t yStart, uint16_t rowCount, const uint8_t *blackData, const uint8_t *colorData)
{
  if (!blackData || rowCount == 0)
    return;

  if (directStreamingPartialRefresh)
  {
    // For partial refresh: convert 2bpp grayscale to 1bpp BW for fast refresh
    size_t requiredSize = ((DISPLAY_RESOLUTION_X + 7) / 8) * rowCount;

    if (bwConversionBuffer.canFit(requiredSize))
    {
      Logger::log<Logger::Level::DEBUG, Logger::Topic::DISP>(
        "Direct streaming: Converted {} rows of grayscale to BW for partial refresh\n", rowCount);
      PixelPacker::convertGrayscaleToBW(blackData, bwConversionBuffer.data, DISPLAY_RESOLUTION_X, rowCount);
      epd_draw_grayscale_image(setToPartialWindow(0, yStart, DISPLAY_RESOLUTION_X, rowCount), bwConversionBuffer.data);
    }
    else
    {
      Logger::log<Logger::Level::ERROR, Logger::Topic::DISP>("BW conversion buffer not available or too small\n");
      // Fallback: draw original grayscale data (will flash)
      epd_draw_grayscale_image(setToPartialWindow(0, yStart, DISPLAY_RESOLUTION_X, rowCount), (uint8_t *)blackData);
    }
  }
  else
  {
    Logger::log<Logger::Level::DEBUG, Logger::Topic::DISP>(
      "Direct streaming: Wrote {} rows of grayscale data for full refresh\n", rowCount);
    epd_draw_grayscale_image(setToFullWindow(), (uint8_t *)blackData);
  }
}

void finishDirectStreaming()
{
  if (directStreamingPartialRefresh)
  {
    Logger::log<Logger::Level::DEBUG, Logger::Topic::DISP>("Finishing direct streaming with PARTIAL refresh\n");
    // refresh(true);
  }
  else
  {
    Logger::log<Logger::Level::DEBUG, Logger::Topic::DISP>("Finishing direct streaming with FULL refresh\n");
    // refresh(false);
  }

  bwConversionBuffer.release();
}

//FIXME: Remove when epd_driver.h is fully integrated
void refreshDisplay() {
  epd_draw_grayscale_image(epd_full_screen(), framebuffer);
}

void showNoWiFiError(uint64_t sleepSeconds, const String &wikiUrl)
{
  powerOnAndInit();

    epd_fill_rect(0, 0, DISPLAY_RESOLUTION_X, DISPLAY_RESOLUTION_Y, static_cast<uint8_t>(Color::White), framebuffer);

    centeredText(font20px, "Cannot connect to Wi-Fi", DISPLAY_RESOLUTION_X / 2, DISPLAY_RESOLUTION_Y / 2 - 30, defaultDrawMode, &defaultFontProps);
    centeredText(font16px, "Retries in " + String((sleepSeconds + 30) / 60) + " minutes.", DISPLAY_RESOLUTION_X / 2,
                 DISPLAY_RESOLUTION_Y / 2 + 15, defaultDrawMode, &defaultFontProps);
    centeredText(font14px, "Docs: " + wikiUrl, DISPLAY_RESOLUTION_X / 2, DISPLAY_RESOLUTION_Y - 20, defaultDrawMode, &defaultFontProps);

    delay(100);
    refreshDisplay();
  // Disable power supply for ePaper
  Board::setEPaperPowerOn(false);
}

void showWiFiError(const String &hostname, const String &password, const String &urlWeb, const String &wikiUrl)
{
  /*
    QR code hint
    Common format: WIFI:S:<SSID>;T:<WEP|WPA|nopass>;P:<PASSWORD>;H:<true|false|blank>;;
    Sample: WIFI:S:MySSID;T:WPA;P:MyPassW0rd;;
  */
  const String qrString = "WIFI:S:" + hostname + ";T:WPA;P:" + password + ";;";
  // Logger::log<Logger::Level::DEBUG, Logger::Topic::WIFI>("Generated string: {}\n", qrString);

  powerOnAndInit();
      epd_fill_rect(0, 0, DISPLAY_RESOLUTION_X, 90, static_cast<uint8_t>(Color::Black), framebuffer);
      centeredText(font20px, "No Wi-Fi configured OR connection lost", DISPLAY_RESOLUTION_X / 2, 25, invertedDrawMode, &invertedFontProps);
      centeredText(font18px, "Retries in a few minutes if lost.", DISPLAY_RESOLUTION_X / 2, 65, invertedDrawMode, &invertedFontProps);
      centeredText(font16px, "To setup or change Wi-Fi configuration", DISPLAY_RESOLUTION_X / 2, 120, defaultDrawMode, &defaultFontProps);
      centeredText(font16px, "(with mobile data turned off):", DISPLAY_RESOLUTION_X / 2, 155, defaultDrawMode, &defaultFontProps);
      centeredText(font16px, "1) Connect to this AP:", DISPLAY_RESOLUTION_X / 4, (DISPLAY_RESOLUTION_Y / 2) - 60, defaultDrawMode, &defaultFontProps);
      centeredText(font16px, "2) Open in web browser:", DISPLAY_RESOLUTION_X * 3 / 4, (DISPLAY_RESOLUTION_Y / 2) - 60, defaultDrawMode, &defaultFontProps);

      drawQrCode(qrString.c_str(), 4, (DISPLAY_RESOLUTION_Y / 2) + 40, DISPLAY_RESOLUTION_X / 4, 4);
      
      epd_draw_line(DISPLAY_RESOLUTION_X / 2 - 1, (DISPLAY_RESOLUTION_Y / 2) - 60, DISPLAY_RESOLUTION_X / 2 - 1,
                       (DISPLAY_RESOLUTION_Y / 2) + 170, static_cast<uint8_t>(Color::Black), framebuffer);
      epd_draw_line(DISPLAY_RESOLUTION_X / 2, (DISPLAY_RESOLUTION_Y / 2) - 60, DISPLAY_RESOLUTION_X / 2,
                       (DISPLAY_RESOLUTION_Y / 2) + 170, static_cast<uint8_t>(Color::Black), framebuffer);
      drawQrCode(urlWeb.c_str(), 4, (DISPLAY_RESOLUTION_Y / 2) + 40, DISPLAY_RESOLUTION_X * 3 / 4, 4);

      centeredText(font16px, "SSID: " + hostname, DISPLAY_RESOLUTION_X / 4, (DISPLAY_RESOLUTION_Y / 2) + 130, defaultDrawMode, &defaultFontProps);
      centeredText(font16px, "Password: " + password, DISPLAY_RESOLUTION_X / 4, (DISPLAY_RESOLUTION_Y / 2) + 165, defaultDrawMode, &defaultFontProps);
      centeredText(font16px, urlWeb, DISPLAY_RESOLUTION_X * 3 / 4, (DISPLAY_RESOLUTION_Y / 2) + 130, defaultDrawMode, &defaultFontProps);
      epd_fill_rect(0, DISPLAY_RESOLUTION_Y - 40, DISPLAY_RESOLUTION_X, DISPLAY_RESOLUTION_Y, static_cast<uint8_t>(Color::Black), framebuffer);
      centeredText(font18px, "Documentation: " + wikiUrl, DISPLAY_RESOLUTION_X / 2, DISPLAY_RESOLUTION_Y - 22, invertedDrawMode, &invertedFontProps);
  delay(100);
  refreshDisplay();
  // Disable power supply for ePaper
  Board::setEPaperPowerOn(false);
}
} // namespace Display
