#include <WiFi.h>
#include <HTTPClient.h>
#include "common.h"
#include "weather.h"

// 一周天气：Open-Meteo 免费API(无需密钥)，坐标在 common.h 配置
static int codes[7];            // 简化天气代码，0=今天
static bool valid = false;
static unsigned long lastFetch = 0;

// WMO天气代码 → 简化分类
static int classify(int c){
  if(c == 0) return 0;                                  // 晴
  if(c == 1 || c == 2) return 1;                        // 多云
  if(c == 3) return 2;                                  // 阴
  if(c == 45 || c == 48) return 5;                      // 雾
  if(c == 95 || c == 96 || c == 99) return 6;           // 雷雨
  if((c >= 71 && c <= 77) || c == 85 || c == 86) return 4; // 雪
  if(c >= 51) return 3;                                 // 雨
  return 1;
}

// 从JSON提取 "weather_code":[..7个整数..]
static void parseCodes(const String& body){
  int p = body.indexOf("\"weather_code\"");
  if(p < 0) return;
  p = body.indexOf('[', p);
  if(p < 0) return;
  int n = 0;
  for(int i = p + 1; i < (int)body.length() && n < 7; i++){
    char c = body[i];
    if(c == ']') break;
    if(c >= '0' && c <= '9'){
      int v = 0;
      while(i < (int)body.length() && body[i] >= '0' && body[i] <= '9'){
        v = v * 10 + (body[i] - '0');
        i++;
      }
      codes[n++] = classify(v);
      i--;
    }
  }
  if(n > 0) valid = true;
}

static void fetchWeather(){
  if(WiFi.status() != WL_CONNECTED) return;
  String url = String("http://api.open-meteo.com/v1/forecast?latitude=") + WEATHER_LAT
             + "&longitude=" + WEATHER_LON
             + "&daily=weather_code&timezone=auto&forecast_days=7";
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(url);
  http.setTimeout(8000);
  int code = http.GET();
  Serial.print("天气获取: HTTP ");
  Serial.println(code);
  if(code == 200) parseCodes(http.getString());
  http.end();
}

void weatherBegin(){
  fetchWeather();
  lastFetch = millis();
}

void weatherLoop(){
  if(WiFi.status() != WL_CONNECTED) return;
  if(millis() - lastFetch < 1800000UL) return;  // 30分钟刷新一次
  lastFetch = millis();
  fetchWeather();
}

bool weatherValid(){ return valid; }
int weatherCode(int i){ return (i >= 0 && i < 7) ? codes[i] : 0; }
