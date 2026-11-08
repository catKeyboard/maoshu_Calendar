#ifndef __SETTINGS_H
#define __SETTINGS_H

#include <Arduino.h>

// 运行期配置(NVS保存，网页可改，掉电保留)
// 未配置时回退到 common.h 里的默认值
void settingsBegin();

String cfgWeatherLat();                 // 天气纬度字符串，如 "25.61"
String cfgWeatherLon();                 // 天气经度字符串，如 "100.27"
String cfgWeatherCity();                // 地区名称，如 "云南大理"
void   settingsSetWeather(const String& lat, const String& lon, const String& city);

String cfgBemfaUid();                   // 巴法云私钥UID
String cfgBemfaTopic();                 // 巴法云主题名
void   settingsSetBemfa(const String& uid, const String& topic);

int  cfgPomoFocus();                    // 番茄钟专注时长(分钟)
int  cfgPomoShort();                    // 短休息(分钟)
int  cfgPomoLong();                     // 长休息(分钟)
int  cfgPomoCycle();                    // 每几个专注进入长休息
void settingsSetPomo(int focus, int shortMin, int longMin, int cycle);

#endif
