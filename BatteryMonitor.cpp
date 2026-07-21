#include "EsptftApp.h"

static uint32_t lastBatterySampleAt = 0;
static uint32_t lastLowBatteryBlinkAt = 0;
static bool lowBatteryLedOn = false;

static void writeLowBatteryLed(bool on) {
  lowBatteryLedOn = on;
  const uint8_t activeLevel = LOW_BATTERY_LED_ACTIVE_HIGH ? HIGH : LOW;
  const uint8_t inactiveLevel = LOW_BATTERY_LED_ACTIVE_HIGH ? LOW : HIGH;
  digitalWrite(LOW_BATTERY_LED_PIN, on ? activeLevel : inactiveLevel);
}

void initBatteryMonitor() {
  pinMode(LOW_BATTERY_LED_PIN, OUTPUT);
  writeLowBatteryLed(false);

  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  batteryMilliVolts = readBatteryMilliVolts();
  lowBattery = batteryMilliVolts > 0 && batteryMilliVolts <= LOW_BATTERY_MV;
}

uint32_t readBatteryMilliVolts() {
  uint32_t pinMilliVolts = analogReadMilliVolts(BATTERY_ADC_PIN);
  return static_cast<uint32_t>(pinMilliVolts * BATTERY_VOLTAGE_SCALE);
}

void updateBatteryMonitor() {
  uint32_t now = millis();

  if (lastBatterySampleAt == 0 ||
      static_cast<int32_t>(now - lastBatterySampleAt) >= static_cast<int32_t>(BATTERY_SAMPLE_INTERVAL_MS)) {
    lastBatterySampleAt = now;
    batteryMilliVolts = readBatteryMilliVolts();

    if (!lowBattery && batteryMilliVolts > 0 && batteryMilliVolts <= LOW_BATTERY_MV) {
      lowBattery = true;
      lastLowBatteryBlinkAt = 0;
    } else if (lowBattery && batteryMilliVolts >= LOW_BATTERY_RECOVER_MV) {
      lowBattery = false;
      writeLowBatteryLed(false);
    }
  }

  if (!lowBattery) {
    return;
  }

  if (lastLowBatteryBlinkAt == 0 ||
      static_cast<int32_t>(now - lastLowBatteryBlinkAt) >= static_cast<int32_t>(LOW_BATTERY_BLINK_INTERVAL_MS)) {
    lastLowBatteryBlinkAt = now;
    writeLowBatteryLed(!lowBatteryLedOn);
  }
}
