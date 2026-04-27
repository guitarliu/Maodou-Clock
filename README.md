# Maodou Clock | 毛豆时钟 🕒☁️

**毛豆时钟** 是一款基于 **ESP32-C3** 和 **ST7735S 1.8寸 TFT屏幕** 开发的高颜值桌面互联网天气时钟。深度还原 DuduDuck 设计语言，采用 `U8g2` 渲染引擎完美解决了中文字体在小屏幕上的马赛克锯齿问题。支持智能配网、实时 NTP 对时、动态天气抓取及空气质量显示。

![Language](https://img.shields.io/badge/Language-C++-blue.svg) ![Framework](https://img.shields.io/badge/Framework-Arduino-orange.svg) ![Platform](https://img.shields.io/badge/Platform-ESP32--C3-red.svg) ![Partition](https://img.shields.io/badge/Partition-Huge_App(3MB)-green.svg)

![Preview](./preview.png)

## 🌟 核心特性

- **🚀 零代码智能配网**：集成 `WiFiManager`，无需在代码中硬编码 WiFi 账号密码。通过手机连接热点即可在网页端轻松配置网络。
- **💎 视网膜级显示**：集成 `U8g2` 字库引擎，完美渲染 12px/16px 抗锯齿中文字体及矢量数字。
- **🌤️ 实时天气抓取**：通过心知天气（Seniverse）API 获取实时气象、体感温度及 AQI 空气质量，支持根据污染等级自动变色。
- **⏰ 精准 NTP 对时**：自动同步阿里云 NTP 时间服务器，红色秒针实时平滑跳动。
- **📐 动态排版引擎**：顶部标题自动居中，内容元素根据字数自动计算间距，防止文字重叠。
- **👾 灵动交互**：右下角内置经典太空人两帧循环动画，提升桌面氛围感。

---

## 🛠️ 硬件清单

| 模块 | 规格 | 备注 |
| :--- | :--- | :--- |
| **主控** | ESP32-C3 | WeAct Studio / SuperMini 或同型号 |
| **屏幕** | 1.8寸 RGB TFT | 驱动 IC: ST7735S, 分辨率: 128x160 |
| **线材** | 杜邦线 | 建议长度不超过 10cm 以保证 SPI 稳定性 |

---

## 🔌 硬件接线 (Wiring)

请根据代码中的引脚定义进行如下连接：

| 屏幕引脚 | ESP32-C3 引脚 | 说明 |
| :--- | :--- | :--- |
| **VCC** | 3.3V | 电源正极 |
| **GND** | GND | 电源地 |
| **SCL (SCK)** | GPIO 4 | SPI 时钟 |
| **SDA (MOSI)** | GPIO 6 | SPI 数据输入 |
| **RES (RST)** | GPIO 3 | 屏幕复位 |
| **DC (A0)** | GPIO 2 | 数据/命令选择 |
| **CS** | GPIO 7 | 片选引脚 |
| **BLK** | 3.3V/GPIO 5 | 背光引脚 (接3.3V常亮) |

---

## 🚀 软件环境配置

本项目推荐使用 **VS Code + PlatformIO** 插件进行开发。

### 1. 依赖库 (Dependencies)
在 `platformio.ini` 中自动下载或手动安装：
- `Adafruit GFX Library`
- `Adafruit ST7735 and ST7789 Library`
- `U8g2_for_Adafruit_GFX`
- `ArduinoJson`
- `WiFiManager`

### 2. 分区配置 (Partitions)
由于中文字库体积较大，必须在 `platformio.ini` 中启用大程序分区：
```ini
board_build.partitions = huge_app.csv
```

---

## 📝 快速开始

1. 克隆本项目：
   ```bash
   git clone https://github.com/guitarliu/Maodou-Clock.git
   ```
2. 在 `src/main.cpp` 中修改心知天气 API 配置（**不再需要填写 WiFi 信息**）：
   ```cpp
   const String apiKey = "你的心知天气私钥";
   const String location = "shenzhen"; // 城市拼音
   ```
3. 点击 PlatformIO 的 **Upload** 按钮进行烧录。

---

## 🌐 智能配网指南

第一次使用或更换 WiFi 环境时，请按照以下步骤配网：

1. **进入模式**：烧录后，若时钟无法连接已知 WiFi，屏幕会变色并提示 **“WiFi 配网模式开启”**。
2. **连接热点**：打开手机 WiFi，搜索并连接名为 `Maodou-Clock-Setup` 的无密码热点。
3. **网页配置**：连接后手机通常会自动弹出配置页面。若未弹出，请在浏览器访问 `192.168.4.1`。
4. **输入密码**：点击 **Configure WiFi**，选择你的 WiFi 名称并输入密码，点击保存。
5. **完工**：时钟会自动重启并尝试连接新网络，成功后进入天气时钟界面。

---

## 🎨 UI 界面说明

- **标题区**：居中显示“毛豆时钟”16px 粗体汉字。
- **状态栏**：左侧展示城市名，右侧展示实时空气质量（AQI）文字。
- **气象区**：展示体感温度及当前气象状态（晴/阴/雨等）。
- **时间区**：28px 超大平滑矢量数字。
- **秒针区**：红色小数字每秒实时跳动。
- **底部**：通过彩色进度条直观展示温度与湿度。

---

## 📜 许可证 (License)

本项目基于 MIT 协议开源。

## 🙏 鸣谢 (Credits)

- [U8g2](https://github.com/olikraus/u8g2)
- [WiFiManager](https://github.com/tzapu/WiFiManager)
- [Adafruit_GFX](https://github.com/adafruit/Adafruit-GFX-Library)
- [ArduinoJson](https://arduinojson.org/)

---
*如果你喜欢这个项目，欢迎点个 ⭐️ Star！*