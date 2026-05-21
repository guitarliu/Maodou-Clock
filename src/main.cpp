#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <WiFiManager.h> 
#include <WebSocketsClient.h>  

// ----------------- 引脚定义 -----------------
#define TFT_SCLK 4
#define TFT_MOSI 6
#define TFT_RST  3
#define TFT_DC   2
#define TFT_CS   7
#define BTN_PIN  9       

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
#define COLOR_CYAN      0x07FF 

// ----------------- 图标点阵数据 -----------------
static const unsigned char icon_thermo[] PROGMEM = { 0x18,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x24,0x00,0x5a,0x00,0x5a,0x00,0x7e,0x00,0x7e,0x00,0x7e,0x00,0x3c,0x00,0x00,0x00 };
static const unsigned char icon_drop[] PROGMEM = { 0x00,0x00,0x08,0x00,0x1c,0x00,0x1c,0x00,0x3e,0x00,0x3e,0x00,0x7f,0x00,0x7f,0x00,0xff,0x00,0xff,0x00,0xff,0x00,0xff,0x00,0x7f,0x00,0x3e,0x00,0x1c,0x00,0x00,0x00 };
static const unsigned char icon_rain[] PROGMEM = { 0x00,0x00,0x03,0x80,0x07,0xc0,0x0f,0xe0,0x1f,0xf0,0x3f,0xf8,0x7f,0xfc,0xff,0xfe,0x01,0x80,0x01,0x80,0x09,0x90,0x11,0x20,0x22,0x40,0x44,0x80,0x88,0x00,0x00,0x00 };
static const unsigned char icon_sun[] PROGMEM = { 0x01,0x80,0x01,0x80,0x08,0x10,0x0c,0x30,0x07,0xe0,0x0f,0xf0,0x1f,0xf8,0x3f,0xfc,0x3f,0xfc,0x1f,0xf8,0x0f,0xf0,0x07,0xe0,0x0c,0x30,0x08,0x10,0x01,0x80,0x01,0x80 };
static const unsigned char icon_cloud[] PROGMEM = { 0x00,0x00,0x01,0x80,0x03,0xc0,0x07,0xe0,0x1f,0xf0,0x3f,0xf8,0x3f,0xf8,0x7f,0xfc,0xff,0xfe,0xff,0xfe,0xff,0xfe,0xff,0xfe,0x7f,0xfc,0x3f,0xf8,0x00,0x00,0x00,0x00 };
static const unsigned char icon_astro_1[] PROGMEM = { 0x03,0xc0,0x07,0xe0,0x0f,0xf0,0x1f,0xf8,0x3c,0x3c,0x38,0x1c,0x3f,0xfc,0x1f,0xf8,0x0f,0xf0,0x1f,0xf8,0x3f,0xfc,0x3f,0xfc,0x1b,0xd8,0x09,0x90,0x0c,0x30,0x00,0x00 };
static const unsigned char icon_astro_2[] PROGMEM = { 0x03,0xc0,0x07,0xe0,0x0f,0xf0,0x1f,0xf8,0x3c,0x3c,0x38,0x1c,0x3f,0xfc,0x1f,0xf8,0x0f,0xf0,0x1f,0xf8,0x3f,0xfc,0x3f,0xfc,0x1b,0xd8,0x19,0x98,0x30,0x0c,0x00,0x00 };

// ----------------- 配置区 -----------------
const String apiKey = "心知天气 API 密钥";   
const String location = "shenzhen"; // 城市拼音        

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

// ----------------- 界面控制与 WebSocket 变量 -----------------
enum DisplayMode { MODE_CLOCK, MODE_MONITOR };
DisplayMode currentMode = MODE_CLOCK;

// WebSocket 配置
String agentIP = "192.168.xxx.xxx"; // 替换为你的 Agent 服务器 IP 地址  
int agentPort = 9119;
String wsPath = "/api/agent/status";

WebSocketsClient webSocket; 

// 数据变量
String agentStatus = "连接中...";
String agentTask = "";
String agentResult = "";          
int agentProgress = 0;
long agentTokens = 0;

// 【防闪烁系统】状态记录变量，容量提升至3行
String lastDrawnTask[3] = {"", "", ""};
String lastDrawnResult[5] = {"", "", "", "", ""}; 
String lastDrawnStatus = "";
long lastDrawnTokens = -1;
int lastDrawnProgress = -1;
int lastReplyLabelY = -1; // 用于检测布局是否发生弹性抖动

// ----------------- 函数声明 -----------------
void fetchWeatherData();
void drawStaticUI();
void updateDynamicUI();
void drawProgressBar(int x, int y, int w, int h, int percent, uint16_t color);
String getWeekday(int wday);

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length);
void drawMonitorStaticUI();
void updateMonitorUI();
void drawMonitorProgressBar(int x, int y, int w, int h, int percent, uint16_t color);
void resetMonitorState();
void renderVerticalScroll(const String& text, int textY, int lineSpacing, int maxLines, unsigned long ms, uint16_t textColor, String lastDrawn[]);

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
  pinMode(BTN_PIN, INPUT_PULLUP); 
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

  WiFiManager wm;
  wm.setAPCallback(configModeCallback); 
  if (!wm.autoConnect("Maodou-Clock-Setup")) {
    delay(3000);
    ESP.restart();
  }

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

  webSocket.begin(agentIP, agentPort, wsPath);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(3000); 

  tft.fillScreen(COLOR_BG);
  drawStaticUI();
}

void loop() {
  webSocket.loop(); 

  static unsigned long lastPing = 0;
  if (webSocket.isConnected() && millis() - lastPing > 15000) {
      webSocket.sendPing();
      lastPing = millis();
  }

  static bool lastBtnState = HIGH;
  bool btnState = digitalRead(BTN_PIN);
  if (btnState == LOW && lastBtnState == HIGH) {
    delay(50); 
    currentMode = (currentMode == MODE_CLOCK) ? MODE_MONITOR : MODE_CLOCK;
    if (currentMode == MODE_CLOCK) {
      tft.fillScreen(COLOR_BG);
      drawStaticUI();
      forceRefresh = true;
    } else {
      drawMonitorStaticUI();
    }
  }
  lastBtnState = btnState;

  if (currentMode == MODE_CLOCK) {
    if (millis() - lastWeatherUpdate > 600000 || lastWeatherUpdate == 0) {
      fetchWeatherData();
      lastWeatherUpdate = millis();
    }
    updateDynamicUI();
  } else {
    // 监控模式每帧自动执行状态检查，只有发生变化时才会绘制，绝对不闪屏
    updateMonitorUI();
  }
  delay(10); 
}


// ==============================================================================
// 天气时钟代码 (你的完美原版，绝对一字未改)
// ==============================================================================

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

  tft.drawLine(10, 26, 118, 26, ST77XX_WHITE);
  tft.drawBitmap(10, 125, icon_thermo, 12, 16, ST77XX_RED);
  tft.drawBitmap(10, 145, icon_drop, 12, 16, ST77XX_CYAN);
}

void updateDynamicUI() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return;

  static unsigned long lastAstroUpdate = 0;
  if (millis() - lastAstroUpdate > 500 || forceRefresh) {
    lastAstroUpdate = millis();
    astroFrame = !astroFrame;
    tft.fillRect(106, 135, 16, 16, COLOR_BG); 
    if(astroFrame) tft.drawBitmap(106, 135, icon_astro_1, 16, 16, ST77XX_WHITE);
    else tft.drawBitmap(106, 135, icon_astro_2, 16, 16, ST77XX_WHITE);
  }

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

    tft.fillRect(10, 66, 90, 35, COLOR_BG); 
    u8g2.setFont(u8g2_font_logisoso28_tn); 
    char hh[10], mm[10];
    snprintf(hh, sizeof(hh), "%02d", timeinfo.tm_hour);
    snprintf(mm, sizeof(mm), "%02d", timeinfo.tm_min);
    u8g2.setCursor(12, 98); 
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
    u8g2.setCursor(100, 96);
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


// ==============================================================================
// 弹性流式排版、零闪屏 WebSocket 监控界面
// ==============================================================================

// 全局变量：自动折行切割存储器 (最高支持15行长文本)
#define MAX_WRAPPED_LINES 15
String wrappedLines[MAX_WRAPPED_LINES];
int numWrappedLines = 0;

// 【核心算法】将字符串根据屏幕宽度自适应切割为多行（保证不把中文汉字劈成乱码）
void wrapTextUniform(const String& text, int maxUnits) {
  numWrappedLines = 0;
  int byteLen = text.length();
  int bytePos = 0;

  while (bytePos < byteLen && numWrappedLines < MAX_WRAPPED_LINES) {
    int currentU = 0;
    int startByte = bytePos;

    while (bytePos < byteLen) {
      uint8_t c = text[bytePos];
      int charU = 0, charB = 0;
      if ((c & 0x80) == 0) { charU = 1; charB = 1; }
      else if ((c & 0xE0) == 0xC0) { charU = 2; charB = 2; }
      else if ((c & 0xF0) == 0xE0) { charU = 2; charB = 3; }
      else if ((c & 0xF8) == 0xF0) { charU = 2; charB = 4; }
      else { charU = 1; charB = 1; }

      if (currentU + charU > maxUnits) break;
      currentU += charU;
      bytePos += charB;
    }
    wrappedLines[numWrappedLines++] = text.substring(startByte, bytePos);
  }
}

// 【核心算法】将切割好的行，进行平滑且无闪烁的上下滚动显示
void renderVerticalScroll(const String& text, int textY, int lineSpacing, int maxLines, unsigned long ms, uint16_t textColor, String lastDrawn[]) {
  if (text.length() == 0) {
    for(int i = 0; i < maxLines; i++) {
      if (lastDrawn[i] != "") { 
        // 擦除当前行的旧文字 (擦除高度高度匹配 14 像素)
        tft.fillRect(10, textY + i * lineSpacing - 12, 118, 13, COLOR_BG); 
        lastDrawn[i] = ""; 
      }
    }
    return;
  }

  // 18 个展示单位 = 恰好填满屏幕宽度不截断 (1单位=1个英文, 2单位=1个中文)
  wrapTextUniform(text, 18); 

  int scrollIdx = 0;
  if (numWrappedLines > maxLines) {
    int maxScroll = numWrappedLines - maxLines;
    int steps = maxScroll + 4; // 头尾各停顿缓冲一下
    int currentStep = (ms / 1500) % steps; // 滚动速度：1.5秒跳一行
    
    if (currentStep < 2) scrollIdx = 0; 
    else if (currentStep >= 2 + maxScroll) scrollIdx = maxScroll; 
    else scrollIdx = currentStep - 2;
  }

  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.setForegroundColor(textColor);

  for(int i = 0; i < maxLines; i++) {
    String line = (scrollIdx + i < numWrappedLines) ? wrappedLines[scrollIdx + i] : "";
    if (line != lastDrawn[i]) {
      // 擦除当前行的旧文字
      tft.fillRect(10, textY + i * lineSpacing - 12, 118, 13, COLOR_BG);
      if (line != "") {
        u8g2.setCursor(10, textY + i * lineSpacing);
        u8g2.print(line.c_str());
      }
      lastDrawn[i] = line;
    }
  }
}

// ----------------- WebSocket 事件接收 -----------------
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      agentStatus = "已离线";
      agentTask = "连接断开，尝试重连中...";
      break;
      
    case WStype_CONNECTED:
      agentStatus = "已连接";
      agentTask = "已建立 WS 通信，等待任务...";
      break;
      
    case WStype_TEXT:
      DynamicJsonDocument doc(8192);
      DeserializationError error = deserializeJson(doc, payload);
      
      if (!error) {
        if (doc.containsKey("status")) agentStatus = doc["status"].as<String>();
        else if (doc.containsKey("state")) agentStatus = doc["state"].as<String>();
        else agentStatus = "运行中";
        
        if (doc.containsKey("task")) agentTask = doc["task"].as<String>();
        else if (doc.containsKey("current_task")) agentTask = doc["current_task"].as<String>();
        else if (doc.containsKey("message")) agentTask = doc["message"].as<String>();
        else if (doc.containsKey("content")) agentTask = doc["content"].as<String>();
        
        if (doc.containsKey("progress")) agentProgress = doc["progress"].as<int>();
        else if (doc.containsKey("percent")) agentProgress = doc["percent"].as<int>();
        
        if (doc.containsKey("tokens")) {
          agentTokens = doc["tokens"].as<long>();
        } else if (doc.containsKey("usage") && doc["usage"].containsKey("total_tokens")) {
          agentTokens = doc["usage"]["total_tokens"].as<long>();
        }
        
        if (doc.containsKey("result")) {
          agentResult = doc["result"].as<String>();
        }
      }
      break;
  }
}

// ----------------- 界面绘制与刷新 -----------------
void resetMonitorState() {
  for(int i = 0; i < 3; i++) lastDrawnTask[i] = "";
  for(int i = 0; i < 5; i++) lastDrawnResult[i] = ""; // 扩充至5行缓存，防止动态扩充时数组越界
  lastDrawnStatus = "";
  lastDrawnTokens = -1;
  lastDrawnProgress = -1;
}

void drawMonitorStaticUI() {
  tft.fillScreen(COLOR_BG);
  
  // 标题：最小且精致的12px字体
  u8g2.setFont(u8g2_font_wqy12_t_gb2312); 
  u8g2.setForegroundColor(COLOR_CYAN);
  u8g2.setCursor(35, 14); 
  u8g2.print("Agent 监控");
  
  tft.drawLine(10, 18, 118, 18, ST77XX_WHITE);

  lastReplyLabelY = -1; // 强制触发首帧计算绘制
  resetMonitorState(); 
}

void updateMonitorUI() {
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);

  // 【弹性排版核心】：先动态计算任务行的实际高度
  wrapTextUniform(agentTask, 18);
  int taskLinesCount = numWrappedLines;
  if (taskLinesCount == 0) taskLinesCount = 1;
  int taskLinesDisp = min(taskLinesCount, 3); // 任务内容最多展现 3 行

  // 根据任务行数，弹性推算下面“回复:”标签的 Y 坐标（留出 14 像素完美的防重叠和字元溢出间隔）
  int replyLabelY = 58 + (taskLinesDisp - 1) * 13 + 14; 

  // 【零闪重绘机制】：只有当布局坐标发生改变时，才重写一次静态标签
  if (replyLabelY != lastReplyLabelY) {
    if (lastReplyLabelY != -1) {
      // 非第一帧时，精准清空发生位置移动的下方显示区域
      tft.fillRect(10, 20, 118, 130, COLOR_BG);
    }
    u8g2.setForegroundColor(COLOR_GRAY);
    u8g2.setCursor(10, 42); u8g2.print("任务:");
    u8g2.setCursor(10, replyLabelY); u8g2.print("回复:");
    u8g2.setCursor(10, 150); u8g2.print("Tokens:"); // Tokens 略微下移到 150
    
    resetMonitorState(); // 布局重排后，强制下一帧刷新内容
    lastReplyLabelY = replyLabelY;
  }

  // 1. 状态行 (Y=30)
  if (agentStatus != lastDrawnStatus) {
    tft.fillRect(10, 30 - 11, 118, 13, COLOR_BG);
    u8g2.setForegroundColor(COLOR_GRAY);
    u8g2.setCursor(10, 30); 
    u8g2.print("状态: ");
    u8g2.setForegroundColor(agentStatus == "已离线" || agentStatus == "连接中..." ? ST77XX_RED : COLOR_GREEN);
    u8g2.print(agentStatus.c_str());
    lastDrawnStatus = agentStatus;
  }

  // 2. 任务行 (调整 Y起点=58, 任务标签在 42, 留出了 16 像素高差，完美解决折行首字被遮挡的问题！)
  renderVerticalScroll(agentTask, 58, 13, taskLinesDisp, millis(), ST77XX_WHITE, lastDrawnTask);

  // 3. 回复行 (调整 Y起点=replyLabelY+14, 回复标签在 replyLabelY, 留出了 14 像素高差，完美防遮挡！)
  int replyTextY = replyLabelY + 14;
  int maxResultLines = 3;
  if (replyLabelY == 72) maxResultLines = 5;      // 任务 1 行时，回复自适应扩容到 5 行！
  else if (replyLabelY == 85) maxResultLines = 4; // 任务 2 行时，回复自适应扩容到 4 行！
  else if (replyLabelY == 98) maxResultLines = 3; // 任务 3 行时，回复展现 3 行
  
  renderVerticalScroll(agentResult, replyTextY, 13, maxResultLines, millis(), COLOR_CYAN, lastDrawnResult);

  // 4. Tokens 行 (Y=150)
  if (agentTokens != lastDrawnTokens) {
    tft.fillRect(10, 150 - 11, 118, 13, COLOR_BG);
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2.setForegroundColor(COLOR_GRAY);
    u8g2.setCursor(10, 150); 
    u8g2.print("Tokens: ");
    u8g2.setForegroundColor(COLOR_YELLOW);
    u8g2.print(String(agentTokens).c_str());
    lastDrawnTokens = agentTokens;
  }

  // 5. 进度条 (Y=155)
  if (agentProgress != lastDrawnProgress) {
    drawMonitorProgressBar(10, 155, 108, 4, agentProgress, COLOR_ORANGE);
    lastDrawnProgress = agentProgress;
  }
}

void drawMonitorProgressBar(int x, int y, int w, int h, int percent, uint16_t color) {
  if (percent > 100) percent = 100;
  if (percent < 0) percent = 0;
  int fillWidth = ((w - 2) * percent) / 100;
  tft.drawRect(x, y, w, h, COLOR_GRAY);
  tft.fillRect(x + 1, y + 1, w - 2, h - 2, COLOR_BG); 
  if (fillWidth > 0) tft.fillRect(x + 1, y + 1, fillWidth, h - 2, color);
}