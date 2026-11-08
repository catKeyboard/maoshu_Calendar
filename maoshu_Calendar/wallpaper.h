#ifndef __WALLPAPER_H
#define __WALLPAPER_H

#include "common.h"
#include <TFT_eSPI.h>

// 壁纸：日/夜两张图编译进固件，随主题自动切换
// 日历页双击进入壁纸页；单击换下一套，双击启停，长按回日历
void wallpaperBegin();                 // 初始化(套编号越界时夹紧)
bool wallpaperOn();                    // 壁纸是否启用作日历页背景
int  wallpaperSetIndex();              // 当前套编号(0起)
int  wallpaperSetCount();              // 固件内置套数
void wallpaperNextSet();               // 切到下一套(存NVS)
bool wallpaperToggle();                // 启用/停用(存NVS)，返回新状态
void wallpaperDrawBg(TFT_eSprite &s);  // 把当前壁纸铺到sprite(0,0)
void drawWallPage();                   // 壁纸页整页绘制

#endif
