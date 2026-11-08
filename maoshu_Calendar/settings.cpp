#include <Preferences.h>
#include "common.h"
#include "settings.h"

// 运行期配置：NVS namespace "cfg"，网页可修改，掉电保留
static Preferences cfgPrefs;
static String wLat, wLon, wCity, bUid, bTopic;

void settingsBegin(){
  cfgPrefs.begin("cfg", true);
  wLat  = cfgPrefs.getString("lat", WEATHER_LAT);
  wLon  = cfgPrefs.getString("lon", WEATHER_LON);
  wCity = cfgPrefs.getString("city", WEATHER_CITY);
  bUid  = cfgPrefs.getString("uid", BEMFA_UID);
  bTopic = cfgPrefs.getString("topic", BEMFA_TOPIC);
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
