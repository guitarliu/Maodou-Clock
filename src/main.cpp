#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <WiFiManager.h> // 【新增】WiFi管理器

// ----------------- 引脚定义 -----------------
#define TFT_SCLK 4
#define TFT_MOSI 6
#define TFT_RST  3
#define TFT_DC   2
#define TFT_CS   7

Adafruit_ST7735 tft = Adafruit_ST7735(&SPI, TFT_CS, TFT_DC, TFT_RST);
U8G2_FOR_ADAFRUIT_GFX u8g2;

// ----------------- 自定义颜色 -----------------
#define COLOR_BG        0x0000
#define COLOR_DUDU      0x541B
#define COLOR_LINE      0x3186
#define COLOR_ORANGE    0xFD20
#define COLOR_GREEN     0x07E0
#define COLOR_YELLOW    0xFFE0
#define COLOR_GRAY      0xAD75

// ----------------- 图标点阵数据 -----------------
static const unsigned char icon_thermo[] PROGMEM = { 0x18,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x5a,0x00,0x5a,0x00,0x7e,0x00,0x7e,0x00,0x7e,0x00,0x3c,0x00,0x00,0x00 };
static const unsigned char icon_drop[] PROGMEM = { 0x00,0x00,0x08,0x00,0x1c,0x00,0x1c,0x00,0x3e,0x00,0x3e,0x00,0x7f,0x00,0x7f,0x00,0xff,0x00,0xff,0x00,0xff,0x00,0xff,0x00,0x7f,0x00,0x3e,0x00,0x1c,0x00,0x00,0x00 };
static const unsigned char icon_rain[] PROGMEM = { 0x00,0x00,0x03,0x80,0x07,0xc0,0x0f,0xe0,0x1f,0xf0,0x3f,0xf8,0x7f,0xfc,0xff,0xfe,0x01,0x80,0x01,0x80,0x09,0x90,0x11,0x20,0x22,0x40,0x44,0x80,0x88,0x00,0x00,0x00 };
static const unsigned char icon_sun[] PROGMEM = { 0x01,0x80,0x01,0x80,0x08,0x10,0x0c,0x30,0x07,0xe0,0x0f,0xf0,0x1f,0xf8,0x3f,0xfc,0x3f,0xfc,0x1f,0xf8,0x0f,0xf0,0x07,0xe0,0x0c,0x30,0x08,0x10,0x01,0x80,0x01,0x80 };
static const unsigned char icon_cloud[] PROGMEM = { 0x00,0x00,0x01,0x80,0x03,0xc0,0x07,0xe0,0x1f,0xf0,0x3f,0xf8,0x3f,0xf8,0x7f,0xfc,0xff,0xfe,0xff,0xfe,0xff,0xfe,0xff,0xfe,0x7f,0xfc,0x3f,0xf8,0x00,0x00,0x00,0x00 };
static const unsigned char icon_astro_1[] PROGMEM = { 0x03,0xc0,0x07,0xe0,0x0f,0xf0,0x1f,0xf8,0x3c,0x3c,0x38,0x1c,0x3f,0xfc,0x1f,0xf8,0x0f,0xf0,0x1f,0xf8,0x3f,0xfc,0x3f,0xfc,0x1b,0xd8,0x09,0x90,0x0c,0x30,0x00,0x00 };
static const unsigned char icon_astro_2[] PROGMEM = { 0x03,0xc0,0x07,0xe0,0x0f,0xf0,0x1f,0xf8,0x3c,0x3c,0x38,0x1c,0x3f,0xfc,0x1f,0xf8,0x0f,0xf0,0x1f,0xf8,0x3f,0xfc,0x3f,0xfc,0x1b,0xd8,0x19,0x98,0x30,0x0c,0x00,0x00 };

// ----------------- 配置区 -----------------
const String apiKey = "xxxxxxxx";   // 改成你的私钥
const String location = "xxxxxxxxx";        // 城市拼音

const char* ntpServer = "ntp.aliyun.com";
const long  gmtOffset_sec = 8 * 3600;

String cityName = "...";
String aqiText = "优"; 
String weatherText = "...";
int currentTemp = 0;
int currentHum = 0;

int lastMinute = -1;
int lastSecond = -1;
unsigned long lastWeatherUpdate = 0;
bool astroFrame = false;
bool forceRefresh = true;

// ----------------- 函数声明 -----------------
void fetchWeatherData();
void drawStaticUI();
void updateDynamicUI();
void drawProgressBar(int x, int y, int w, int h, int percent, uint16_t color);
String getWeekday(int wday);

// 【新增】WiFiManager 进入配网模式时的屏幕回调
void configModeCallback(WiFiManager *myWiFiManager) {
  tft.fillScreen(COLOR_BG);
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.setForegroundColor(ST77XX_YELLOW);
  u8g2.setCursor(10, 40);
  u8g2.print("WiFi 配网模式开启");
  u8g2.setForegroundColor(ST77XX_WHITE);
  u8g2.setCursor(10, 65);
  u8g2.print("请连接热点:");
  u8g2.setCursor(10, 85);
  u8g2.print("Maodou-Clock-Setup");
  u8g2.setCursor(10, 110);
  u8g2.print("地址: 192.168.4.1");
}

void setup() {
  Serial.begin(115200);
  delay(1000); 

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_BLACKTAB); 
  tft.setRotation(0); 
  tft.fillScreen(COLOR_BG);

  u8g2.begin(tft);
  u8g2.setFontMode(1); 
  u8g2.setFontDirection(0);

  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.setForegroundColor(ST77XX_WHITE);
  u8g2.setCursor(10, 80);
  u8g2.print("正在检查网络...");

  // --- WiFiManager 逻辑 ---
  WiFiManager wm;
  wm.setAPCallback(configModeCallback); // 绑定屏幕显示回调
  
  // 自动连接上次存储的WiFi，若无则启动 AP 热点
  if (!wm.autoConnect("Maodou-Clock-Setup")) {
    Serial.println("连接失败，重启中...");
    delay(3000);
    ESP.restart();
  }

  // 成功连接后的显示
  tft.fillScreen(COLOR_BG);
  u8g2.setCursor(10, 60);
  u8g2.print("网络已连接!");
  u8g2.setCursor(10, 80);
  u8g2.print("IP: ");
  u8g2.print(WiFi.localIP().toString().c_str());
  delay(2000);

  configTime(gmtOffset_sec, 0, ntpServer);
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) { delay(500); }

  tft.fillScreen(COLOR_BG);
  drawStaticUI();
}

void loop() {
  if (millis() - lastWeatherUpdate > 600000 || lastWeatherUpdate == 0) {
    fetchWeatherData();
    lastWeatherUpdate = millis();
  }
  updateDynamicUI();
  delay(1000);
}

void fetchWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    DynamicJsonDocument doc(1280);

    http.begin("http://api.seniverse.com/v3/weather/now.json?key=" + apiKey + "&location=" + location + "&language=zh-Hans&unit=c");
    if (http.GET() == 200) {
      deserializeJson(doc, http.getString());
      cityName = doc["results"][0]["location"]["name"].as<String>();
      weatherText = doc["results"][0]["now"]["text"].as<String>();
      currentTemp = doc["results"][0]["now"]["temperature"].as<int>();
      if (doc["results"][0]["now"]["humidity"]) {
        currentHum = doc["results"][0]["now"]["humidity"].as<int>();
      }
    }
    http.end();

    http.begin("http://api.seniverse.com/v3/air/now.json?key=" + apiKey + "&location=" + location + "&language=zh-Hans");
    if (http.GET() == 200) {
      String payload = http.getString();
      deserializeJson(doc, payload);
      if(!doc["results"][0]["air"]["city"]["quality"].isNull()) {
        aqiText = doc["results"][0]["air"]["city"]["quality"].as<String>();
      }
    }
    http.end();

    forceRefresh = true;
  }
}

void drawStaticUI() {
  u8g2.setFont(u8g2_font_wqy16_t_gb2312); 
  u8g2.setForegroundColor(ST77XX_WHITE);
  String title = "毛豆时钟";
  int tW = u8g2.getUTF8Width(title.c_str());
  u8g2.setCursor((128 - tW) / 2, 20); 
  u8g2.print(title.c_str());

  tft.drawLine(10, 26, 118, 26, COLOR_LINE);
  tft.drawBitmap(10, 125, icon_thermo, 12, 16, ST77XX_RED);
  tft.drawBitmap(10, 145, icon_drop, 12, 16, ST77XX_CYAN);
}

void updateDynamicUI() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  astroFrame = !astroFrame;
  tft.fillRect(100, 135, 16, 16, COLOR_BG); 
  if(astroFrame) tft.drawBitmap(100, 135, icon_astro_1, 16, 16, ST77XX_WHITE);
  else tft.drawBitmap(100, 135, icon_astro_2, 16, 16, ST77XX_WHITE);

  if (timeinfo.tm_min != lastMinute || forceRefresh) {
    lastMinute = timeinfo.tm_min;
    tft.fillRect(10, 27, 118, 38, COLOR_BG); 

    if (weatherText.indexOf("雨") != -1 || weatherText.indexOf("雪") != -1) tft.drawBitmap(100, 28, icon_rain, 16, 16, ST77XX_CYAN);
    else if (weatherText.indexOf("云") != -1 || weatherText.indexOf("阴") != -1) tft.drawBitmap(100, 28, icon_cloud, 16, 16, 0xC618); 
    else tft.drawBitmap(100, 28, icon_sun, 16, 16, ST77XX_YELLOW);

    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2.setForegroundColor(COLOR_ORANGE);
    u8g2.setCursor(15, 42);
    u8g2.print(cityName.c_str());

    if (aqiText != "") {
      int cityWidth = u8g2.getUTF8Width(cityName.c_str());
      uint16_t aCol = COLOR_GREEN;
      if (aqiText == "良") aCol = COLOR_YELLOW;
      else if (aqiText.indexOf("污染") != -1) aCol = ST77XX_RED;
      u8g2.setForegroundColor(aCol);
      u8g2.setCursor(15 + cityWidth + 8, 42); 
      u8g2.print(aqiText.c_str());
    }

    u8g2.setForegroundColor(COLOR_GRAY);
    u8g2.setCursor(15, 62);
    String feels = "体感温度 " + String(currentTemp) + "℃";
    u8g2.print(feels.c_str());

    int feelsWidth = u8g2.getUTF8Width(feels.c_str());
    u8g2.setForegroundColor(ST77XX_CYAN);
    u8g2.setCursor(15 + feelsWidth + 8, 62);
    u8g2.print(weatherText.c_str());

    tft.fillRect(5, 66, 88, 35, COLOR_BG); 
    u8g2.setFont(u8g2_font_logisoso28_tn); 
    char hh[10], mm[10];
    snprintf(hh, sizeof(hh), "%02d", timeinfo.tm_hour);
    snprintf(mm, sizeof(mm), "%02d", timeinfo.tm_min);
    u8g2.setCursor(6, 98);
    u8g2.setForegroundColor(ST77XX_WHITE);
    u8g2.print(hh); 
    u8g2.setForegroundColor(COLOR_GRAY);
    u8g2.print(":"); 
    u8g2.setForegroundColor(COLOR_ORANGE);
    u8g2.print(mm);

    tft.fillRect(10, 102, 118, 15, COLOR_BG);
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2.setForegroundColor(COLOR_GRAY);
    u8g2.setCursor(15, 114);
    char dateStr[32];
    snprintf(dateStr, sizeof(dateStr), "%d月%d日  ", timeinfo.tm_mon + 1, timeinfo.tm_mday);
    u8g2.print(dateStr);
    u8g2.print(getWeekday(timeinfo.tm_wday).c_str());

    tft.fillRect(80, 125, 35, 16, COLOR_BG);
    u8g2.setCursor(82, 137);
    u8g2.print((String(currentTemp) + "℃").c_str());
    tft.fillRect(80, 145, 35, 16, COLOR_BG);
    u8g2.setCursor(82, 157);
    u8g2.print((String(currentHum) + "%").c_str());
  }

  if (timeinfo.tm_sec != lastSecond || forceRefresh) {
    lastSecond = timeinfo.tm_sec;
    tft.fillRect(92, 80, 30, 18, COLOR_BG); 
    u8g2.setFont(u8g2_font_helvB12_tf); 
    u8g2.setForegroundColor(ST77XX_RED);
    u8g2.setCursor(94, 96);
    char ss[10];
    snprintf(ss, sizeof(ss), "%02d", timeinfo.tm_sec);
    u8g2.print(ss);
  }

  forceRefresh = false;
  drawProgressBar(26, 131, 50, 4, (currentTemp * 100) / 40, ST77XX_CYAN); 
  drawProgressBar(26, 151, 50, 4, currentHum, COLOR_GREEN);
}

void drawProgressBar(int x, int y, int w, int h, int percent, uint16_t color) {
  if (percent > 100) percent = 100;
  if (percent < 0) percent = 0;
  int fillWidth = (w * percent) / 100;
  tft.drawRect(x, y, w, h, COLOR_GRAY);
  tft.fillRect(x + 1, y + 1, fillWidth, h - 2, color);
}

String getWeekday(int wday) {
  const char* weekDays[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
  return (wday >= 0 && wday <= 6) ? weekDays[wday] : "";
}