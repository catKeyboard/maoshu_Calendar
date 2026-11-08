#ifndef __CAL_H
#define __CAL_H

#include <Arduino.h>

// 农历查询结果
typedef struct {
  int lYear, lMonth, lDay;
  bool isLeap;
  String monthCn;   // "正月"/"冬月"/"闰四月"
  String dayCn;     // "初一"/"廿一"
  String festival;  // 农历/公历节日名(无则空)
} LunarInfo;

void calInit();                                   // 预留初始化
LunarInfo calSolar2Lunar(int y, int m, int d);    // 公历转农历
String calYearGZ(int lYear);                      // 农历年干支+生肖 如"丙午马年"
int calDaysInMonth(int y, int m);                 // 当月天数
int calDow(int y, int m, int d);                  // 星期 0=周日
void drawCalPage();                               // 绘制日历主页面(整页)

#endif
