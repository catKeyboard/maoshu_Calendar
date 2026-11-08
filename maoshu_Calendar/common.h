#ifndef __COMMON_H
#define __COMMON_H

#include <Arduino.h>

// ===== 硬件 =====
#define BUTTON 8   // 按键引脚(与原 DuduClock 相同)

// ===== NTP 校时 =====
#define NTP1 "ntp.aliyun.com"
#define NTP2 "ntp1.aliyun.com"
#define NTP3 "ntp.ntsc.ac.cn"     // 备用：国家授时中心
#define TZ_INFO "CST-8"          // 中国标准时间 UTC+8

// ===== 巴法云推送(备忘录) =====
// 手机端：微信搜索"巴法云"小程序，绑定账号后选择主题发送消息
// 电脑端：登录 https://bemfa.com 网页控制台推送
#define BEMFA_UID   "youUID"   // 巴法云私钥UID
#define BEMFA_TOPIC "yourTopic"                              // 巴法云主题名

// ===== 天气(Open-Meteo免费API，无需密钥) =====
// 坐标改成你所在城市的经纬度(保留两位小数即可)
#define WEATHER_LAT "25.61"   // 纬度(云南大理)
#define WEATHER_LON "100.27"  // 经度(云南大理)

// ===== 配网热点 =====
#define AP_SSID "maoshu_Calendar"
#define AP_PASS "12345678"

// ===== 页面枚举 =====
enum CurrentPage { PAGE_CAL, PAGE_MEMO };
extern enum CurrentPage currentPage;

#endif
