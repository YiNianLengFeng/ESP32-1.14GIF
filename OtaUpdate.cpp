#include "EsptftApp.h"

#include <Update.h>

namespace {
int firmwareResponseCode = 500;
String firmwareResponse = "{\"ok\":false,\"message\":\"update not started\"}";
size_t firmwareExpectedBytes = 0;
size_t firmwareWrittenBytes = 0;
size_t firmwareNextProgressLog = 256UL * 1024UL;
uint32_t firmwareStartedAt = 0;
bool firmwareUploadFailed = false;
bool firmwareRestartPending = false;
uint32_t firmwareRestartAt = 0;

void setFirmwareError(const char *message, int responseCode) {
  if (Update.isRunning()) {
    Update.abort();
  }
  uploadInProgress = false;
  firmwareUploadFailed = true;
  firmwareResponseCode = responseCode;
  firmwareResponse = String("{\"ok\":false,\"message\":\"") + message + "\"}";
  lastMessage = message;
  showMessage("OTA failed", message);
  Serial.printf("OTA failed: %s\n", message);
}

void beginFirmwareUpload() {
  closePlayback();
  uploadInProgress = true;
  firmwareExpectedBytes = static_cast<size_t>(server.clientContentLength());
  firmwareWrittenBytes = 0;
  firmwareNextProgressLog = 256UL * 1024UL;
  firmwareStartedAt = millis();
  firmwareUploadFailed = false;
  firmwareRestartPending = false;
  firmwareResponseCode = 500;
  firmwareResponse = "{\"ok\":false,\"message\":\"update incomplete\"}";

  if (firmwareExpectedBytes == 0) {
    setFirmwareError("invalid firmware size", 400);
    return;
  }

  disableLoopWDT();
  if (!Update.begin(firmwareExpectedBytes, U_FLASH)) {
    setFirmwareError(Update.errorString(), 400);
    return;
  }

  lastMessage = "updating firmware";
  showMessage("Updating firmware...", "Do not power off");
  Serial.printf("OTA start: %u bytes\n", static_cast<unsigned>(firmwareExpectedBytes));
}

void writeFirmwareChunk(HTTPRaw &raw) {
  if (firmwareUploadFailed || raw.currentSize == 0) {
    return;
  }
  if (firmwareWrittenBytes + raw.currentSize > firmwareExpectedBytes) {
    setFirmwareError("firmware too large", 413);
    return;
  }

  size_t bytesWritten = Update.write(raw.buf, raw.currentSize);
  firmwareWrittenBytes += bytesWritten;
  if (bytesWritten != raw.currentSize) {
    setFirmwareError(Update.errorString(), 500);
    return;
  }

  if (firmwareWrittenBytes >= firmwareNextProgressLog) {
    Serial.printf("OTA progress: %u/%u bytes\n",
                  static_cast<unsigned>(firmwareWrittenBytes),
                  static_cast<unsigned>(firmwareExpectedBytes));
    firmwareNextProgressLog += 256UL * 1024UL;
  }
  delay(1);
}

void finishFirmwareUpload() {
  uploadInProgress = false;
  if (firmwareUploadFailed) {
    return;
  }
  if (firmwareWrittenBytes != firmwareExpectedBytes) {
    setFirmwareError("incomplete firmware", 400);
    return;
  }
  if (!Update.end(false)) {
    setFirmwareError(Update.errorString(), 500);
    return;
  }

  uint32_t elapsedMs = millis() - firmwareStartedAt;
  firmwareResponseCode = 200;
  firmwareResponse = String("{\"ok\":true,\"bytes\":") + firmwareWrittenBytes +
                     ",\"elapsedMs\":" + elapsedMs + "}";
  lastMessage = "firmware update ok";
  showMessage("OTA complete", "Restarting...");
  Serial.printf("OTA complete: %u bytes in %u ms\n",
                static_cast<unsigned>(firmwareWrittenBytes),
                static_cast<unsigned>(elapsedMs));
}

void abortFirmwareUpload() {
  setFirmwareError("firmware upload aborted", 400);
}
}  // namespace

void handleFirmwareUpload() {
  HTTPRaw &raw = server.raw();
  if (raw.status == RAW_START) {
    beginFirmwareUpload();
  } else if (raw.status == RAW_WRITE) {
    writeFirmwareChunk(raw);
  } else if (raw.status == RAW_END) {
    finishFirmwareUpload();
  } else if (raw.status == RAW_ABORTED) {
    abortFirmwareUpload();
  }
}

void handleFirmwareUploadFinish() {
  server.send(firmwareResponseCode, "application/json", firmwareResponse);
  if (firmwareResponseCode == 200) {
    firmwareRestartPending = true;
    firmwareRestartAt = millis() + 1000;
  }
}

void handleFirmwareRestart() {
  if (!firmwareRestartPending ||
      static_cast<int32_t>(millis() - firmwareRestartAt) < 0) {
    return;
  }
  firmwareRestartPending = false;
  Serial.println("Restarting into updated firmware");
  Serial.flush();
  delay(20);
  ESP.restart();
}
