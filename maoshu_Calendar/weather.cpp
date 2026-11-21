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

// 刷新节奏：成功后 30 分钟一次；失败则 60s→120s→…→30min 退避，接口抖动可自愈
#define WEATHER_PERIOD    1800000UL   // 30 分钟
#define WEATHER_RETRY_MIN 60000UL     // 失败后首次重试间隔
static bool tried = false;            // 是否尝试过
static bool fresh = false;            // 上次请求是否成功
static unsigned long lastTry = 0;     // 上次请求时刻
static unsigned long waitGap = WEATHER_RETRY_MIN;  // 下次等待间隔(本次生效)
static unsigned long backoff = WEATHER_RETRY_MIN;  // 失败间隔阶梯(60s起翻倍)

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

static bool fetchWeather(){
  if(WiFi.status() != WL_CONNECTED) return false;
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
  return code == 200 && valid;
}

// 刷新一次并更新重试节奏：成功→30分钟周期；失败→本次等 backoff，之后 backoff 翻倍(30min封顶)
static void refreshWeather(){
  tried = true;
  lastTry = millis();
  if(fetchWeather()){
    fresh = true;
    waitGap = WEATHER_PERIOD;
    backoff = WEATHER_RETRY_MIN;
  } else {
    fresh = false;
    waitGap = backoff;                                  // 本次等待用当前阶梯(首次60s)
    if(backoff < WEATHER_PERIOD) backoff = (backoff * 2 > WEATHER_PERIOD) ? WEATHER_PERIOD : backoff * 2;
  }
}

void weatherBegin(){
  // WiFi没连上就不取，也不计时：等 loop 里连上后再补首取
  if(WiFi.status() == WL_CONNECTED) refreshWeather();
}

void weatherLoop(){
  if(WiFi.status() != WL_CONNECTED) return;
  if(tried && (millis() - lastTry) < waitGap) return;
  refreshWeather();
}

// 地区变更后调用：立即用新坐标重新获取
void weatherRefreshNow(){
  valid = false;
  hasTemp = false;
  tried = false;                      // 允许立即重取
  backoff = WEATHER_RETRY_MIN;
  waitGap = WEATHER_RETRY_MIN;
}

bool weatherValid(){ return valid; }
int weatherCode(int i){ return (i >= 0 && i < 7) ? codes[i] : 0; }
bool weatherHasTemp(){ return hasTemp; }
int weatherHigh(int i){ return (i >= 0 && i < 7) ? highs[i] : 0; }
int weatherLow(int i){ return (i >= 0 && i < 7) ? lows[i] : 0; }
