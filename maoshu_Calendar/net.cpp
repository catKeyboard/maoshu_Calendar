#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
#include "common.h"
#include "tftUtil.h"
#include "memo.h"
#include "net.h"

// 网络：WiFi连接 + 精简AP配网 + NTP校时
static Preferences netPrefs;
static WebServer* server = nullptr;
static bool portalMode = false;

// 配网模式屏幕提示
static void drawPortalScreen(){
  reflashTFT();
  clk.setColorDepth(16);
  clk.createSprite(320, 240);
  clk.fillSprite(backFillColor);
  drawMemoString(clk, (320 - 160) / 2, 50, "未连接WiFi，进入配网", orangeColor);
  drawMemoString(clk, (320 - 104) / 2, 95, "手机连接热点:", penColor);
  drawMemoString(clk, (320 - 208) / 2, 125, String(AP_SSID) + " 密码 " + AP_PASS, cyanColor);
  drawMemoString(clk, (320 - 88) / 2, 165, "浏览器打开:", penColor);
  drawMemoString(clk, (320 - 88) / 2, 195, "192.168.4.1", cyanColor);
  clk.pushSprite(0, 0);
  clk.deleteSprite();
}

// 配置页面(配网/换网共用：显示当前网络，自动扫描周围WiFi，点击选择)
static const char CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<title>maoshu_Calendar配网</title>
<style>
body{text-align:center;font-size:1.2rem;font-family:sans-serif;max-width:420px;margin:0 auto;padding:12px}
input{width:90%;height:40px;font-size:1rem;margin:6px 0;padding:0 8px;box-sizing:border-box}
button{width:70%;height:42px;font-size:1rem;background:#1E90FF;color:#fff;border:0;border-radius:6px;margin-top:8px}
#cur{padding:8px;background:#f0f7ff;border-radius:6px;font-size:1rem}
#list{text-align:left;margin-top:14px;max-height:45vh;overflow-y:auto}
.ap{padding:10px 8px;border-bottom:1px solid #ddd;cursor:pointer;display:flex;justify-content:space-between}
.ap:active{background:#e3f2fd}
a{color:#1E90FF}
</style></head><body>
<h3>maoshu_Calendar 网络配置</h3>
<div id='cur'>正在获取状态…</div>
<form action='/save' method='post'>
<p><input id='ssid' name='ssid' placeholder='WiFi名称(点下方列表选择)'></p>
<p><input name='pass' type='password' placeholder='WiFi密码'></p>
<p><button type='submit'>保存并重启</button></p>
</form>
<div id='list'>正在搜索周围WiFi…</div>
<p><a href='#' onclick='loadScan();return false'>重新扫描</a></p>
<script>
fetch('/status').then(function(r){return r.json()}).then(function(s){
  var el=document.getElementById('cur');
  if(s.mode==='ap') el.textContent='当前未连接WiFi，请选择网络配置';
  else if(s.ssid) el.textContent='当前网络：'+s.ssid+' ('+s.ip+')';
  else el.textContent='WiFi未连接';
});
function loadScan(){
  var list=document.getElementById('list');
  list.textContent='正在搜索周围WiFi…';
  fetch('/scan').then(function(r){return r.json()}).then(function(d){
    list.innerHTML='';
    if(!d.length){list.textContent='未发现WiFi，请手动输入';return;}
    d.forEach(function(n){
      var div=document.createElement('div');
      div.className='ap';
      var name=document.createElement('span');
      name.textContent=n.ssid;
      var sig=document.createElement('span');
      var s=n.rssi>-55?'强':(n.rssi>-70?'中':'弱');
      sig.textContent=s+' '+n.rssi+'dBm';
      sig.style.color=n.rssi>-55?'#4CAF50':(n.rssi>-70?'#FF9800':'#F44336');
      div.appendChild(name);div.appendChild(sig);
      div.onclick=function(){document.getElementById('ssid').value=n.ssid;window.scrollTo(0,0);};
      list.appendChild(div);
    });
  }).catch(function(){list.textContent='扫描失败，请手动输入';});
}
loadScan();
</script>
</body></html>
)rawliteral";

// 扫描周围WiFi，返回JSON(按信号强度降序，同名去重留最强)
static void handleScan(){
  int n = WiFi.scanNetworks();      // 同步扫描，约2秒
  String json = "[";
  int cnt = 0;
  for(int i = 0; i < n && cnt < 20; i++){
    String ssid = WiFi.SSID(i);
    if(!ssid.length()) continue;    // 跳过隐藏网络
    ssid.replace("\\", "\\\\");     // JSON转义
    ssid.replace("\"", "\\\"");
    if(json.indexOf("\"" + ssid + "\"") >= 0) continue;  // 同名去重
    if(cnt) json += ",";
    json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    cnt++;
  }
  json += "]";
  WiFi.scanDelete();
  server->send(200, "application/json", json);
}

// 当前网络状态
static void handleStatus(){
  String s;
  if(portalMode) s = "{\"mode\":\"ap\"}";
  else if(WiFi.status() == WL_CONNECTED)
    s = "{\"mode\":\"sta\",\"ssid\":\"" + WiFi.SSID() + "\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  else s = "{\"mode\":\"sta\",\"ssid\":\"\",\"ip\":\"\"}";
  server->send(200, "application/json", s);
}

// 保存新凭证并重启
static void handleSave(){
  String ssid = server->arg("ssid");
  String pass = server->arg("pass");
  if(ssid.length()){
    netPrefs.begin("net", false);
    netPrefs.putString("ssid", ssid);
    netPrefs.putString("pass", pass);
    netPrefs.end();
    server->send(200, "text/html", "<meta charset='utf-8'>已保存，设备重启中...");
    delay(1000);
    ESP.restart();
  } else {
    server->send(200, "text/html", "<meta charset='utf-8'>WiFi名称不能为空");
  }
}

static void registerRoutes(){
  server->on("/", [](){ server->send(200, "text/html", CONFIG_HTML); });
  server->on("/scan", handleScan);
  server->on("/status", handleStatus);
  server->on("/save", HTTP_POST, handleSave);
}

static void startPortal(){
  portalMode = true;
  WiFi.mode(WIFI_AP_STA);  // AP+STA双模：STA用于扫描周围WiFi
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.println(String("进入配网模式，热点: ") + AP_SSID);
  drawPortalScreen();
  server = new WebServer(80);
  registerRoutes();
  server->begin();
}

static void tryConnect(const String& ssid, const String& pass){
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("连接WiFi: " + ssid);
  int n = 0;
  while(WiFi.status() != WL_CONNECTED && n < 24){  // 最多等12秒
    delay(500);
    n++;
    Serial.print(".");
  }
  Serial.println();
  if(WiFi.status() == WL_CONNECTED){
    Serial.println("WiFi连接成功, IP: " + WiFi.localIP().toString());
    configTime(8 * 3600, 0, NTP1, NTP2, NTP3);   // NTP校时(UTC+8)
    setenv("TZ", TZ_INFO, 1);
    tzset();
    // 常驻配置服务器：浏览器访问设备IP可查看/更换网络
    server = new WebServer(80);
    registerRoutes();
    server->begin();
  }
}

void netBegin(){
  netPrefs.begin("net", true);
  String ssid = netPrefs.getString("ssid", "");
  String pass = netPrefs.getString("pass", "");
  netPrefs.end();
  if(ssid.length()){
    tryConnect(ssid, pass);
    if(WiFi.status() == WL_CONNECTED) return;
  }
  startPortal();  // 无凭证或连接失败 → 配网模式
}

void netLoop(){
  if(server) server->handleClient();
  if(portalMode) return;
  static unsigned long lastCheck = 0;
  if(millis() - lastCheck > 30000){
    lastCheck = millis();
    if(WiFi.status() != WL_CONNECTED){
      Serial.println("WiFi断线，重连中...");
      WiFi.reconnect();
    }
  }
}

bool netConnected(){
  return !portalMode && WiFi.status() == WL_CONNECTED;
}

String netDiag(){
  if(portalMode) return String("配网热点:") + AP_SSID;
  if(WiFi.status() == WL_CONNECTED) return "IP:" + WiFi.localIP().toString();
  return "WiFi未连接";
}

bool inPortalMode(){
  return portalMode;
}
