#include "EsptftApp.h"

void showMessage(const char *line1, const char *line2) {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(4, 8);
  tft.print(line1);
  if (line2 != nullptr) {
    tft.setCursor(4, 22);
    tft.print(line2);
  }
}

void applyBacklightBrightness() {
  if (TFT_BL < 0) {
    return;
  }
  ledcWrite(TFT_BL, TFT_BACKLIGHT_ON ? tftBacklightBrightness : 0);
}

void setBacklightBrightness(uint8_t brightness, bool saveToFlash) {
  tftBacklightBrightness = brightness;
  applyBacklightBrightness();
  if (saveToFlash) {
    saveAppSettings();
  }
}

void loadAppSettings() {
  Preferences preferences;
  if (!preferences.begin("esptft", true)) {
    applyBacklightBrightness();
    return;
  }
  uint8_t brightness = preferences.getUChar("brightness", TFT_BACKLIGHT_DEFAULT_BRIGHTNESS);
  preferences.end();
  setBacklightBrightness(brightness, false);
}

void saveAppSettings() {
  Preferences preferences;
  if (!preferences.begin("esptft", false)) {
    return;
  }
  preferences.putUChar("brightness", tftBacklightBrightness);
  preferences.end();
}

void showUploadWindowPrompt() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(2);
  tft.setTextWrap(false);

  tft.setCursor(8, 12);
  tft.print("WiFi Upload");
  tft.setCursor(8, 50);
  tft.print(AP_SSID);
  tft.setCursor(8, 88);
  tft.print("192.168.4.1");
}

int16_t centeredOffset(int16_t viewSize, int16_t contentSize) {
  if (contentSize <= viewSize) {
    return (viewSize - contentSize) / 2;
  }
  return -((contentSize - viewSize) / 2);
}

void showTestPattern() {
  closePlayback();
  tft.fillScreen(ST77XX_BLACK);
  tft.drawRect(0, 0, TFT_W, TFT_H, ST77XX_WHITE);
  tft.drawRect(1, 1, TFT_W - 2, TFT_H - 2, ST77XX_YELLOW);

  const int sw = (TFT_W - 16) / 3;
  const int sh = (TFT_H - 44) / 2;
  tft.fillRect(4, 4, sw, sh, ST77XX_RED);
  tft.fillRect(8 + sw, 4, sw, sh, ST77XX_GREEN);
  tft.fillRect(12 + sw * 2, 4, sw, sh, ST77XX_BLUE);
  tft.fillRect(4, 8 + sh, sw, sh, ST77XX_CYAN);
  tft.fillRect(8 + sw, 8 + sh, sw, sh, ST77XX_MAGENTA);
  tft.fillRect(12 + sw * 2, 8 + sh, sw, sh, ST77XX_YELLOW);

  tft.setTextColor(ST77XX_WHITE, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setCursor(6, TFT_H - 20);
  tft.print("R G B 240x135");
  tft.setCursor(6, TFT_H - 10);
  tft.print("Edge box OK?");
  lastMessage = "test pattern";
}

void initDisplay() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

  if (TFT_BL >= 0) {
    ledcAttach(TFT_BL, TFT_BACKLIGHT_PWM_HZ, TFT_BACKLIGHT_PWM_BITS);
    applyBacklightBrightness();
  }

  tft.init(TFT_NATIVE_W, TFT_NATIVE_H);
  tft.setSPISpeed(TFT_SPI_HZ);
  tft.setRotation(TFT_ROTATION);
  tft.invertDisplay(TFT_INVERT);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);
}
