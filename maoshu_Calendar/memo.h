#ifndef __MEMO_H
#define __MEMO_H

#include "common.h"
#include <TFT_eSPI.h>

// 单条备忘录最大字节数(UTF-8)，约 200 个汉字
#define MEMO_MAX_BYTES 600

// 备忘录功能（巴法云推送 + 局域网网页推送，两者共用同一入队/存储路径）
void memoBegin(void);        // 初始化：读取NVS中的备忘录并准备云端连接
void memoCloudLoop(void);    // 主循环调用：维持MQTT连接、接收云端推送
void memoNextPage(void);     // 手动翻到下一页
bool memoAtLastPage(void);   // 是否已显示最后一页(末页单击进番茄页)
void drawMemoPage(void);     // 绘制整个备忘录页面
void drawMemoContent(void);  // 只重绘内容区（翻页时用）
void memoClearAll(void);     // 清空全部备忘录
void memoAddText(String txt, bool fromCloud = true); // 新增一条(巴法云/局域网同一路径)
void memoReloadConfig(void); // 配置页改了巴法云参数后调用，重新连接
bool memoConfigured(void);   // 是否已配置巴法云推送

// 供日历页复用的点阵文字绘制(16x16中文字库 + 8x16 ASCII)
int  drawMemoChar(TFT_eSprite &s, int x, int y, uint32_t cp, uint16_t color);
void drawMemoString(TFT_eSprite &s, int x, int y, const String& t, uint16_t color);
int  memoTextWidth(const String& t);  // 计算点阵文字显示宽度(像素)

#endif
