#ifndef __ALARM_H
#define __ALARM_H

#include <Arduino.h>
#include "common.h"

// 闹钟：最多 ALARM_MAX 组，网页配置，NVS 保存
// 到点对 BUZZER_PIN 上的有源蜂鸣器持续拉响 ALARM_RING_MS，任意按键停止
// 触发判定按"分钟"做，阻塞式网络请求（天气首取最长 8 秒）不会错过整分钟
void alarmBegin();                 // 初始化引脚 + 从 NVS 读 4 组
void alarmLoop();                  // 主循环调用(非阻塞)：到点拉响、超时自动停
bool alarmRinging();               // 是否正在响
void alarmStop();                  // 停止(按键调用)
bool alarmGet(int i, bool& on, int& hour, int& minute, bool& once, bool& fired);
void alarmSet(int i, bool on, int hour, int minute, bool once);  // 网页保存(顺带清除"已响过")
String alarmJson();                // /status 用："[{on,t,once,fired},...]"

// 番茄钟阶段提示音(复用同一蜂鸣器，非阻塞节拍，由 alarmLoop 驱动)
void beepFocusDone();              // 专注结束：两声短促
void beepRestDone();               // 休息结束：一声长

#endif
