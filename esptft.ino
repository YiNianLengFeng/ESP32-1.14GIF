/*
  ESP32-S3 N8R8 + ST7789 1.14" TFT animation uploader/player

  Arduino libraries needed:
  - Adafruit GFX Library
  - Adafruit ST7735 and ST7789 Library
  - AnimatedGIF by Larry Bank

  Board notes:
  - Use the included custom partition table with the gifdata partition.
  - The web page uploads the original GIF directly.
  - Most 1.14" SPI IPS modules use ST7789 with native 135x240 pixels.
*/

#include "EsptftApp.h"

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);
AnimatedGIF gif;
WebServer server(80);

uint16_t lineBuf[TFT_W];
bool gifOpen = false;
int16_t gifOffsetX = 0;
int16_t gifOffsetY = 0;
String lastMessage;
size_t uploadBytes = 0;
PlaybackMode playbackMode = PLAY_NONE;
bool wifiApActive = false;
bool uploadInProgress = false;
bool wifiShutdownPending = false;
bool playbackAfterWifiShutdownPending = false;
uint32_t wifiShutdownAt = 0;
uint32_t wifiUploadRemainingMs = 0;
uint32_t wifiCountdownLastMs = 0;
bool lowBattery = false;
uint32_t batteryMilliVolts = 0;
uint8_t tftBacklightBrightness = TFT_BACKLIGHT_DEFAULT_BRIGHTNESS;

const char *resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_EXT: return "external reset";
    case ESP_RST_SW: return "software reset";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: return "interrupt watchdog";
    case ESP_RST_TASK_WDT: return "task watchdog";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep sleep wake";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_SDIO: return "SDIO reset";
    case ESP_RST_USB: return "USB reset";
    case ESP_RST_JTAG: return "JTAG reset";
    case ESP_RST_EFUSE: return "efuse error";
    case ESP_RST_PWR_GLITCH: return "power glitch";
    case ESP_RST_CPU_LOCKUP: return "CPU lockup";
    default: return "unknown";
  }
}

void startSavedPlayback() {
  if (temporaryGifIsActive() || gifStorageHasValidGif()) {
    openGifForPlayback();
  } else {
    showMessage("No animation");
  }
}

void stopWiFiAp() {
  if (!wifiApActive) {
    return;
  }
  server.stop();
  WiFi.softAPdisconnect(true);
  wifiApActive = false;
  wifiShutdownPending = false;
  playbackAfterWifiShutdownPending = false;
  wifiShutdownAt = 0;
  wifiUploadRemainingMs = 0;
  wifiCountdownLastMs = 0;
  lastMessage = "wifi off";
}

void beginUploadWindow() {
  startWebServer();
  wifiApActive = true;
  uploadInProgress = false;
  wifiShutdownPending = false;
  playbackAfterWifiShutdownPending = false;
  wifiShutdownAt = 0;
  wifiUploadRemainingMs = WIFI_UPLOAD_WINDOW_MS;
  wifiCountdownLastMs = millis();

  IPAddress ip = WiFi.softAPIP();
  Serial.print("AP SSID: ");
  Serial.println(AP_SSID);
  Serial.print("AP IP: ");
  Serial.println(ip);

  showUploadWindowPrompt();
}

void requestWifiShutdownAfterUpload() {
  disableLoopWDT();
  wifiShutdownPending = true;
  playbackAfterWifiShutdownPending = true;
  wifiShutdownAt = millis();
  Serial.println("Upload complete; WiFi shutdown scheduled");
}

void handleUploadWindow() {
  if (!wifiApActive) {
    return;
  }

  uint32_t now = millis();

  if (wifiShutdownPending) {
    if (static_cast<int32_t>(now - wifiShutdownAt) >= 0) {
      bool startPlayback = playbackAfterWifiShutdownPending;
      if (WIFI_SHUTDOWN_AFTER_UPLOAD_MS > 0) {
        delay(WIFI_SHUTDOWN_AFTER_UPLOAD_MS);
      }
      stopWiFiAp();
      if (startPlayback) {
        Serial.println("WiFi off; starting saved animation");
        startSavedPlayback();
      }
    }
    return;
  }

  if (uploadInProgress) {
    wifiCountdownLastMs = now;
    return;
  }

  if (WiFi.softAPgetStationNum() > 0) {
    wifiCountdownLastMs = now;
    return;
  }

  uint32_t elapsedMs = now - wifiCountdownLastMs;
  wifiCountdownLastMs = now;

  if (elapsedMs < wifiUploadRemainingMs) {
    wifiUploadRemainingMs -= elapsedMs;
    return;
  }

  stopWiFiAp();
  if (playbackMode == PLAY_NONE) {
    startSavedPlayback();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  esp_reset_reason_t resetReason = esp_reset_reason();
  Serial.printf("Reset reason: %d (%s)\n",
                static_cast<int>(resetReason),
                resetReasonName(resetReason));
  Serial.printf("PSRAM total/free: %u/%u bytes\n",
                static_cast<unsigned>(ESP.getPsramSize()),
                static_cast<unsigned>(ESP.getFreePsram()));

  initDisplay();
  initBatteryMonitor();
  showMessage("Booting...");

  if (!initGifStorage()) {
    showMessage("GIF storage failed");
    while (true) {
      delay(1000);
    }
  }
  disableLoopWDT();
  loadAppSettings();

  gif.begin(LITTLE_ENDIAN_PIXELS);
  if (ENABLE_WIFI_UPLOAD_WINDOW) {
    beginUploadWindow();
  } else {
    WiFi.mode(WIFI_OFF);
    startSavedPlayback();
  }
}

void loop() {
  updateBatteryMonitor();

  if (wifiApActive) {
    server.handleClient();
    handleUploadWindow();
  }

  handleFirmwareRestart();

  if (playbackMode != PLAY_GIF) {
    delay(2);
    return;
  }

  int frameDelayMs = 0;
  if (!gif.playFrame(true, &frameDelayMs)) {
    gif.reset();
  }
}
