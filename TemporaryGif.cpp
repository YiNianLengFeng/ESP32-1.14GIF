#include "EsptftApp.h"

#include <cstring>

namespace {
constexpr size_t TEMPORARY_PSRAM_RESERVE_BYTES = 128UL * 1024UL;

uint8_t *temporaryBuffer = nullptr;
size_t temporarySize = 0;
size_t temporaryWrittenBytes = 0;
bool temporaryValid = false;
bool temporaryActive = false;
char temporaryName[MAX_GIF_FILENAME_BYTES] = {};
uint32_t temporaryVersionNumber = 0;

int temporaryResponseCode = 500;
String temporaryResponse = "{\"ok\":false,\"message\":\"upload not started\"}";
size_t temporaryExpectedBytes = 0;
bool temporaryUploadFailed = false;
uint32_t temporaryStartedAt = 0;

bool validGifSignature() {
  return temporaryBuffer != nullptr && temporarySize >= 6 &&
         (std::memcmp(temporaryBuffer, "GIF87a", 6) == 0 ||
          std::memcmp(temporaryBuffer, "GIF89a", 6) == 0);
}

void setTemporaryUploadError(const char *message, int responseCode) {
  temporaryGifDelete();
  uploadInProgress = false;
  temporaryUploadFailed = true;
  temporaryResponseCode = responseCode;
  temporaryResponse = String("{\"ok\":false,\"message\":\"") + message + "\"}";
  lastMessage = message;
  showMessage("RAM upload failed", message);
}

void beginTemporaryUpload() {
  uploadInProgress = true;
  closePlayback();
  temporaryGifDelete();
  temporaryExpectedBytes = static_cast<size_t>(server.clientContentLength());
  temporaryWrittenBytes = 0;
  temporaryUploadFailed = false;
  temporaryStartedAt = millis();
  temporaryResponseCode = 500;
  temporaryResponse = "{\"ok\":false,\"message\":\"upload incomplete\"}";

  String uploadName = WebServer::urlDecode(server.header("X-Gif-Name"));
  if (uploadName.length() == 0) {
    uploadName = "temporary.gif";
  }
  if (uploadName.length() >= MAX_GIF_FILENAME_BYTES || temporaryExpectedBytes == 0) {
    setTemporaryUploadError("invalid temporary GIF", 400);
    return;
  }

  size_t maxUploadBytes = temporaryGifMaxUploadBytes();
  if (temporaryExpectedBytes > maxUploadBytes) {
    setTemporaryUploadError("not enough PSRAM", 507);
    temporaryResponse = String("{\"ok\":false,\"message\":\"not enough PSRAM\"") +
                        ",\"fileBytes\":" + static_cast<unsigned long>(temporaryExpectedBytes) +
                        ",\"freeBytes\":" + static_cast<unsigned long>(maxUploadBytes) + "}";
    return;
  }

  temporaryBuffer = static_cast<uint8_t *>(
      heap_caps_malloc(temporaryExpectedBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (temporaryBuffer == nullptr) {
    setTemporaryUploadError("PSRAM allocation failed", 507);
    return;
  }

  std::strcpy(temporaryName, uploadName.c_str());
  temporarySize = temporaryExpectedBytes;
  lastMessage = "uploading temporary GIF";
  showMessage("Uploading to PSRAM...", temporaryName);
  Serial.printf("Temporary GIF upload: %u bytes, PSRAM free %u\n",
                static_cast<unsigned>(temporaryExpectedBytes),
                static_cast<unsigned>(ESP.getFreePsram()));
}

void writeTemporaryChunk(HTTPRaw &raw) {
  if (temporaryUploadFailed || temporaryBuffer == nullptr || raw.currentSize == 0) {
    return;
  }
  if (temporaryWrittenBytes + raw.currentSize > temporaryExpectedBytes) {
    setTemporaryUploadError("temporary GIF too large", 413);
    return;
  }
  std::memcpy(temporaryBuffer + temporaryWrittenBytes, raw.buf, raw.currentSize);
  temporaryWrittenBytes += raw.currentSize;
  delay(1);
}

void finishTemporaryUpload() {
  uploadInProgress = false;
  if (temporaryUploadFailed) {
    return;
  }
  if (temporaryWrittenBytes != temporaryExpectedBytes || !validGifSignature()) {
    setTemporaryUploadError("invalid GIF file", 400);
    return;
  }

  temporaryValid = true;
  temporaryActive = true;
  temporaryVersionNumber++;
  if (temporaryVersionNumber == 0) {
    temporaryVersionNumber = 1;
  }
  uint32_t elapsedMs = millis() - temporaryStartedAt;
  temporaryResponseCode = 200;
  temporaryResponse = String("{\"ok\":true,\"bytes\":") + temporarySize +
                      ",\"elapsedMs\":" + elapsedMs + "}";
  lastMessage = "temporary GIF ready";
  Serial.printf("Temporary GIF ready: %u bytes in %u ms\n",
                static_cast<unsigned>(temporarySize), static_cast<unsigned>(elapsedMs));
  openGifForPlayback();
}

void abortTemporaryUpload() {
  setTemporaryUploadError("temporary upload aborted", 400);
}
}  // namespace

bool temporaryGifHasFile() {
  return temporaryValid && temporaryBuffer != nullptr;
}

bool temporaryGifIsActive() {
  return temporaryGifHasFile() && temporaryActive;
}

bool temporaryGifActivate() {
  if (!temporaryGifHasFile()) {
    return false;
  }
  temporaryActive = true;
  return true;
}

void temporaryGifDeactivate() {
  temporaryActive = false;
}

void temporaryGifDelete() {
  if (temporaryBuffer != nullptr) {
    heap_caps_free(temporaryBuffer);
    temporaryBuffer = nullptr;
  }
  temporarySize = 0;
  temporaryWrittenBytes = 0;
  temporaryValid = false;
  temporaryActive = false;
  temporaryName[0] = '\0';
}

size_t temporaryGifSize() {
  return temporaryGifHasFile() ? temporarySize : 0;
}

size_t temporaryGifMaxUploadBytes() {
  size_t largestBlock = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return largestBlock > TEMPORARY_PSRAM_RESERVE_BYTES ?
         largestBlock - TEMPORARY_PSRAM_RESERVE_BYTES : 0;
}

uint32_t temporaryGifVersion() {
  return temporaryVersionNumber;
}

const char *temporaryGifName() {
  return temporaryGifHasFile() ? temporaryName : "";
}

bool temporaryGifRead(size_t offset, void *buffer, size_t length) {
  if (!temporaryGifHasFile() || buffer == nullptr || offset > temporarySize ||
      length > temporarySize - offset) {
    return false;
  }
  std::memcpy(buffer, temporaryBuffer + offset, length);
  return true;
}

void handleTemporaryGifUpload() {
  HTTPRaw &raw = server.raw();
  if (raw.status == RAW_START) {
    beginTemporaryUpload();
  } else if (raw.status == RAW_WRITE) {
    writeTemporaryChunk(raw);
  } else if (raw.status == RAW_END) {
    finishTemporaryUpload();
  } else if (raw.status == RAW_ABORTED) {
    abortTemporaryUpload();
  }
}

void handleTemporaryGifUploadFinish() {
  server.send(temporaryResponseCode, "application/json", temporaryResponse);
}
