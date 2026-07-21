#include "EsptftApp.h"

#include <algorithm>

namespace {
AnimatedGIF thumbnailGif;
uint32_t thumbnailFileId = 0;
GifStorageFileInfo thumbnailFileInfo = {};
uint16_t *thumbnailFrameBuffer = nullptr;
int16_t thumbnailOffsetX = 0;
int16_t thumbnailOffsetY = 0;

void *thumbnailOpen(const char *filename, int32_t *size) {
  (void)filename;
  if (thumbnailFileId == 0 && temporaryGifHasFile()) {
    *size = static_cast<int32_t>(temporaryGifSize());
    return reinterpret_cast<void *>(1);
  }
  if (!gifStorageGetFileById(thumbnailFileId, &thumbnailFileInfo)) {
    return nullptr;
  }
  *size = static_cast<int32_t>(thumbnailFileInfo.size);
  return reinterpret_cast<void *>(1);
}

void thumbnailClose(void *handle) {
  (void)handle;
}

int32_t thumbnailRead(GIFFILE *file, uint8_t *buffer, int32_t length) {
  int32_t remaining = file->iSize - file->iPos;
  if (length > remaining) {
    length = remaining;
  }
  if (length <= 0) {
    return 0;
  }
  bool readOk = thumbnailFileId == 0 ?
                temporaryGifRead(file->iPos, buffer, static_cast<size_t>(length)) :
                gifStorageReadFile(thumbnailFileId, file->iPos, buffer, static_cast<size_t>(length));
  if (!readOk) {
    return 0;
  }
  file->iPos += length;
  return length;
}

int32_t thumbnailSeek(GIFFILE *file, int32_t position) {
  if (position < 0) {
    position = 0;
  } else if (position > file->iSize) {
    position = file->iSize;
  }
  file->iPos = position;
  return position;
}

void thumbnailDraw(GIFDRAW *draw) {
  int screenX = draw->iX + thumbnailOffsetX;
  int screenY = draw->iY + draw->y + thumbnailOffsetY;
  if (screenY < 0 || screenY >= TFT_H || screenX >= TFT_W) {
    return;
  }

  int width = draw->iWidth;
  uint8_t *source = draw->pPixels;
  if (screenX < 0) {
    int skip = -screenX;
    if (skip >= width) {
      return;
    }
    source += skip;
    width -= skip;
    screenX = 0;
  }
  if (screenX + width > TFT_W) {
    width = TFT_W - screenX;
  }
  if (width <= 0) {
    return;
  }

  uint16_t *palette = draw->pPalette;
  for (int x = 0; x < width; x++) {
    uint8_t colorIndex = source[x];
    if (!draw->ucHasTransparency || colorIndex != draw->ucTransparent) {
      thumbnailFrameBuffer[screenY * TFT_W + screenX + x] = palette[colorIndex];
    }
  }
}
}  // namespace

bool renderGifFirstFrame(uint32_t id, uint16_t *frameBuffer) {
  bool sourceExists = id == 0 ? temporaryGifHasFile() :
                      gifStorageGetFileById(id, &thumbnailFileInfo);
  if (frameBuffer == nullptr || !sourceExists) {
    return false;
  }

  thumbnailFileId = id;
  thumbnailFrameBuffer = frameBuffer;
  std::fill_n(frameBuffer, TFT_W * TFT_H, static_cast<uint16_t>(ST77XX_BLACK));

  thumbnailGif.begin(LITTLE_ENDIAN_PIXELS);
  if (!thumbnailGif.open(
          "thumbnail", thumbnailOpen, thumbnailClose, thumbnailRead, thumbnailSeek, thumbnailDraw)) {
    thumbnailFrameBuffer = nullptr;
    return false;
  }

  thumbnailOffsetX = centeredOffset(TFT_W, thumbnailGif.getCanvasWidth());
  thumbnailOffsetY = centeredOffset(TFT_H, thumbnailGif.getCanvasHeight());
  int frameDelayMs = 0;
  bool decoded = thumbnailGif.playFrame(false, &frameDelayMs);
  thumbnailGif.close();
  thumbnailFrameBuffer = nullptr;
  return decoded;
}
