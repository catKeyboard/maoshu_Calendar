#ifndef __WEATHER_H
#define __WEATHER_H

#include <Arduino.h>

void weatherBegin();            // WiFi就绪后调用，发起首次获取
void weatherLoop();             // 主循环调用，每30分钟自动刷新
bool weatherValid();            // 是否已获取到数据
int  weatherCode(int i);        // 第i天(0=今天)的简化天气代码
                                // 0晴 1多云 2阴 3雨 4雪 5雾 6雷雨

#endif
