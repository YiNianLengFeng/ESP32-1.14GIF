#include "EsptftApp.h"

#include <cstddef>
#include <cstring>

namespace {
constexpr uint32_t LEGACY_STORAGE_MAGIC = 0x31464947;  // "GIF1"
constexpr uint32_t DIRECTORY_MAGIC = 0x32444647;       // "GFD2"
constexpr uint32_t DIRECTORY_VERSION = 2;
constexpr size_t FLASH_SECTOR_BYTES = 0x1000;
constexpr size_t FLASH_ERASE_CHUNK_BYTES = 64UL * 1024UL;
constexpr char DIRECTORY_NAMESPACE[] = "gifstore";
constexpr char DIRECTORY_KEY[] = "directory";

struct LegacyGifMetadata {
  uint32_t magic;
  uint32_t version;
  uint32_t length;
  uint32_t crc32;
  uint32_t reserved[4];
};

struct GifDirectoryEntry {
  uint32_t id;
  uint32_t offset;
  uint32_t length;
  uint32_t allocatedBytes;
  uint32_t crc32;
  char name[MAX_GIF_FILENAME_BYTES];
};

struct GifDirectory {
  uint32_t magic;
  uint32_t version;
  uint32_t nextId;
  uint32_t activeId;
  uint32_t count;
  uint32_t reserved[3];
  GifDirectoryEntry entries[MAX_GIF_FILES];
  uint32_t crc32;
};

const esp_partition_t *gifPartition = nullptr;
GifDirectory directory = {};
bool uploadActive = false;
uint32_t uploadOffset = 0;
uint32_t uploadAllocatedBytes = 0;
size_t uploadExpectedBytes = 0;
size_t uploadWrittenBytes = 0;
uint32_t uploadCrc32 = 0xFFFFFFFF;
char uploadFileName[MAX_GIF_FILENAME_BYTES] = {};
const char *lastStorageError = "none";

uint32_t updateCrc32(uint32_t crc, const uint8_t *data, size_t length) {
  while (length-- > 0) {
    crc ^= *data++;
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1UL)));
    }
  }
  return crc;
}

uint32_t calculateDirectoryCrc(const GifDirectory &value) {
  return ~updateCrc32(
      0xFFFFFFFF, reinterpret_cast<const uint8_t *>(&value), offsetof(GifDirectory, crc32));
}

uint32_t alignToSector(size_t value) {
  return static_cast<uint32_t>(
      (value + FLASH_SECTOR_BYTES - 1) & ~(FLASH_SECTOR_BYTES - 1));
}

bool isGifSignature(const uint8_t signature[6]) {
  return std::memcmp(signature, "GIF87a", 6) == 0 ||
         std::memcmp(signature, "GIF89a", 6) == 0;
}

bool entryHasGifSignature(const GifDirectoryEntry &entry) {
  uint8_t signature[6];
  return gifPartition != nullptr && entry.length >= sizeof(signature) &&
         esp_partition_read(gifPartition, entry.offset, signature, sizeof(signature)) == ESP_OK &&
         isGifSignature(signature);
}

GifDirectoryEntry *findEntry(uint32_t id) {
  for (size_t index = 0; index < directory.count; index++) {
    if (directory.entries[index].id == id) {
      return &directory.entries[index];
    }
  }
  return nullptr;
}

const GifDirectoryEntry *activeEntry() {
  return findEntry(directory.activeId);
}

bool validFileName(const char *fileName) {
  if (fileName == nullptr) {
    return false;
  }
  size_t length = std::strlen(fileName);
  if (length == 0 || length >= MAX_GIF_FILENAME_BYTES) {
    return false;
  }
  for (size_t index = 0; index < length; index++) {
    if (static_cast<uint8_t>(fileName[index]) < 0x20) {
      return false;
    }
  }
  return true;
}

bool fileNameExists(const char *fileName) {
  for (size_t index = 0; index < directory.count; index++) {
    if (std::strcmp(directory.entries[index].name, fileName) == 0) {
      return true;
    }
  }
  return false;
}

bool rangesOverlap(uint32_t firstOffset, uint32_t firstLength,
                   uint32_t secondOffset, uint32_t secondLength) {
  return firstOffset < secondOffset + secondLength && secondOffset < firstOffset + firstLength;
}

bool directoryIsValid(const GifDirectory &value) {
  if (value.magic != DIRECTORY_MAGIC || value.version != DIRECTORY_VERSION ||
      value.count > MAX_GIF_FILES || value.nextId == 0 ||
      value.crc32 != calculateDirectoryCrc(value)) {
    return false;
  }

  for (size_t index = 0; index < value.count; index++) {
    const GifDirectoryEntry &entry = value.entries[index];
    if (entry.id == 0 || entry.length == 0 || entry.allocatedBytes < entry.length ||
        entry.offset < GIF_DATA_OFFSET || entry.offset % FLASH_SECTOR_BYTES != 0 ||
        entry.allocatedBytes % FLASH_SECTOR_BYTES != 0 ||
        entry.offset > gifPartition->size ||
        entry.allocatedBytes > gifPartition->size - entry.offset ||
        std::memchr(entry.name, '\0', sizeof(entry.name)) == nullptr) {
      return false;
    }
    for (size_t other = index + 1; other < value.count; other++) {
      const GifDirectoryEntry &candidate = value.entries[other];
      if (entry.id == candidate.id ||
          rangesOverlap(entry.offset, entry.allocatedBytes,
                        candidate.offset, candidate.allocatedBytes)) {
        return false;
      }
    }
  }
  return true;
}

bool saveDirectory() {
  directory.magic = DIRECTORY_MAGIC;
  directory.version = DIRECTORY_VERSION;
  if (directory.nextId == 0) {
    directory.nextId = 1;
  }
  directory.crc32 = calculateDirectoryCrc(directory);

  Preferences preferences;
  if (!preferences.begin(DIRECTORY_NAMESPACE, false)) {
    lastStorageError = "directory open failed";
    return false;
  }
  size_t bytesWritten = preferences.putBytes(DIRECTORY_KEY, &directory, sizeof(directory));
  preferences.end();
  if (bytesWritten != sizeof(directory)) {
    lastStorageError = "directory write failed";
    return false;
  }
  return true;
}

bool loadDirectory() {
  Preferences preferences;
  if (!preferences.begin(DIRECTORY_NAMESPACE, true)) {
    return false;
  }
  bool loaded = preferences.getBytesLength(DIRECTORY_KEY) == sizeof(directory) &&
                preferences.getBytes(DIRECTORY_KEY, &directory, sizeof(directory)) == sizeof(directory);
  preferences.end();
  return loaded && directoryIsValid(directory);
}

bool migrateLegacyGif() {
  LegacyGifMetadata legacy = {};
  if (esp_partition_read(gifPartition, 0, &legacy, sizeof(legacy)) != ESP_OK ||
      legacy.magic != LEGACY_STORAGE_MAGIC || legacy.version != 1 || legacy.length == 0 ||
      legacy.length > gifPartition->size - GIF_DATA_OFFSET) {
    return false;
  }

  GifDirectoryEntry &entry = directory.entries[0];
  entry.id = 1;
  entry.offset = GIF_DATA_OFFSET;
  entry.length = legacy.length;
  entry.allocatedBytes = alignToSector(legacy.length);
  entry.crc32 = legacy.crc32;
  std::strcpy(entry.name, "animation.gif");
  if (!entryHasGifSignature(entry)) {
    std::memset(&directory, 0, sizeof(directory));
    return false;
  }

  directory.count = 1;
  directory.activeId = entry.id;
  directory.nextId = 2;
  Serial.printf("Migrating existing GIF: %u bytes\n", static_cast<unsigned>(entry.length));
  if (!saveDirectory()) {
    return false;
  }
  esp_partition_erase_range(gifPartition, 0, FLASH_SECTOR_BYTES);
  return true;
}

void initializeEmptyDirectory() {
  std::memset(&directory, 0, sizeof(directory));
  directory.magic = DIRECTORY_MAGIC;
  directory.version = DIRECTORY_VERSION;
  directory.nextId = 1;
}

void sortEntryIndexesByOffset(size_t indexes[MAX_GIF_FILES]) {
  for (size_t index = 0; index < directory.count; index++) {
    indexes[index] = index;
  }
  for (size_t index = 1; index < directory.count; index++) {
    size_t value = indexes[index];
    size_t position = index;
    while (position > 0 &&
           directory.entries[indexes[position - 1]].offset > directory.entries[value].offset) {
      indexes[position] = indexes[position - 1];
      position--;
    }
    indexes[position] = value;
  }
}

bool findFreeRange(uint32_t requiredBytes, uint32_t *resultOffset) {
  size_t indexes[MAX_GIF_FILES];
  sortEntryIndexesByOffset(indexes);
  uint32_t cursor = GIF_DATA_OFFSET;
  for (size_t index = 0; index < directory.count; index++) {
    const GifDirectoryEntry &entry = directory.entries[indexes[index]];
    if (entry.offset >= cursor && entry.offset - cursor >= requiredBytes) {
      *resultOffset = cursor;
      return true;
    }
    uint32_t entryEnd = entry.offset + entry.allocatedBytes;
    if (entryEnd > cursor) {
      cursor = entryEnd;
    }
  }
  if (cursor <= gifPartition->size && gifPartition->size - cursor >= requiredBytes) {
    *resultOffset = cursor;
    return true;
  }
  return false;
}

void resetUploadState() {
  uploadActive = false;
  uploadOffset = 0;
  uploadAllocatedBytes = 0;
  uploadExpectedBytes = 0;
  uploadWrittenBytes = 0;
  uploadCrc32 = 0xFFFFFFFF;
  uploadFileName[0] = '\0';
}
}  // namespace

bool initGifStorage() {
  gifPartition = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, GIF_PARTITION_LABEL);
  if (gifPartition == nullptr || gifPartition->size <= GIF_DATA_OFFSET) {
    Serial.println("GIF partition not found");
    return false;
  }

  Serial.printf("GIF partition: %u bytes at 0x%08x\n",
                static_cast<unsigned>(gifPartition->size),
                static_cast<unsigned>(gifPartition->address));

  if (!loadDirectory()) {
    initializeEmptyDirectory();
    if (!migrateLegacyGif()) {
      initializeEmptyDirectory();
      if (!saveDirectory()) {
        return false;
      }
    }
  }

  const GifDirectoryEntry *selectedEntry = activeEntry();
  if (directory.activeId != 0 &&
      (selectedEntry == nullptr || !entryHasGifSignature(*selectedEntry))) {
    directory.activeId = directory.count > 0 ? directory.entries[0].id : 0;
    saveDirectory();
  }
  return true;
}

bool gifStorageHasValidGif() {
  const GifDirectoryEntry *entry = activeEntry();
  return entry != nullptr && entryHasGifSignature(*entry);
}

size_t gifStorageCapacity() {
  return gifPartition != nullptr ? gifPartition->size - GIF_DATA_OFFSET : 0;
}

size_t gifStorageGifSize() {
  const GifDirectoryEntry *entry = activeEntry();
  return entry != nullptr ? entry->length : 0;
}

size_t gifStorageFreeBytes() {
  size_t usedBytes = 0;
  for (size_t index = 0; index < directory.count; index++) {
    usedBytes += directory.entries[index].allocatedBytes;
  }
  size_t capacity = gifStorageCapacity();
  return capacity > usedBytes ? capacity - usedBytes : 0;
}

size_t gifStorageMaxContiguousBytes() {
  if (gifPartition == nullptr) {
    return 0;
  }
  size_t indexes[MAX_GIF_FILES];
  sortEntryIndexesByOffset(indexes);
  uint32_t cursor = GIF_DATA_OFFSET;
  uint32_t largest = 0;
  for (size_t index = 0; index < directory.count; index++) {
    const GifDirectoryEntry &entry = directory.entries[indexes[index]];
    if (entry.offset > cursor && entry.offset - cursor > largest) {
      largest = entry.offset - cursor;
    }
    uint32_t entryEnd = entry.offset + entry.allocatedBytes;
    if (entryEnd > cursor) {
      cursor = entryEnd;
    }
  }
  if (gifPartition->size > cursor && gifPartition->size - cursor > largest) {
    largest = gifPartition->size - cursor;
  }
  return largest;
}

size_t gifStorageFileCount() {
  return directory.count;
}

bool gifStorageGetFile(size_t index, GifStorageFileInfo *info) {
  if (info == nullptr || index >= directory.count) {
    return false;
  }
  const GifDirectoryEntry &entry = directory.entries[index];
  info->id = entry.id;
  info->size = entry.length;
  info->allocatedBytes = entry.allocatedBytes;
  info->active = entry.id == directory.activeId;
  std::memcpy(info->name, entry.name, sizeof(info->name));
  info->name[sizeof(info->name) - 1] = '\0';
  return true;
}

bool gifStorageGetFileById(uint32_t id, GifStorageFileInfo *info) {
  GifDirectoryEntry *entry = findEntry(id);
  if (entry == nullptr || info == nullptr) {
    return false;
  }
  info->id = entry->id;
  info->size = entry->length;
  info->allocatedBytes = entry->allocatedBytes;
  info->active = entry->id == directory.activeId;
  std::memcpy(info->name, entry->name, sizeof(info->name));
  info->name[sizeof(info->name) - 1] = '\0';
  return true;
}

bool gifStorageBeginUpload(const char *fileName, size_t expectedBytes) {
  resetUploadState();
  if (gifPartition == nullptr || expectedBytes == 0 || expectedBytes > gifStorageCapacity()) {
    lastStorageError = "invalid file size";
    return false;
  }
  if (!validFileName(fileName)) {
    lastStorageError = "invalid file name";
    return false;
  }
  if (fileNameExists(fileName)) {
    lastStorageError = "file already exists";
    return false;
  }
  if (directory.count >= MAX_GIF_FILES) {
    lastStorageError = "too many files";
    return false;
  }

  uint32_t requiredBytes = alignToSector(expectedBytes);
  if (!findFreeRange(requiredBytes, &uploadOffset)) {
    lastStorageError = "not enough contiguous space";
    return false;
  }

  uint32_t eraseStartedAt = millis();
  for (size_t offset = 0; offset < requiredBytes; offset += FLASH_ERASE_CHUNK_BYTES) {
    size_t chunkBytes = requiredBytes - offset;
    if (chunkBytes > FLASH_ERASE_CHUNK_BYTES) {
      chunkBytes = FLASH_ERASE_CHUNK_BYTES;
    }
    esp_err_t error = esp_partition_erase_range(gifPartition, uploadOffset + offset, chunkBytes);
    if (error != ESP_OK) {
      Serial.printf("GIF erase failed at %u: %s\n",
                    static_cast<unsigned>(uploadOffset + offset), esp_err_to_name(error));
      lastStorageError = "flash erase failed";
      resetUploadState();
      return false;
    }
    delay(1);
  }

  uploadAllocatedBytes = requiredBytes;
  uploadExpectedBytes = expectedBytes;
  std::strcpy(uploadFileName, fileName);
  uploadActive = true;
  lastStorageError = "none";
  Serial.printf("GIF erase: %u bytes at %u in %u ms\n",
                static_cast<unsigned>(requiredBytes), static_cast<unsigned>(uploadOffset),
                static_cast<unsigned>(millis() - eraseStartedAt));
  return true;
}

bool gifStorageWrite(const uint8_t *data, size_t length) {
  if (!uploadActive || data == nullptr || length == 0 ||
      uploadWrittenBytes + length > uploadExpectedBytes) {
    lastStorageError = "invalid write";
    return false;
  }

  esp_err_t error = esp_partition_write(
      gifPartition, uploadOffset + uploadWrittenBytes, data, length);
  if (error != ESP_OK) {
    Serial.printf("GIF flash write failed at %u: %s\n",
                  static_cast<unsigned>(uploadWrittenBytes), esp_err_to_name(error));
    lastStorageError = "flash write failed";
    return false;
  }

  uploadCrc32 = updateCrc32(uploadCrc32, data, length);
  uploadWrittenBytes += length;
  return true;
}

bool gifStorageFinishUpload() {
  if (!uploadActive || uploadWrittenBytes != uploadExpectedBytes) {
    lastStorageError = "incomplete upload";
    return false;
  }

  uint8_t signature[6];
  if (esp_partition_read(gifPartition, uploadOffset, signature, sizeof(signature)) != ESP_OK ||
      !isGifSignature(signature)) {
    lastStorageError = "invalid GIF file";
    gifStorageAbortUpload();
    return false;
  }

  GifDirectory previousDirectory = directory;
  GifDirectoryEntry &entry = directory.entries[directory.count];
  std::memset(&entry, 0, sizeof(entry));
  entry.id = directory.nextId++;
  entry.offset = uploadOffset;
  entry.length = uploadExpectedBytes;
  entry.allocatedBytes = uploadAllocatedBytes;
  entry.crc32 = ~uploadCrc32;
  std::strcpy(entry.name, uploadFileName);
  directory.count++;
  directory.activeId = entry.id;

  if (!saveDirectory()) {
    directory = previousDirectory;
    resetUploadState();
    return false;
  }

  resetUploadState();
  lastStorageError = "none";
  return true;
}

void gifStorageAbortUpload() {
  resetUploadState();
}

bool gifStorageRead(size_t offset, void *buffer, size_t length) {
  const GifDirectoryEntry *entry = activeEntry();
  if (entry == nullptr || buffer == nullptr || offset > entry->length ||
      length > entry->length - offset) {
    return false;
  }
  return esp_partition_read(gifPartition, entry->offset + offset, buffer, length) == ESP_OK;
}

bool gifStorageReadFile(uint32_t id, size_t offset, void *buffer, size_t length) {
  GifDirectoryEntry *entry = findEntry(id);
  if (entry == nullptr || buffer == nullptr || offset > entry->length ||
      length > entry->length - offset) {
    return false;
  }
  return esp_partition_read(gifPartition, entry->offset + offset, buffer, length) == ESP_OK;
}

bool gifStorageSelectFile(uint32_t id) {
  GifDirectoryEntry *entry = findEntry(id);
  if (entry == nullptr || !entryHasGifSignature(*entry)) {
    lastStorageError = "file not found";
    return false;
  }
  uint32_t previousActiveId = directory.activeId;
  directory.activeId = id;
  if (!saveDirectory()) {
    directory.activeId = previousActiveId;
    return false;
  }
  lastStorageError = "none";
  return true;
}

bool gifStorageDeleteFile(uint32_t id) {
  size_t index = 0;
  while (index < directory.count && directory.entries[index].id != id) {
    index++;
  }
  if (index >= directory.count) {
    lastStorageError = "file not found";
    return false;
  }

  GifDirectory previousDirectory = directory;
  for (size_t next = index + 1; next < directory.count; next++) {
    directory.entries[next - 1] = directory.entries[next];
  }
  directory.count--;
  std::memset(&directory.entries[directory.count], 0, sizeof(GifDirectoryEntry));
  if (directory.activeId == id) {
    directory.activeId = directory.count > 0 ? directory.entries[0].id : 0;
  }

  if (!saveDirectory()) {
    directory = previousDirectory;
    return false;
  }
  lastStorageError = "none";
  return true;
}

const char *gifStorageLastError() {
  return lastStorageError;
}
