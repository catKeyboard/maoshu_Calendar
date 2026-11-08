#include <Preferences.h>
#include "common.h"
#include "settings.h"

// 运行期配置：NVS namespace "cfg"，网页可修改，掉电保留
static Preferences cfgPrefs;
static String wLat, wLon, wCity, bUid, bTopic;
static int pFocus, pShort, pLong, pCycle;
static bool wallOn;
static int wallSet;

void settingsBegin(){
  cfgPrefs.begin("cfg", true);
  wLat  = cfgPrefs.getString("lat", WEATHER_LAT);
  wLon  = cfgPrefs.getString("lon", WEATHER_LON);
  wCity = cfgPrefs.getString("city", WEATHER_CITY);
  bUid  = cfgPrefs.getString("uid", BEMFA_UID);
  bTopic = cfgPrefs.getString("topic", BEMFA_TOPIC);
  pFocus = cfgPrefs.getInt("pFocus", POMO_FOCUS_MIN);
  pShort = cfgPrefs.getInt("pShort", POMO_SHORT_MIN);
  pLong  = cfgPrefs.getInt("pLong", POMO_LONG_MIN);
  pCycle = cfgPrefs.getInt("pCycle", POMO_CYCLE);
  wallOn = cfgPrefs.getBool("wpOn", false);
  wallSet = cfgPrefs.getInt("wpSet", 0);
  cfgPrefs.end();
  // 开源固件里的占位符视为"未配置"
  if(bUid == "youUID") bUid = "";
  if(bTopic == "yourTopic") bTopic = "";
  Serial.println("配置: " + wCity + " (" + wLat + "," + wLon + ")");
}

String cfgWeatherLat(){ return wLat; }
String cfgWeatherLon(){ return wLon; }
String cfgWeatherCity(){ return wCity; }

void settingsSetWeather(const String& lat, const String& lon, const String& city){
  if(lat.length()) wLat = lat;
  if(lon.length()) wLon = lon;
  if(city.length()) wCity = city;
  cfgPrefs.begin("cfg", false);
  cfgPrefs.putString("lat", wLat);
  cfgPrefs.putString("lon", wLon);
  cfgPrefs.putString("city", wCity);
  cfgPrefs.end();
}

String cfgBemfaUid(){ return bUid; }
String cfgBemfaTopic(){ return bTopic; }

void settingsSetBemfa(const String& uid, const String& topic){
  bUid = uid;
  bTopic = topic;
  cfgPrefs.begin("cfg", false);
  cfgPrefs.putString("uid", bUid);
  cfgPrefs.putString("topic", bTopic);
  cfgPrefs.end();
}

int cfgPomoFocus(){ return pFocus; }
int cfgPomoShort(){ return pShort; }
int cfgPomoLong(){ return pLong; }
int cfgPomoCycle(){ return pCycle; }

void settingsSetPomo(int focus, int shortMin, int longMin, int cycle){
  // 时长限制 1~120 分钟，循环数 1~12
  auto clamp = [](int v, int lo, int hi){ return v < lo ? lo : (v > hi ? hi : v); };
  pFocus = clamp(focus, 1, 120);
  pShort = clamp(shortMin, 1, 60);
  pLong  = clamp(longMin, 1, 60);
  pCycle = clamp(cycle, 1, 12);
  cfgPrefs.begin("cfg", false);
  cfgPrefs.putInt("pFocus", pFocus);
  cfgPrefs.putInt("pShort", pShort);
  cfgPrefs.putInt("pLong", pLong);
  cfgPrefs.putInt("pCycle", pCycle);
  cfgPrefs.end();
}

bool cfgWallOn(){ return wallOn; }
int  cfgWallSet(){ return wallSet; }

void settingsSetWall(bool on, int set){
  wallOn = on;
  wallSet = set < 0 ? 0 : set;
  cfgPrefs.begin("cfg", false);
  cfgPrefs.putBool("wpOn", wallOn);
  cfgPrefs.putInt("wpSet", wallSet);
  cfgPrefs.end();
}
