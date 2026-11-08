#ifndef __MEMO_H
#define __MEMO_H

#include "common.h"
#include <TFT_eSPI.h>

// 备忘录功能（通过巴法云推送，手机微信小程序/巴法App、电脑网页控制台均可推送）
void memoBegin(void);        // 初始化：读取NVS中的备忘录并准备云端连接
void memoCloudLoop(void);    // 主循环调用：维持MQTT连接、接收云端推送
void memoLoop(void);         // 备忘录页面轮询：多页时自动翻页
void memoNextPage(void);     // 手动翻到下一页
void drawMemoPage(void);     // 绘制整个备忘录页面
void drawMemoContent(void);  // 只重绘内容区（翻页时用）
void memoClearAll(void);     // 清空全部备忘录
bool memoConfigured(void);   // 是否已配置巴法云推送

// 供日历页复用的点阵文字绘制(16x16中文字库 + 8x16 ASCII)
int  drawMemoChar(TFT_eSprite &s, int x, int y, uint32_t cp, uint16_t color);
void drawMemoString(TFT_eSprite &s, int x, int y, const String& t, uint16_t color);
int  memoTextWidth(const String& t);  // 计算点阵文字显示宽度(像素)

#endif
