#include <WiFi.h>
#include <HTTPClient.h>
#include "common.h"
#include "settings.h"
#include "weather.h"

// 一周天气：Open-Meteo 免费API(无需密钥)，坐标可在配置网页中修改(存NVS)
static int codes[7];            // 简化天气代码，0=今天
static int highs[7], lows[7];   // 每天最高/最低温(℃)
static bool valid = false;
static bool hasTemp = false;
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

// 从JSON提取 "key":[..7个数值..]，四舍五入为整数(支持负数与小数)
// 注意：必须连同 "[": 一起匹配，否则会先命中 daily_units 里的同名字段
static bool parseSeries(const String& body, const char* key, int* dst){
  String pat = String("\"") + key + "\":[";
  int p = body.indexOf(pat);
  if(p < 0) return false;
  p += pat.length() - 1;   // 指向 '['
  int n = 0;
  for(int i = p + 1; i < (int)body.length() && n < 7; i++){
    char c = body[i];
    if(c == ']') break;
    if(c == '-' || (c >= '0' && c <= '9')){
      bool neg = (c == '-');
      if(neg) i++;
      float v = 0, f = 0.1f;
      bool frac = false;
      while(i < (int)body.length()){
        char d = body[i];
        if(d >= '0' && d <= '9'){
          if(frac){ v += (d - '0') * f; f *= 0.1f; }
          else v = v * 10 + (d - '0');
          i++;
        } else if(d == '.' && !frac){ frac = true; i++; }
        else break;
      }
      dst[n++] = (int)(v + 0.5f) * (neg ? -1 : 1);
      i--;
    }
  }
  return n > 0;
}

static void fetchWeather(){
  if(WiFi.status() != WL_CONNECTED) return;
  String url = String("http://api.open-meteo.com/v1/forecast?latitude=") + cfgWeatherLat()
             + "&longitude=" + cfgWeatherLon()
             + "&daily=weather_code,temperature_2m_max,temperature_2m_min&timezone=auto&forecast_days=7";
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.begin(url);
  http.setTimeout(8000);
  int code = http.GET();
  Serial.print("天气获取: HTTP ");
  Serial.println(code);
  if(code == 200){
    String body = http.getString();
    parseCodes(body);
    bool hi = parseSeries(body, "temperature_2m_max", highs);
    bool lo = parseSeries(body, "temperature_2m_min", lows);
    hasTemp = hi && lo;
  }
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

// 地区变更后调用：立即用新坐标重新获取
void weatherRefreshNow(){
  valid = false;
  hasTemp = false;
  lastFetch = millis() - 1800000UL;  // 让 weatherLoop 立即触发一次获取
}

bool weatherValid(){ return valid; }
int weatherCode(int i){ return (i >= 0 && i < 7) ? codes[i] : 0; }
bool weatherHasTemp(){ return hasTemp; }
int weatherHigh(int i){ return (i >= 0 && i < 7) ? highs[i] : 0; }
int weatherLow(int i){ return (i >= 0 && i < 7) ? lows[i] : 0; }
