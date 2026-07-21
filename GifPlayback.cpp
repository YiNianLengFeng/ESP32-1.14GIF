#include "EsptftApp.h"

void *gifOpenFile(const char *filename, int32_t *size) {
  (void)filename;
  if (temporaryGifIsActive()) {
    *size = static_cast<int32_t>(temporaryGifSize());
    return reinterpret_cast<void *>(1);
  }
  if (!gifStorageHasValidGif()) {
    return nullptr;
  }
  *size = static_cast<int32_t>(gifStorageGifSize());
  return reinterpret_cast<void *>(1);
}

void gifCloseFile(void *handle) {
  (void)handle;
}

int32_t gifReadFile(GIFFILE *file, uint8_t *buffer, int32_t length) {
  int32_t remaining = file->iSize - file->iPos;
  if (length > remaining) {
    length = remaining;
  }
  if (length <= 0) {
    return 0;
  }

  bool readOk = temporaryGifIsActive() ?
                temporaryGifRead(file->iPos, buffer, length) :
                gifStorageRead(file->iPos, buffer, length);
  if (!readOk) {
    return 0;
  }
  file->iPos += length;
  return length;
}

int32_t gifSeekFile(GIFFILE *file, int32_t position) {
  if (position < 0) {
    position = 0;
  } else if (position > file->iSize) {
    position = file->iSize;
  }
  file->iPos = position;
  return file->iPos;
}

void gifDraw(GIFDRAW *draw) {
  int screenX = draw->iX + gifOffsetX;
  const int screenY = draw->iY + draw->y + gifOffsetY;

  if (screenY < 0 || screenY >= TFT_H || screenX >= TFT_W) {
    return;
  }

  int width = draw->iWidth;
  uint8_t *src = draw->pPixels;

  if (screenX < 0) {
    const int skip = -screenX;
    if (skip >= width) {
      return;
    }
    src += skip;
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

  if (draw->ucDisposalMethod == 2) {
    for (int x = 0; x < width; x++) {
      if (src[x] == draw->ucTransparent) {
        src[x] = draw->ucBackground;
      }
    }
    draw->ucHasTransparency = 0;
  }

  if (draw->ucHasTransparency) {
    int x = 0;
    while (x < width) {
      while (x < width && src[x] == draw->ucTransparent) {
        x++;
      }

      int runStart = x;
      while (x < width && src[x] != draw->ucTransparent) {
        lineBuf[x - runStart] = palette[src[x]];
        x++;
      }

      int runWidth = x - runStart;
      if (runWidth > 0) {
        tft.drawRGBBitmap(screenX + runStart, screenY, lineBuf, runWidth, 1);
      }
    }
  } else {
    for (int x = 0; x < width; x++) {
      lineBuf[x] = palette[src[x]];
    }
    tft.drawRGBBitmap(screenX, screenY, lineBuf, width, 1);
  }
}

void closeGif() {
  if (gifOpen) {
    gif.close();
    gifOpen = false;
  }
  gifOffsetX = 0;
  gifOffsetY = 0;
}

bool openGifForPlayback() {
  closePlayback();

  if (!temporaryGifIsActive() && !gifStorageHasValidGif()) {
    return false;
  }

  gifOpen = gif.open("gifdata", gifOpenFile, gifCloseFile, gifReadFile, gifSeekFile, gifDraw);

  if (!gifOpen) {
    showMessage("GIF open failed");
  } else {
    const int canvasW = gif.getCanvasWidth();
    const int canvasH = gif.getCanvasHeight();
    gifOffsetX = centeredOffset(TFT_W, canvasW);
    gifOffsetY = centeredOffset(TFT_H, canvasH);
    tft.fillScreen(ST77XX_BLACK);
    playbackMode = PLAY_GIF;
  }
  return gifOpen;
}
