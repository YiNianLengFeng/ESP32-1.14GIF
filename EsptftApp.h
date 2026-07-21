#ifndef ESPTFT_APP_H
#define ESPTFT_APP_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <AnimatedGIF.h>
#include <esp_heap_caps.h>
#include <esp_partition.h>
#include <esp_system.h>
#include <Preferences.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>

// -------------------- Hardware config --------------------
static constexpr int TFT_MOSI = 11;
static constexpr int TFT_SCLK = 12;
static constexpr int TFT_CS   = 10;
static constexpr int TFT_DC   = 13;
static constexpr int TFT_RST  = 9;
static constexpr int TFT_BL   = 14;  // Set to -1 if your backlight is wired to 3V3.

static constexpr int TFT_W = 240;
static constexpr int TFT_H = 135;
static constexpr int TFT_NATIVE_W = 135;
static constexpr int TFT_NATIVE_H = 240;
static constexpr uint32_t TFT_SPI_HZ = 40000000;
static constexpr uint8_t TFT_ROTATION = 1;
static constexpr bool TFT_INVERT = true;
static constexpr bool TFT_BACKLIGHT_ON = true;
static constexpr uint32_t TFT_BACKLIGHT_PWM_HZ = 20000;
static constexpr uint8_t TFT_BACKLIGHT_PWM_BITS = 8;
static constexpr uint8_t TFT_BACKLIGHT_DEFAULT_BRIGHTNESS = 120;

// -------------------- Battery monitor config --------------------
static constexpr int BATTERY_ADC_PIN = 1;
static constexpr int LOW_BATTERY_LED_PIN = 2;
static constexpr bool LOW_BATTERY_LED_ACTIVE_HIGH = true;
static constexpr float BATTERY_VOLTAGE_SCALE = 2.0f;  // Set to 2.0 for a 1:1 voltage divider.
static constexpr uint32_t LOW_BATTERY_MV = 3400;
static constexpr uint32_t LOW_BATTERY_RECOVER_MV = 3600;
static constexpr uint32_t BATTERY_SAMPLE_INTERVAL_MS = 1000;
static constexpr uint32_t LOW_BATTERY_BLINK_INTERVAL_MS = 500;

// -------------------- Wi-Fi upload window config --------------------
static constexpr bool ENABLE_WIFI_UPLOAD_WINDOW = true;
static constexpr uint32_t WIFI_UPLOAD_WINDOW_MS = 20000;
static constexpr uint32_t WIFI_SHUTDOWN_AFTER_UPLOAD_MS = 100;
static const char *const AP_SSID = "ESP32S3-TFT";
static const char *const AP_PASS = "12345678";  // 8+ chars required by WPA2.

// -------------------- Storage config --------------------
static const char *const GIF_PARTITION_LABEL = "gifdata";
static constexpr size_t GIF_PARTITION_BYTES = 0x560000;
static constexpr size_t GIF_DATA_OFFSET = 0x1000;
static constexpr size_t MAX_UPLOAD_BYTES = GIF_PARTITION_BYTES - GIF_DATA_OFFSET;
static constexpr size_t MAX_GIF_FILES = 24;
static constexpr size_t MAX_GIF_FILENAME_BYTES = 64;
static constexpr size_t UPLOAD_WRITE_BUFFER_BYTES = 64UL * 1024UL;
static constexpr size_t UPLOAD_INTERNAL_WRITE_BUFFER_BYTES = 16UL * 1024UL;
static constexpr size_t FLASH_WRITE_CHUNK_BYTES = 16UL * 1024UL;

enum PlaybackMode {
  PLAY_NONE,
  PLAY_GIF
};

struct GifStorageFileInfo {
  uint32_t id;
  uint32_t size;
  uint32_t allocatedBytes;
  bool active;
  char name[MAX_GIF_FILENAME_BYTES];
};

extern Adafruit_ST7789 tft;
extern AnimatedGIF gif;
extern WebServer server;

extern uint16_t lineBuf[TFT_W];
extern bool gifOpen;
extern int16_t gifOffsetX;
extern int16_t gifOffsetY;
extern String lastMessage;
extern size_t uploadBytes;
extern PlaybackMode playbackMode;
extern bool wifiApActive;
extern bool uploadInProgress;
extern bool wifiShutdownPending;
extern bool playbackAfterWifiShutdownPending;
extern uint32_t wifiShutdownAt;
extern uint32_t wifiUploadRemainingMs;
extern uint32_t wifiCountdownLastMs;
extern bool lowBattery;
extern uint32_t batteryMilliVolts;
extern uint8_t tftBacklightBrightness;

void initDisplay();
void applyBacklightBrightness();
void setBacklightBrightness(uint8_t brightness, bool saveToFlash);
void loadAppSettings();
void saveAppSettings();
void showMessage(const char *line1, const char *line2 = nullptr);
void showUploadWindowPrompt();
void showTestPattern();
int16_t centeredOffset(int16_t viewSize, int16_t contentSize);

void startWebServer();
void handleFirmwareUpload();
void handleFirmwareUploadFinish();
void handleFirmwareRestart();
void beginUploadWindow();
void handleUploadWindow();
void requestWifiShutdownAfterUpload();
void stopWiFiAp();
void startSavedPlayback();

bool openGifForPlayback();
void closeGif();
void closePlayback();

bool initGifStorage();
bool gifStorageHasValidGif();
size_t gifStorageCapacity();
size_t gifStorageGifSize();
size_t gifStorageFreeBytes();
size_t gifStorageMaxContiguousBytes();
size_t gifStorageFileCount();
bool gifStorageGetFile(size_t index, GifStorageFileInfo *info);
bool gifStorageGetFileById(uint32_t id, GifStorageFileInfo *info);
bool gifStorageBeginUpload(const char *fileName, size_t expectedBytes);
bool gifStorageWrite(const uint8_t *data, size_t length);
bool gifStorageFinishUpload();
void gifStorageAbortUpload();
bool gifStorageRead(size_t offset, void *buffer, size_t length);
bool gifStorageReadFile(uint32_t id, size_t offset, void *buffer, size_t length);
bool gifStorageSelectFile(uint32_t id);
bool gifStorageDeleteFile(uint32_t id);
const char *gifStorageLastError();

bool renderGifFirstFrame(uint32_t id, uint16_t *frameBuffer);

bool temporaryGifHasFile();
bool temporaryGifIsActive();
bool temporaryGifActivate();
void temporaryGifDeactivate();
void temporaryGifDelete();
size_t temporaryGifSize();
size_t temporaryGifMaxUploadBytes();
uint32_t temporaryGifVersion();
const char *temporaryGifName();
bool temporaryGifRead(size_t offset, void *buffer, size_t length);
void handleTemporaryGifUpload();
void handleTemporaryGifUploadFinish();

void initBatteryMonitor();
void updateBatteryMonitor();
uint32_t readBatteryMilliVolts();

#endif
