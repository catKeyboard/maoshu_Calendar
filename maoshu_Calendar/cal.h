#ifndef __CAL_H
#define __CAL_H

#include <Arduino.h>

// 农历查询结果
typedef struct {
  int lYear, lMonth, lDay;
  bool isLeap;
  String monthCn;   // "正月"/"冬月"/"闰四月"
  String dayCn;     // "初一"/"廿一"
} LunarInfo;

void calInit();                                   // 预留初始化
LunarInfo calSolar2Lunar(int y, int m, int d);    // 公历转农历
void drawCalPage();                               // 绘制桌面页(星期/大时钟/农历/天气/宜忌)
void drawWeatherPage();                           // 绘制本周天气页

// 启动信息页(版本+IP)：停留10秒或首次按键进入桌面
void calBootInfoShow();                           // 显示并开始计时
void calBootInfoLoop();                           // 主循环调用：到时自动回桌面
void calBootInfoDismiss();                        // 首次按键：立即进入桌面
bool calBootInfoActive();                         // 是否仍停在启动信息页

#endif
