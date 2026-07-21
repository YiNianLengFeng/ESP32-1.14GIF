# ESP32-S3 TFT GIF 播放器

这是一个基于 ESP32-S3、8MB Flash、8MB OPI PSRAM 和 ST7789 TFT 的 GIF 播放器。
设备开机后建立 WiFi AP，通过 Web 页面上传、管理和播放 GIF，也支持 OTA 更新固件。

## 项目资料

- 嘉立创开源硬件项目：[ESP32-S3 TFT GIF 播放器电路板](https://oshwhub.com/yinianlengfeng/project_ehqrgpks)
- Bilibili 演示视频：[Web 上传 GIF 与播放演示](https://www.bilibili.com/video/BV1JVKt6fE7D/)

## 功能

- ST7789 1.14 英寸 TFT 播放 GIF。
- GIF 自动居中显示，超出屏幕时从中心裁剪。
- GIF 循环播放，切换循环时避免黑屏闪烁。
- Flash 裸分区保存多个 GIF，最多 24 个文件。
- Web 页面显示文件缩略图、文件大小、已用空间、剩余空间和最大连续空间。
- 点击 GIF 第一帧缩略图即可选择并播放。
- 勾选多个文件后可以批量删除。
- 支持“临时播放（不保存）”：GIF 直接放入 PSRAM，断电或重启后消失。
- Web 修改 TFT 背光亮度，设置保存到 NVS。
- 电池低电量检测，低于阈值时 IO2 红灯闪烁。
- Web OTA 更新固件，更新成功后自动重启。
- Web 按钮一键退出并关闭 WiFi。

## 硬件接线

### TFT

| TFT 信号 | ESP32-S3 GPIO |
| --- | ---: |
| MOSI | IO11 |
| SCLK | IO12 |
| CS | IO10 |
| DC | IO13 |
| RST | IO9 |
| LEDA / 背光 | IO14 |

当前 TFT 参数：

- 分辨率：240 x 135
- 原生屏幕尺寸：135 x 240
- 旋转：1
- SPI：40MHz
- 背光 PWM：20kHz，8bit
- 默认亮度：120 / 255

### 电池和低电量指示灯

| 功能 | ESP32-S3 GPIO |
| --- | ---: |
| 电池 ADC | IO1 |
| 红色低电量 LED 正极 | IO2 |

电池使用两个 100K 电阻组成 1:1 分压，代码中的电压倍率为 2.0。

- 低电量阈值：3.4V
- 恢复阈值：3.6V
- 采样周期：1秒
- LED 闪烁周期：500ms

IO1 输入电压不能超过 ESP32 ADC 安全范围。电池、分压电阻和地线应连接可靠，brownout 复位通常属于供电压降问题，不能通过软件关闭欠压保护来解决。

## 开发环境

推荐使用以下配置：

- Arduino IDE 2.x 或 Arduino CLI
- ESP32 Arduino Core：3.3.10
- 开发板：ESP32S3 Dev Module
- Flash Size：8MB
- PSRAM：OPI PSRAM
- CPU：240MHz
- Loop Core：1
- Events Core：1
- Flash Mode：QIO

## 依赖库

安装以下 Arduino 库：

- Adafruit GFX Library
- Adafruit ST7735 and ST7789 Library
- AnimatedGIF

ESP32 Core 自带以下库：

- WiFi
- WebServer
- Preferences
- Update
- SPI

## Flash 分区

当前 `partitions.csv` 使用以下布局：

| 名称 | 类型 | 偏移 | 大小 | 用途 |
| --- | --- | ---: | ---: | --- |
| nvs | data | `0x9000` | 20KB | 亮度和 GIF 目录 |
| otadata | data | `0xE000` | 8KB | OTA 启动信息 |
| app0 | app | `0x10000` | 1280KB | OTA 固件分区 0 |
| app1 | app | `0x150000` | 1280KB | OTA 固件分区 1 |
| gifdata | data | `0x290000` | 5504KB | GIF 裸 Flash 分区 |
| coredump | data | `0x7F0000` | 64KB | 崩溃信息 |

GIF 分区前 4KB 作为保留区域，实际数据空间为：

```text
0x560000 - 0x1000 = 5,632,000 bytes，约 5.37MiB
```

GIF 文件按 4KB Flash 扇区分配，因此实际占用空间会向上取整。例如 100KB 文件实际占用约 104KB。

文件目录保存在 NVS，GIF 数据保存在 `gifdata`。删除文件只释放目录中的空间，新文件会优先复用已删除文件留下的空间。

## 首次烧录

由于项目使用自定义双 OTA 分区，第一次使用此版本时必须通过 USB 烧录一次完整固件，包括新的分区表。

重要事项：

1. 第一次烧录时使用项目中的 `partitions.csv`。
2. 不要选择“擦除全部 Flash”。
3. `gifdata` 的地址没有改变，正常烧录不会删除已有 GIF 和 NVS 目录。
4. 如果之前使用的是旧版 SPIFFS 单 GIF 固件，程序会尝试自动迁移旧 GIF。
5. 烧录后设备会启动 AP，使用 Web 页面重新确认文件列表。

Arduino IDE 的菜单一般使用：

```text
工具 -> 开发板 -> ESP32S3 Dev Module
工具 -> Flash Size -> 8MB
工具 -> PSRAM -> OPI PSRAM
```

## Web 页面

### 连接

默认 AP 参数：

```text
SSID: ESP32S3-TFT
密码: 12345678
地址: http://192.168.4.1/
```

开机后设备建立 AP。当前上传窗口为 20 秒：

- 没有客户端连接时，倒计时继续。
- 手机或电脑连接 AP 后，倒计时暂停。
- 客户端断开后，倒计时继续。
- 点击“退出并关闭 WiFi”会立即安排关闭热点并播放当前选中的 GIF。
- 上传完成后不会自动关闭 WiFi，方便继续上传和管理多个文件。

### 保存到 Flash

不勾选“临时播放（不保存）”时，GIF 会保存到 Flash：

1. 可以一次选择多个 GIF。
2. 文件会按顺序上传。
3. 上传完成后出现在文件列表中。
4. 点击第一帧缩略图切换播放文件。
5. 勾选右侧复选框后点击“删除勾选”释放空间。

如果总剩余空间足够但最大连续空间不足，说明 Flash 中存在空洞。此时需要删除占用空间的文件，再上传大 GIF。

### 临时播放

勾选“临时播放（不保存）”后：

- 每次只能选择一个 GIF。
- GIF 直接写入 PSRAM，不占用 `gifdata` Flash 空间。
- 上传完成后立即播放。
- 临时 GIF 会出现在文件列表顶部并标记为“临时”。
- 断电、复位、OTA 重启后临时 GIF 消失。
- 切换到 Flash GIF 时，临时 GIF 仍会保留在 PSRAM 中，直到删除或重启。

临时 GIF 最大大小根据当前 PSRAM 最大连续空闲块动态计算，并预留约 128KB 给缩略图和运行时操作。实际最大值以 Web 页面和 `/status` 返回值为准。

### 亮度

使用亮度滑块可以实时调节 IO14 背光 PWM。拖动时只临时修改，松开滑块后保存到 NVS。

### OTA 固件更新

Web 页面选择 Arduino 编译生成的固件文件：

```text
esptft.ino.bin
```

不要上传以下文件：

- `merged.bin`
- `bootloader.bin`
- `partitions.bin`

OTA 流程：

1. 设备连接 `ESP32S3-TFT` AP。
2. 选择 `.bin` 文件。
3. 点击“更新固件”。
4. 固件写入备用 App 分区。
5. `Update.end(false)` 校验成功后切换启动分区。
6. 网页收到成功响应后设备自动重启。

OTA 中途断电或网络中断时，当前运行的旧固件仍然保留。固件必须小于 1280KB，目前固件约 1MB，仍有约 270KB 余量。

## 项目结构

| 文件 | 作用 |
| --- | --- |
| `esptft.ino` | 全局对象、启动流程和主循环 |
| `EsptftApp.h` | 硬件配置、公共声明和常量 |
| `Display.cpp` | TFT 初始化、提示画面和亮度设置 |
| `GifPlayback.cpp` | GIF 播放、居中显示和循环播放 |
| `GifStorage.cpp` | Flash 裸分区、GIF 目录、文件分配和删除 |
| `TemporaryGif.cpp` | PSRAM 临时 GIF 上传和播放 |
| `GifThumbnail.cpp` | GIF 第一帧解码和缩略图生成 |
| `WebServerUi.cpp` | Web 页面、GIF 管理、亮度、缩略图和 WiFi 控制 |
| `OtaUpdate.cpp` | Web OTA 固件更新和重启 |
| `BatteryMonitor.cpp` | 电池电压和低电量 LED |
| `Playback.cpp` | 播放状态公共函数 |
| `partitions.csv` | Flash 分区布局 |

## 常用调整

### 修改 TFT SPI 速度

在 `EsptftApp.h` 修改：

```cpp
static constexpr uint32_t TFT_SPI_HZ = 40000000;
```

40MHz 是当前稳定值。短 PCB 走线可以尝试 60MHz，80MHz 是否稳定取决于 TFT 模块和布线质量，出现花屏、错行或黑屏时应降回 40MHz。

### 修改 WiFi 上传窗口

```cpp
static constexpr uint32_t WIFI_UPLOAD_WINDOW_MS = 20000;
```

客户端连接时倒计时暂停，因此上传和文件管理期间不会因时间到期自动关闭。

### 修改 AP 名称和密码

```cpp
static const char *const AP_SSID = "ESP32S3-TFT";
static const char *const AP_PASS = "12345678";
```

WPA2 密码至少需要 8 个字符。

## 故障排查

### 串口显示 PSRAM 为 0

检查：

- 开发板是否选择支持 OPI PSRAM 的 ESP32-S3 配置。
- Arduino IDE 的 PSRAM 是否选择 OPI PSRAM。
- 串口是否显示类似：

```text
PSRAM total/free: 8388608/...
```

### 上传提示空间不足

先看 Web 页面显示的“剩余空间”和“最大连续空间”：

- 剩余空间不足：删除一个或多个 Flash GIF。
- 最大连续空间不足：删除造成空洞的文件，再重试。
- 临时播放模式不使用 Flash 空间，但受 PSRAM 最大连续空间限制。

### GIF 播放失败

确认文件确实是 GIF87a 或 GIF89a 格式，并且上传过程中没有复位。上传中出现 brownout、网络中断或 Flash 写入错误时，应重新上传。

### 设备自动复位并显示 brownout

`brownout` 表示供电电压瞬间低于阈值，常见原因：

- 电池内阻或电量不足。
- 3.3V 稳压器电流能力不足。
- WiFi 发射和 Flash 擦写时供电压降。
- 电源线过长、接触不良或去耦不足。

应优先检查硬件供电，不建议通过软件关闭 brownout 保护。

### OTA 后无法启动

确保：

- 上传的是 `esptft.ino.bin`。
- 固件没有超过 1280KB。
- OTA 过程中没有断电。
- 如果分区表还没有切换到双 OTA 版本，先通过 USB 完整烧录一次。

## 当前验证环境

当前代码使用 ESP32 Arduino Core 3.3.10 完整编译通过，固件约 1.04MB，使用 8MB Flash、OPI PSRAM 和双 OTA 分区。
