#ifndef __TFTUTIL_H
#define __TFTUTIL_H

#include <TFT_eSPI.h>

extern TFT_eSPI tft;
extern TFT_eSprite clk;

// ===== 主题色(日间/夜间自动切换) =====
extern uint16_t backFillColor;   // 页面背景
extern uint16_t penColor;        // 主文字
extern uint16_t subColor;        // 次要文字/灰字
extern uint16_t dividerColor;    // 分隔线
extern uint16_t orangeColor;     // 橙(标题/周日/晴)
extern uint16_t cyanColor;       // 青(周六/诊断)
extern uint16_t greenColor;      // 绿(黄历/宜/在线)
extern uint16_t redColor;        // 红(忌/今日高亮)
extern uint16_t lightCyanColor;  // 雪(天气)
extern uint16_t yellowColor;     // 雷(天气)
extern uint16_t sunColor;        // 周日日期
extern uint16_t satColor;        // 周六日期
extern uint16_t dateColor;       // 平日日期
extern uint16_t wifiRedColor;    // 离线红点
extern uint16_t waitColor;       // 等待提示灰

void tftInit();
void reflashTFT();
// 按时段套用主题(7:00-19:00日间)，主题发生变化时返回true
bool themeTick();

#endif
