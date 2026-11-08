/*
 * maoshu_Calendar - 日历 + 备忘录 + 番茄钟
 * 硬件：安信可 Core-ESP32(ESP32-C3) + 2.4寸 ST7789V2 SPI屏(240x320, 横屏320x240, 4线SPI) + 单按键(GPIO8)
 * 功能：
 *   - 日历主页：公历月历 + 农历/干支/节日(NTP校时，断网不丢农历)
 *   - 备忘录页：巴法云推送，NVS保存，断电不丢
 *   - 番茄钟页：专注/短休/长休自动循环，时长网页可配
 * 交互(单按键)：
 *   - 日历页 单击 → 备忘录页；备忘录页 单击翻页；番茄页 单击开始/暂停
 *   - 备忘录页 双击回日历 / 长按3秒清空
 *   - 番茄页 双击重置当前阶段 / 长按回日历
 *   - 收到新备忘录自动跳到备忘录页
 */
#include <WiFi.h>
#include <time.h>
#include <OneButton.h>
#include "common.h"
#include "tftUtil.h"
#include "cal.h"
#include "memo.h"
#include "net.h"
#include "weather.h"
#include "settings.h"
#include "pomodoro.h"

// 页面状态
enum CurrentPage currentPage = PAGE_CAL;
static OneButton button(BUTTON, true);
static int lastMin = -1;

// 单击：日历→备忘录→番茄→日历 循环；备忘录翻页；番茄开始/暂停
static void onClick(){
  if(currentPage == PAGE_CAL){
    currentPage = PAGE_MEMO;
    drawMemoPage();
  } else if(currentPage == PAGE_MEMO){
    memoNextPage();
  } else {
    pomoClick();
  }
}
// 双击：备忘录→日历；番茄→重置当前阶段
static void onDoubleClick(){
  if(currentPage == PAGE_MEMO){
    currentPage = PAGE_CAL;
    drawCalPage();
  } else if(currentPage == PAGE_POMO){
    pomoReset();
  }
}
// 长按3秒：备忘录清空全部；番茄回日历
static void onLongPress(){
  if(currentPage == PAGE_MEMO){
    memoClearAll();
  } else if(currentPage == PAGE_POMO){
    currentPage = PAGE_CAL;
    drawCalPage();
  }
}

void setup(){
  Serial.begin(115200);
  tftInit();
  Serial.println("maoshu_Calendar 启动");
  calInit();
  settingsBegin(); // 读取NVS中的天气地区/巴法云/番茄钟配置
  netBegin();    // WiFi/NTP(无凭证或失败进入配网模式)
  memoBegin();   // 备忘录 NVS + 巴法云连接
  weatherBegin(); // 一周天气(Open-Meteo)
  pomoBegin();   // 番茄钟(默认停在专注25:00，待开始)
  button.attachClick(onClick);
  button.attachDoubleClick(onDoubleClick);
  button.attachLongPressStart(onLongPress);
  button.setPressMs(3000);
  drawCalPage();  // 默认日历页(未联网时显示等待同步提示)
}

void loop(){
  button.tick();
  memoCloudLoop();
  netLoop();
  weatherLoop();
  pomoLoop();     // 番茄钟始终计时(切到其他页也继续)
  // 主题自动切换(7:00-19:00日间)：变化时重绘当前页
  static unsigned long lastTheme = 0;
  if(millis() - lastTheme > 10000){
    lastTheme = millis();
    if(themeTick()){
      if(currentPage == PAGE_CAL) drawCalPage();
      else if(currentPage == PAGE_MEMO) drawMemoPage();
      else drawPomoPage();
    }
  }
  if(currentPage == PAGE_MEMO) memoLoop();
  // 日历页每分钟整页刷新；未同步期间每5秒刷新诊断
  if(currentPage == PAGE_CAL){
    time_t now = time(nullptr);
    if(now > 100000){
      struct tm* t = localtime(&now);
      if(t->tm_min != lastMin){
        lastMin = t->tm_min;
        drawCalPage();
      }
    }else{
      static unsigned long lastDiag = 0;
      if(millis() - lastDiag > 5000){ lastDiag = millis(); drawCalPage(); }
    }
  }
}
