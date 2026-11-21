#include "wallpaper.h"
#include "wallpaper_data.h"
#include "settings.h"
#include "tftUtil.h"
#include "memo.h"
#include "net.h"

// 壁纸数据来自 wallpaper_data.h（由 tools/wallpaper_convert.py 生成）
// 每套一张白天版，320x240 RGB565，直接读Flash铺进sprite

static int clampSet(int i){
  if(i < 0) return 0;
  if(i >= WALLPAPER_SET_COUNT) return WALLPAPER_SET_COUNT - 1;
  return i;
}

void wallpaperBegin(){
  // 桌面改版：首次升级自动启用第1套(插画面板桌面)
  if(cfgWallRev() < 3){
    settingsSetWall(true, 0);
    settingsSetWallRev(3);
  }
  int s = clampSet(cfgWallSet());
  if(s != cfgWallSet()) settingsSetWall(cfgWallOn(), s);
}

bool wallpaperOn(){ return cfgWallOn(); }
int  wallpaperSetIndex(){ return clampSet(cfgWallSet()); }
int  wallpaperSetCount(){ return WALLPAPER_SET_COUNT; }

// 当前套的壁纸图（只有白天版）
static const uint16_t* currentImage(){
  return (const uint16_t*)wallpaperDay[wallpaperSetIndex()];
}

// 壁纸原图铺进sprite(0,0)
void wallpaperDrawBg(TFT_eSprite &s){
  const uint16_t *src = currentImage();
  uint16_t *dst = (uint16_t*)s.getPointer();
  if(dst == nullptr) return;
  int n = WALLPAPER_W * WALLPAPER_H;
  for(int i = 0; i < n; i++){
    uint16_t c = pgm_read_word(&src[i]);
    // sprite 内部为字节交换格式存储(与fillRect等绘图一致)
    dst[i] = (uint16_t)((c >> 8) | (c << 8));
  }
}

void wallpaperNextSet(){
  if(WALLPAPER_SET_COUNT < 1) return;
  int n = (wallpaperSetIndex() + 1) % WALLPAPER_SET_COUNT;
  settingsSetWall(cfgWallOn(), n);
}

bool wallpaperToggle(){
  bool on = !cfgWallOn();
  settingsSetWall(on, cfgWallSet());
  return on;
}

// 壁纸页：整屏预览 + 顶栏/底部实底说明条
void drawWallPage(){
  clk.setColorDepth(16);
  clk.createSprite(320, 240);
  wallpaperDrawBg(clk);   // 预览显示原图
  // 顶栏与底部用主题底色遮住图片，保证文字清晰
  clk.fillRect(0, 0, 320, 28, backFillColor);
  clk.fillRect(0, 196, 320, 44, backFillColor);
  clk.drawFastHLine(0, 28, 320, dividerColor);
  clk.drawFastHLine(0, 196, 320, dividerColor);
  // 顶栏：壁纸 | 启用状态 | 第N/M套 | WiFi点
  drawMemoString(clk, 8, 8, "壁纸", orangeColor);
  String st = cfgWallOn() ? "已启用" : "已停用";
  drawMemoString(clk, 160 - memoTextWidth(st) / 2, 8, st,
                 cfgWallOn() ? greenColor : subColor);
  String idx = String("第 ") + String(wallpaperSetIndex() + 1) + "/"
             + String(WALLPAPER_SET_COUNT) + " 套";
  drawMemoString(clk, 294 - memoTextWidth(idx), 8, idx, subColor);
  clk.fillCircle(310, 14, 5, netConnected() ? greenColor : wifiRedColor);
  // 底部：当前状态 + 操作提示
  String mode = cfgWallOn() ? "已开启" : "未开启";
  drawMemoString(clk, (320 - memoTextWidth(mode)) / 2, 202, mode, penColor);
  String tip = "单击 换壁纸  双击 启停  长按 返回";
  drawMemoString(clk, (320 - memoTextWidth(tip)) / 2, 220, tip, subColor);
  clk.pushSprite(0, 0);
  clk.deleteSprite();
}
