#include <Preferences.h>
#include <time.h>
#include "common.h"
#include "alarm.h"

// 每组闹钟打包存一个 uint32(键 a0~a3)：
//   bit0-5 时   bit6-11 分   bit12 启用   bit13 仅一次   bit14 已响过
static Preferences ap;
static struct Slot {
  bool on;
  uint8_t hour;
  uint8_t minute;
  bool once;
  bool fired;
} slots[ALARM_MAX];

static bool ringing = false;
static unsigned long ringStart = 0;
static long lastMinKey = -1;      // 上次检查过的"日+时:分"，用于每分钟只判一次
static bool beepLevel = false;    // 蜂鸣器当前电平(间歇响用)
static int pendingIdx = -1;       // 响铃期间又到点的下一组(最多排队1组)
static long ringKey = -1;         // 当前响铃对应的"分钟键"
static long pendingKey = -1;      // 排队那组的"分钟键"
// 提示音序列(与闹钟共用蜂鸣器)：元素交替表示 响/停/响… 的毫秒时长
static const uint16_t SEQ_FOCUS[] = {BEEP_FOCUS_ON_MS, BEEP_FOCUS_GAP_MS, BEEP_FOCUS_ON_MS};
static const uint16_t SEQ_REST[]  = {BEEP_REST_ON_MS};
static const uint16_t* seq = nullptr;
static uint8_t seqLen = 0, seqIdx = 0;
static bool seqOn = false;
static uint32_t seqAt = 0;

static void buzzerWrite(bool sound){
#if BUZZER_ACTIVE_LOW
  digitalWrite(BUZZER_PIN, sound ? LOW : HIGH);
#else
  digitalWrite(BUZZER_PIN, sound ? HIGH : LOW);
#endif
}

static uint32_t slotPack(int i){
  const Slot& s = slots[i];
  return (uint32_t)(s.hour & 0x3F) | ((uint32_t)(s.minute & 0x3F) << 6)
       | ((uint32_t)(s.on ? 1 : 0) << 12)
       | ((uint32_t)(s.once ? 1 : 0) << 13)
       | ((uint32_t)(s.fired ? 1 : 0) << 14);
}

static void slotSave(int i){
  ap.begin("cfg", false);
  ap.putUInt((String("a") + i).c_str(), slotPack(i));
  ap.end();
}

static bool slotEffective(int i){
  return slots[i].on && slots[i].hour < 24 && slots[i].minute < 60
      && !(slots[i].once && slots[i].fired);
}

void alarmBegin(){
  pinMode(BUZZER_PIN, OUTPUT);
  buzzerWrite(false);
  ap.begin("cfg", true);
  for(int i = 0; i < ALARM_MAX; i++){
    uint32_t v = ap.getUInt((String("a") + i).c_str(), 0);
    Slot& s = slots[i];
    s.hour   = v & 0x3F;
    s.minute = (v >> 6) & 0x3F;
    s.on     = (v >> 12) & 1;
    s.once   = (v >> 13) & 1;
    s.fired  = (v >> 14) & 1;
    if(s.hour > 23 || s.minute > 59){ s.on = false; s.hour = 0; s.minute = 0; }
  }
  ap.end();
}

bool alarmGet(int i, bool& on, int& hour, int& minute, bool& once, bool& fired){
  if(i < 0 || i >= ALARM_MAX) return false;
  on = slots[i].on;
  hour = slots[i].hour;
  minute = slots[i].minute;
  once = slots[i].once;
  fired = slots[i].fired;
  return true;
}

void alarmSet(int i, bool on, int hour, int minute, bool once){
  if(i < 0 || i >= ALARM_MAX) return;
  slots[i].on = on;
  slots[i].hour = (uint8_t)(hour < 0 ? 0 : (hour > 23 ? 23 : hour));
  slots[i].minute = (uint8_t)(minute < 0 ? 0 : (minute > 59 ? 59 : minute));
  slots[i].once = once;
  slots[i].fired = false;          // 重新保存即重新生效
  slotSave(i);
}

bool alarmRinging(){ return ringing; }

void alarmStop(){
  if(!ringing) return;
  ringing = false;
  pendingIdx = -1;                 // 按键停止 = 当前与排队的都取消
  buzzerWrite(false);
  Serial.println("闹钟已停止");
}

// 开始一组响铃：间歇响，由 alarmLoop 内的 millis() 节拍驱动
static void startRing(int i, long key){
  ringing = true;
  ringStart = millis();
  ringKey = key;
  seq = nullptr;                   // 打断可能还在播的番茄提示音，避免电平互相抢
  seqOn = false;
  beepLevel = true;                // 立刻先响一下
  buzzerWrite(true);
  Serial.println(String("闹钟响: ") + String(slots[i].hour) + ":" + String(slots[i].minute)
               + (slots[i].once ? " (仅一次)" : " (每天)"));
}

// 非阻塞节拍：响 ON_MS / 停 OFF_MS 交替，只在电平变化时写 GPIO
static void updateBeep(){
  unsigned long t = (millis() - ringStart) % (ALARM_BEEP_ON_MS + ALARM_BEEP_OFF_MS);
  bool want = (t < ALARM_BEEP_ON_MS);
  if(want != beepLevel){
    beepLevel = want;
    buzzerWrite(want);
  }
}

// ===== 提示音播放器(与闹钟共用蜂鸣器) =====
static void seqStart(const uint16_t* s, uint8_t len){
  if(ringing) return;              // 闹钟正在响：丢弃本次提示音，不叠加
  if(!s || len == 0) return;
  seq = s; seqLen = len; seqIdx = 0;
  seqOn = true;
  seqAt = millis() + s[0];
  buzzerWrite(true);
}

void beepFocusDone(){ seqStart(SEQ_FOCUS, 3); }   // 响200 / 停120 / 响200
void beepRestDone(){ seqStart(SEQ_REST, 1); }     // 响600

static void seqLoop(){
  if(!seq) return;
  if((int32_t)(millis() - seqAt) < 0) return;
  if(seqOn){                       // 当前是一段"响"，收尾
    seqOn = false;
    buzzerWrite(false);
    if(seqIdx + 1 >= seqLen){ seq = nullptr; return; }   // 全部放完
    seqIdx++;
    seqAt = millis() + seq[seqIdx];
  } else {                         // 当前是间隙，进入下一段"响"
    seqOn = true;
    buzzerWrite(true);
    seqIdx++;
    seqAt = millis() + seq[seqIdx];
  }
}

void alarmLoop(){
  seqLoop();                       // 番茄提示音节拍(响铃时 seq 已被清空)
  // 1) 响铃中：先跑节拍，到 60 秒静音收尾
  if(ringing){
    updateBeep();
    if(millis() - ringStart >= ALARM_RING_MS){
      ringing = false;
      beepLevel = false;
      buzzerWrite(false);          // 超时确保停在静音电平，不留长鸣
      Serial.println("闹钟超时停止");
      if(pendingIdx >= 0){         // 上一组结束后，排队的下一组立刻开始
        int i = pendingIdx;
        pendingIdx = -1;
        startRing(i, pendingKey);
      }
    }
  }
  // 2) 未校时(NTP 尚未成功)不判定
  time_t now = time(nullptr);
  if(now < 100000) return;
  struct tm* t = localtime(&now);
  if(t->tm_year < 120) return;
  // 3) 每分钟只判定一次：时:分 变化时检查，阻塞请求最多迟到 8 秒不会跨过整分钟
  long key = (long)t->tm_year * 366L * 1440L + (long)t->tm_yday * 1440L
           + t->tm_hour * 60 + t->tm_min;
  if(key == lastMinKey) return;
  lastMinKey = key;
  for(int i = 0; i < ALARM_MAX; i++){
    if(!slotEffective(i)) continue;
    if(slots[i].hour != t->tm_hour || slots[i].minute != t->tm_min) continue;
    if(slots[i].once){
      slots[i].fired = true;       // 仅一次：一旦命中即失效(排队中也算)
      slotSave(i);
    }
    if(ringing){
      // 已在响：同一分钟的其它组直接忽略(不叠加，也不排 2 分钟队)；
      // 不同分钟到点的组排队最多 1 组，等当前这轮结束再响
      if(key != ringKey && pendingIdx < 0){
        pendingIdx = i;
        pendingKey = key;
      }
      continue;
    }
    startRing(i, key);
    break;                          // 同一分钟只响一组
  }
}

String alarmJson(){
  String s = "[";
  for(int i = 0; i < ALARM_MAX; i++){
    if(i) s += ",";
    char buf[7];
    snprintf(buf, sizeof(buf), "%02d:%02d", slots[i].hour, slots[i].minute);
    s += String("{\"on\":") + (slots[i].on ? "true" : "false")
       + ",\"t\":\"" + buf + "\""
       + ",\"once\":" + (slots[i].once ? "true" : "false")
       + ",\"fired\":" + (slots[i].fired ? "true" : "false") + "}";
  }
  return s + "]";
}
