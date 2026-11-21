/*
 * maoshu_Calendar - 日历 + 备忘录 + 番茄钟
 * 硬件：安信可 Core-ESP32(ESP32-C3) + 2.4寸 ST7789V2 SPI屏(240x320, 横屏320x240, 4线SPI) + 单按键(GPIO8)
 * 功能：
 *   - 桌面页(日历)：本周日程卡 + 大时钟 + 今日天气/宜忌
 *   - 天气页：本周7天预报(Open-Meteo)
 *   - 备忘录页：巴法云推送，NVS保存，断电不丢
 *   - 番茄钟页：专注/短休/长休自动循环，时长网页可配
 *   - 壁纸页：日/夜壁纸随主题切换，单击换套、双击启停
 * 交互(单按键)：
 *   - 桌面页 单击 → 备忘录页；双击 → 壁纸页；长按 → 本周天气页
 *   - 备忘录页 单击翻页；番茄页 单击开始/暂停
 *   - 备忘录页 双击回桌面 / 长按3秒清空
 *   - 番茄页 双击重置当前阶段 / 长按回桌面
 *   - 壁纸页 单击换下一套 / 双击启用停用 / 长按回桌面
 *   - 天气页 单击或长按回桌面
 *   - 收到新备忘录自动跳到备忘录页；翻页只由按键触发，无自动翻页
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
#include "wallpaper.h"
#include "alarm.h"

// 页面状态
enum CurrentPage currentPage = PAGE_CAL;
static OneButton button(BUTTON, true);
static int lastMin = -1;

// 单击：桌面→备忘录；备忘录翻页(末页再单击进番茄)；壁纸换套；天气/番茄按页面处理
static void onClick(){
  if(alarmRinging()){ alarmStop(); return; }   // 闹钟响时任意按键停止，不切页
  if(calBootInfoActive()){ calBootInfoDismiss(); return; }   // 首次操作：关掉启动信息页
  if(memoClearConfirmActive()){ memoClearConfirmCancel(); return; }   // 清空确认中：单击取消
  if(currentPage == PAGE_CAL){
    currentPage = PAGE_MEMO;
    drawMemoPage();
  } else if(currentPage == PAGE_MEMO){
    if(memoAtLastPage()){
      currentPage = PAGE_POMO;
      drawPomoPage();
    } else {
      memoNextPage();
    }
  } else if(currentPage == PAGE_WALL){
    wallpaperNextSet();
    drawWallPage();
  } else if(currentPage == PAGE_WEATHER){
    currentPage = PAGE_CAL;
    drawCalPage();
  } else {
    pomoClick();
  }
}
// 双击：桌面→天气；壁纸启用/停用；番茄重置；备忘录等返回桌面
static void onDoubleClick(){
  if(alarmRinging()){ alarmStop(); return; }
  if(calBootInfoActive()){ calBootInfoDismiss(); return; }
  if(memoClearConfirmActive()){ memoClearConfirmCancel(); return; }   // 清空确认中：取消
  if(currentPage == PAGE_CAL){
    currentPage = PAGE_WEATHER;
    drawWeatherPage();
  } else if(currentPage == PAGE_WALL){
    wallpaperToggle();
    drawWallPage();
  } else if(currentPage == PAGE_POMO){
    pomoReset();
  } else {
    currentPage = PAGE_CAL;
    drawCalPage();
  }
}
// 三击及以上：桌面→壁纸；其他页面等同双击
static void onMultiClick(){
  if(alarmRinging()){ alarmStop(); return; }
  if(calBootInfoActive()){ calBootInfoDismiss(); return; }
  if(memoClearConfirmActive()){ memoClearConfirmCancel(); return; }   // 清空确认中：取消
  if(currentPage == PAGE_CAL){
    currentPage = PAGE_WALL;
    drawWallPage();
  } else {
    onDoubleClick();
  }
}
// 长按3秒：备忘录需二次长按确认清空；番茄/壁纸/天气回日历
static void onLongPress(){
  if(alarmRinging()){ alarmStop(); return; }
  if(calBootInfoActive()){ calBootInfoDismiss(); return; }
  if(currentPage == PAGE_MEMO){
    if(memoClearConfirmActive()) memoClearAll();   // 第二次长按才真正清空
    else memoClearConfirmStart();                  // 第一次长按只显示确认提示
  } else if(currentPage == PAGE_POMO || currentPage == PAGE_WALL || currentPage == PAGE_WEATHER){
    currentPage = PAGE_CAL;
    drawCalPage();
  }
}

void setup(){
  Serial.begin(115200);
  tftInit();
  Serial.println(String("maoshu_Calendar ") + FW_VERSION + " [设备 " + cfgDeviceId() + "] 启动");
  calInit();
  settingsBegin(); // 读取NVS中的天气地区/巴法云/番茄钟/壁纸配置
  wallpaperBegin(); // 壁纸开关与套编号
  netBegin();    // WiFi/NTP(无凭证或失败进入配网模式)
  memoBegin();   // 备忘录 NVS + 巴法云连接
  weatherBegin(); // 一周天气(Open-Meteo)
  pomoBegin();   // 番茄钟(默认停在专注25:00，待开始)
  alarmBegin();  // 闹钟4组(NVS) + 蜂鸣器静音初始化
  button.attachClick(onClick);
  button.attachDoubleClick(onDoubleClick);
  button.attachMultiClick(onMultiClick);
  button.attachLongPressStart(onLongPress);
  button.setClickTicks(300);   // 连击判定窗口(兼顾单击响应与三击)
  button.setPressMs(3000);
  if(inPortalMode()){
    // 配网模式：屏幕上保留配网提示(热点名/192.168.4.1)
  }else{
    calBootInfoShow();   // 启动信息页(版本+IP)，停留10秒或首次按键进入桌面
  }
}

void loop(){
  button.tick();
  alarmLoop();    // 闹钟判定(非阻塞，按分钟触发，60秒自动停)
  calBootInfoLoop(); // 启动信息页到时自动切回桌面
  memoClearConfirmLoop(); // 清空确认提示8秒未执行则自动取消
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
      else if(currentPage == PAGE_POMO) drawPomoPage();
      else if(currentPage == PAGE_WEATHER) drawWeatherPage();
      else drawWallPage();
    }
  }
  // 天气页等数据期间每5秒重绘一次
  if(currentPage == PAGE_WEATHER && !weatherValid()){
    static unsigned long lastWDiag = 0;
    if(millis() - lastWDiag > 5000){ lastWDiag = millis(); drawWeatherPage(); }
  }
  // 日历页每分钟整页刷新；未同步期间每5秒刷新诊断(启动信息页期间不刷新)
  if(currentPage == PAGE_CAL && !calBootInfoActive()){
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
