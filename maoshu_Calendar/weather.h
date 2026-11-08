#ifndef __WEATHER_H
#define __WEATHER_H

#include <Arduino.h>

void weatherBegin();            // WiFi就绪后调用，发起首次获取
void weatherLoop();             // 主循环调用，每30分钟自动刷新
void weatherRefreshNow();       // 配置页改了地区后调用，立即按新坐标刷新
bool weatherValid();            // 是否已获取到数据
int  weatherCode(int i);        // 第i天(0=今天)的简化天气代码
                                // 0晴 1多云 2阴 3雨 4雪 5雾 6雷雨
bool weatherHasTemp();          // 是否已获取到高低温
int  weatherHigh(int i);        // 第i天最高温(℃)
int  weatherLow(int i);         // 第i天最低温(℃)

#endif
