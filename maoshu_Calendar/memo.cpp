#include <WiFi.h>
#include <PubSubClient.h>
#include <Preferences.h>
#include <vector>
#include "common.h"
#include "tftUtil.h"
#include "memo.h"
#include "font/memo_font_16.h"

///////////////// 参数配置 /////////////////
#define MEMO_MAX_COUNT 10      // 最多保存10条备忘录
#define MEMO_MAX_BYTES 240     // 单条备忘录最大字节数(UTF-8)，约80个汉字
#define MEMO_MQTT_SERVER "bemfa.com"  // 巴法云MQTT服务器
#define MEMO_MQTT_PORT   9501        // 巴法云MQTT端口
#define MEMO_LINES_PER_PAGE 8        // 每页显示的行数(横屏240高)
#define MEMO_PAGE_INTERVAL  6000     // 自动翻页间隔(毫秒)
/////////////////////////////////////////////

// NVS存储：namespace "memo"，键 cnt、m0~m9、lastPush
Preferences memoPrefs;
String memoList[MEMO_MAX_COUNT]; // 备忘录缓存(供绘制使用)
int memoCount = 0;               // 当前备忘录条数
String lastPushText;             // 最后处理的一条云端消息，用于过滤巴法云保留消息
int memoPageIdx = 0;             // 当前显示的页码
int memoPages = 1;               // 总页数

WiFiClient memoClient;
PubSubClient mqtt(memoClient);

// 判断是否配置了巴法云
bool memoConfigured(){
  return String(BEMFA_UID).length() > 0 && String(BEMFA_TOPIC).length() > 0;
}

///////////////// NVS存取区域 /////////////////
// 从NVS读取备忘录到缓存
void memoLoadFromNVS(){
  memoCount = 0;
  memoPrefs.begin("memo", true); // 只读模式
  int cnt = memoPrefs.getInt("cnt", 0);
  if(cnt > MEMO_MAX_COUNT) cnt = MEMO_MAX_COUNT;
  for(int i = 0; i < cnt; i++){
    memoList[i] = memoPrefs.getString(("m" + String(i)).c_str(), "");
  }
  memoCount = cnt;
  lastPushText = memoPrefs.getString("lastPush", "");
  memoPrefs.end();
}
// 将缓存写回NVS
void memoSaveToNVS(){
  memoPrefs.begin("memo", false);
  memoPrefs.putInt("cnt", memoCount);
  for(int i = 0; i < MEMO_MAX_COUNT; i++){
    if(i < memoCount){
      memoPrefs.putString(("m" + String(i)).c_str(), memoList[i]);
    }else{
      memoPrefs.putString(("m" + String(i)).c_str(), "");
    }
  }
  memoPrefs.end();
}
//////////////////////////////////////////////

///////////////// UTF-8与字库绘制区域 /////////////////
// UTF-8截断：保证不切断多字节字符
String utf8Truncate(const String& s, int maxBytes){
  if((int)s.length() <= maxBytes) return s;
  int i = maxBytes;
  while(i > 0 && ((unsigned char)s[i] & 0xC0) == 0x80) i--;
  return s.substring(0, i);
}
// 从s的i位置解码一个UTF-8字符，返回码点，i前进到下一字符
uint32_t utf8Next(const String& s, int &i){
  unsigned char c = s[i];
  int len;
  uint32_t cp;
  if(c < 0x80){
    i++;
    return c;
  }else if((c & 0xE0) == 0xC0){
    len = 2; cp = c & 0x1F;
  }else if((c & 0xF0) == 0xE0){
    len = 3; cp = c & 0x0F;
  }else if((c & 0xF8) == 0xF0){
    len = 4; cp = c & 0x07;
  }else{
    i++;
    return 0xFFFD;
  }
  if(i + len > (int)s.length()){
    i++;
    return 0xFFFD;
  }
  for(int k = 1; k < len; k++){
    unsigned char cc = s[i + k];
    if((cc & 0xC0) != 0x80){
      i++;
      return 0xFFFD;
    }
    cp = (cp << 6) | (cc & 0x3F);
  }
  i += len;
  return cp;
}
// 计算字符串显示宽度(像素)
int memoTextWidth(const String& t){
  int w = 0;
  int i = 0;
  while(i < (int)t.length()){
    uint32_t cp = utf8Next(t, i);
    w += (cp < 0x80) ? 8 : 16;
  }
  return w;
}
// 在精灵上绘制一个字符(ASCII为8x16点阵，汉字为16x16点阵)，返回该字符宽度
int drawMemoChar(TFT_eSprite &s, int x, int y, uint32_t cp, uint16_t color){
  if(cp < 0x80){
    if(cp < 0x20 || cp > 0x7E) return 8; // 控制字符等不可显示，仅留空
    for(int row = 0; row < 16; row++){
      unsigned char bits = pgm_read_byte(&memoFontAscii[cp - 0x20][row]);
      for(int col = 0; col < 8; col++){
        if(bits & (0x80 >> col)) s.drawPixel(x + col, y + row, color);
      }
    }
    return 8;
  }
  // 二分查找汉字字模(字库按Unicode升序排列)
  int lo = 0, hi = MEMO_CJK_COUNT - 1, idx = -1;
  while(lo <= hi){
    int mid = (lo + hi) / 2;
    unsigned short v = pgm_read_word(&memoFontCp[mid]);
    if(v == cp){
      idx = mid;
      break;
    }else if(v < cp){
      lo = mid + 1;
    }else{
      hi = mid - 1;
    }
  }
  if(idx < 0){ // 字库中没有的字符(生僻字/emoji等)，画空心方框占位提示
    s.drawRect(x + 1, y + 1, 14, 14, color);
    return 16;
  }
  for(int row = 0; row < 16; row++){
    unsigned int bits = ((unsigned int)pgm_read_byte(&memoFontData[idx][row * 2]) << 8)
                      | pgm_read_byte(&memoFontData[idx][row * 2 + 1]);
    for(int col = 0; col < 16; col++){
      if(bits & (0x8000 >> col)) s.drawPixel(x + col, y + row, color);
    }
  }
  return 16;
}
// 在精灵上绘制一整行文字
void drawMemoString(TFT_eSprite &s, int x, int y, const String& t, uint16_t color){
  int i = 0;
  while(i < (int)t.length()){
    uint32_t cp = utf8Next(t, i);
    x += drawMemoChar(s, x, y, cp, color);
  }
}
// 绘制备忘录大标题的一个字(32x32点阵)
void drawMemoTitleGlyph(int glyphIdx, int x, int y){
  for(int row = 0; row < 32; row++){
    for(int b = 0; b < 4; b++){
      unsigned char bits = pgm_read_byte(&memoTitle[glyphIdx][row * 4 + b]);
      for(int col = 0; col < 8; col++){
        if(bits & (0x80 >> col)) tft.drawPixel(x + b * 8 + col, y + row, penColor);
      }
    }
  }
}
//////////////////////////////////////////////////////

///////////////// 云端接收区域 /////////////////
void memoAddText(String txt); // 前向声明
// 收到云端消息
void mqttCallback(char* topic, byte* payload, unsigned int length){
  String txt = "";
  for(unsigned int i = 0; i < length; i++){
    txt += (char)payload[i];
  }
  memoAddText(txt);
}
// 连接巴法云并订阅主题
void memoConnect(){
  if(!memoConfigured()) return;
  if(WiFi.status() != WL_CONNECTED) return;
  if(mqtt.connected()) return;
  Serial.println("正在连接巴法云...");
  // 用户私钥作为clientId，无需用户名密码
  if(mqtt.connect(BEMFA_UID)){
    Serial.println("巴法云连接成功");
    mqtt.subscribe(BEMFA_TOPIC);
  }else{
    Serial.print("巴法云连接失败,状态码:");
    Serial.println(mqtt.state());
  }
}
// 初始化云端连接
void memoBegin(){
  if(!memoConfigured()){
    Serial.println("未配置巴法云(common.h中BEMFA_UID为空)，备忘录推送功能停用");
    return;
  }
  memoLoadFromNVS();
  mqtt.setServer(MEMO_MQTT_SERVER, MEMO_MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(4);
  Serial.println("备忘录功能已就绪");
}
// 主循环中调用，维持连接并处理消息
void memoCloudLoop(){
  if(!memoConfigured()) return;
  if(mqtt.connected()){
    mqtt.loop();
  }else{
    static unsigned long lastTry = 0;
    unsigned long now = millis();
    if(now - lastTry > 30000){ // 每30秒尝试重连一次
      lastTry = now;
      memoConnect();
    }
  }
}
// 新增一条备忘录(来自云端推送)
void memoAddText(String txt){
  txt.replace("\r", "");
  txt.trim();
  if(txt.length() == 0) return;
  if(txt.length() > MEMO_MAX_BYTES) txt = utf8Truncate(txt, MEMO_MAX_BYTES);
  if(txt.equals(lastPushText)) return; // 过滤巴法云保留消息，防止重启后重复添加
  lastPushText = txt;
  if(memoCount >= MEMO_MAX_COUNT){ // 已满时丢弃最旧的一条
    for(int i = 0; i < MEMO_MAX_COUNT - 1; i++){
      memoList[i] = memoList[i + 1];
    }
    memoCount = MEMO_MAX_COUNT - 1;
  }
  memoList[memoCount] = txt;
  memoCount++;
  // 持久化
  memoPrefs.begin("memo", false);
  memoPrefs.putString("lastPush", lastPushText);
  memoPrefs.end();
  memoSaveToNVS();
  Serial.println("收到新备忘录: " + txt);
  // 自动切换到备忘录页面展示新内容
  currentPage = PAGE_MEMO;
  memoPageIdx = 0;
  drawMemoPage();
}
// 清空全部备忘录(长按触发，云端保留消息也不会再回灌)
void memoClearAll(){
  memoCount = 0;
  memoSaveToNVS();
  memoPageIdx = 0;
  drawMemoPage();
}
//////////////////////////////////////////////

///////////////// 页面绘制区域 /////////////////
// 把全部备忘录整理成显示行(自动折行、备忘录之间空一行)
void buildMemoLines(std::vector<String> &lines){
  const int maxWidth = 304; // 两侧各留8像素
  lines.clear();
  for(int m = 0; m < memoCount; m++){
    String txt = memoList[m];
    int i = 0;
    int lineStart = 0;
    int lineWidth = 0;
    while(i < (int)txt.length()){
      if(txt[i] == '\n'){ // 保留用户消息中的换行
        lines.push_back(txt.substring(lineStart, i));
        i++;
        lineStart = i;
        lineWidth = 0;
        continue;
      }
      int i0 = i;
      uint32_t cp = utf8Next(txt, i);
      int cw = (cp < 0x80) ? 8 : 16;
      if(lineWidth + cw > maxWidth && i0 > lineStart){ // 折行
        lines.push_back(txt.substring(lineStart, i0));
        lineStart = i0;
        lineWidth = 0;
      }
      lineWidth += cw;
    }
    lines.push_back(txt.substring(lineStart, i));
    lines.push_back(""); // 备忘录之间空一行
  }
  if(lines.size() > 0) lines.pop_back(); // 去掉最后一个空行
}
// 绘制底部页码/条数指示
void drawMemoFooter(){
  String info = "";
  if(memoCount > 0){
    info = String(memoPageIdx + 1) + "/" + String(memoPages) + "页 " + String(memoCount) + "条";
  }
  clk.setColorDepth(8);
  clk.createSprite(320, 18);
  clk.fillSprite(backFillColor);
  if(info.length() > 0){
    int w = memoTextWidth(info);
    drawMemoString(clk, 312 - w, 2, info, penColor);
  }
  clk.pushSprite(0, 222);
  clk.deleteSprite();
}
// 只重绘内容区(翻页时用)
void drawMemoContent(){
  clk.setColorDepth(8);
  clk.createSprite(320, 174);
  clk.fillSprite(backFillColor);
  if(memoCount == 0){ // 空状态提示
    std::vector<String> tips;
    tips.push_back("暂无备忘录");
    tips.push_back("");
    if(memoConfigured()){
      tips.push_back("手机端: 微信小程序巴法云");
      tips.push_back("电脑端: bemfa.com 控制台");
      tips.push_back("向下面主题推送消息即可");
      tips.push_back(String(BEMFA_TOPIC));
    }else{
      tips.push_back("未配置推送账号");
      tips.push_back("请在common.h中填写");
      tips.push_back("BEMFA_UID 和 BEMFA_TOPIC");
    }
    int y = 40;
    for(size_t i = 0; i < tips.size(); i++){
      int w = memoTextWidth(tips[i]);
      drawMemoString(clk, (320 - w) / 2, y, tips[i], penColor);
      y += 20;
    }
    memoPages = 1;
    memoPageIdx = 0;
  }else{
    std::vector<String> lines;
    buildMemoLines(lines);
    int total = (int)lines.size();
    memoPages = (total + MEMO_LINES_PER_PAGE - 1) / MEMO_LINES_PER_PAGE;
    if(memoPages < 1) memoPages = 1;
    if(memoPageIdx >= memoPages) memoPageIdx = memoPages - 1;
    int start = memoPageIdx * MEMO_LINES_PER_PAGE;
    int end = start + MEMO_LINES_PER_PAGE;
    if(end > total) end = total;
    for(int li = start; li < end; li++){
      int y = (li - start) * 20 + 2;
      drawMemoString(clk, 8, y, lines[li], penColor);
    }
  }
  clk.pushSprite(0, 48);
  clk.deleteSprite();
  drawMemoFooter();
}
// 绘制整个备忘录页面(标题+内容)
void drawMemoPage(){
  reflashTFT();
  // 大标题"备忘录"：3个32x32字模，字间隔8像素，总宽112，居中
  for(int i = 0; i < MEMO_TITLE_COUNT; i++){
    drawMemoTitleGlyph(i, 104 + i * 40, 8);
  }
  tft.drawFastHLine(0, 44, 320, penColor);
  drawMemoContent();
}
// 翻到下一页
void memoNextPage(){
  if(memoPages <= 1) return;
  memoPageIdx = (memoPageIdx + 1) % memoPages;
  drawMemoContent();
}
// 备忘录页面轮询：多页时自动翻页
void memoLoop(){
  static unsigned long last = 0;
  if(memoPages <= 1) return;
  if(millis() - last < MEMO_PAGE_INTERVAL) return;
  last = millis();
  memoNextPage();
}
//////////////////////////////////////////////
