#ifndef __COMMON_H
#define __COMMON_H

#include <Arduino.h>

// ===== 固件版本 =====
#define FW_VERSION "V01.9"

// ===== 硬件 =====
#define BUTTON 8   // 按键引脚(与原 DuduClock 相同)

// ===== 闹钟蜂鸣器(有源蜂鸣器：正极→BUZZER_PIN，负极→GND) =====
// GPIO6 空闲、非 Strapping 脚(GPIO2/8/9 才是)，与 SPI(2/3/4/5/7)和按键(8)不冲突
#define BUZZER_PIN 6
#define BUZZER_ACTIVE_LOW 0   // 1=低电平触发(静音时输出高)；本机用高电平触发
#define ALARM_RING_MS 60000   // 到点持续响 60 秒，按键可提前停止
#define ALARM_BEEP_ON_MS 300  // 间歇响：响 300ms
#define ALARM_BEEP_OFF_MS 250 // 间歇响：停 250ms
#define ALARM_MAX 4           // 最多 4 组

// 番茄钟阶段提示音(与闹钟共用蜂鸣器)：专注结束两声短促，休息结束一声长
#define BEEP_FOCUS_ON_MS  200 // 短促音时长
#define BEEP_FOCUS_GAP_MS 120 // 两声之间的间隙
#define BEEP_REST_ON_MS   600 // 休息结束的长音

// ===== NTP 校时 =====
#define NTP1 "ntp.aliyun.com"
#define NTP2 "ntp1.aliyun.com"
#define NTP3 "ntp.ntsc.ac.cn"     // 备用：国家授时中心
#define TZ_INFO "CST-8"          // 中国标准时间 UTC+8

// ===== 巴法云推送(备忘录) =====
// 手机端：微信搜索"巴法云"小程序，绑定账号后选择主题发送消息
// 电脑端：登录 https://bemfa.com 网页控制台推送
#define BEMFA_UID   "youUID"                             // 巴法云私钥UID(开源占位符，使用前替换为你自己的)
#define BEMFA_TOPIC "yourTopic"                          // 巴法云主题名(开源占位符，使用前替换为你自己的)

// ===== 天气(Open-Meteo免费API，无需密钥) =====
// 坐标改成你所在城市的经纬度(保留两位小数即可)；联网后也可在设备配置网页里改
#define WEATHER_LAT "25.61"   // 纬度(云南大理)
#define WEATHER_LON "100.27"  // 经度(云南大理)
#define WEATHER_CITY "云南大理"  // 地区名称(仅用于网页显示)

// ===== 配网热点 =====
#define AP_SSID "maoshu_Calendar"
#define AP_PASS "12345678"

// ===== 番茄钟默认时长(分钟，网页可改并存NVS) =====
#define POMO_FOCUS_MIN 25   // 专注
#define POMO_SHORT_MIN 5    // 短休息
#define POMO_LONG_MIN  15   // 长休息
#define POMO_CYCLE     4    // 每完成几个专注进入长休息

// ===== 页面枚举 =====
enum CurrentPage { PAGE_CAL, PAGE_MEMO, PAGE_POMO, PAGE_WALL, PAGE_WEATHER };
extern enum CurrentPage currentPage;

#endif
