#ifndef __MEMO_H
#define __MEMO_H

#include "common.h"
#include <TFT_eSPI.h>

// 单条备忘录最大字节数(UTF-8)，约 200 个汉字
#define MEMO_MAX_BYTES 600

// 备忘录功能（巴法云推送 + 局域网网页推送，两者共用同一入队/存储路径）
void memoBegin(void);        // 初始化：读取NVS中的备忘录并准备云端连接
void memoCloudLoop(void);    // 主循环调用：维持MQTT连接、接收云端推送
void memoNextPage(void);     // 单击：下一页(末页回到第1页)
void memoPrevPage(void);     // 双击：上一页(第1页回末页)
void memoFirstPage(void);    // 三击：回到第1页(最新内容)
void memoMenuStart(void);    // 备忘录页长按：弹出操作菜单(退出/清空)
bool memoMenuActive(void);   // 菜单是否打开
void memoMenuClose(void);    // 关闭菜单(不重绘，调用方负责刷新)
void memoMenuCancel(void);   // 关闭菜单并重绘备忘录内容区
void memoMenuLoop(void);     // 主循环调用：菜单8秒无操作自动关闭
void drawMemoPage(void);     // 绘制整个备忘录页面
void drawMemoContent(void);  // 只重绘内容区（翻页时用）
void memoClearAll(void);     // 清空全部备忘录(删除NVS条目，保留云端去重标记)
void memoClearConfirmStart(void);  // 长按第一次：屏上显示清空确认提示
bool memoClearConfirmActive(void);  // 是否处于清空确认状态
void memoClearConfirmCancel(void);  // 单击取消：清除提示并重绘备忘录页
void memoClearConfirmLoop(void);    // 主循环调用：确认提示8秒未执行则自动取消
void memoAddText(String txt, bool fromCloud = true); // 新增一条(巴法云/局域网同一路径)
void memoReloadConfig(void); // 配置页改了巴法云参数后调用，重新连接
bool memoConfigured(void);   // 是否已配置巴法云推送

// 供日历页复用的点阵文字绘制(16x16中文字库 + 8x16 ASCII)
int  drawMemoChar(TFT_eSprite &s, int x, int y, uint32_t cp, uint16_t color);
void drawMemoString(TFT_eSprite &s, int x, int y, const String& t, uint16_t color);
int  memoTextWidth(const String& t);  // 计算点阵文字显示宽度(像素)

#endif
