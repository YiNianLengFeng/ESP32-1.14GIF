#include "EsptftApp.h"

#include <cstring>

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>ESP32-S3 TFT Uploader</title>
  <style>
    :root { color-scheme: light dark; font-family: system-ui, -apple-system, Segoe UI, sans-serif; }
    body { margin: 0; min-height: 100vh; background: #101418; color: #eef3f5; }
    main { width: min(92vw, 560px); margin: 0 auto; padding: 22px 0; }
    h1 { margin: 0 0 8px; font-size: 24px; }
    p { margin: 0 0 16px; color: #b9c5ca; line-height: 1.55; }
    .panel { display: grid; gap: 14px; padding: 16px; border: 1px solid #2e3a40; border-radius: 8px; background: #171d22; }
    label { display: grid; gap: 6px; color: #d8e1e5; font-size: 14px; }
    input[type=file] { width: 100%; padding: 10px; border: 1px solid #52626a; border-radius: 6px; box-sizing: border-box; background: #0f1418; color: #eef3f5; }
    input[type=range] { width: 100%; accent-color: #33c481; }
    .row { display: flex; justify-content: space-between; gap: 12px; align-items: center; }
    button { min-height: 42px; border: 0; border-radius: 6px; background: #33c481; color: #06120d; font-weight: 700; cursor: pointer; }
    button:disabled { opacity: .45; cursor: wait; }
    button.secondary { background: #2a92d4; color: white; }
    button.danger { background: #d9534f; color: white; }
    button:active { transform: translateY(1px); }
    .actions { display: grid; gap: 10px; }
    .storage { padding-top: 12px; border-top: 1px solid #2e3a40; color: #c8d3d8; font-size: 14px; line-height: 1.6; }
    .files { display: grid; border-top: 1px solid #2e3a40; }
    .file-row { display: grid; grid-template-columns: 120px minmax(0, 1fr) 28px; gap: 10px; align-items: center; min-height: 82px; border-bottom: 1px solid #2e3a40; }
    .thumb-button { width: 120px; height: 68px; min-height: 68px; padding: 0; overflow: hidden; background: #080b0d; border: 2px solid transparent; }
    .thumb-button.active { border-color: #33c481; }
    .thumb-button img { display: block; width: 120px; height: 68px; object-fit: cover; }
    .file-name { overflow-wrap: anywhere; color: #eef3f5; }
    .file-size { color: #9dabb1; font-size: 12px; }
    .empty { padding: 14px 0; color: #9dabb1; }
    input[type=radio], input[type=checkbox] { width: 18px; height: 18px; margin: 0; accent-color: #33c481; }
    .meta { margin-top: 14px; font-size: 14px; color: #a8b5bb; line-height: 1.7; overflow-wrap: anywhere; }
    a { color: #6dd6ff; }
  </style>
</head>
<body>
  <main>
    <h1>ESP32-S3 TFT &#21160;&#30011;&#19978;&#20256;</h1>
    <p>&#36873;&#25321; GIF&#65292;&#30452;&#25509;&#19978;&#20256;&#21407;&#22987; GIF &#26412;&#20307;&#21040;&#26495;&#23376;&#24182;&#33258;&#21160;&#25773;&#25918;&#12290;</p>
    <section class="panel">
      <label>GIF &#25991;&#20214;
        <input id="file" type="file" accept="image/gif" multiple required>
      </label>
      <label class="row">
        <span>&#20020;&#26102;&#25773;&#25918;&#65288;&#19981;&#20445;&#23384;&#65289;</span>
        <input id="temporary" type="checkbox">
      </label>
      <label>
        <span class="row"><span>&#23631;&#24149;&#20142;&#24230;</span><span id="brightnessValue">--</span></span>
        <input id="brightness" type="range" min="0" max="255" step="1">
      </label>
      <button id="upload" type="button">&#19978;&#20256;&#21407;&#22987; GIF</button>
      <div id="storage" class="storage">&#35835;&#21462;&#23384;&#20648;&#31354;&#38388;...</div>
      <div id="fileList" class="files"></div>
      <div class="actions">
        <button id="delete" class="danger" type="button">&#21024;&#38500;&#21246;&#36873;</button>
        <button id="exit" class="secondary" type="button">&#36864;&#20986;&#24182;&#20851;&#38381; WiFi</button>
      </div>
      <button id="test" class="secondary" type="button">&#26174;&#31034;&#27979;&#35797;&#30011;&#38754;</button>
      <div class="storage">&#22266;&#20214;&#26356;&#26032;</div>
      <label>ESP32 &#22266;&#20214; (.bin)
        <input id="firmware" type="file" accept=".bin,application/octet-stream">
      </label>
      <button id="updateFirmware" class="secondary" type="button">&#26356;&#26032;&#22266;&#20214;</button>
    </section>
    <div class="meta">
      <div>&#29366;&#24577;&#65306;<span id="status">&#35835;&#21462;&#20013;...</span></div>
      <div>&#25509;&#21475;&#65306;<a href="/status">/status</a></div>
    </div>
  </main>
  <script>
    const fileInput = document.getElementById('file');
    const statusEl = document.getElementById('status');
    const uploadBtn = document.getElementById('upload');
    const temporaryInput = document.getElementById('temporary');
    const deleteBtn = document.getElementById('delete');
    const exitBtn = document.getElementById('exit');
    const testBtn = document.getElementById('test');
    const firmwareInput = document.getElementById('firmware');
    const updateFirmwareBtn = document.getElementById('updateFirmware');
    const storageEl = document.getElementById('storage');
    const fileListEl = document.getElementById('fileList');
    const brightnessInput = document.getElementById('brightness');
    const brightnessValue = document.getElementById('brightnessValue');
    let brightnessTimer = 0;
    let currentStatus = null;

    function setStatus(text) { statusEl.textContent = text; }
    function formatBytes(bytes) {
      if (bytes >= 1048576) return `${(bytes / 1048576).toFixed(2)} MB`;
      if (bytes >= 1024) return `${(bytes / 1024).toFixed(1)} KB`;
      return `${bytes} B`;
    }
    function setBrightnessUi(value) {
      brightnessInput.value = value;
      brightnessValue.textContent = value;
    }
    function needFiles() {
      const files = Array.from(fileInput.files || []);
      if (!files.length) throw new Error('\u8bf7\u5148\u9009\u62e9 GIF \u6587\u4ef6');
      return files;
    }
    function postBlob(blob, path, label) {
      return new Promise((resolve, reject) => {
        const xhr = new XMLHttpRequest();
        const startedAt = performance.now();
        xhr.open('POST', path);
        xhr.setRequestHeader('Content-Type', 'application/octet-stream');
        xhr.setRequestHeader('X-Gif-Name', encodeURIComponent(blob.name));
        xhr.upload.onprogress = event => {
          if (!event.lengthComputable) return;
          const elapsedSeconds = Math.max((performance.now() - startedAt) / 1000, 0.001);
          const percent = Math.round(event.loaded * 100 / event.total);
          const speed = event.loaded / elapsedSeconds / 1024;
          setStatus(`${label}\uff1a${percent}%\uff0c${speed.toFixed(0)} KB/s`);
        };
        xhr.onerror = () => reject(new Error('\u7f51\u7edc\u4e2d\u65ad'));
        xhr.onload = async () => {
          if (xhr.status < 200 || xhr.status >= 300) {
            let message = xhr.responseText || `HTTP ${xhr.status}`;
            try {
              const error = JSON.parse(xhr.responseText);
              if (error.message && error.message.includes('space')) {
                const fileMb = (error.fileBytes / 1048576).toFixed(2);
                const freeMb = (error.freeBytes / 1048576).toFixed(2);
                const totalMb = (error.totalBytes / 1048576).toFixed(2);
                message = `\u7a7a\u95f4\u4e0d\u8db3\uff1a\u6587\u4ef6 ${fileMb} MB\uff0c\u5269\u4f59 ${freeMb} MB\uff0c\u603b\u5bb9\u91cf ${totalMb} MB`;
              } else if (error.message) {
                message = error.message;
              }
            } catch (_) {}
            reject(new Error(message));
            return;
          }
          let result = null;
          try { result = JSON.parse(xhr.responseText); } catch (_) {}
          if (result && result.speedKBps != null) {
            setStatus(`\u4e0a\u4f20\u5b8c\u6210\uff0c${result.speedKBps} KB/s\uff0c${result.elapsedMs} ms`);
          } else {
            setStatus('\u4e0a\u4f20\u5b8c\u6210');
          }
          resolve();
        };
        xhr.send(blob);
      });
    }
    async function sendBrightness(value, save) {
      const res = await fetch(`/brightness?value=${encodeURIComponent(value)}&save=${save ? 1 : 0}`, { method: 'POST' });
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      await refreshStatus();
    }
    function renderFiles(status) {
      storageEl.textContent = `\u5df2\u4fdd\u5b58 ${status.savedFileCount}/${status.maxFiles}\uff0c\u5df2\u7528 ${formatBytes(status.usedBytes)}\uff0c\u5269\u4f59 ${formatBytes(status.freeBytes)}\uff0c\u6700\u5927\u8fde\u7eed ${formatBytes(status.maxContiguousBytes)}`;
      fileListEl.replaceChildren();
      if (!status.files.length) {
        const empty = document.createElement('div');
        empty.className = 'empty';
        empty.textContent = '\u6682\u65e0 GIF \u6587\u4ef6';
        fileListEl.append(empty);
        return;
      }
      for (const file of status.files) {
        const row = document.createElement('div');
        row.className = 'file-row';

        const thumbnail = document.createElement('button');
        thumbnail.type = 'button';
        thumbnail.className = `thumb-button${file.active ? ' active' : ''}`;
        thumbnail.title = `\u64ad\u653e ${file.name}`;
        const image = document.createElement('img');
        image.src = `/thumbnail?id=${encodeURIComponent(file.id)}&v=${encodeURIComponent(file.thumbnailVersion || 0)}`;
        image.alt = file.name;
        image.loading = 'lazy';
        thumbnail.append(image);
        thumbnail.onclick = async () => {
          try {
            thumbnail.disabled = true;
            const response = await fetch(`/play?id=${encodeURIComponent(file.id)}`, { method: 'POST' });
            if (!response.ok) throw new Error('\u5207\u6362\u64ad\u653e\u5931\u8d25');
            await refreshStatus();
          } catch (error) {
            setStatus(error.message || String(error));
          } finally {
            thumbnail.disabled = false;
          }
        };

        const details = document.createElement('div');
        const name = document.createElement('div');
        name.className = 'file-name';
        name.textContent = file.name;
        const size = document.createElement('div');
        size.className = 'file-size';
        size.textContent = `${file.temporary ? '\u4e34\u65f6 \u00b7 ' : ''}${formatBytes(file.size)}${file.active ? ' \u00b7 \u5df2\u9009\u4e2d' : ''}`;
        details.append(name, size);

        const remove = document.createElement('input');
        remove.type = 'checkbox';
        remove.dataset.deleteId = file.id;
        remove.dataset.allocatedBytes = file.allocatedBytes;
        remove.title = '\u9009\u62e9\u5220\u9664';
        row.append(thumbnail, details, remove);
        fileListEl.append(row);
      }
    }
    async function refreshStatus() {
      const s = await fetch('/status').then(r => r.json());
      currentStatus = s;
      setBrightnessUi(s.backlightBrightness || 0);
      renderFiles(s);
      setStatus(`${s.files.length ? `\u5df2\u4fdd\u5b58 ${s.files.length} \u4e2a GIF` : '\u672a\u4e0a\u4f20 GIF'}\uff0c\u5269\u4f59 ${formatBytes(s.freeBytes)}`);
      return s;
    }
    brightnessInput.oninput = () => {
      const value = brightnessInput.value;
      brightnessValue.textContent = value;
      clearTimeout(brightnessTimer);
      brightnessTimer = setTimeout(() => {
        sendBrightness(value, false).catch(err => setStatus(err.message || String(err)));
      }, 150);
    };
    brightnessInput.onchange = () => {
      clearTimeout(brightnessTimer);
      sendBrightness(brightnessInput.value, true).catch(err => setStatus(err.message || String(err)));
    };
    uploadBtn.onclick = async () => {
      try {
        uploadBtn.disabled = true;
        const files = needFiles();
        const status = await refreshStatus();
        const encoder = new TextEncoder();
        if (temporaryInput.checked) {
          if (files.length != 1) {
            throw new Error('\u4e34\u65f6\u64ad\u653e\u6bcf\u6b21\u53ea\u80fd\u4e0a\u4f20\u4e00\u4e2a GIF');
          }
          const file = files[0];
          if (encoder.encode(file.name).length >= 64) {
            throw new Error(`\u6587\u4ef6\u540d\u8fc7\u957f\uff1a${file.name}`);
          }
          await postBlob(file, '/upload-gif-ram', `\u4e34\u65f6\u4e0a\u4f20 ${file.name}`);
          await refreshStatus();
          return;
        }
        const requiredBytes = files.reduce((sum, file) => sum + Math.ceil(file.size / 4096) * 4096, 0);
        if (status.files.length + files.length > status.maxFiles) {
          throw new Error(`\u6587\u4ef6\u6570\u8d85\u8fc7 ${status.maxFiles}\uff0c\u8bf7\u5148\u5220\u9664\u6587\u4ef6`);
        }
        if (requiredBytes > status.freeBytes) {
          throw new Error(`\u5269\u4f59\u7a7a\u95f4\u4e0d\u8db3\uff0c\u9700\u8981 ${formatBytes(requiredBytes)}\uff0c\u8bf7\u52fe\u9009\u6587\u4ef6\u5220\u9664`);
        }
        for (const file of files) {
          if (encoder.encode(file.name).length >= 64) {
            throw new Error(`\u6587\u4ef6\u540d\u8fc7\u957f\uff1a${file.name}`);
          }
          const latest = await refreshStatus();
          const allocated = Math.ceil(file.size / 4096) * 4096;
          if (allocated > latest.maxContiguousBytes) {
            throw new Error(`\u8fde\u7eed\u7a7a\u95f4\u4e0d\u8db3\uff0c\u8bf7\u52fe\u9009\u6587\u4ef6\u5220\u9664`);
          }
          const label = `\u4e0a\u4f20 ${file.name}`;
          await postBlob(file, '/upload-gif-raw', label);
        }
        await refreshStatus();
      } catch (err) {
        setStatus(err.message || String(err));
      } finally {
        uploadBtn.disabled = false;
      }
    };
    deleteBtn.onclick = async () => {
      try {
        const selected = Array.from(document.querySelectorAll('input[data-delete-id]:checked'));
        if (!selected.length) throw new Error('\u8bf7\u52fe\u9009\u8981\u5220\u9664\u7684 GIF');
        deleteBtn.disabled = true;
        for (const item of selected) {
          const res = await fetch(`/delete?id=${encodeURIComponent(item.dataset.deleteId)}`, { method: 'POST' });
          if (!res.ok) throw new Error('\u5220\u9664\u6587\u4ef6\u5931\u8d25');
        }
        await refreshStatus();
      } catch (err) {
        setStatus(err.message || String(err));
      } finally {
        deleteBtn.disabled = false;
      }
    };
    exitBtn.onclick = async () => {
      try {
        exitBtn.disabled = true;
        setStatus('\u6b63\u5728\u5173\u95ed WiFi...');
        const response = await fetch('/exit', { method: 'POST' });
        if (!response.ok) throw new Error('\u5173\u95ed WiFi \u5931\u8d25');
        setStatus('WiFi \u5373\u5c06\u5173\u95ed');
      } catch (error) {
        exitBtn.disabled = false;
        setStatus(error.message || String(error));
      }
    };
    updateFirmwareBtn.onclick = async () => {
      try {
        const firmware = firmwareInput.files && firmwareInput.files[0];
        if (!firmware) throw new Error('\u8bf7\u9009\u62e9 .bin \u56fa\u4ef6');
        if (!firmware.name.toLowerCase().endsWith('.bin')) {
          throw new Error('\u53ea\u80fd\u4e0a\u4f20 .bin \u56fa\u4ef6');
        }
        updateFirmwareBtn.disabled = true;
        uploadBtn.disabled = true;
        deleteBtn.disabled = true;
        exitBtn.disabled = true;
        await postBlob(firmware, '/update-firmware', '\u66f4\u65b0\u56fa\u4ef6');
        setStatus('\u56fa\u4ef6\u66f4\u65b0\u6210\u529f\uff0c\u8bbe\u5907\u6b63\u5728\u91cd\u542f');
      } catch (error) {
        updateFirmwareBtn.disabled = false;
        uploadBtn.disabled = false;
        deleteBtn.disabled = false;
        exitBtn.disabled = false;
        setStatus(error.message || String(error));
      }
    };
    testBtn.onclick = async () => {
      try {
        testBtn.disabled = true;
        await fetch('/test');
        await refreshStatus();
      } catch (err) {
        setStatus(err.message || String(err));
      } finally {
        testBtn.disabled = false;
      }
    };
    refreshStatus().catch(() => setStatus('\u65e0\u6cd5\u8bfb\u53d6\u72b6\u6001'));
  </script>
</body>
</html>
)HTML";

static int rawUploadResponseCode = 500;
static String rawUploadResponse = "{\"ok\":false,\"message\":\"upload not started\"}";
static size_t rawUploadExpectedBytes = 0;
static size_t rawUploadReceivedBytes = 0;
static uint32_t rawUploadStartedAt = 0;
static bool rawUploadFailed = false;
static uint8_t *rawUploadWriteBuffer = nullptr;
static size_t rawUploadWriteBufferUsed = 0;
static size_t rawUploadWriteBufferCapacity = 0;
static String rawUploadFileName;

static void releaseRawUploadWriteBuffer() {
  if (rawUploadWriteBuffer != nullptr) {
    heap_caps_free(rawUploadWriteBuffer);
    rawUploadWriteBuffer = nullptr;
  }
  rawUploadWriteBufferUsed = 0;
  rawUploadWriteBufferCapacity = 0;
}

static void setRawUploadError(const char *message, int responseCode) {
  releaseRawUploadWriteBuffer();
  gifStorageAbortUpload();
  rawUploadFailed = true;
  rawUploadResponseCode = responseCode;
  rawUploadResponse = String("{\"ok\":false,\"message\":\"") + message + "\"}";
  lastMessage = message;
  showMessage("Upload failed", message);
}

static void beginRawGifUpload() {
  uploadInProgress = true;
  closePlayback();
  temporaryGifDeactivate();
  releaseRawUploadWriteBuffer();

  uploadBytes = 0;
  rawUploadReceivedBytes = 0;
  rawUploadExpectedBytes = static_cast<size_t>(server.clientContentLength());
  rawUploadFileName = WebServer::urlDecode(server.header("X-Gif-Name"));
  if (rawUploadFileName.length() == 0) {
    rawUploadFileName = "animation.gif";
  }
  rawUploadStartedAt = 0;
  rawUploadFailed = false;
  rawUploadResponseCode = 500;
  rawUploadResponse = "{\"ok\":false,\"message\":\"upload incomplete\"}";

  size_t storageCapacity = gifStorageCapacity();
  if (rawUploadExpectedBytes == 0 || rawUploadExpectedBytes > MAX_UPLOAD_BYTES ||
      rawUploadExpectedBytes > storageCapacity) {
    setRawUploadError("invalid file size", 413);
    return;
  }

  showMessage("Preparing GIF...", "Please wait");
  if (!gifStorageBeginUpload(rawUploadFileName.c_str(), rawUploadExpectedBytes)) {
    String storageError = gifStorageLastError();
    int responseCode = storageError.indexOf("space") >= 0 ? 507 :
                       storageError == "file already exists" ? 409 :
                       storageError.indexOf("flash") >= 0 ? 500 : 400;
    setRawUploadError(storageError.c_str(), responseCode);
    if (responseCode == 507) {
      rawUploadResponse = String("{\"ok\":false,\"message\":\"") + storageError + "\"" +
                          ",\"fileBytes\":" + static_cast<unsigned long>(rawUploadExpectedBytes) +
                          ",\"freeBytes\":" + static_cast<unsigned long>(gifStorageFreeBytes()) +
                          ",\"maxContiguousBytes\":" +
                              static_cast<unsigned long>(gifStorageMaxContiguousBytes()) +
                          ",\"totalBytes\":" + static_cast<unsigned long>(storageCapacity) + "}";
    }
    return;
  }

  rawUploadWriteBuffer = static_cast<uint8_t *>(
      heap_caps_malloc(UPLOAD_WRITE_BUFFER_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (rawUploadWriteBuffer != nullptr) {
    rawUploadWriteBufferCapacity = UPLOAD_WRITE_BUFFER_BYTES;
  } else {
    rawUploadWriteBuffer = static_cast<uint8_t *>(
        heap_caps_malloc(UPLOAD_INTERNAL_WRITE_BUFFER_BYTES, MALLOC_CAP_8BIT));
    if (rawUploadWriteBuffer != nullptr) {
      rawUploadWriteBufferCapacity = UPLOAD_INTERNAL_WRITE_BUFFER_BYTES;
    }
  }
  if (rawUploadWriteBuffer == nullptr) {
    setRawUploadError("upload buffer failed", 500);
    return;
  }

  Serial.printf("Upload buffer: %u bytes, PSRAM free: %u bytes\n",
                static_cast<unsigned>(rawUploadWriteBufferCapacity),
                static_cast<unsigned>(ESP.getFreePsram()));

  rawUploadStartedAt = millis();
  lastMessage = "uploading raw";
  showMessage("Uploading GIF...", "Binary mode");
}

static bool flushRawUploadWriteBuffer() {
  if (rawUploadWriteBufferUsed == 0) {
    return true;
  }

  size_t bufferOffset = 0;
  while (bufferOffset < rawUploadWriteBufferUsed) {
    size_t chunkBytes = rawUploadWriteBufferUsed - bufferOffset;
    if (chunkBytes > FLASH_WRITE_CHUNK_BYTES) {
      chunkBytes = FLASH_WRITE_CHUNK_BYTES;
    }

    if (!gifStorageWrite(rawUploadWriteBuffer + bufferOffset, chunkBytes)) {
      Serial.printf("GIF partition write failed at %u bytes\n",
                    static_cast<unsigned>(uploadBytes));
      setRawUploadError("flash write failed", 507);
      return false;
    }

    uploadBytes += chunkBytes;
    bufferOffset += chunkBytes;
    delay(1);
  }

  rawUploadWriteBufferUsed = 0;
  return true;
}

static void writeRawGifChunk(HTTPRaw &raw) {
  if (rawUploadFailed || rawUploadWriteBuffer == nullptr || raw.currentSize == 0) {
    return;
  }

  if (rawUploadReceivedBytes + raw.currentSize > rawUploadExpectedBytes ||
      rawUploadReceivedBytes + raw.currentSize > MAX_UPLOAD_BYTES) {
    setRawUploadError("file too large", 413);
    return;
  }

  size_t sourceOffset = 0;
  while (sourceOffset < raw.currentSize) {
    size_t bufferSpace = rawUploadWriteBufferCapacity - rawUploadWriteBufferUsed;
    size_t copyBytes = raw.currentSize - sourceOffset;
    if (copyBytes > bufferSpace) {
      copyBytes = bufferSpace;
    }

    std::memcpy(rawUploadWriteBuffer + rawUploadWriteBufferUsed,
                raw.buf + sourceOffset,
                copyBytes);
    rawUploadWriteBufferUsed += copyBytes;
    rawUploadReceivedBytes += copyBytes;
    sourceOffset += copyBytes;

    if (rawUploadWriteBufferUsed == rawUploadWriteBufferCapacity && !flushRawUploadWriteBuffer()) {
      return;
    }
  }
}

static void finishRawGifUpload() {
  uploadInProgress = false;
  if (rawUploadFailed) {
    return;
  }

  if (!flushRawUploadWriteBuffer()) {
    return;
  }
  releaseRawUploadWriteBuffer();

  if (rawUploadReceivedBytes != rawUploadExpectedBytes || uploadBytes != rawUploadExpectedBytes) {
    setRawUploadError("incomplete upload", 400);
    return;
  }

  if (!gifStorageFinishUpload()) {
    String storageError = gifStorageLastError();
    int responseCode = storageError == "invalid GIF file" ? 400 : 500;
    setRawUploadError(storageError.c_str(), responseCode);
    return;
  }
  uint32_t elapsedMs = millis() - rawUploadStartedAt;
  if (elapsedMs == 0) {
    elapsedMs = 1;
  }
  uint32_t speedKbps = static_cast<uint32_t>((uploadBytes * 1000ULL) / elapsedMs / 1024ULL);

  rawUploadResponseCode = 200;
  rawUploadResponse = String("{\"ok\":true,\"bytes\":") + uploadBytes +
                      ",\"elapsedMs\":" + elapsedMs +
                      ",\"speedKBps\":" + speedKbps + "}";
  lastMessage = "upload ok";

  Serial.printf("GIF upload: %u bytes in %u ms (%u KB/s)\n",
                static_cast<unsigned>(uploadBytes),
                static_cast<unsigned>(elapsedMs),
                static_cast<unsigned>(speedKbps));

  showMessage("Upload OK", rawUploadFileName.c_str());
}

static void abortRawGifUpload() {
  releaseRawUploadWriteBuffer();
  gifStorageAbortUpload();
  uploadInProgress = false;
  rawUploadFailed = true;
  lastMessage = "upload aborted";
  showMessage("Upload aborted");
}

void handleRawGifUpload() {
  HTTPRaw &raw = server.raw();
  if (raw.status == RAW_START) {
    beginRawGifUpload();
  } else if (raw.status == RAW_WRITE) {
    writeRawGifChunk(raw);
  } else if (raw.status == RAW_END) {
    finishRawGifUpload();
  } else if (raw.status == RAW_ABORTED) {
    abortRawGifUpload();
  }
}

void handleRawUploadFinish() {
  server.send(rawUploadResponseCode, "application/json", rawUploadResponse);
}

void handleRoot() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.sendHeader("Pragma", "no-cache");
  server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void handleTest() {
  showTestPattern();
  server.send(200, "text/plain; charset=utf-8", "OK");
}

static void appendJsonEscaped(String &json, const char *value) {
  while (value != nullptr && *value != '\0') {
    char character = *value++;
    if (character == '"' || character == '\\') {
      json += '\\';
      json += character;
    } else if (static_cast<uint8_t>(character) >= 0x20) {
      json += character;
    }
  }
}

static void writeLittleEndian16(uint8_t *buffer, size_t offset, uint16_t value) {
  buffer[offset] = static_cast<uint8_t>(value);
  buffer[offset + 1] = static_cast<uint8_t>(value >> 8);
}

static void writeLittleEndian32(uint8_t *buffer, size_t offset, uint32_t value) {
  buffer[offset] = static_cast<uint8_t>(value);
  buffer[offset + 1] = static_cast<uint8_t>(value >> 8);
  buffer[offset + 2] = static_cast<uint8_t>(value >> 16);
  buffer[offset + 3] = static_cast<uint8_t>(value >> 24);
}

void handleThumbnail() {
  static constexpr uint16_t THUMBNAIL_W = 120;
  static constexpr uint16_t THUMBNAIL_H = 68;
  static constexpr size_t BMP_HEADER_BYTES = 54;
  static constexpr size_t BMP_ROW_BYTES = THUMBNAIL_W * 3;
  static constexpr size_t BMP_DATA_BYTES = BMP_ROW_BYTES * THUMBNAIL_H;

  if (!server.hasArg("id") || uploadInProgress) {
    server.send(400, "text/plain", "invalid request");
    return;
  }

  uint32_t id = static_cast<uint32_t>(server.arg("id").toInt());
  uint16_t *frameBuffer = static_cast<uint16_t *>(
      heap_caps_malloc(TFT_W * TFT_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (frameBuffer == nullptr) {
    frameBuffer = static_cast<uint16_t *>(
        heap_caps_malloc(TFT_W * TFT_H * sizeof(uint16_t), MALLOC_CAP_8BIT));
  }
  if (frameBuffer == nullptr) {
    server.send(503, "text/plain", "thumbnail buffer failed");
    return;
  }

  if (!renderGifFirstFrame(id, frameBuffer)) {
    heap_caps_free(frameBuffer);
    server.send(404, "text/plain", "thumbnail unavailable");
    return;
  }

  uint8_t header[BMP_HEADER_BYTES] = {};
  header[0] = 'B';
  header[1] = 'M';
  writeLittleEndian32(header, 2, BMP_HEADER_BYTES + BMP_DATA_BYTES);
  writeLittleEndian32(header, 10, BMP_HEADER_BYTES);
  writeLittleEndian32(header, 14, 40);
  writeLittleEndian32(header, 18, THUMBNAIL_W);
  writeLittleEndian32(header, 22, THUMBNAIL_H);
  writeLittleEndian16(header, 26, 1);
  writeLittleEndian16(header, 28, 24);
  writeLittleEndian32(header, 34, BMP_DATA_BYTES);

  server.sendHeader("Cache-Control", "public, max-age=31536000, immutable");
  server.setContentLength(BMP_HEADER_BYTES + BMP_DATA_BYTES);
  server.send(200, "image/bmp", "");
  server.setContentLength(CONTENT_LENGTH_NOT_SET);
  server.sendContent(reinterpret_cast<const char *>(header), sizeof(header));

  uint8_t row[BMP_ROW_BYTES];
  for (int destinationY = THUMBNAIL_H - 1; destinationY >= 0; destinationY--) {
    int sourceY = destinationY * TFT_H / THUMBNAIL_H;
    for (int destinationX = 0; destinationX < THUMBNAIL_W; destinationX++) {
      int sourceX = destinationX * TFT_W / THUMBNAIL_W;
      uint16_t color = frameBuffer[sourceY * TFT_W + sourceX];
      size_t output = destinationX * 3;
      row[output] = static_cast<uint8_t>((color & 0x1F) * 255 / 31);
      row[output + 1] = static_cast<uint8_t>(((color >> 5) & 0x3F) * 255 / 63);
      row[output + 2] = static_cast<uint8_t>(((color >> 11) & 0x1F) * 255 / 31);
    }
    server.sendContent(reinterpret_cast<const char *>(row), sizeof(row));
    if ((destinationY & 7) == 0) {
      delay(1);
    }
  }
  heap_caps_free(frameBuffer);
}

void handleStatus() {
  size_t total = gifStorageCapacity();
  bool gifExists = gifStorageHasValidGif();
  bool temporaryExists = temporaryGifHasFile();
  size_t gifSize = temporaryGifIsActive() ? temporaryGifSize() : gifStorageGifSize();
  size_t freeBytes = gifStorageFreeBytes();
  size_t used = total > freeBytes ? total - freeBytes : 0;

  String json = "{";
  json += "\"aniExists\":";
  json += "false";
  json += ",\"gifExists\":";
  json += gifExists ? "true" : "false";
  json += ",\"fileExists\":";
  json += (gifExists || temporaryExists) ? "true" : "false";
  json += ",\"aniSize\":";
  json += 0;
  json += ",\"gifSize\":";
  json += gifSize;
  json += ",\"totalBytes\":";
  json += total;
  json += ",\"usedBytes\":";
  json += used;
  json += ",\"freeBytes\":";
  json += freeBytes;
  json += ",\"maxContiguousBytes\":";
  json += gifStorageMaxContiguousBytes();
  json += ",\"maxFiles\":";
  json += MAX_GIF_FILES;
  json += ",\"savedFileCount\":";
  json += gifStorageFileCount();
  json += ",\"temporaryExists\":";
  json += temporaryExists ? "true" : "false";
  json += ",\"temporaryMaxBytes\":";
  json += temporaryGifMaxUploadBytes();
  json += ",\"psramTotal\":";
  json += ESP.getPsramSize();
  json += ",\"psramFree\":";
  json += ESP.getFreePsram();
  json += ",\"aniRamSize\":0";
  json += ",\"backlightBrightness\":";
  json += static_cast<int>(tftBacklightBrightness);
  json += ",\"batteryMilliVolts\":";
  json += batteryMilliVolts;
  json += ",\"lowBattery\":";
  json += lowBattery ? "true" : "false";
  json += ",\"mode\":\"";
  if (playbackMode == PLAY_GIF) {
    json += "gif";
  } else {
    json += "none";
  }
  json += "\"";
  json += ",\"message\":\"";
  json += lastMessage;
  json += "\",\"files\":[";
  bool firstFile = true;
  if (temporaryExists) {
    json += "{\"id\":0,\"name\":\"";
    appendJsonEscaped(json, temporaryGifName());
    json += "\",\"size\":";
    json += temporaryGifSize();
    json += ",\"allocatedBytes\":";
    json += temporaryGifSize();
    json += ",\"active\":";
    json += temporaryGifIsActive() ? "true" : "false";
    json += ",\"temporary\":true,\"thumbnailVersion\":";
    json += temporaryGifVersion();
    json += '}';
    firstFile = false;
  }
  for (size_t index = 0; index < gifStorageFileCount(); index++) {
    GifStorageFileInfo info = {};
    if (!gifStorageGetFile(index, &info)) {
      continue;
    }
    if (!firstFile) {
      json += ',';
    }
    json += "{\"id\":";
    json += info.id;
    json += ",\"name\":\"";
    appendJsonEscaped(json, info.name);
    json += "\",\"size\":";
    json += info.size;
    json += ",\"allocatedBytes\":";
    json += info.allocatedBytes;
    json += ",\"active\":";
    json += (!temporaryGifIsActive() && info.active) ? "true" : "false";
    json += ",\"temporary\":false,\"thumbnailVersion\":0";
    json += '}';
    firstFile = false;
  }
  json += "]}";

  server.send(200, "application/json", json);
}

void handlePlayFile() {
  if (!server.hasArg("id") || uploadInProgress) {
    server.send(400, "application/json", "{\"ok\":false,\"message\":\"invalid request\"}");
    return;
  }

  uint32_t id = static_cast<uint32_t>(server.arg("id").toInt());
  closePlayback();
  bool selected = false;
  if (id == 0) {
    selected = temporaryGifActivate();
  } else {
    temporaryGifDeactivate();
    selected = gifStorageSelectFile(id);
  }
  if (!selected || !openGifForPlayback()) {
    server.send(404, "application/json", "{\"ok\":false,\"message\":\"file not found\"}");
    return;
  }

  lastMessage = "playing selected GIF";
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleDeleteFile() {
  if (!server.hasArg("id") || uploadInProgress) {
    server.send(400, "application/json", "{\"ok\":false,\"message\":\"invalid request\"}");
    return;
  }

  uint32_t id = static_cast<uint32_t>(server.arg("id").toInt());
  closePlayback();
  if (id == 0) {
    temporaryGifDelete();
    lastMessage = "temporary GIF deleted";
    startSavedPlayback();
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  if (!gifStorageDeleteFile(id)) {
    startSavedPlayback();
    server.send(404, "application/json", "{\"ok\":false,\"message\":\"file not found\"}");
    return;
  }

  lastMessage = "file deleted";
  if (gifStorageHasValidGif()) {
    openGifForPlayback();
  } else {
    showMessage("No animation");
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleExitWifi() {
  if (uploadInProgress) {
    server.send(409, "application/json", "{\"ok\":false,\"message\":\"upload in progress\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
  requestWifiShutdownAfterUpload();
}

void handleBrightness() {
  if (!server.hasArg("value")) {
    server.send(400, "text/plain; charset=utf-8", "missing value");
    return;
  }

  int brightness = server.arg("value").toInt();
  if (brightness < 0) {
    brightness = 0;
  } else if (brightness > 255) {
    brightness = 255;
  }

  bool saveToFlash = true;
  if (server.hasArg("save")) {
    saveToFlash = server.arg("save").toInt() != 0;
  }

  setBacklightBrightness(static_cast<uint8_t>(brightness), saveToFlash);
  server.send(200, "text/plain; charset=utf-8", "OK");
}

void startWebServer() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);

  const char *headerKeys[] = {"X-Gif-Name"};
  server.collectHeaders(headerKeys, 1);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/thumbnail", HTTP_GET, handleThumbnail);
  server.on("/test", HTTP_GET, handleTest);
  server.on("/brightness", HTTP_GET, handleBrightness);
  server.on("/brightness", HTTP_POST, handleBrightness);
  server.on("/play", HTTP_POST, handlePlayFile);
  server.on("/delete", HTTP_POST, handleDeleteFile);
  server.on("/exit", HTTP_POST, handleExitWifi);
  server.on("/upload-gif-raw", HTTP_POST, handleRawUploadFinish, handleRawGifUpload);
  server.on("/upload-gif-ram", HTTP_POST, handleTemporaryGifUploadFinish, handleTemporaryGifUpload);
  server.on("/update-firmware", HTTP_POST, handleFirmwareUploadFinish, handleFirmwareUpload);
  server.begin();
}
